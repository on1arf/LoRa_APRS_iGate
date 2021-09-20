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

  pathnode callsign_pn(callsign_tmp);
  if (!(callsign_pn.valid)) {
        logPrintD("Error: mycall ");
        logPrintD(callsign_tmp);
        logPrintlnD(" does not have a valid format");
        String mycall = "INVALID";
        return false;
  };

  // mycall is OK
  String mycall=callsign_tmp;


  // create pathnodelist with invalid calls
  for (auto invalidcall:splitstr2v( system.getUserConfig()->digi.invalidcalls,',')) {
      pathnode pn(invalidcall);

      if (!pn.valid) {
        logPrintD("Warning: invalid-callsign ");
        logPrintD(invalidcall);
        logPrintlnD("does not have a valid format");
      } else {
        _invalidcall.push_back(pn);
      };
  };


  // check if own callsign is not nin the invalidcalllist
  if (isinvector(_invalidcall,callsign_pn)) {
    logPrintD("Error: mycall ");
    logPrintD(callsign_tmp);
    logPrintlnD("is int he invalid_call list");
    return false;
  };


  // fill "do not digi" list
  for (auto nodigi:splitstr2v(system.getUserConfig()->digi.donotdigi,',')){
      pathnode pn(nodigi);

      if (!pn.valid) {
        logPrintD("Warning: donotdigi callsign ");
        logPrintD(nodigi);
        logPrintlnD("does not have a valid format");
      } else {
        _invalidcall.push_back(pn);
      };
  };

  // End ON1ARF


  // setup beacon
  _beacon_timer.setTimeout(system.getUserConfig()->beacon.timeout * 60 * 1000);

  _beaconMsg = std::shared_ptr<APRSMessage>(new APRSMessage());
  //_beaconMsg->setSource(system.getUserConfig()->callsign);
  _beaconMsg->setSource(mycall);

  _beaconMsg->setDestination("APLG01");
  String lat = create_lat_aprs(system.getUserConfig()->beacon.positionLatitude);
  String lng = create_long_aprs(system.getUserConfig()->beacon.positionLongitude);
  _beaconMsg->getBody()->setData(String("=") + lat + "L" + lng + "&" + system.getUserConfig()->beacon.message);

  //String mycall=system.getUserConfig()->callsign; 


  return true;
}

bool RouterTask::loop(System &system) {
  // do routing
  if (!_fromModem.empty()) {
    std::shared_ptr<APRSMessage> modemMsg = _fromModem.getElement();

    if (system.getUserConfig()->aprs_is.active && modemMsg->getSource() != system.getUserConfig()->callsign) {
      std::shared_ptr<APRSMessage> aprsIsMsg = std::make_shared<APRSMessage>(*modemMsg);
      String                       path      = aprsIsMsg->getPath();

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

    if (system.getUserConfig()->digi.active && modemMsg->getSource() != system.getUserConfig()->callsign) {
      std::shared_ptr<APRSMessage> digiMsg = std::make_shared<APRSMessage>(*modemMsg);
      String                       path    = digiMsg->getPath();
      String                       thiscall  = system.getUserConfig()->callsign;


      // create aprs path
      aprspath apath(system.getUserConfig()->digi.maxhopnowide);


      bool receivedok=true;



      for (auto thisnode:splitstr2v(path,',')) {
        pathnode pn=pathnode(thisnode);
        if (!pn.valid) {
          logPrintlnD("Error: invalid pathnode " + thisnode + "  ... ignoring");
          receivedok=false;
          break;
        }
        apath.appendnodetopath(pn);
      };

      // continue if ok
      if (receivedok) {
        // check total path
        receivedok=apath.checkpath();

        if (!receivedok) {
          logPrintlnD("Error: Checkpath failed");
        }
      }


      // continue if ok
      if (receivedok) {
        // loopcheck
        if (!apath.doloopcheck(thiscall)) {
          logPrintlnD("Error: loopcheck failed");
          receivedok=false;
        }
      }

      // continue if ok
      if (receivedok) {
        // add mycall to path
        receivedok=apath.adddigi(thiscall, system.getUserConfig()->digi.fillin);
        if (!receivedok) {
          logPrintlnI("Info: adddigi returned False");
        }
      };


      // continue if ok
      if (receivedok) {
        // done. get new path
        logPrintD("DIGI NEW PATH: ");
        logPrintlnD(apath.printfullpath());

        digiMsg->setPath(apath.printfullpath());

        logPrintD("DIGI: ");
        logPrintlnD(digiMsg->toString());

        _toModem.addElement(digiMsg);
      
      }

      


      /* Removed by ON1ARF
      // simple loop check
      if (path.indexOf("WIDE1-1") >= 0 || path.indexOf(system.getUserConfig()->callsign) == -1) {
        // fixme
        digiMsg->setPath(system.getUserConfig()->callsign + "*");

        logPrintD("DIGI: ");
        logPrintlnD(digiMsg->toString());

        _toModem.addElement(digiMsg);
      }
      END removed by ON1ARF */

    }

 }

 
  // check for beacon
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

  return true;
}
