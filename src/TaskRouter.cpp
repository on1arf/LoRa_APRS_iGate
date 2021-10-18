#include <logger.h>

#include <TimeLib.h>

#include <memory>

#include <aprspath.h>
#include <aprsmsgmsg.h>

#include "Task.h"
#include "TaskRouter.h"
#include "project_configuration.h"

using std::make_shared;

String create_lat_aprs(double lat);
String create_long_aprs(double lng);

RouterTask::RouterTask(TaskQueue<std::shared_ptr<APRSMessage>> &fromModem, TaskQueue<std::shared_ptr<APRSMessage>> &toModem, TaskQueue<std::shared_ptr<APRSMessage>> &toAprsIs, TaskQueue<std::shared_ptr<APRSMessage>> &fromAprsIs) : Task(TASK_ROUTER, TaskRouter), _fromModem(fromModem), _toModem(toModem), _toAprsIs(toAprsIs), _fromAprsIs(fromAprsIs), _callsign_pn(make_shared<pathnode>()), _msghist(make_shared<msghist>()) {
}

RouterTask::~RouterTask() {
}



/////////////////////////////////////////////////////////////
///////////////////////////// SETUP ////////////////////////
///////////////////////////////////////////////////////////

bool RouterTask::setup(System &system) {

  // validity checking
  String callsign_uc = system.getUserConfig()->callsign;
  callsign_uc.toUpperCase();

  _callsign_pn->configure(callsign_uc, normalnode);
  if (!(_callsign_pn->valid)) {
        logPrintD("Error: callsign " + callsign_uc + " does not have a valid format");
        _beaconMsg=NULL;
        return false;
  };


  // create invalid-call list
  String thisinvalidcalls = system.getUserConfig()->digi.invalidcalls;
  
  for (auto invalidcall:splitstr2v( thisinvalidcalls,',')) {
      shared_ptr<pathnode>  pn = make_shared<pathnode> (invalidcall, normalnode);

      if (!pn->valid) {
        logPrintlnD("Warning invalid-call: invalid-callsign " + invalidcall + " does not have a valid format");
      } else {
        _invalidcall.push_back(pn);
      };
  };


  // check if my own callsign is not in the invalid-call list
  if (isinvector(_invalidcall,_callsign_pn, nocheckssid)) {
    logPrintlnD("Error: callsign " + callsign_uc + " is in the invalid_call list");
    return false;
  };


  // create "do not digi" list
 String thisdonotdigi = system.getUserConfig()->digi.donotdigi;
  
  for (auto nodigi:splitstr2v(thisdonotdigi,',')){
      shared_ptr<pathnode> pn = make_shared<pathnode> (nodigi, normalnode);

      if (!pn->valid) {
        logPrintlnD("Warning: donotdigi callsign " + nodigi + " does not have a valid format");
      } else {
        _donotdigi.push_back(pn);
      };
  };


  // create "noaprsis" list
 String thisnoaprsis = system.getUserConfig()->aprs_is.noaprsis;
  
  for (auto noaprsis:splitstr2v(thisnoaprsis,',')){
      shared_ptr<pathnode> pn = make_shared<pathnode> (noaprsis, normalnode);

      if (!pn->valid) {
        logPrintlnD("Warning: noaprsis callsign " + noaprsis + " does not have a valid format");
      } else {
        _noaprsis.push_back(pn);
      };
  };



  // check destination
  String destination_uc = system.getUserConfig()->digi.destination;
  destination_uc.toUpperCase();

  shared_ptr<pathnode> destination_pn = make_shared<pathnode> (destination_uc, normalnode);

  if (!(destination_pn->valid)) {
        logPrintD("Error: destination " + destination_uc + " does not have a valid format");
        _beaconMsg=NULL;
        return false;
  };

  // destination should not be in the "invalid call" list
  if (isinvector(_invalidcall, destination_pn, nocheckssid)) {
        logPrintD("Error: destination: " + destination_uc + " in in the invalid-call list!");
        _beaconMsg=NULL;
        return false;
  }


  // setup beacon
  _beacon_timer.setTimeout(system.getUserConfig()->beacon.timeout * 60 * 1000);

  _beaconMsg = std::shared_ptr<APRSMessage>(new APRSMessage());
  _beaconMsg->setSource(callsign_uc);


  // destination is OK
  _beaconMsg->setDestination(destination_uc);

  String lat = create_lat_aprs(system.getUserConfig()->beacon.positionLatitude);
  String lng = create_long_aprs(system.getUserConfig()->beacon.positionLongitude);
  _beaconMsg->getBody()->setData(String("=") + lat + "L" + lng + "&" + system.getUserConfig()->beacon.message);


  // configure message history
  // should be moved to configuration parameters later on
  _msghist->configure(1800000, 900000, 10000); // keeptime is 30 minutes, keep_short time is 15 minutes, dup-check is 10 seconds

  return true;
}


