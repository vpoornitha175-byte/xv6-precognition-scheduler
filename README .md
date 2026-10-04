# Precognition CPU-Prediction Scheduler for xv6-riscv

## Overview

This project extends the xv6-riscv operating system with a
**Precognition CPU-prediction-based scheduler**.

The scheduler observes the CPU burst behavior of processes, keeps a
short history of their previous CPU bursts, predicts their future CPU
demand, and uses that prediction to calculate a scheduling score.

The main idea is:

``` text
CPU burst
    ↓
CPU history
    ↓
Predicted CPU demand
    ↓
Scheduling score
    ↓
Process selection
```

A process with a smaller predicted CPU demand receives a higher score
and is preferred by the scheduler.

------------------------------------------------------------------------

## Project Goal

The default xv6 scheduler is primarily a simple round-robin scheduler.
This project experiments with a prediction-based scheduling policy.

The Precognition scheduler attempts to answer:

> "Based on the CPU time this process has used recently, how much CPU
> time is it likely to need next?"

The prediction is then used to rank runnable processes.

------------------------------------------------------------------------

## Scheduling Algorithm

Each process contains a Precognition state:

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

### CPU burst collection

Every timer tick, if the current process is running, its CPU burst
counter is incremented.

Conceptually:

``` text
Running process
      ↓
Timer interrupt
      ↓
cpu_burst++
```

When the process completes a CPU burst, the value is recorded in its
history.

The history is implemented as a circular buffer with a maximum of 8
samples.

------------------------------------------------------------------------

## Prediction

The current implementation uses a simple arithmetic mean.

For example, if a process has:

``` text
CPU history = [1, 2, 3]
```

then:

``` text
Prediction = (1 + 2 + 3) / 3
           = 2
```

The prediction is calculated by:

``` c
total / history_count
```

Integer arithmetic is used because this is kernel code.

If there is no history yet, the scheduler treats the prediction as `1`
so that a new process can participate in scheduling.

------------------------------------------------------------------------

## Scheduling Score

The scheduler converts the prediction into a score:

``` c
score = 1000 / (predicted + 1);
```

Therefore:

    Predicted CPU   Score
  --------------- -------
                1     500
                2     333
                3     250
                4     200
                5     166

The scheduler selects the runnable process with the **highest score**.

Therefore:

``` text
Smaller predicted CPU demand
            ↓
       Higher score
            ↓
       Higher priority
```

This makes the policy similar to a prediction-based shortest-job
preference.

------------------------------------------------------------------------

## Modified Files

The main project changes are:

``` text
kernel/proc.h
kernel/precog.h
kernel/precog.c
kernel/proc.c
kernel/trap.c
user/precogtest.c
user/fairtest.c
```

Additional syscall files may also be modified if the `getprecogcount()`
syscall is present:

``` text
kernel/syscall.h
kernel/syscall.c
kernel/sysproc.c
user/user.h
user/usys.pl
```

------------------------------------------------------------------------

## Kernel Changes

### `kernel/proc.h`

The process structure contains a Precognition state.

This allows every process to maintain:

-   CPU burst history
-   number of recorded samples
-   current history position
-   predicted CPU demand
-   current CPU burst
-   scheduling count

The Precognition state belongs to the process, so each process learns
independently.

------------------------------------------------------------------------

### `kernel/precog.h`

Contains the declarations for the Precognition functions.

Important functions include:

``` c
void precog_init_process(struct proc *p);
void precog_record_cpu(struct proc *p, uint64 cpu_ticks);
uint64 precog_predict_cpu(struct proc *p);
struct precog_state *precog_get_state(struct proc *p);
```

------------------------------------------------------------------------

### `kernel/precog.c`

This is the main Precognition implementation.

#### Process initialization

`precog_init_process()` initializes the prediction state for a process.

Initially:

``` text
history_count = 0
history_index = 0
predicted_cpu = 0
cpu_burst = 0
```

