#ifndef PRECOG_H
#define PRECOG_H

#include "types.h"

// Forward declaration.
// The complete struct proc is defined in proc.h.
struct proc;

// Initialize the precognition system.
void precog_init(void);

// Initialize precognition state for a process.
void precog_init_process(struct proc *p);

// Record one CPU observation for a process.
void precog_record_cpu(struct proc *p, uint64 cpu_ticks);

// Calculate predicted CPU demand.
uint64 precog_predict_cpu(struct proc *p);

// Get the precognition state of a process.
struct precog_state *precog_get_state(struct proc *p);

#endif