/////////////////////////////////////////////////////////////
///////////////////////////// LOOP /////////////////////////
///////////////////////////////////////////////////////////

bool RouterTask::loop(System &system) {
  // some config variables .. to be moved to the 'configuration' part later
 // int aprsis2RF_wide=1;

  bool continueok;

  // do routing
  // Part 1: Routing from RF
  ///////////////////////////




  if (!_fromModem.empty()) {
    std::shared_ptr<APRSMessage> modemMsg  = _fromModem.getElement();
    std::shared_ptr<APRSMessage> aprsIsMsg = std::make_shared<APRSMessage>(*modemMsg);
    String                       path      = aprsIsMsg->getPath();
    String                       source    = aprsIsMsg->getSource();
    String                       dest      = aprsIsMsg->getDestination();

    shared_ptr<aprspath> apath=make_shared<aprspath>(system.getUserConfig()->digi.maxhopnowide); // create aprs path object


    continueok=true;

    // part 1.1: create APRS-IS message

    // is aprs_is active in the configuration?
    if (!system.getUserConfig()->aprs_is.active) {
      logPrintlnD("APRS-IS: disabled");
      continueok=false;
    };

    // ignore if receiving my own packet
    if (continueok) { // continue if ok
      if (source == system.getUserConfig()->callsign) {
        logPrintlnD("APRS-IS: no forward => own packet received");
        continueok=false;
      };
    };


    if (continueok) { // continue if OK
      // create aprspath object based on path (txt) 
      continueok=apath->appendpathtxttopath(path, _invalidcall);      
    }


    if (continueok) {  // continue if ok
      // check total path
      continueok=apath->checkpath();
      if (!continueok) {
        logPrintlnD("Error: Checkpath failed");
        continueok=false;
      }
    }


    // check if any element in the path is in the "no-aprsis" list (e.g."NOGATE" or "RFONLY")
    if (continueok) {  // continue if ok
      for (auto pn:apath->getpath()) {
        if (isinvector(_noaprsis,pn,nocheckssid)) {
          logPrintlnD("APRS-IS: no forward => RFonly");
          continueok=false;
          break;
        }
      };
    };


    // ignore message if it has already been received during the "dup" periode
    if (continueok) {
      if (_msghist->checkexist_dup(aprsIsMsg)) {
        continueok=false;
      };

    };


    // register message
    if (continueok) {
      _msghist->receivemsg(aprsIsMsg,apath->getpathlength());
    }

    if (continueok) {     // continue if ok
      // add "qAR" if directly-heared or up to 1 hop away
      // should be moved to a configuration-variable
      logPrintD("Pathlength: ");
      logPrintlnD(apath->printfullpath());
      logPrintlnD(String(apath->getpathlength()));

      if (apath->getpathlength() <= 1) {
        shared_ptr<pathnode> pn=make_shared<pathnode>("qAR",specialnode); // create "special" node qAR (bidirectional gateway)
        apath->appendnodetopath(pn);
      } else {
        shared_ptr<pathnode> pn=make_shared<pathnode>("qAO",specialnode); // create "special" node qAO (receive only iGate)
        apath->appendnodetopath(pn);
      };

      // next, add callsign to path
      // remove "digipeat" from my callsign (just to be sure)
      _callsign_pn->digipeat=false;
      apath->appendnodetopath(_callsign_pn);

      // create new path
      aprsIsMsg->setPath(apath->printfullpath());
      
      // transmit it
      logPrintD("APRS-IS: ");
      logPrintlnD(apath->printfullpath());
      logPrintlnD(source);
      logPrintlnD(dest);
      logPrintlnD(apath->printfullpath());
      logPrintlnD(aprsIsMsg->toString());
      _toAprsIs.addElement(aprsIsMsg);
    }



    // part 1.2: RF digipeat
    continueok=true;

    if (!(system.getUserConfig()->digi.active)) continueok=false; // stop if "digi.active" is disabled
    if (modemMsg->getSource() == system.getUserConfig()->callsign) continueok=false; // stop if my own callsign

    if (continueok) {     // continue if OK

      std::shared_ptr<APRSMessage> digiMsg = std::make_shared<APRSMessage>(*modemMsg);
      String                       path    = digiMsg->getPath();


      // find "lowest" message, if in a "3th party data" hierarchy
      APRSMessage *AMessage = aprsIsMsg->getLowestMessage(); // returns pointer to first 'non-3th party' APRS message

      // special case: messagetype = Message
      // check if the message is addressed to me

      aprsmsgmsg replymsg = aprsmsgmsg(); // reply message

      if (AMessage->getType() == APRSMessageType::Message) {
        String body = AMessage->getRawBody();
        
        aprsmsgmsg amsg = aprsmsgmsg(body); // create "aprs message" message

        if (!amsg.valid) {

          logPrintlnD("aprs-message: invalid format of message");

        } else  {

          // is this message for me?
          if (_callsign_pn->equalcall(amsg.callsign, docheckssid)) {

            // Yes, this message is for me

            // if not an 'ack' message (which we ignore)
            if (!amsg.isack) {

              // send ack (if needed)
              if (amsg.hasack) {
                replymsg.isack=true;
                replymsg.msgno=amsg.msgno;
                digiMsg->getBody()->setData(replymsg.fulltxt());
                logPrintlnD(replymsg.fulltxt());

                digiMsg->setSource(_callsign_pn->pathnode2str());
                digiMsg->setDestination(system.getUserConfig()->digi.destination);
                _toModem.addElement(aprsIsMsg);
              }

            } // end (isack)

            // done :: set 'continueok' to false to stop further execution
            continueok=false;

          } // end (message to me)

        } // end (amsg is valid)


      }; // end (special case: aprs message)




      // create aprs path
      shared_ptr<aprspath> apath = make_shared<aprspath>(system.getUserConfig()->digi.maxhopnowide);

      if (continueok) {       // continue if ok
        // create aprspath object based on path (txt) 
        continueok=apath->appendpathtxttopath(path, _invalidcall);
      }

      if (continueok) {       // continue if ok
        // check total path
        continueok=apath->checkpath();
        if (!continueok) logPrintlnD("Error: Checkpath failed");
      }


      if (continueok) {       // continue if ok
        // loopcheck
        continueok=apath->doloopcheck(_callsign_pn);        
        if (!continueok) logPrintlnD("Error: loopcheck failed");
      }



      // create a pathnode object from the source
      shared_ptr<pathnode> p_source = make_shared<pathnode>(source, normalnode);

      // continue if ok
      if (continueok) {
        continueok=p_source->valid;
        if (!continueok) logPrintlnI("Error: source "+source+" has an invalid format");
      }

      // continue if ok
      if (continueok) {
        // source call should not be in invalid-call list

        if (isinvector(_invalidcall, p_source, nocheckssid)) {
          logPrintlnI("Error: source "+source+" is in the invalid-call list");
          continueok=false;
        }
      }


      // "do not digi" checking: check if "lastnode" or the source is in the "do-not-digi' list

      if (continueok) {     // continue if ok
        // check the "lastnode" if it exists
        // note: "getlastnode" will only work correctly if a "checkpath" has been done
        shared_ptr<pathnode> * ln = apath->getlastnode();

        // if lastnode exist, check if it is the invalid call list
        if (ln != nullptr) {
          if (isinvector(_donotdigi,*ln, docheckssid)) {
            logPrintlnD("Error: lastnode " + (*ln)->pathnode2str() + " is in the do-not-digi list");
            continueok=false;
          };
        } else {
          // lastnode does not exist, (path is empty)
          // , check if the source is in the do-not-digi call list
          if (isinvector(_donotdigi, p_source, docheckssid)) {
            logPrintlnI("Error: source "+source+" is in the do-not-digi list");
            continueok=false;
          };
        };
      };


      if (continueok) {       // continue if ok
        // add mycall to path
        continueok=apath->adddigi(_callsign_pn, system.getUserConfig()->digi.fillin);
        if (!continueok) logPrintlnI("Info: adddigi failed for node " + _callsign_pn->pathnode2str());
      };


      if (continueok) {       // continue if ok
        // done. print and transmit new path
        logPrintlnD("DIGI NEW PATH: "+ apath->printfullpath());
        digiMsg->setPath(apath->printfullpath());

        logPrintlnD("DIGI: "+ digiMsg->toString());
        _toModem.addElement(digiMsg);
      
      }

    }

 }


  // Part 2:
  // From APRS-iS
  /////////////////

   if (!_fromAprsIs.empty()) {
    std::shared_ptr<APRSMessage> aprsIsMsg = _fromAprsIs.getElement();
    String                       path      = aprsIsMsg->getPath();
    String                       source    = aprsIsMsg->getSource();
    String                       body      = aprsIsMsg->getRawBody();
    String                       dest      = aprsIsMsg->getDestination();

    String msgmsgdest;
    int destfound;

    continueok=true;

    std::shared_ptr<pathnode> source_pn = std::make_shared<pathnode> (source, normalnode);
    //std::shared_ptr<pathnode> source_pn(new pathnode(source, normalnode));
    if (!(source_pn->valid)) {
      logPrintlnD("APRS-IS MSG: Error: Source message of APRS-IS has invalid format");
      continueok=false;
    }


    ////////////////////////////////
    // special case: messagetype = Message
    // check if the message is addressed to me
    APRSMessage *AMessage = aprsIsMsg->getLowestMessage(); // returns pointer to first 'non-3th party' APRS message

    // only formard "message" type of APRSMsgages
    if (AMessage->getType() != APRSMessageType::Message) {
      continueok=false;
    };


    // reset "msg destination", just to be sure
    msgmsgdest="";

    if (continueok) {
      String body = AMessage->getRawBody();
      std::shared_ptr<aprsmsgmsg> amsg = std::make_shared<aprsmsgmsg>(body);
      std::shared_ptr<aprsmsgmsg> replymsg = std::make_shared<aprsmsgmsg> ();


      if (!amsg->valid) {
        logPrintlnD("aprs-message: invalid format of message");
        // mark message as invalid
        continueok=false;

      } else  {

        if (_callsign_pn->equalcall((amsg->callsign), true)) {   
          // this message is for me
          // 1: send ack (if needed)

          // ignore "ack" messages (step 3)
          if (!amsg->isack) {
            // create reply message
            replymsg->callsign=source_pn;; // reply back to original sender
            replymsg->valid=true;
  
            // send ack (if needed) (step 1)
            if (amsg->hasack) {
              replymsg->isack=true;
              replymsg->msgno=amsg->msgno;
              aprsIsMsg->getBody()->setData(replymsg->fulltxt());
              logPrintlnD(replymsg->fulltxt());

              aprsIsMsg->setPath("");
              aprsIsMsg->setSource(_callsign_pn->pathnode2str());
              aprsIsMsg->setDestination(system.getUserConfig()->digi.destination);
              _toAprsIs.addElement(aprsIsMsg);
            }

          } // end (isack)

          // done :: set 'continueok' to false to stop further execution
          continueok=false;
        }; // end (message to me)

        // store msgmsg destination (we will need it later)
        msgmsgdest=amsg->callsign->pathnode2str();


      } // end (amsg is valid)
    }; // continueok

    // the message is not directed to me, forward to RF if the destination has been heared on the RF side

    if (continueok) {
      destfound=_msghist->checkexist_sender(msgmsgdest);

      // check if the destination of the message is known on RF
      // if not, stop further processing
      if (destfound==found_notfound) {
        logPrintlnI("APRSIS-IN: Received message to unknown destination"+source+" "+path+" "+dest+ " "+body);
        continueok=false;
      } 
    }


    // continue if OK  
    if (continueok){
      logPrintlnI("APRSIS-IN: "+source+" "+path+" "+dest+ " " + destfound + " " +body);

      // create thirdParty Message: a copy of the original text with modified path
      aprsIsMsg->getBody()->setData("}"+source+">"+dest+",TCPIP,"+_callsign_pn->pathnode2str() + "*:" + body);
      aprsIsMsg->setDestination(system.getUserConfig()->digi.destination);
      aprsIsMsg->setSource(_callsign_pn->pathnode2str());


      /*
      if (aprsis2RF_wide==0) {
        aprsIsMsg->setPath("WIDE1-0");
      } else {
        aprsIsMsg->setPath("WIDE1-1");
      }
      */

      // this has to be corrected in the future
      // distance should the distance as received when "found_shortdist"
      // else, if found later, go to WIDE1-1
      aprsIsMsg->setPath("WIDE1-0");

      logPrintlnI("APRSIS2RF: "+source+" "+path+" "+dest+ " "+body);

      logPrintlnI("DIGI: "+ aprsIsMsg->toString());
      _toModem.addElement(aprsIsMsg);

      // add message into msghistory, for duplicate-message detection
      _msghist->receivemsg(aprsIsMsg,1);
    }

   };
 
  // part 3: transmit beacon message
  if (_beaconMsg ) {

    if (_beacon_timer.check()) {
      logPrintD("[" + timeString() + "] ");
      logPrintlnD(_beaconMsg->encode());

      if (system.getUserConfig()->aprs_is.active)
        _toAprsIs.addElement(_beaconMsg);

      if (system.getUserConfig()->digi.beacon) {
        _toModem.addElement(_beaconMsg);
      }

      system.getDisplay().addFrame(std::shared_ptr<DisplayFrame>(new TextFrame("BEACON", _beaconMsg->toString())));
      _beacon_timer.start();
    }

    uint32_t diff = _beacon_timer.getTriggerTimeInSec();
    _stateInfo    = "beacon " + String(uint32_t(diff / 60)) + ":" + String(uint32_t(diff % 60));
  };

  return true;
}
