#ifndef TASK_ROUTER_H_
#define TASK_ROUTER_H_

#include <APRSMessage.h>
#include <TaskManager.h>
#include <aprspath.h>
#include <msghist.h>




using std::shared_ptr;

class RouterTask : public Task {
public:
  RouterTask(TaskQueue<shared_ptr<APRSMessage>> &fromModem, TaskQueue<shared_ptr<APRSMessage>> &toModem, TaskQueue<shared_ptr<APRSMessage>> &toAprsIs, TaskQueue<shared_ptr<APRSMessage>> &fromAprsIs);
  virtual ~RouterTask();

  virtual bool setup(System &system) override;
  virtual bool loop(System &system) override;

private:
  TaskQueue<shared_ptr<APRSMessage>> &_fromModem;
  TaskQueue<shared_ptr<APRSMessage>> &_toModem;
  TaskQueue<shared_ptr<APRSMessage>> &_toAprsIs;
  TaskQueue<shared_ptr<APRSMessage>> &_fromAprsIs;

  shared_ptr<APRSMessage>      _beaconMsg;
  Timer                        _beacon_timer;
  vector<shared_ptr<pathnode>> _invalidcall;
  vector<shared_ptr<pathnode>> _donotdigi;
  vector<shared_ptr<pathnode>> _noaprsis;
  shared_ptr<pathnode>         _callsign_pn;

  shared_ptr<msghist>          _msghist;
};

#endif
