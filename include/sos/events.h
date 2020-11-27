#ifndef SOS_SOS_EVENTS_H
#define SOS_SOS_EVENTS_H

#include <sdk/types.h>

enum {
  SOS_EVENT_ROOT_RESET,
  SOS_EVENT_ROOT_FATAL,
  SOS_EVENT_ROOT_TASK_INITIALIZED,
  SOS_EVENT_ROOT_INITIALIZED,
  SOS_EVENT_ROOT_WDT_TIMEDOUT,
  SOS_EVENT_ROOT_DEBUG_INITIALIZED,
  SOS_EVENT_ROOT_MPU_INITIALIZED,
  SOS_EVENT_ROOT_ENABLE_CACHE = SOS_EVENT_ROOT_MPU_INITIALIZED,
  SOS_EVENT_ROOT_PREPARE_DEEPSLEEP,
  SOS_EVENT_ROOT_RECOVER_DEEPSLEEP,
  SOS_EVENT_ROOT_ENABLE_DEBUG_LED,
  SOS_EVENT_ROOT_DISABLE_DEBUG_LED,
  SOS_EVENT_ROOT_GET_SERIAL_NUMBER,
  SOS_EVENT_FATAL,
  SOS_EVENT_TASK_INITIALIZED,
  SOS_EVENT_FILESYSTEM_INITIALIZED,
  SOS_EVENT_START_LINK,
  SOS_EVENT_HIBERNATE,
  SOS_EVENT_POWERDOWN,
  SOS_EVENT_WAKEUP_FROM_HIBERNATE,
  SOS_EVENT_SERVICE_CALL_PERMISSION_DENIED,
  SOS_EVENT_SCHEDULER_IDLE,
  SOS_EVENT_ASSERT
};

#endif // SOS_EVENTS_H
