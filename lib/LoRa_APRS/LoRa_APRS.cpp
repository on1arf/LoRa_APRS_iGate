#include "LoRa_APRS.h"

#include "logger.h"


using std::shared_ptr;
using std::make_shared;

LoRa_APRS::LoRa_APRS() : _RxFrequency(433775000), _TxFrequency(433775000) {
}

bool LoRa_APRS::checkMessage() {
  if (!parsePacket()) {
    return false;
  }
  // read header:
  char dummy[4];
  readBytes(dummy, 3);
  if (dummy[0] != '<') {
    // is no APRS message, ignore message
    while (available()) {
      read();
    }
    return false;
  }
  // read APRS data:
  String str;
  while (available()) {
    str += (char)read();
  }
  shared_ptr<APRSMessage>  _LastReceivedMsg = make_shared<APRSMessage>();
  _LastReceivedMsg->decode(str);
  _ReceivedMsgQueue.push_back(_LastReceivedMsg);
  return true;
}

shared_ptr<APRSMessage> LoRa_APRS::getMessage() {
  shared_ptr<APRSMessage>  _LastReceivedMsg = _ReceivedMsgQueue.front();
  _ReceivedMsgQueue.pop_front();
  return _LastReceivedMsg;
}


void LoRa_APRS::sendMessage(const shared_ptr<APRSMessage> msg) {
  int waitcount;

  // switch to transmit frequency
  if (_RxFrequency != _TxFrequency) {
    setFrequency(_TxFrequency);
  };

  // first wait until the radio-channel is free
  // maximum 15 seconds (30 times 500 ms), just to be sure
  waitcount=0;
  while ((isChannelBusy()) && (waitcount < 30)) {
    logPrintD("B");
    delay(500);
    waitcount++;
  };
  if (waitcount > 0) logPrintlnD("C");

  String data = msg->encode();
  beginPacket();
  // Header:
  write('<');
  write(0xFF);
  write(0x01);
  // APRS Data:
  write((const uint8_t *)data.c_str(), data.length());
  endPacket();

  if (_RxFrequency != _TxFrequency) {
    setFrequency(_RxFrequency);
  }
}

void LoRa_APRS::setRxFrequency(long frequency) {
  _RxFrequency = frequency;
  setFrequency(_RxFrequency);
}

// cppcheck-suppress unusedFunction
long LoRa_APRS::getRxFrequency() const {
  return _RxFrequency;
}

void LoRa_APRS::setTxFrequency(long frequency) {
  _TxFrequency = frequency;
}

// cppcheck-suppress unusedFunction
long LoRa_APRS::getTxFrequency() const {
  return _TxFrequency;
}

