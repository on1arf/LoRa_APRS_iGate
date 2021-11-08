#ifndef LORA_H_
#define LORA_H_

#include <Arduino.h>

#include <APRS-Decoder.h>
#include <LoRa.h>
#include <memory>
#include <deque>

using std::shared_ptr;
using std::deque;

class LoRa_APRS : public LoRaClass {
public:
  LoRa_APRS();

  bool                         checkMessage();
  shared_ptr<APRSMessage> getMessage();

  void sendMessage(const shared_ptr<APRSMessage> msg);

  void setRxFrequency(long frequency);
  long getRxFrequency() const;

  void setTxFrequency(long frequency);
  long getTxFrequency() const;

private:
  deque<shared_ptr<APRSMessage>>      _ReceivedMsgQueue;
  long                                 _RxFrequency;
  long                                 _TxFrequency;
};

#endif