#### Recording CPU bursts

`precog_record_cpu()` stores a completed CPU burst in the circular
history buffer.

The oldest sample is eventually overwritten when the buffer becomes
full.

#### Prediction

`precog_predict_cpu()` calculates the average of the available CPU burst
samples.

------------------------------------------------------------------------

### `kernel/trap.c`

The timer interrupt was extended so that the currently running process
accumulates CPU activity.

Conceptually:

``` c
if(p != 0 && p->state == RUNNING)
  p->precog.cpu_burst++;
```

This gives the scheduler an observation of how long the process has been
consuming CPU.

------------------------------------------------------------------------

### `kernel/proc.c`

This contains the major scheduler integration.

The scheduler:

1.  Scans runnable processes.
2.  Calculates each process's CPU prediction.
3.  Converts the prediction into a score.
4.  Selects the process with the highest score.
5.  Increments the process's scheduling count.
6.  Runs the selected process.

The scheduler also prints limited prediction information for testing:

``` text
PRECOG SELECT: PID=10 | Prediction=3 | Score=250
```

The logging is intentionally limited to prediction changes so that the
console is not flooded with messages.

------------------------------------------------------------------------

## CPU Burst Recording

CPU bursts are recorded when a process enters a state where its current
CPU burst has completed, such as when it goes to sleep.

The recorded burst is then available to the prediction algorithm.

A possible future improvement is to record bursts at additional
scheduling boundaries, such as `yield()`, so that CPU-bound processes
can update their history more frequently.

------------------------------------------------------------------------

## Scheduling Count

Each process has:

``` c
uint64 scheduling_count;
```

This is incremented whenever the Precognition scheduler selects that
process to run.

The test program can retrieve this through the `getprecogcount()`
syscall when that syscall is enabled in the project.

This is useful for observing scheduling behavior and comparing processes
with different workloads.

------------------------------------------------------------------------

## Test Programs

### `user/precogtest.c`

This test creates three child processes with different CPU workloads:

``` text
Child 1 → small workload
Child 2 → medium workload
Child 3 → large workload
```

The workloads currently use approximately:

``` text
10000
50000
100000
```

iterations.

The test demonstrates that CPU behavior is observed and that predictions
change as processes execute.

Example:

``` text
PRECOG SELECT: PID=6 | Prediction=2 | Score=333
PRECOG SELECT: PID=6 | Prediction=3 | Score=250
```

------------------------------------------------------------------------

### `user/fairtest.c`

This test evaluates scheduling behavior using three workloads:

``` text
Small   → 10000
Medium  → 50000
Large   → 100000
```

It reports:

-   completion tick
-   scheduling count
-   total elapsed ticks

Example:

``` text
Process 1: Small workload completed at tick 129
Scheduling count: 2

Process 2: Medium workload completed at tick 132
Scheduling count: 7

Process 3: Large workload completed at tick 134
Scheduling count: 13
```

The test demonstrates that all workloads eventually complete.

This should be described as a **fairness/progress observation**, not as
a formal proof of fairness.

------------------------------------------------------------------------

## Building the Project

From the xv6-riscv directory:

``` bash
cd /home/vpoor/projects/xv6-riscv
```

Clean and build:

``` bash
make clean
make
```

Run xv6 with one CPU for easier-to-read output:

``` bash
make qemu CPUS=1
```

Inside xv6:

``` text
$ precogtest
```

Then:

``` text
$ fairtest
```

Exit QEMU with:

``` text
Ctrl+A
X
```

------------------------------------------------------------------------

## Recommended Testing

### Test 1: Precognition behavior

Run:

``` text
$ precogtest
```

Look for:

``` text
PRECOG SELECT: PID=... | Prediction=... | Score=...
```

The important observation is that predictions can change as CPU history
is collected.

For example:

``` text
Prediction=2 | Score=333
Prediction=3 | Score=250
```

This demonstrates that the scheduler is adapting to observed CPU
behavior.

