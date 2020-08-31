/**
 * @file task.h
 * @author Christian Menges
 */

#ifndef PARALLEL_PACKED_CSR_TASK_H
#define PARALLEL_PACKED_CSR_TASK_H

/** Struct for tasks to the threads */
struct task {
  bool add;    // True if this is an add task. If this is false it means it's a delete.
  bool read;   // True if this is a read task.
  int src;     // Source vertex for this task's edge
  int target;  // Target vertex for this task's edge
};

#endif  // PARALLEL_PACKED_CSR_TASK_H
