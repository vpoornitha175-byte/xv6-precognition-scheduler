# Precognition Scheduler --- Team Member Guide

## 1. Purpose of This Document

This document explains exactly what was changed in the xv6-riscv source
tree and why.

Use this file when working on the project so that team members
understand:

-   which files were modified
-   what each modification does
-   how the scheduler works
-   where CPU history is collected
-   how predictions are calculated
-   how the tests demonstrate the feature
-   which parts should not be changed without understanding the
    scheduler design

------------------------------------------------------------------------

# 2. High-Level Architecture

The implementation has four main stages:

``` text
Timer interrupt
      ↓
CPU burst tracking
      ↓
Precognition history
      ↓
CPU prediction
      ↓
Scheduler score
      ↓
Process selection
```

A process has its own Precognition state.

The scheduler examines every runnable process and asks:

``` text
How much CPU time does this process appear likely to need?
```

It then prefers processes with lower predicted CPU demand.

------------------------------------------------------------------------

# 3. Files Modified

The main files are:

``` text
kernel/proc.h
kernel/precog.h
kernel/precog.c
kernel/proc.c
kernel/trap.c
user/precogtest.c
user/fairtest.c
```

If the scheduling-count syscall is enabled, these files are also
involved:

``` text
kernel/syscall.h
kernel/syscall.c
kernel/sysproc.c
user/user.h
user/usys.pl
```

------------------------------------------------------------------------

# 4. `kernel/proc.h`

## What changed

A Precognition state was added to each process.

The state contains:

``` c
#define PRECOG_HISTORY 8

struct precog_state {
  int valid;
  uint64 cpu_history[PRECOG_HISTORY];
  int history_count;
  int history_index;
  uint64 predicted_cpu;
  uint64 cpu_burst;
  uint64 observation_ticks;
  uint64 scheduling_count;
};
```

## Meaning of each field

### `valid`

Indicates whether the Precognition state has been initialized.

### `cpu_history[]`

Stores recent CPU burst lengths.

Maximum history:

``` text
8 samples
```

### `history_count`

Number of valid samples currently stored.

It starts at `0` and increases until it reaches `8`.

### `history_index`

Points to the next location in the circular history buffer.

### `predicted_cpu`

Stores the most recently calculated CPU prediction.

### `cpu_burst`

Counts CPU ticks consumed during the current burst.

### `observation_ticks`

Reserved for tracking observation time and future extensions.

### `scheduling_count`

Counts how many times the scheduler selected the process.

------------------------------------------------------------------------

# 5. `kernel/precog.h`

This file contains declarations for the Precognition functions.

Important functions:

``` c
void precog_init_process(struct proc *p);
void precog_record_cpu(struct proc *p, uint64 cpu_ticks);
uint64 precog_predict_cpu(struct proc *p);
struct precog_state *precog_get_state(struct proc *p);
```

When adding new Precognition functionality, place the function
declaration here and the implementation in `precog.c`.

------------------------------------------------------------------------

# 6. `kernel/precog.c`

This is the core prediction module.

## `precog_init_process()`

Initializes a process's prediction state.

It resets:

``` text
history_count
history_index
predicted_cpu
cpu_burst
CPU history entries
```

A new process therefore starts with no CPU history.

------------------------------------------------------------------------

## `precog_record_cpu()`

The current implementation:

``` c
void
precog_record_cpu(struct proc *p, uint64 cpu_ticks)
{
  if(p == 0)
    return;

  if(!p->precog.valid)
    precog_init_process(p);

  p->precog.cpu_history[p->precog.history_index] = cpu_ticks;

  p->precog.history_index =
      (p->precog.history_index + 1) % PRECOG_HISTORY;

  if(p->precog.history_count < PRECOG_HISTORY)
    p->precog.history_count++;
}
```

### Important

This function should primarily record history.

Do not add frequent `printk()` calls here unless debugging is
specifically required.

Printing every history update makes the QEMU console difficult to read,
especially when multiple processes are running.

------------------------------------------------------------------------

## `precog_predict_cpu()`

The current prediction algorithm is an arithmetic average.

Conceptually:

``` text
total = sum of recorded CPU bursts

prediction = total / number of samples
```

Example:

``` text
History:
1, 2, 3

Prediction:
6 / 3 = 2
```

If there is no history, the function returns `0`.

The scheduler then converts that initial `0` into a prediction of `1`.

------------------------------------------------------------------------

# 7. `kernel/trap.c`

## Why this file changed

The timer interrupt is the natural place to observe CPU usage.

When a timer tick occurs:

``` c
p = myproc();

if(p != 0 && p->state == RUNNING){
  p->precog.cpu_burst++;
}
```

This means that every timer tick contributes to the currently running
process's CPU burst.