### Test 2: Scheduling behavior

Run:

``` text
$ fairtest
```

Compare:

-   completion ticks
-   scheduling counts
-   prediction messages

All test processes should eventually complete.

------------------------------------------------------------------------

## Example Output

A representative run may look like:

``` text
$ precogtest
Precognition scheduler test starting
Child 1 finished
Child 2 finished
PRECOG SELECT: PID=6 | Prediction=2 | Score=333
PRECOG SELECT: PID=6 | Prediction=3 | Score=250
Child 3 finished
Precognition scheduler test finished
```

And:

``` text
$ fairtest

=== PRECOGNITION FAIRNESS TEST ===
Process 1: Small workload completed at tick 129
Scheduling count: 2
PRECOG SELECT: PID=10 | Prediction=2 | Score=333
PRECOG SELECT: PID=9 | Prediction=2 | Score=333
Process 2: Medium workload completed at tick 132
Scheduling count: 7
PRECOG SELECT: PID=10 | Prediction=3 | Score=250
Process 3: Large workload completed at tick 134
Scheduling count: 13

All processes completed
Total elapsed ticks: 5
=== FAIRNESS TEST FINISHED ===
```

Exact PIDs, ticks, scheduling counts, and message ordering can vary
between runs.

------------------------------------------------------------------------

## Why Output Can Still Interleave

xv6 may run multiple processes concurrently, particularly when using
multiple harts.

Several processes can write to the same QEMU console at approximately
the same time.

Therefore, messages such as:

``` text
Child 1...
Child 2...
Child 3...
```

can occasionally appear interleaved.

This does not by itself indicate a scheduler failure.

For a cleaner demonstration, use:

``` bash
make qemu CPUS=1
```

------------------------------------------------------------------------

## Limitations

This is an experimental scheduler, not a production scheduling
algorithm.

### 1. Simple prediction model

The prediction currently uses only an arithmetic average.

More advanced approaches could use:

-   exponential moving averages
-   weighted averages
-   burst classification
-   adaptive prediction
-   machine-learning-based prediction

### 2. No formal fairness guarantee

The scheduler prioritizes processes with lower predicted CPU demand.

A process with a consistently high predicted CPU demand can receive a
lower score.

The current implementation does not provide a formal
starvation-prevention mechanism.

### 3. Integer arithmetic

Predictions use integer division.

For example:

``` text
26 / 7 = 3
```

rather than a fractional value.

### 4. Small history window

Only the most recent eight CPU burst samples are retained.

### 5. Prediction cold start

A process without history has no learned CPU behavior.

The scheduler therefore uses an initial prediction of `1`.

------------------------------------------------------------------------

## Future Improvements

Possible future extensions include:

1.  Record CPU bursts at every scheduling boundary.
2.  Use an exponential moving average instead of a simple average.
3.  Add aging to prevent starvation.
4.  Add a maximum wait-time mechanism.
5.  Compare Precognition against the original xv6 scheduler.
6.  Collect more detailed scheduling statistics.
7.  Add a syscall to expose prediction values to user programs.
8.  Experiment with different history sizes.
9.  Compare different scoring functions.
10. Evaluate performance with more workloads.

------------------------------------------------------------------------

## Project Summary

The project modifies xv6-riscv to implement a prediction-based CPU
scheduler.

The scheduler:

``` text
observes CPU behavior
        ↓
stores recent CPU bursts
        ↓
predicts future CPU demand
        ↓
calculates a scheduling score
        ↓
selects the highest-scoring runnable process
```

The implementation demonstrates how historical CPU behavior can be used
to influence scheduling decisions inside a small operating-system
kernel.

------------------------------------------------------------------------

## Team

Add team member names and responsibilities here.

Example:

``` text
Member 1:
- Precognition algorithm
- CPU history and prediction

Member 2:
- Scheduler integration
- Process scheduling statistics

Member 3:
- Test programs
- Testing and documentation
```
