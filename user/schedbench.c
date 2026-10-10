#include "kernel/types.h"
#include "kernel/sched.h"
#include "user/user.h"

#define TASKS 3
#define WORK_TICKS 10
#define AGE_HIGH_TICKS 70
#define AGE_LOW_TICKS 8
#define BOOST_TICKS 58

enum workload { CPU_WORK, IO_WORK, MIXED_WORK };

static char *policy_name(int policy)
{
  if (policy == SCHED_RR)
    return "RR";
  if (policy == SCHED_PRIORITY)
    return "Priority + Aging";
  return "MLFQ";
}

static char *workload_name(enum workload work)
{
  if (work == CPU_WORK)
    return "CPU-bound";
  if (work == IO_WORK)
    return "I/O-bound";
  return "Mixed";
}

static void
print_hundredths(uint value)
{
  uint fraction = value % 100;
  printf("%d.", value / 100);
  if (fraction < 10)
    printf("0");
  printf("%d", fraction);
}

static void
busy_for_ticks(int duration)
{
  uint start = uptime();
  volatile uint64 checksum = getpid();

  while (uptime() - start < duration) {
    for (int i = 0; i < 2000; i++)
      checksum = checksum * 1664525 + 1013904223;
  }
  if (checksum == 0xdeadbeef)
    printf("checksum: %d\n", (int)checksum);
}

static void
io_work(void)
{
  for (int i = 0; i < 8; i++) {
    volatile int work = i;
    for (int j = 0; j < 1000; j++)
      work = work * 33 + j;
    pause(1);
  }
}

static int
child_priority(int policy, enum workload work, int index)
{
  if (policy != SCHED_PRIORITY)
    return 15;
  if (work == CPU_WORK)
    return index == 0 ? 2 : 24;
  if (work == MIXED_WORK)
    return index == 2 ? 2 : 24;
  return 12;
}

static void
run_child(int priority, enum workload work, int index)
{
  if (sched_set_priority(priority) < 0)
    exit(1);

  if (work == IO_WORK || (work == MIXED_WORK && index == 2))
    io_work();
  else
    busy_for_ticks(WORK_TICKS);
  exit(0);
}

static int
wait_for_children(int pids[], int count, struct sched_stats results[],
                  int watch_high_pid, int watch_low_pid)
{
  int done[TASKS] = {0, 0, 0};
  int finished = 0;
  int aged_low_ran = 0;

  while (finished < count) {
    struct sched_stats high, low;
    for (int i = 0; i < count; i++) {
      if (done[i])
        continue;
      if (get_sched_stats(pids[i], &results[i]) == 0 && results[i].complete) {
        done[i] = 1;
        finished++;
      }
    }

    if (watch_low_pid > 0 && !aged_low_ran &&
        get_sched_stats(watch_high_pid, &high) == 0 && !high.complete &&
        get_sched_stats(watch_low_pid, &low) == 0 && low.run_time > 0) {
      aged_low_ran = 1;
      printf("Aging check: low-priority process ran while the high-priority process was still active.\n");
    }

    if (finished < count)
      pause(1);
  }

  for (int i = 0; i < count; i++) {
    get_sched_stats(pids[i], &results[i]);
    wait(0);
  }
  return aged_low_ran;
}

static void
print_results(int policy, enum workload work, struct sched_stats results[],
              int count, uint elapsed)
{
  uint total_wait = 0, total_response = 0, total_turnaround = 0;
  uint total_switches = 0, total_runtime = 0, sum_squares = 0;
  int response_count = 0;
  uint fairness;
  uint throughput;

  printf("\nPolicy: %s | Workload: %s\n", policy_name(policy),
         workload_name(work));
  printf("PID   Run   Wait   Response   Turnaround   Switches\n");
  for (int i = 0; i < count; i++) {
    printf("%d    %u    %u       %d          %u          %u\n",
           results[i].pid, results[i].run_time, results[i].wait_time,
           results[i].response_time, results[i].turnaround_time,
           results[i].context_switches);
    total_wait += results[i].wait_time;
    if (results[i].response_time >= 0) {
      total_response += results[i].response_time;
      response_count++;
    }
    total_turnaround += results[i].turnaround_time;
    total_switches += results[i].context_switches;
    total_runtime += results[i].run_time;
    sum_squares += results[i].run_time * results[i].run_time;
  }

  fairness = sum_squares == 0 ? 0 :
             (total_runtime * total_runtime * 10000) / (count * sum_squares);
  printf("Average wait/response/turnaround: %u / %u / %u ticks\n",
         total_wait / count,
         response_count ? total_response / response_count : 0,
         total_turnaround / count);
  printf("Context switches total/average: %u / %u\n", total_switches,
         total_switches / count);
  throughput = elapsed ? count * 1000 / elapsed : 0;
  printf("Throughput: ");
  print_hundredths(throughput);
  printf(" processes/sec | Jain fairness: ");
  if (sum_squares == 0)
    printf("n/a\n");
  else {
    print_hundredths(fairness);
    printf("\n");
  }
}

