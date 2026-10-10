# SchedForge sample results

Captured from one Ubuntu 24.04 WSL2 / QEMU run with one hart. Values are timer ticks (about 0.1 seconds each) and processes per second. Results can vary between runs.

| Workload | Policy | Avg wait | Avg response | Avg turnaround | Avg switches | Throughput | Jain fairness |
|---|---|---:|---:|---:|---:|---:|---:|
| CPU-bound | RR | 8 | 4 | 14 | 2 | 1.57 | 93.10 |
| CPU-bound | Priority + Aging | 10 | 7 | 17 | 5 | 1.36 | 91.30 |
| CPU-bound | MLFQ | 8 | 2 | 13 | 4 | 2.00 | 96.07 |
| I/O-bound | RR | 0 | 0 | 8 | 9 | 3.33 | n/a |
| I/O-bound | Priority + Aging | 0 | 0 | 8 | 9 | 3.75 | n/a |
| I/O-bound | MLFQ | 0 | 0 | 8 | 9 | 3.33 | n/a |
| Mixed | RR | 7 | 4 | 15 | 4 | 1.42 | 65.33 |
| Mixed | Priority + Aging | 4 | 1 | 10 | 7 | 2.50 | 66.12 |
| Mixed | MLFQ | 5 | 2 | 12 | 7 | 2.30 | 66.66 |

The long-wait check also observed the priority-31 process receive CPU before its priority-0 peer finished. The MLFQ boost workload ran beyond the configured 50-tick boost interval. I/O run-time fairness is `n/a` because its short CPU bursts rounded to zero at the timer's tick resolution; switch counts and turnaround still show the sleep/wakeup activity.
