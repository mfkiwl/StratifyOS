// Copyright 2011-2021 Tyler Gilbert and Stratify Labs, Inc; see LICENSE.md

/*! \addtogroup SCHED
 * @{
 *
 */

/*! \file */

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <pthread.h>
#include <reent.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>

#include "../unistd/unistd_local.h"
#include "sched.h"
#include "scheduler_root.h"
#include "sos/debug.h"

#include "cortexm/fault_local.h"

#include "trace.h"

static void start_first_thread();
static void svcall_fault_logged(void *args) MCU_ROOT_EXEC_CODE;

static int check_faults();

int scheduler_check_tid(int id) {
  if (id < task_get_total()) {
    if (task_enabled(id)) {
      return 0;
    }
  }
  return -1;
}

/* \details This function should be called in main() after all hardware
 * and devices have been initialized.
 *
 * The scheduler executes as task 0.  It is run as part of the round robin with all
 * active tasks of the highest priority.
 *
 * When a task calls sleep() or usleep(), it goes inactive.  When a task does
 * a synchronous read() or write() using underlying asynchronous hardware,
 * the task goes inactive until the operation completes.
 *
 * It an active task has a lower priority of another active task, it stays
 * "active" but is not executed until all higher priority tasks go inactive.
 *
 * If a lower priority task is executing and a higher priority task goes "active" (for
 * example, after sleep() or usleep() timer is expired), the lower priority task is
 * pre-empted.
 */
void scheduler() {

  scheduler_prepare();

  sos_debug_log_info(SOS_DEBUG_SCHEDULER, "Start first thread");
  start_first_thread();
  while (1) {
    check_faults(); // check to see if a fault needs to be logged

    // Sleep when nothing else is going on
    if (task_get_exec_count() == 0) {
      sos_config.sleep.idle();
    } else {
      // Otherwise switch to the active task
      sched_yield();
    }
  }
}

void svcall_fault_logged(void * args) {
  CORTEXM_SVCALL_ENTER();
  MCU_UNUSED_ARGUMENT(args);
  m_cortexm_fault.fault.num = 0;
}

int check_faults() {
  if (m_cortexm_fault.fault.num != 0) {
    char buffer[LINK_POSIX_TRACE_DATA_SIZE + 1];
    // Trace the fault -- and output on debug
    buffer[LINK_POSIX_TRACE_DATA_SIZE] = 0;

    snprintf(buffer, LINK_POSIX_TRACE_DATA_SIZE, "fault:%d", m_cortexm_fault.fault.num);
    sos_trace_event_addr_tid(
      POSIX_TRACE_FATAL, buffer, strlen(buffer), (u32)m_cortexm_fault.fault.pc + 1,
      m_cortexm_fault.tid);

    usleep(2000);
    sos_debug_log_error(SOS_DEBUG_SYS, "%s", buffer);

    snprintf(
      buffer, LINK_POSIX_TRACE_DATA_SIZE - 1, "addr:%p", m_cortexm_fault.fault.addr);

    sos_trace_event_addr_tid(
      POSIX_TRACE_FATAL, buffer, strlen(buffer), (u32)m_cortexm_fault.fault.pc + 1,
      m_cortexm_fault.tid);
    usleep(2000);
    sos_debug_log_error(SOS_DEBUG_SYS, "%s", buffer);

    strncpy(buffer, "caller", LINK_POSIX_TRACE_DATA_SIZE);
    sos_trace_event_addr_tid(
      POSIX_TRACE_MESSAGE, buffer, strlen(buffer), (u32)m_cortexm_fault.fault.caller,
      m_cortexm_fault.tid);
    sos_debug_log_error(
      SOS_DEBUG_SYS, "Caller 0x%lX %ld", (u32)m_cortexm_fault.fault.caller,
      m_cortexm_fault.tid);
    usleep(2000);

    snprintf(
      buffer, LINK_POSIX_TRACE_DATA_SIZE - 1, "stack:%ld",
      m_cortexm_fault.free_stack_size);

    sos_trace_event_addr_tid(
      POSIX_TRACE_MESSAGE, buffer, strlen(buffer), (u32)m_cortexm_fault.fault.pc + 1,
      m_cortexm_fault.tid);
    sos_debug_log_error(
      SOS_DEBUG_SYS, "Stack free %ld %ld", m_cortexm_fault.free_stack_size,
      m_cortexm_fault.tid);
    usleep(2000);

    snprintf(
      buffer, LINK_POSIX_TRACE_DATA_SIZE - 1, "heap:%ld", m_cortexm_fault.free_heap_size);

    sos_trace_event_addr_tid(
      POSIX_TRACE_MESSAGE, buffer, strlen(buffer), (u32)m_cortexm_fault.fault.pc + 1,
      m_cortexm_fault.tid);
    sos_debug_log_error(
      SOS_DEBUG_SYS, "Heap free %ld %ld", m_cortexm_fault.free_heap_size,
      m_cortexm_fault.tid);
    usleep(2000);

    strcpy(buffer, "root pc");
    sos_trace_event_addr_tid(
      POSIX_TRACE_MESSAGE, buffer, strlen(buffer),
      (u32)m_cortexm_fault.fault.handler_pc + 1, m_cortexm_fault.tid);
    sos_debug_log_error(
      SOS_DEBUG_SYS, "ROOT PC 0x%lX %ld", (u32)m_cortexm_fault.fault.handler_pc + 1,
      m_cortexm_fault.tid);
    usleep(2000);

    strcpy(buffer, "root caller");
    sos_trace_event_addr_tid(
      POSIX_TRACE_MESSAGE, buffer, strlen(buffer),
      (u32)m_cortexm_fault.fault.handler_caller, m_cortexm_fault.tid);
    sos_debug_log_error(
      SOS_DEBUG_SYS, "ROOT Caller 0x%lX %ld", (u32)m_cortexm_fault.fault.handler_caller,
      m_cortexm_fault.tid);
    usleep(2000);

    cortexm_svcall(svcall_fault_logged, NULL);
  }

  return 0;
}