static void
run_trial(int policy, enum workload work)
{
  int pids[TASKS];
  struct sched_stats results[TASKS];
  uint start, elapsed;
  int count = 0;

  if (sched_set_policy(policy) < 0) {
    printf("schedbench: cannot select policy %d\n", policy);
    return;
  }
  start = uptime();
  for (int i = 0; i < TASKS; i++) {
    int pid = fork();
    if (pid == 0)
      run_child(child_priority(policy, work, i), work, i);
    if (pid < 0) {
      printf("schedbench: fork failed\n");
      break;
    }
    pids[count++] = pid;
  }
  if (count == 0)
    return;

  wait_for_children(pids, count, results, 0, 0);
  elapsed = uptime() - start;
  print_results(policy, work, results, count, elapsed);
}

static void
run_aging_check(void)
{
  int pids[2];
  struct sched_stats results[2];
  uint start, elapsed;

  if (sched_set_policy(SCHED_PRIORITY) < 0)
    return;
  start = uptime();
  pids[0] = fork();
  if (pids[0] == 0) {
    sched_set_priority(0);
    busy_for_ticks(AGE_HIGH_TICKS);
    exit(0);
  }
  pids[1] = fork();
  if (pids[1] == 0) {
    sched_set_priority(SCHED_PRIORITY_MAX);
    busy_for_ticks(AGE_LOW_TICKS);
    exit(0);
  }
  if (pids[0] < 0 || pids[1] < 0) {
    printf("Aging check: fork failed\n");
    return;
  }
  if (!wait_for_children(pids, 2, results, pids[0], pids[1]))
    printf("Aging check: low-priority process did not run before the high-priority workload ended.\n");
  elapsed = uptime() - start;
  print_results(SCHED_PRIORITY, CPU_WORK, results, 2, elapsed);
}

static void
run_boost_check(void)
{
  int pids[TASKS];
  struct sched_stats results[TASKS];
  uint start, elapsed;

  if (sched_set_policy(SCHED_MLFQ) < 0)
    return;
  start = uptime();
  for (int i = 0; i < TASKS; i++) {
    pids[i] = fork();
    if (pids[i] == 0) {
      busy_for_ticks(BOOST_TICKS);
      exit(0);
    }
    if (pids[i] < 0) {
      printf("MLFQ boost check: fork failed\n");
      return;
    }
  }
  wait_for_children(pids, TASKS, results, 0, 0);
  elapsed = uptime() - start;
  print_results(SCHED_MLFQ, CPU_WORK, results, TASKS, elapsed);
  printf("MLFQ boost check: long-running CPU workload crossed the 50-tick boost interval.\n");
}

int
main(int argc, char **argv)
{
  int first_policy = SCHED_RR;
  int last_policy = SCHED_MLFQ;

  if (argc > 1) {
    if (strcmp(argv[1], "rr") == 0)
      first_policy = last_policy = SCHED_RR;
    else if (strcmp(argv[1], "priority") == 0)
      first_policy = last_policy = SCHED_PRIORITY;
    else if (strcmp(argv[1], "mlfq") == 0)
      first_policy = last_policy = SCHED_MLFQ;
    else {
      printf("usage: schedbench [rr|priority|mlfq]\n");
      exit(1);
    }
  }

  printf("SchedForge: CPU scheduler comparison (tick-based measurements)\n");
  for (int policy = first_policy; policy <= last_policy; policy++) {
    run_trial(policy, CPU_WORK);
    run_trial(policy, IO_WORK);
    run_trial(policy, MIXED_WORK);
  }
  if (first_policy <= SCHED_PRIORITY && last_policy >= SCHED_PRIORITY)
    run_aging_check();
  if (first_policy <= SCHED_MLFQ && last_policy >= SCHED_MLFQ)
    run_boost_check();
  printf("\nSchedForge benchmark complete.\n");
  exit(0);
}
