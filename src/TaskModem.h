#ifndef TASK_LORA_H_
#define TASK_LORA_H_

#include <BoardFinder.h>
#include <LoRa_APRS.h>
#include <TaskManager.h>

using std::shared_ptr;

class ModemTask : public Task {
public:
  explicit ModemTask(TaskQueue<shared_ptr<APRSMessage>> &fromModem, TaskQueue<shared_ptr<APRSMessage>> &_toModem);
  virtual ~ModemTask();

  virtual bool setup(System &system) override;
  virtual bool loop(System &system) override;

private:
  LoRa_APRS _lora_aprs;

  TaskQueue<shared_ptr<APRSMessage>> &_fromModem;
  TaskQueue<shared_ptr<APRSMessage>> &_toModem;

  unsigned int _lasttransmit;
};

#endif
