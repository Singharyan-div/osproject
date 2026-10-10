#include "kernel/types.h"
#include "kernel/sched.h"
#include "user/user.h"

static char *
state_name(int state)
{
  switch (state) {
  case SCHED_USED: return "used";
  case SCHED_SLEEPING: return "sleep";
  case SCHED_RUNNABLE: return "ready";
  case SCHED_RUNNING: return "running";
  case SCHED_ZOMBIE: return "zombie";
  default: return "?";
  }
}

static char *
policy_name(int policy)
{
  if (policy == SCHED_PRIORITY)
    return "priority";
  if (policy == SCHED_MLFQ)
    return "mlfq";
  return "rr";
}

int
main(void)
{
  struct sched_proc_info entries[SCHED_MAX_PROCS];
  int count = get_sched_procs(entries, SCHED_MAX_PROCS);

  if (count < 0) {
    printf("ps: process table query failed\n");
    exit(1);
  }
  printf("Scheduler: %s\n", policy_name(get_sched_policy()));
  printf("PID STATE PRI Q RUN WAIT SW NAME\n");
  for (int i = 0; i < count; i++) {
    printf("%d %s %d %d %u %u %u %s\n",
           entries[i].pid, state_name(entries[i].state),
           entries[i].priority, entries[i].mlfq_level,
           entries[i].run_time, entries[i].wait_time,
           entries[i].context_switches, entries[i].name);
  }
  exit(0);
}
