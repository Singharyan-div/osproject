#include "kernel/types.h"
#include "kernel/sched.h"
#include "user/user.h"

static void
usage(void)
{
  printf("usage: schedctl [rr|priority|mlfq]\n");
  printf("       schedctl priority <pid> <0-31>\n");
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
main(int argc, char *argv[])
{
  int policy;

  if (argc == 1) {
    printf("Current scheduler: %s\n", policy_name(get_sched_policy()));
    exit(0);
  }
  if (argc == 2) {
    if (strcmp(argv[1], "rr") == 0)
      policy = SCHED_RR;
    else if (strcmp(argv[1], "priority") == 0)
      policy = SCHED_PRIORITY;
    else if (strcmp(argv[1], "mlfq") == 0)
      policy = SCHED_MLFQ;
    else {
      usage();
      exit(1);
    }
    if (sched_set_policy(policy) < 0) {
      printf("schedctl: could not change scheduler\n");
      exit(1);
    }
    printf("Scheduler set to %s\n", policy_name(policy));
    exit(0);
  }
  if (argc == 4 && strcmp(argv[1], "priority") == 0) {
    int pid = atoi(argv[2]);
    int priority = atoi(argv[3]);
    if (priority < 0 || priority > SCHED_PRIORITY_MAX ||
        sched_set_process_priority(pid, priority) < 0) {
      printf("schedctl: invalid PID or priority (valid range: 0-31)\n");
      exit(1);
    }
    printf("PID %d priority set to %d\n", pid, priority);
    exit(0);
  }
  usage();
  exit(1);
}