Example:

``` text
Process starts running
        ↓
Timer tick → cpu_burst = 1
Timer tick → cpu_burst = 2
Timer tick → cpu_burst = 3
        ↓
Burst is recorded
```

------------------------------------------------------------------------

# 8. `kernel/proc.c`

This is the most important integration point.

## Scheduler

The Precognition scheduler scans all processes:

``` c
for(p = proc; p < &proc[NPROC]; p++)
```

For every `RUNNABLE` process it calculates:

``` c
predicted = precog_predict_cpu(p);
```

If the prediction is zero:

``` c
if(predicted == 0)
  predicted = 1;
```

Then:

``` c
score = 1000 / (predicted + 1);
```

The process with the highest score becomes the candidate.

------------------------------------------------------------------------

## Why the score works

Example:

``` text
Prediction = 1
Score = 1000 / 2 = 500

Prediction = 2
Score = 1000 / 3 = 333

Prediction = 3
Score = 1000 / 4 = 250
```

Therefore:

``` text
Lower prediction → Higher score → Earlier selection
```

------------------------------------------------------------------------

## Scheduling count

Before running the selected process:

``` c
best->precog.scheduling_count++;
```

This is useful for the fairness/progress test.

------------------------------------------------------------------------

## Scheduler logging

The current logging is intentionally limited.

The scheduler can maintain:

``` c
static uint64 last_prediction[NPROC];
```

and print only when a process's prediction changes.

Example:

``` text
PRECOG SELECT: PID=10 | Prediction=2 | Score=333
PRECOG SELECT: PID=10 | Prediction=3 | Score=250
```

This is better than printing every scheduling decision.

------------------------------------------------------------------------

# 9. `sleep()` and `yield()`

The current implementation records a CPU burst when a process goes to
sleep.

The relevant logic is conceptually:

``` c
if(p->precog.cpu_burst > 0){
  precog_record_cpu(p, p->precog.cpu_burst);
  p->precog.cpu_burst = 0;
}
```

This turns the current CPU burst into a history sample.

## Important future improvement

A CPU-bound process may call `yield()` without going to sleep.

For faster learning, the project could also record a burst in `yield()`.

However, this should be implemented carefully to avoid recording the
same CPU burst twice.

Do not change this casually; understand the `sched()` state transition
first.

------------------------------------------------------------------------

# 10. `user/precogtest.c`

This is the basic functional test.

It creates three children with different CPU workloads:

``` text
Child 1 → 10000
Child 2 → 50000
Child 3 → 100000
```

The purpose is to create visibly different CPU usage patterns.

The test should not try to access kernel internals directly.

Instead, the kernel prints prediction information.

Expected useful output:

``` text
PRECOG SELECT: PID=6 | Prediction=2 | Score=333
PRECOG SELECT: PID=6 | Prediction=3 | Score=250
```

------------------------------------------------------------------------

# 11. `user/fairtest.c`

This test evaluates scheduling progress.

It creates:

``` text
Small workload   = 10000
Medium workload  = 50000
Large workload   = 100000
```

Each child prints:

-   completion tick
-   scheduling count

Example:

``` text
Process 1: Small workload completed at tick 129
Scheduling count: 2

Process 2: Medium workload completed at tick 132
Scheduling count: 7

Process 3: Large workload completed at tick 134
Scheduling count: 13
```

The test should be interpreted as a **fairness/progress observation**.

It does not prove formal fairness.

------------------------------------------------------------------------

# 12. `getprecogcount()` Syscall

If enabled, the `getprecogcount()` syscall allows a user process to
retrieve its own scheduling count.

The syscall path normally involves:

``` text
user program
    ↓
user/user.h
    ↓
user/usys.pl
    ↓
kernel/syscall.h
    ↓
kernel/syscall.c
    ↓
kernel/sysproc.c
    ↓
current process
    ↓
precog.scheduling_count
```

This lets `fairtest` print:

``` text
Scheduling count: 13
```

without exposing the entire kernel data structure to user space.

------------------------------------------------------------------------

# 13. Testing Procedure

Build:

``` bash
make clean
make
```

For cleaner output:

``` bash
make qemu CPUS=1
```

Run:

``` text
$ precogtest
```

Then:

``` text
$ fairtest
```

Exit QEMU:

``` text
Ctrl+A
X
```

------------------------------------------------------------------------

# 14. Understanding the Test Output

Consider:

``` text
PRECOG SELECT: PID=10 | Prediction=2 | Score=333
```

This means:

``` text
PID = 10
Predicted CPU demand = 2
Score = 333
```

The score comes from:

``` text
1000 / (2 + 1)
= 333
```

Later:

``` text
PRECOG SELECT: PID=10 | Prediction=3 | Score=250
```

