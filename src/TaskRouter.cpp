#include <logger.h>

#include <TimeLib.h>


#include <aprspath.h>

#include "Task.h"
#include "TaskRouter.h"
#include "project_configuration.h"

#include <string>

using std::string;


String create_lat_aprs(double lat);
String create_long_aprs(double lng);

RouterTask::RouterTask(TaskQueue<std::shared_ptr<APRSMessage>> &fromModem, TaskQueue<std::shared_ptr<APRSMessage>> &toModem, TaskQueue<std::shared_ptr<APRSMessage>> &toAprsIs) : Task(TASK_ROUTER, TaskRouter), _fromModem(fromModem), _toModem(toModem), _toAprsIs(toAprsIs) {
}

RouterTask::~RouterTask() {
}

bool RouterTask::setup(System &system) {
  // Added ON1ARF 202010914
  // sanity checking
  String callsign_tmp = system.getUserConfig()->callsign;
  callsign_tmp.toUpperCase();
  String mycall;

  pathnode callsign_pn(callsign_tmp);
  if (!(callsign_pn.valid)) {
        logPrintD("Error: mycall " + callsign_tmp + " does not have a valid format");
        _beaconMsg=NULL;
        return false;
  };

  // mycall is OK
  mycall=callsign_tmp;


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


  // check if own callsign is not in the invalid-call list
  if (isinvector(_invalidcall,callsign_pn, false)) {
    logPrintlnD("Error: mycall " + callsign_tmp + " is in the invalid_call list");
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


  // End ON1ARF


  // setup beacon
  _beacon_timer.setTimeout(system.getUserConfig()->beacon.timeout * 60 * 1000);

  _beaconMsg = std::shared_ptr<APRSMessage>(new APRSMessage());
  _beaconMsg->setSource(mycall);


  // check destination
  String destination_tmp = system.getUserConfig()->digi.destination;
  destination_tmp.toUpperCase();

  pathnode destination_pn(destination_tmp);

  if (!(destination_pn.valid)) {
        logPrintD("Error: destination " + destination_tmp + " does not have a valid format");
        _beaconMsg=NULL;
        return false;
  };

  // destination should begin with an 'A'
  if (!(destination_tmp.startsWith("A"))) {
        logPrintD("Error: Invalid destination: " + destination_tmp + " should start with an A");
        _beaconMsg=NULL;
        return false;
  };

  // destination should not be in the "invalid call" list
  if (isinvector(_invalidcall, destination_pn, false)) {
        logPrintD("Error: destination: " + destination_tmp + " in in the invalid-call list!");
        _beaconMsg=NULL;
        return false;
  }

  // destination is OK
  _beaconMsg->setDestination(destination_tmp);

  String lat = create_lat_aprs(system.getUserConfig()->beacon.positionLatitude);
  String lng = create_long_aprs(system.getUserConfig()->beacon.positionLongitude);
  _beaconMsg->getBody()->setData(String("=") + lat + "L" + lng + "&" + system.getUserConfig()->beacon.message);

  return true;
}

bool RouterTask::loop(System &system) {
  // do routing
  if (!_fromModem.empty()) {
    std::shared_ptr<APRSMessage> modemMsg  = _fromModem.getElement();
    std::shared_ptr<APRSMessage> aprsIsMsg = std::make_shared<APRSMessage>(*modemMsg);
    String                       path      = aprsIsMsg->getPath();
    String                       source    = aprsIsMsg->getSource();

    // part 1: create APRS-IS message (if needed)

    if (system.getUserConfig()->aprs_is.active && source != system.getUserConfig()->callsign) {

      // do not iGate to APRS-IS if path contains "RFONLY", "NOGATE" or "TCPIP"

      if (!(path.indexOf("RFONLY") != -1 || path.indexOf("NOGATE") != -1 || path.indexOf("TCPIP") != -1)) {

        if (!path.isEmpty()) {
          path += ",";
        }

        aprsIsMsg->setPath(path + "qAR," + system.getUserConfig()->callsign);

        logPrintD("APRS-IS: ");
        logPrintlnD(aprsIsMsg->toString());
        _toAprsIs.addElement(aprsIsMsg);
      } else {
        logPrintlnD("APRS-IS: no forward => RFonly");
      }

    } else {
      if (!system.getUserConfig()->aprs_is.active)
        logPrintlnD("APRS-IS: disabled");

      if (modemMsg->getSource() == system.getUserConfig()->callsign)
        logPrintlnD("APRS-IS: no forward => own packet received");
    }


    // part 2: RF digipeat
    if (system.getUserConfig()->digi.active && modemMsg->getSource() != system.getUserConfig()->callsign) {
      std::shared_ptr<APRSMessage> digiMsg = std::make_shared<APRSMessage>(*modemMsg);
      String                       path    = digiMsg->getPath();
      String                       thiscall  = system.getUserConfig()->callsign;
      thiscall.toUpperCase();

      // create aprs path
      aprspath apath(system.getUserConfig()->digi.maxhopnowide);


      bool receivedok=true;

      // go over list, create aprsapth
      for (auto thisnode:splitstr2v(path,',')) {
        pathnode pn=pathnode(thisnode);
        if (!pn.valid) {
          logPrintlnD("Error: invalid pathnode " + thisnode + "  ... ignoring packet");
          receivedok=false;
          break;
        }

        // check if node is in the "invalid call" list
        if (isinvector(_invalidcall, pn, false)) {
          logPrintlnI("Error: pathnode "+thisnode+" is in the invalid-call list");
          receivedok=false;
          break;
        }

        apath.appendnodetopath(pn);
      }; // end for


      // continue if ok
      if (receivedok) {
        // check total path
        receivedok=apath.checkpath();
        if (!receivedok) logPrintlnD("Error: Checkpath failed");
      }


      // continue if ok
      if (receivedok) {
        // loopcheck
        receivedok=apath.doloopcheck(thiscall);        
        if (!receivedok) logPrintlnD("Error: loopcheck failed");
      }



      // create a pathnode object from the source
      pathnode p_source = pathnode(source);

      // continue if ok
      if (receivedok) {
        receivedok=p_source.valid;
        if (!receivedok) logPrintlnI("Error: source "+source+" has an invalid format");
      }

      // continue if ok
      if (receivedok) {
        // source call should not be in invalid-call list

        if (isinvector(_invalidcall, p_source, false)) {
          logPrintlnI("Error: source "+source+" is in the invalid-call list");
          receivedok=false;
        }
      }


      // "do not digi" checking: check if "lastnode" or the source is in the "do-not-digi' list

      // continue if ok
      if (receivedok) {
        // check the "lastnode" if it exists
        // note: "getlastnode" will only work correctly if a "checkpath" has been done
        pathnode* ln = apath.getlastnode();

        // if lastnode exist, check if it is the invalid call list
        if (ln) {

          if (isinvector(_donotdigi,*ln, true)) {
            logPrintlnD("Error: lastnode " + ln->pathnode2str() + " is in the do-not-digi list");
            receivedok=false;
          };

        } else {
          // path is empty, check if the source is in the do-not-digi call list

          if (isinvector(_donotdigi, p_source, true)) {
            logPrintlnI("Error: source "+source+" is in the do-not-digi list");
            receivedok=false;
          };

        };
      };


      // continue if ok
      if (receivedok) {
        // add mycall to path
        receivedok=apath.adddigi(thiscall, system.getUserConfig()->digi.fillin);
        if (!receivedok) logPrintlnI("Info: adddigi returned False for node " + thiscall);
      };


      // continue if ok
      if (receivedok) {
        // done. create new path
        logPrintlnD("DIGI NEW PATH: "+ apath.printfullpath());
        digiMsg->setPath(apath.printfullpath());

        logPrintlnD("DIGI: "+ digiMsg->toString());
        _toModem.addElement(digiMsg);
      
      }

    }

 }

 
  // part 3: check for beacon
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
