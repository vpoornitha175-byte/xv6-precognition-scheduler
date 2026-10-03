#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "precog.h"

// Initialize the precognition system.
void
precog_init(void)
{
  // Precognition state is stored inside
  // each process structure.
}

// Initialize precognition state for one process.
void
precog_init_process(struct proc *p)
{
  int i;

  if(p == 0)
    return;

  p->precog.valid = 1;
  p->precog.history_count = 0;
  p->precog.history_index = 0;
  p->precog.predicted_cpu = 0;
  p->precog.cpu_burst = 0;

  for(i = 0; i < PRECOG_HISTORY; i++)
    p->precog.cpu_history[i] = 0;
}

// Record one CPU observation for a process.

void
precog_record_cpu(struct proc *p, uint64 cpu_ticks)
{
  static int record_log_count = 0;

  if(p == 0)
    return;

  if(!p->precog.valid)
    precog_init_process(p);

  p->precog.cpu_history[p->precog.history_index] = cpu_ticks;

  p->precog.history_index =
      (p->precog.history_index + 1) % PRECOG_HISTORY;

  if(p->precog.history_count < PRECOG_HISTORY)
    p->precog.history_count++;

  if(record_log_count < 30){
    printk("HISTORY: PID=%d CPU=%d Count=%d\n",
           p->pid,
           (int)cpu_ticks,
           p->precog.history_count);
    record_log_count++;
  }
}
// Calculate predicted CPU demand.
uint64
precog_predict_cpu(struct proc *p)
{
  uint64 total;
  int i;

  if(p == 0)
    return 0;

  if(!p->precog.valid || p->precog.history_count == 0)
    return 0;

  total = 0;

  for(i = 0; i < p->precog.history_count; i++)
    total += p->precog.cpu_history[i];

  p->precog.predicted_cpu =
      total / p->precog.history_count;

  return p->precog.predicted_cpu;
}

// Get the precognition state of a process.
struct precog_state *
precog_get_state(struct proc *p)
{
  if(p == 0)
    return 0;

  return &p->precog;
}