means that the process's observed history caused its predicted CPU
demand to increase.

The scheduler therefore gives it a lower score.

------------------------------------------------------------------------

# 15. Why PIDs Change Between Runs

Do not hard-code meanings such as:

``` text
PID 10 = always large workload
```

PIDs can change between executions.

What matters is the relationship between:

``` text
process workload
CPU history
prediction
score
scheduling count
```

------------------------------------------------------------------------

# 16. Why Tick Values Change

Completion ticks are not expected to be identical on every run.

They can change because of:

-   scheduler timing
-   process interleaving
-   timer interrupts
-   QEMU execution
-   number of CPUs/harts

Therefore, compare the overall behavior rather than expecting exact tick
values.

------------------------------------------------------------------------

# 17. Why Console Output Can Interleave

When multiple processes print simultaneously, their output can appear
mixed.

For example:

``` text
Child 1...
Child 2...
Child 3...
```

may become visually interleaved.

This is normal for concurrent execution.

For a cleaner demo:

``` bash
make qemu CPUS=1
```

Also avoid unnecessary kernel `printk()` calls.

------------------------------------------------------------------------

# 18. What Not to Change Without Discussion

Be careful when modifying:

### `scheduler()`

Small locking/state changes can break xv6 scheduling.

### `sleep()`

Incorrect changes can cause deadlocks or missed wakeups.

### `yield()`

Incorrect changes can interfere with `sched()` and process state
transitions.

### `trap.c`

Timer changes affect the entire operating system.

### Process locks

Do not remove or move `acquire()`/`release()` calls without
understanding xv6's scheduler locking rules.

------------------------------------------------------------------------

# 19. Known Limitations

Current implementation:

-   uses a simple average
-   has an 8-sample history
-   uses integer predictions
-   has no formal starvation prevention
-   has no formal fairness guarantee
-   has a simple hand-designed score formula
-   learns only from observed execution history

These are acceptable for the current project and provide opportunities
for future work.

------------------------------------------------------------------------

# 20. Suggested Team Division

A possible division of work:

## Member 1 --- Precognition algorithm

Responsible for:

``` text
kernel/precog.h
kernel/precog.c
```

Understand:

-   history buffer
-   CPU burst recording
-   prediction calculation

------------------------------------------------------------------------

## Member 2 --- Scheduler integration

Responsible for:

``` text
kernel/proc.c
kernel/trap.c
kernel/proc.h
```

Understand:

-   CPU tick accounting
-   scheduler scoring
-   process selection
-   scheduling count
-   locking/state transitions

------------------------------------------------------------------------

## Member 3 --- Testing and syscall

Responsible for:

``` text
user/precogtest.c
user/fairtest.c
getprecogcount() syscall files
```

Understand:

-   workload design
-   test interpretation
-   scheduling count measurement
-   output analysis

------------------------------------------------------------------------

## Member 4 --- Documentation and evaluation

Responsible for:

``` text
README.md
TEAM_GUIDE.md
```

Understand:

-   project architecture
-   experiment results
-   limitations
-   comparison with standard xv6 scheduling
-   presentation/report preparation

------------------------------------------------------------------------

# 21. Short Explanation for a Team Presentation

If someone asks, "What did we modify?", explain it like this:

> We extended each xv6 process with a Precognition state that stores
> recent CPU burst history. Timer interrupts update the current CPU
> burst. Completed bursts are stored in a circular history buffer. The
> scheduler calculates the average CPU burst as the predicted CPU demand
> and converts that prediction into a scheduling score. Lower predicted
> CPU demand produces a higher score, so the scheduler prefers processes
> expected to finish with less CPU time. We added test programs with
> small, medium, and large CPU workloads to observe the prediction and
> scheduling behavior.

------------------------------------------------------------------------

# 22. One-Line Explanation of Each File

``` text
proc.h
    → Stores Precognition state inside each process.

precog.h
    → Declares the Precognition functions.

precog.c
    → Records CPU history and calculates predictions.

trap.c
    → Counts CPU usage on timer ticks.

proc.c
    → Uses predictions to choose the next runnable process.

precogtest.c
    → Tests prediction behavior with different workloads.

fairtest.c
    → Tests completion and scheduling counts.

syscall files
    → Provide getprecogcount() to user programs.
```

------------------------------------------------------------------------

# 23. Final Mental Model

Remember the project as:

``` text
        PROCESS
           │
           ▼
    CPU burst measured
           │
           ▼
     History buffer
           │
           ▼
    Average prediction
           │
           ▼
  1000 / (prediction + 1)
           │
           ▼
    Scheduling score
           │
           ▼
   Highest score wins
           │
           ▼
       RUN PROCESS
```

That is the core of the entire Precognition scheduler.
