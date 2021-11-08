#include <logger.h>

#include "Task.h"
#include "TaskAprsIs.h"
#include "project_configuration.h"

using std::shared_ptr;
using std::make_shared;

AprsIsTask::AprsIsTask(TaskQueue<shared_ptr<APRSMessage>> &toAprsIs, TaskQueue<shared_ptr<APRSMessage>> &fromAprsIs) : Task(TASK_APRS_IS, TaskAprsIs), _toAprsIs(toAprsIs), _fromAprsIs(fromAprsIs) {
}

AprsIsTask::~AprsIsTask() {
}

bool AprsIsTask::setup(System &system) {
  _aprs_is.setup(system.getUserConfig()->callsign, system.getUserConfig()->aprs_is.passcode, "ESP32-APRS-IS", "0.2");
  return true;
}

bool AprsIsTask::loop(System &system) {
  String rcvdmsg;

  if (!system.isWifiEthConnected()) {
    return false;
  }
  if (!_aprs_is.connected()) {
    if (!connect(system)) {
      _stateInfo = "not connected";
      _state     = Error;
      return false;
    }
    _stateInfo = "connected";
    _state     = Okay;
    return false;
  }

  //_aprs_is.getAPRSMessage();
  rcvdmsg=_aprs_is.getMessage();
  if (rcvdmsg.length() != 0)
  {
    logPrintlnI("APRS-IS Received: "+rcvdmsg);

    if (!(rcvdmsg.startsWith("#"))) {
      shared_ptr<APRSMessage> msgout = make_shared<APRSMessage>();
      msgout->decode(rcvdmsg);
 
      // send to the _FromAprsIS queue if OK
      if (msgout->getType() != APRSMessageType::Error) _fromAprsIs.addElement(msgout);
 
    }

  }
  

  if (!_toAprsIs.empty()) {
    shared_ptr<APRSMessage> msg = _toAprsIs.getElement();
    logPrintlnI("APRS-IS sending: "+msg->getSource()+" "+msg->getDestination()+" "+msg->getPath()+" "+msg->getBody()->getData());

    _aprs_is.sendMessage(msg);
  }

  return true;
}

bool AprsIsTask::connect(const System &system) {
  logPrintI("connecting to APRS-IS server: ");
  logPrintI(system.getUserConfig()->aprs_is.server);
  logPrintI(" on port: ");
  logPrintlnI(String(system.getUserConfig()->aprs_is.port));
  if (!_aprs_is.connect(system.getUserConfig()->aprs_is.server, system.getUserConfig()->aprs_is.port)) {
    logPrintlnE("Connection failed.");
    return false;
  }
  logPrintlnI("Connected to APRS-IS server!");
  return true;
}
