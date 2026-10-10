#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "sched.h"

extern struct proc proc[NPROC];

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if (t == SBRK_EAGER || n < 0) {
    if (growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if (addr + n < addr)
      return -1;
    if (addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if (n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (killed(myproc())) {
      release(&tickslock);
      return -1;
    }
    sleep_prepare(&ticks);
    release(&tickslock);
    sleep();
    acquire(&tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_get_sched_stats(void)
{
  int pid;
  uint64 addr;
  uint now;
  struct proc *p;
  struct sched_stats stats;
  struct proc *current = myproc();

  argint(0, &pid);
  argaddr(1, &addr);

  acquire(&tickslock);
  now = ticks;
  release(&tickslock);

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->pid == pid && p->state != UNUSED) {
      stats.pid = p->pid;
      stats.run_time = p->run_time;
      stats.wait_time = p->wait_time;
      stats.context_switches = p->context_switches;
      stats.response_time = p->started ?
        (int)(p->first_run_time - p->creation_time) : -1;
      stats.complete = p->state == ZOMBIE;
      stats.turnaround_time = (stats.complete ? p->completion_time : now) -
                              p->creation_time;
      release(&p->lock);
      return copyout(current->pagetable, current->sz, addr, (char *)&stats,
                     sizeof(stats)) < 0 ? -1 : 0;
    }
    release(&p->lock);
  }
  return -1;
}

uint64
sys_sched_set_policy(void)
{
  int policy;
  argint(0, &policy);
  return kset_sched_policy(policy);
}

uint64
sys_sched_set_priority(void)
{
  int priority;
  argint(0, &priority);
  return kset_priority(priority);
}

uint64
sys_get_sched_policy(void)
{
  return kget_sched_policy();
}

uint64
sys_sched_set_process_priority(void)
{
  int pid, priority;
  argint(0, &pid);
  argint(1, &priority);
  return kset_process_priority(pid, priority);
}

uint64
sys_get_sched_procs(void)
{
  uint64 addr;
  int max, count = 0;
  struct proc *p;
  struct proc *current = myproc();
  struct sched_proc_info entry;

  argaddr(0, &addr);
  argint(1, &max);
  if (max <= 0)
    return -1;
  if (max > SCHED_MAX_PROCS)
    max = SCHED_MAX_PROCS;

  for (p = proc; p < &proc[NPROC] && count < max; p++) {
    acquire(&p->lock);
    if (p->state != UNUSED) {
      entry.pid = p->pid;
      entry.state = p->state;
      entry.priority = p->priority;
      entry.mlfq_level = p->mlfq_level;
      entry.run_time = p->run_time;
      entry.wait_time = p->wait_time;
      entry.context_switches = p->context_switches;
      memmove(entry.name, p->name, sizeof(entry.name));
      release(&p->lock);
      if (copyout(current->pagetable, current->sz,
                  addr + count * sizeof(entry), (char *)&entry,
                  sizeof(entry)) < 0)
        return -1;
      count++;
    } else {
      release(&p->lock);
    }
  }
  return count;
}