// Called when the task stops or drops in priority (e.g., releases a mutex)
void scheduler_root_update_on_stopped() {
  int i;
  s8 next_priority;

  // Issue #130

  SOS_DEBUG_ENTER_CYCLE_SCOPE_AVERAGE();
  cortexm_disable_interrupts();
  next_priority = CONFIG_SCHED_LOWEST_PRIORITY;
  for (i = 1; i < task_get_total(); i++) {
    // Find the highest priority of all active tasks
    if (task_enabled_active_not_stopped(i) && (task_get_priority(i) > next_priority)) {
      next_priority = task_get_priority(i);
    }
  }
  task_root_set_current_priority(next_priority);
  cortexm_enable_interrupts();
  SOS_DEBUG_EXIT_CYCLE_SCOPE_AVERAGE(SOS_DEBUG_TASK, scheduler_critical, 5000);

  // this will cause an interrupt to execute but at a lower IRQ priority
  task_root_switch_context();
}

void scheduler_root_update_on_sleep() {
  scheduler_root_deassert_active(task_get_current());
  scheduler_root_update_on_stopped();
}

// called when a task wakes up
void scheduler_root_update_on_wake(int id, int new_priority) {
  // Issue #130
  if (new_priority < task_get_current_priority()) {
    return; // no action needed if the waking task is of lower priority than the currently
            // executing priority
  }

  if ((id > 0) && task_stopped_asserted(id)) {
    return; // this task is stopped on a signal and shouldn't be considered
  }

  // elevate the priority (changes only if new_priority is higher than current
  task_root_elevate_current_priority(new_priority);

  // execute the context switcher but at a lower priority
  task_root_switch_context();
}

// this is called from user space?
int scheduler_get_highest_priority_blocked(void *block_object) {
  int priority;
  int i;
  int new_thread;
  int current_task = task_get_current();

  // check to see if another task is waiting for the mutex
  priority = CONFIG_SCHED_LOWEST_PRIORITY - 1;
  new_thread = -1;

  // Issue #139 - blocked mutexes should be awarded in a round robin fashion started with
  // the current task
  if (current_task == 0) {
    current_task++;
  }
  i = current_task + 1;
  if (i == task_get_total()) {
    i = 1;
  }
  do {
    if (task_enabled(i)) {
      if (
        (sos_sched_table[i].block_object == block_object) && (!task_active_asserted(i))) {
        // it's waiting for the block -- give the block to the highest priority and
        // waiting longest
        if (
          !task_stopped_asserted(i)
          && (sos_sched_table[i].attr.schedparam.sched_priority > priority)) {
          //! \todo Find the task that has been waiting the longest time
          new_thread = i;
          priority = sos_sched_table[i].attr.schedparam.sched_priority;
        }
      }
    }
    // Issue #139
    i++;
    if (i == task_get_total()) {
      i = 1;
    }
  } while (i != current_task);

  return new_thread;
}
// This is only called from SVcall so it is always synchronous -- no re-entrancy issues
// with it
int scheduler_root_unblock_all(void *block_object, int unblock_type) {
  int i;
  int priority;
  priority = CONFIG_SCHED_LOWEST_PRIORITY - 1;
  for (i = 1; i < task_get_total(); i++) {
    if (task_enabled(i)) {
      if (
        (sos_sched_table[i].block_object == block_object) && (!task_active_asserted(i))) {
        // it's waiting for the semaphore -- give the semaphore to the highest priority
        // and waiting longest
        scheduler_root_assert_active(i, unblock_type);
        if (
          !task_stopped_asserted(i)
          && (sos_sched_table[i].attr.schedparam.sched_priority > priority)) {
          priority = sos_sched_table[i].attr.schedparam.sched_priority;
        }
      }
    }
  }
  return priority;
}

void start_first_thread() {
  pthread_attr_t attr;
  attr.stacksize = sos_config.task.start_stack_size;
  attr.stackaddr = malloc(attr.stacksize);
  if (attr.stackaddr == NULL) {
    sos_handle_event(SOS_EVENT_FATAL, "no memory for scheduler");
  }
  PTHREAD_ATTR_SET_IS_INITIALIZED((&attr), 1);
  PTHREAD_ATTR_SET_CONTENTION_SCOPE((&attr), PTHREAD_SCOPE_SYSTEM);
  PTHREAD_ATTR_SET_GUARDSIZE((&attr), CONFIG_TASK_DEFAULT_STACKGUARD_SIZE);
  PTHREAD_ATTR_SET_INHERIT_SCHED((&attr), PTHREAD_EXPLICIT_SCHED);
  PTHREAD_ATTR_SET_DETACH_STATE((&attr), PTHREAD_CREATE_DETACHED);
  PTHREAD_ATTR_SET_SCHED_POLICY((&attr), SCHED_RR);
  attr.schedparam.sched_priority = 21; // not the default priority

  const int err = scheduler_create_thread(
    sos_sched_table[0].init, sos_config.task.start_args, attr.stackaddr, attr.stacksize, &attr);

  if (!err) {
    sos_handle_event(SOS_EVENT_FATAL, "Failed to create thread");
  }
}

void scheduler_check_cancellation() {
  if (
    scheduler_cancel_asserted(task_get_current())
    && task_thread_asserted(task_get_current())) {
    // cancel this thread
    scheduler_thread_cleanup(NULL);
  }
}

/*! @} */
