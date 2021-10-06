#include <logger.h>

#include <TimeLib.h>


#include <aprspath.h>
#include <aprsmsgmsg.h>

#include "Task.h"
#include "TaskRouter.h"
#include "project_configuration.h"

#include <string>

using std::string;


String create_lat_aprs(double lat);
String create_long_aprs(double lng);

RouterTask::RouterTask(TaskQueue<std::shared_ptr<APRSMessage>> &fromModem, TaskQueue<std::shared_ptr<APRSMessage>> &toModem, TaskQueue<std::shared_ptr<APRSMessage>> &toAprsIs, TaskQueue<std::shared_ptr<APRSMessage>> &fromAprsIs) : Task(TASK_ROUTER, TaskRouter), _fromModem(fromModem), _toModem(toModem), _toAprsIs(toAprsIs), _fromAprsIs(fromAprsIs), _callsign_pn(nullptr) {
}

RouterTask::~RouterTask() {
  if (!(_callsign_pn == nullptr)){
    delete _callsign_pn;
  };

}

/////////////////////////////////////////////////////////////
///////////////////////////// SETUP ////////////////////////
///////////////////////////////////////////////////////////

bool RouterTask::setup(System &system) {

  // validdity checking

  String callsign_uc = system.getUserConfig()->callsign;
  callsign_uc.toUpperCase();

  _callsign_pn = new pathnode(callsign_uc);
  if (!(_callsign_pn->valid)) {
        logPrintD("Error: callsign " + callsign_uc + " does not have a valid format");
        _beaconMsg=NULL;
        return false;
  };


  // create invalid-call list
  String thisinvalidcalls = system.getUserConfig()->digi.invalidcalls;
  
  for (auto invalidcall:splitstr2v( thisinvalidcalls,',')) {
      pathnode pn(invalidcall);

      if (!pn.valid) {
        logPrintlnD("Warning invalid-call: invalid-callsign " + invalidcall + " does not have a valid format");
      } else {
        _invalidcall.push_back(pn);
      };
  };


  // check if my own callsign is not in the invalid-call list
  if (isinvector(_invalidcall,*_callsign_pn, false)) {
    logPrintlnD("Error: callsign " + callsign_uc + " is in the invalid_call list");
    return false;
  };


  // fill "do not digi" list
 String thisdonotdigi = system.getUserConfig()->digi.donotdigi;
  
  for (auto nodigi:splitstr2v(thisdonotdigi,',')){
      pathnode pn(nodigi);

      if (!pn.valid) {
        logPrintlnD("Warning: donotdigi callsign " + nodigi + " does not have a valid format");
      } else {
        _donotdigi.push_back(pn);
      };
  };


  // fill "noaprsis" list
 String thisnoaprsis = system.getUserConfig()->aprs_is.noaprsis;
  
  for (auto noaprsis:splitstr2v(thisnoaprsis,',')){
      pathnode pn(noaprsis);

      if (!pn.valid) {
        logPrintlnD("Warning: noaprsis callsign " + noaprsis + " does not have a valid format");
      } else {
        _noaprsis.push_back(pn);
      };
  };


  // setup beacon
  _beacon_timer.setTimeout(system.getUserConfig()->beacon.timeout * 60 * 1000);

  _beaconMsg = std::shared_ptr<APRSMessage>(new APRSMessage());
  _beaconMsg->setSource(callsign_uc);


  // check destination
  String destination_uc = system.getUserConfig()->digi.destination;
  destination_uc.toUpperCase();

  pathnode destination_pn(destination_uc);

  if (!(destination_pn.valid)) {
        logPrintD("Error: destination " + destination_uc + " does not have a valid format");
        _beaconMsg=NULL;
        return false;
  };

  // destination should not be in the "invalid call" list
  if (isinvector(_invalidcall, destination_pn, false)) {
        logPrintD("Error: destination: " + destination_uc + " in in the invalid-call list!");
        _beaconMsg=NULL;
        return false;
  }

  // destination is OK
  _beaconMsg->setDestination(destination_uc);

  String lat = create_lat_aprs(system.getUserConfig()->beacon.positionLatitude);
  String lng = create_long_aprs(system.getUserConfig()->beacon.positionLongitude);
  _beaconMsg->getBody()->setData(String("=") + lat + "L" + lng + "&" + system.getUserConfig()->beacon.message);

  return true;
}


/////////////////////////////////////////////////////////////
///////////////////////////// LOOP /////////////////////////
///////////////////////////////////////////////////////////

