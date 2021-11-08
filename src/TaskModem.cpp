#include <logger.h>

#include <TimeLib.h>

#include "Task.h"
#include "TaskAprsIs.h"
#include "TaskModem.h"
#include "project_configuration.h"

using std::shared_ptr;
using std::make_shared;

ModemTask::ModemTask(TaskQueue<shared_ptr<APRSMessage>> &fromModem, TaskQueue<shared_ptr<APRSMessage>> &toModem) : Task(TASK_MODEM, TaskModem), _lora_aprs(), _fromModem(fromModem), _toModem(toModem) , _lasttransmit(0){
}

ModemTask::~ModemTask() {
}

bool ModemTask::setup(System &system) {
  SPI.begin(system.getBoardConfig()->LoraSck, system.getBoardConfig()->LoraMiso, system.getBoardConfig()->LoraMosi, system.getBoardConfig()->LoraCS);
  _lora_aprs.setPins(system.getBoardConfig()->LoraCS, system.getBoardConfig()->LoraReset, system.getBoardConfig()->LoraIRQ);
  if (!_lora_aprs.begin(system.getUserConfig()->lora.frequencyRx)) {
    logPrintlnE("Starting LoRa failed!");
    _stateInfo = "LoRa-Modem failed";
    _state     = Error;
    while (true)
      ;
  }
  _lora_aprs.setRxFrequency(system.getUserConfig()->lora.frequencyRx);
  _lora_aprs.setTxFrequency(system.getUserConfig()->lora.frequencyTx);
  _lora_aprs.setTxPower(system.getUserConfig()->lora.power);
  _lora_aprs.setSpreadingFactor(system.getUserConfig()->lora.spreadingFactor);
  _lora_aprs.setSignalBandwidth(system.getUserConfig()->lora.signalBandwidth);
  _lora_aprs.setCodingRate4(system.getUserConfig()->lora.codingRate4);
  _lora_aprs.enableCrc();

  _stateInfo = "";

  _lasttransmit=0;
  return true;
}

bool ModemTask::loop(System &system) {
  unsigned int now;

  if (_lora_aprs.checkMessage()) {
    shared_ptr<APRSMessage> msg = _lora_aprs.getMessage();
    // msg->getAPRSBody()->setData(msg->getAPRSBody()->getData() + " 123");
    logPrintlnD("Modemtask:");
    logPrintD("[" + timeString() + "] ");
    logPrintD("Received packet '");
    logPrintD(msg->toString());
    logPrintD("' with RSSI ");
    logPrintD(String(_lora_aprs.packetRssi()));
    logPrintD(" and SNR ");
    logPrintlnD(String(_lora_aprs.packetSnr()));

    _fromModem.addElement(msg);
    system.getDisplay().addFrame(shared_ptr<DisplayFrame>(new TextFrame("LoRa", msg->toString())));
  }

  // reset lasttransmit in case of a wrap-around
  now = millis();

  if (now < _lasttransmit) {
    _lasttransmit = 0;
  }

  // reset "now" timer
  if (!_toModem.empty()) {
    if ((now-_lasttransmit) < 2000) {
      logPrintlnD("Modemtask TX WAIT: "+String(now) + " " + String(_lasttransmit));
      delay(250);
    } else {
      logPrintlnD("Modemtask TX: "+String(now) + " " + String(_lasttransmit));

      shared_ptr<APRSMessage> msg = _toModem.getElement();
      logPrintlnD("Modemtask: Transmit "+msg->getBody()->toString());
      _lora_aprs.sendMessage(msg);

      // wait for end of transmission
      while (_lora_aprs.isTransmitting()) {
        yield();
      };
      
      logPrintlnD("Modemtask: Transmit DONE");
      _lasttransmit=millis(); // wait at least 1 second 
    }
  }

  return true;
}
