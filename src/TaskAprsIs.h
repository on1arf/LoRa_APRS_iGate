#ifndef TASK_APRS_IS_H_
#define TASK_APRS_IS_H_

#include <APRS-IS.h>
#include <APRSMessage.h>
#include <TaskManager.h>
#include <Timer.h>

class AprsIsTask : public Task {
public:
  explicit AprsIsTask(TaskQueue<std::shared_ptr<APRSMessage>> &toAprsIs,TaskQueue<std::shared_ptr<APRSMessage>> &fromAprsIs);
  virtual ~AprsIsTask();

  virtual bool setup(System &system) override;
  virtual bool loop(System &system) override;

private:
  APRS_IS _aprs_is;

  TaskQueue<std::shared_ptr<APRSMessage>> &_toAprsIs;
  TaskQueue<std::shared_ptr<APRSMessage>> &_fromAprsIs;

  bool connect(const System &system);
};

#endif
