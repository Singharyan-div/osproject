# SchedForge

SchedForge adds three teaching schedulers to xv6-riscv: Round Robin, priority scheduling with aging, and a three-level feedback queue. The default policy is Round Robin. The scheduler runs one QEMU hart so the process accounting and shared MLFQ queues stay easy to follow.

## Build and boot on Ubuntu

On a fresh Ubuntu installation, install the xv6 build tools:

```sh
sudo apt update
sudo apt install build-essential make perl bc qemu-system-misc gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
```

Then build and boot xv6 from this directory:

```sh
make clean
make -j4 qemu
```

At the xv6 shell prompt, run the complete comparison:

```text
$ schedbench
```

To run the workload set under one policy, pass `rr`, `priority`, or `mlfq`:

```text
$ schedbench priority
```

The xv6 image also includes a scheduler control utility, a process listing, and
a pipe round-trip benchmark:

```text
$ schedctl                 # show the active scheduler
$ schedctl mlfq             # change the active scheduler
$ ps                        # list process state and scheduler counters
$ schedctl priority 4 2     # set PID 4's base priority to 2 (0 is highest)
$ ipcbench                  # run 500 pipe ping-pong round trips
$ ipcbench 2000             # choose a larger round count
```

## What the benchmark does

The default run executes CPU-bound, sleep-heavy I/O-bound, and mixed workloads under each scheduler. It prints per-process run, wait, response, turnaround, and context-switch counts, along with workload averages, throughput, and Jain's fairness index. It then runs a long-wait priority-aging check and an MLFQ workload that crosses the periodic boost interval.

Times are reported in xv6 timer ticks (about 0.1 seconds per tick in the supplied QEMU setup). A short I/O burst can therefore show zero run-time ticks while still showing its response, turnaround, and context switches.

## Scheduling rules

- **Round Robin:** processes rotate in process-table order after four timer ticks.
- **Priority:** lower numeric values run first; equal effective priorities rotate fairly. A waiting process gains one effective-priority point per two runnable ticks, down to priority zero.
- **MLFQ:** new processes start in queue 0. Quanta are 2, 4, and 8 ticks. A CPU-bound process is demoted when it uses its full quantum; sleeping and woken processes keep their queue. Every 50 ticks, runnable, sleeping, and running processes are boosted to queue 0.

The statistics syscall returns a snapshot for any process still in the process table. A completed child remains queryable as a zombie until its parent calls `wait`, which lets `schedbench` collect its final statistics before reaping it.

## Relevant xv6 flow

In this xv6 version, `struct proc` stores each process's state, lock, saved kernel context, and trap frame. `scheduler()` runs on each configured hart and chooses a `RUNNABLE` process. `yield()` marks a running process runnable, then `sched()` verifies the process-lock handoff and calls the existing assembly `swtch()` to save and restore kernel contexts. `sleep()` records a channel and marks the process sleeping under its lock; `wakeup()` scans for that channel and makes matching processes runnable. Timer interrupts enter through `usertrap()` or `kerneltrap()`, while `clockintr()` advances the shared tick count.

`scheduler()` asks the active policy's `pick_next()` callback for a runnable process. The timer path accounts each tick and invokes `on_tick()`; a policy requests `yield()` when its quantum ends or a higher-priority queue becomes runnable. `wakeup()` invokes `on_wakeup()` while holding the process lock. `swtch()` remains the existing assembly context switch and `sched()` keeps xv6's process-lock handoff.

The kernel tracks execution and runnable-wait time in timer ticks. It records creation, first-dispatch, completion, and dispatch counts in `struct proc`. `get_sched_stats(pid, &stats)` reports the measurements; `sched_set_policy()` chooses a policy at runtime, and `sched_set_priority()` sets the caller's base priority from 0 through 31.
