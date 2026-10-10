#ifndef _SCHED_H_
#define _SCHED_H_

#include "types.h"

enum sched_policy {
  SCHED_RR = 0,
  SCHED_PRIORITY = 1,
  SCHED_MLFQ = 2,
};

#define SCHED_PRIORITY_MAX 31
#define SCHED_MAX_PROCS 64

enum sched_proc_state {
  SCHED_UNUSED = 0,
  SCHED_USED,
  SCHED_SLEEPING,
  SCHED_RUNNABLE,
  SCHED_RUNNING,
  SCHED_ZOMBIE,
};

struct sched_stats {
  int pid;
  uint run_time;
  uint wait_time;
  uint context_switches;
  int response_time;
  uint turnaround_time;
  int complete;
};

struct sched_proc_info {
  int pid;
  int state;
  int priority;
  int mlfq_level;
  uint run_time;
  uint wait_time;
  uint context_switches;
  char name[16];
};

#endif