bool RouterTask::loop(System &system) {
  // some config variables .. to be moved to the 'configuration' part later
  int aprsis2RF_wide=1;

  bool continueok;

  // do routing
  // Part 1: Routing from RF
  ///////////////////////////


  if (!_fromModem.empty()) {
    std::shared_ptr<APRSMessage> modemMsg  = _fromModem.getElement();
    std::shared_ptr<APRSMessage> aprsIsMsg = std::make_shared<APRSMessage>(*modemMsg);
    String                       path      = aprsIsMsg->getPath();
    String                       source    = aprsIsMsg->getSource();

    continueok=true;


    // part 1.1: create APRS-IS message (if needed)

    // is aprs_is active in the configuration?
    if (!system.getUserConfig()->aprs_is.active) {
      logPrintlnD("APRS-IS: disabled");
      continueok=false;
    };

    // ignore if receiving my own packet

    if (source == system.getUserConfig()->callsign) {
      logPrintlnD("APRS-IS: no forward => own packet received");
      continueok=false;
    }

    // do not iGate to APRS-IS if path contains "RFONLY", "NOGATE" or "TCPIP"
    if (path.indexOf("RFONLY") != -1 || path.indexOf("NOGATE") != -1 || path.indexOf("TCPIP") != -1) {
      logPrintlnD("APRS-IS: no forward => RFonly");
      continueok=false;
    };


    // continue if ok
    if (continueok) {

      if (!path.isEmpty()) {
        path += ",";
      }

      aprsIsMsg->setPath(path + "qAR," + system.getUserConfig()->callsign);

      logPrintD("APRS-IS: ");
      logPrintlnD(aprsIsMsg->toString());
      _toAprsIs.addElement(aprsIsMsg);
          
    }



    // part 1.2: RF digipeat
    continueok=true;

    if (!(system.getUserConfig()->digi.active)) continueok=false;
    if (modemMsg->getSource() == system.getUserConfig()->callsign) continueok=false;

    // continue if OK
    if (continueok) {
      std::shared_ptr<APRSMessage> digiMsg = std::make_shared<APRSMessage>(*modemMsg);
      String                       path    = digiMsg->getPath();
      String                       callsign_uc  = system.getUserConfig()->callsign;
      callsign_uc.toUpperCase();

      // special case: messagetype = Message
      // check if the message is addressed to me

      if (digiMsg->getType() == APRSMessageType::Message) {

        String body = digiMsg->getRawBody();
        
        aprsmsgmsg amsg = aprsmsgmsg(body);

        if (!amsg.valid) {

          logPrintlnD("aprs-message: invalid format of message");

        } else  {

          // is this message for me
          if (_callsign_pn->equalcall(* amsg.callsign, true)) {

            // this message is for me

            // we do three things
            // 1: send ack (if needed)
            // 2: send message "Hello from LoRa_APRS_Igate"
            // 3: ignore "ack" that should return

            // ignore "ack" messages (step 3)
            if (!amsg.isack) {

              // send ack (if needed) (step 1)
              if (amsg.hasack) {
                digiMsg->getBody()->setData(String("ack"+amsg.msgno));
                _toModem.addElement(digiMsg);
              }

              // send message (step 2)
              digiMsg->getBody()->setData(String("Hello from LoRA_APRS_iGate/ON1ARF{001"));
              _toModem.addElement(digiMsg);

            } // end (isack)

          } // end (message to me)

        } // end (amsg is valid)

        // done :: set 'continueok' to false to stop further execution
        continueok=false;

      }; // end (special case: aprs message)




      // create aprs path
      aprspath apath(system.getUserConfig()->digi.maxhopnowide);

      // continue if ok
      if (continueok) {
        // go over list, create aprsapth
        for (auto thisnode:splitstr2v(path,',')) {
          pathnode pn=pathnode(thisnode);
          if (!pn.valid) {
            logPrintlnD("Error: invalid pathnode " + thisnode + "  ... ignoring packet");
            continueok=false;
            break;
          }

          // check if node is in the "invalid call" list
          if (isinvector(_invalidcall, pn, false)) {
            logPrintlnI("Error: pathnode "+thisnode+" is in the invalid-call list");
            continueok=false;
            break;
          }

          apath.appendnodetopath(pn);
        }; // end for

      }


      // continue if ok
      if (continueok) {
        // check total path
        continueok=apath.checkpath();
        if (!continueok) logPrintlnD("Error: Checkpath failed");
      }


      // continue if ok
      if (continueok) {
        // loopcheck
        continueok=apath.doloopcheck(callsign_uc);        
        if (!continueok) logPrintlnD("Error: loopcheck failed");
      }



      // create a pathnode object from the source
      pathnode p_source = pathnode(source);

      // continue if ok
      if (continueok) {
        continueok=p_source.valid;
        if (!continueok) logPrintlnI("Error: source "+source+" has an invalid format");
      }

      // continue if ok
      if (continueok) {
        // source call should not be in invalid-call list

        if (isinvector(_invalidcall, p_source, false)) {
          logPrintlnI("Error: source "+source+" is in the invalid-call list");
          continueok=false;
        }
      }


      // "do not digi" checking: check if "lastnode" or the source is in the "do-not-digi' list

      // continue if ok
      if (continueok) {
        // check the "lastnode" if it exists
        // note: "getlastnode" will only work correctly if a "checkpath" has been done
        pathnode* ln = apath.getlastnode();

        // if lastnode exist, check if it is the invalid call list
        if (ln) {

          if (isinvector(_donotdigi,*ln, true)) {
            logPrintlnD("Error: lastnode " + ln->pathnode2str() + " is in the do-not-digi list");
            continueok=false;
          };

        } else {
          // path is empty, check if the source is in the do-not-digi call list

          if (isinvector(_donotdigi, p_source, true)) {
            logPrintlnI("Error: source "+source+" is in the do-not-digi list");
            continueok=false;
          };

        };
      };


      // continue if ok
      if (continueok) {
        // add mycall to path
        continueok=apath.adddigi(callsign_uc, system.getUserConfig()->digi.fillin);
        if (!continueok) logPrintlnI("Info: adddigi returnd False for node " + callsign_uc);
      };


      // continue if ok
      if (continueok) {
        // done. create new path
        logPrintlnD("DIGI NEW PATH: "+ apath.printfullpath());
        digiMsg->setPath(apath.printfullpath());

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

    continueok=true;

    // only formard "message" type of APRSMsgages
    if (aprsIsMsg->getType() != APRSMessageType::Message) {
      continueok=false;
    };


    pathnode source_pn = pathnode(source);

    if (!(source_pn.valid)) {
      logPrintlnD("APRS-IS MSG: Error: Source message of APRS-IS has invalid format");
      continueok=false;
    }

    // special case: messagetype = Message
    // check if the message is addressed to me

    if (continueok) {

      String body = aprsIsMsg->getRawBody();
      
      aprsmsgmsg amsg = aprsmsgmsg(body);

      aprsmsgmsg replymsg = aprsmsgmsg(); // reply message

      logPrintlnD("aprs-message: INSIDE aprsis");
      logPrintlnD("*"+amsg.callsign->pathnode2str()+"*");
      logPrintlnD("*"+_callsign_pn->pathnode2str()+"*");


      if (!amsg.valid) {

        logPrintlnD("aprs-message: invalid format of message");

      } else  {


        
        logPrintlnD("aprs-message: valid");
        // is this message for me

        if (amsg.isack) {
          logPrintlnD("aprs-message: IS ACK");
        }else {
          logPrintlnD("aprs-message: NOT IS ACK");
        }

        if (amsg.hasack) {
          logPrintlnD("aprs-message: HAS ACK");
        }else {
          logPrintlnD("aprs-message: NOT HAS ACK");
        }

        if (_callsign_pn->equalcall(*(amsg.callsign), true)) {
          logPrintlnD("aprs-message: EQUAL");
        } else {
          logPrintlnD("aprs-message: NOT EQUAL");
        }

        if (_callsign_pn->equalcall(*(amsg.callsign), true)) {
          logPrintlnD("aprs-message: for ME");

 


          // this message is for me

          // we do three things
          // 1: send ack (if needed)
          // 2: send message "Hello from LoRa_APRS_Igate"
          // 3: ignore "ack" that should return

          // ignore "ack" messages (step 3)
          if (!amsg.isack) {

            // create reply message
            replymsg.callsign=&source_pn;; // reply back to original sender

            replymsg.valid=true;
  


            // send ack (if needed) (step 1)
            if (amsg.hasack) {
              replymsg.isack=true;
              replymsg.msgno=amsg.msgno;
              aprsIsMsg->getBody()->setData(replymsg.fulltxt());
              logPrintlnD(replymsg.fulltxt());

              aprsIsMsg->setSource(_callsign_pn->pathnode2str());
              aprsIsMsg->setDestination(system.getUserConfig()->digi.destination);
              _toAprsIs.addElement(aprsIsMsg);
            }

            // send message (step 2)
//            replymsg.isack=false;
//            replymsg.hasack=true;
//            replymsg.body=String("Hello from LoRA_APRS_iGate/ON1ARF");
//            replymsg.msgno=1;
//            aprsIsMsg->getBody()->setData(replymsg.fulltxt());
//            logPrintlnD(replymsg.fulltxt());

//            aprsIsMsg->setSource(_callsign_pn->pathnode2str());
//            aprsIsMsg->setDestination(system.getUserConfig()->digi.destination);

            // DEBUG
            //_toAprsIs.addElement(aprsIsMsg);
            

          } // end (isack)

        } // end (message to me)

      } // end (amsg is valid)

      // done :: set 'continueok' to false to stop further execution
      continueok=false;

    }; // end (special case: aprs message)



    // continue if OK  
    if (continueok){
      String newpath="TCPIP," + _callsign_pn->pathnode2str() + "*,";

      if (aprsis2RF_wide==0) {
        newpath += "WIDE1-0";
      } else {
        newpath += "WIDE1-1";
      }

      logPrintlnI("APRSIS: "+source+" "+path+" "+dest+ " "+body);

      // quick hack to see if it works!
      // we only need to change the path and send out the message over the Lora RF interface
      logPrintlnI("APRSIS2RF NEW PATH: "+newpath);
      aprsIsMsg->setPath(newpath);

      logPrintlnI("DIGI: "+ aprsIsMsg->toString());
      _toModem.addElement(aprsIsMsg);
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
