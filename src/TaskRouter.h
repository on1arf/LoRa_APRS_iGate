#ifndef TASK_ROUTER_H_
#define TASK_ROUTER_H_

#include <APRSMessage.h>
#include <TaskManager.h>
#include <aprspath.h>

class RouterTask : public Task {
public:
  RouterTask(TaskQueue<std::shared_ptr<APRSMessage>> &fromModem, TaskQueue<std::shared_ptr<APRSMessage>> &toModem, TaskQueue<std::shared_ptr<APRSMessage>> &toAprsIs, TaskQueue<std::shared_ptr<APRSMessage>> &fromAprsIs);
  virtual ~RouterTask();

  virtual bool setup(System &system) override;
  virtual bool loop(System &system) override;

private:
  TaskQueue<std::shared_ptr<APRSMessage>> &_fromModem;
  TaskQueue<std::shared_ptr<APRSMessage>> &_toModem;
  TaskQueue<std::shared_ptr<APRSMessage>> &_toAprsIs;
  TaskQueue<std::shared_ptr<APRSMessage>> &_fromAprsIs;

  std::shared_ptr<APRSMessage> _beaconMsg;
  Timer                        _beacon_timer;
  vector<pathnode>             _invalidcall;
  vector<pathnode>             _donotdigi;
  vector<pathnode>             _noaprsis;
  pathnode                     *_callsign_pn;
};

#endif
