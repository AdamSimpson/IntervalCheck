#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include "nvml.h"
#include <cuda_runtime.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define EXIT_PRINT(str, args...) do { fprintf(stderr, "ERROR Check GPU: %s:%d:%s(): " str, \
		                               __FILE__, __LINE__, __func__, ##args); \
	                              exit(EXIT_FAILURE); } while(0)

#define SIGKILL_PRINT(str, args...) do { fprintf(stderr, "ERROR Check GPU: %s:%d:%s(): " str, \
                                               __FILE__, __LINE__, __func__, ##args); \
                                      raise(SIGKILL); } while(0)
#define DEBUG_PRINT(str, args...) do {                                              \
if(gh_debug == 1) {                                                                 \
  time_t t = time(NULL);                                                            \
  struct tm *tm = localtime(&t);                                                    \
  char *time = asctime(tm);                                                         \
  time[strlen(time) - 1] = 0;                                                       \
  printf("GH DEBUG(%s): %s: %d: %s: " str, time,  __FILENAME__, __LINE__, __func__, ##args); } \
} while(0)

static bool initialized = false;
static unsigned int gh_gpu_count = 0;
static bool gh_debug = false;
static bool passed_last_test = true;
static unsigned int watchdog_timeout = 0;
static struct itimerspec watchdog_time;
static timer_t watchdog_id = 0;

// Kill the batch job
// This is required as the hangs can make the process non responsive to SIGKILL
void kill_job() {
  char kill_command[1024];

  // Kill LSF job, this kills the entire job and not just the job step
  // e.g. GH_BATCH_KILL=bkill , GH_BATCH_ID_VAR=LSB_JOBID
  sprintf(kill_command, "%s %s", getenv("GH_BATCH_KILL"), getenv(getenv("GH_BATCH_ID_VAR")));
  DEBUG_PRINT("Sending batch kill command %s\n", kill_command);

  system(kill_command);
  sleep(60);
  raise(SIGKILL);
}

void watchdog_handler(int sig) {
  DEBUG_PRINT("Watchdog timer for GPU Health expired: GPU hung for %d seconds\n", watchdog_timeout);
  kill_job();
}

// Determine the number of GPU's available on the system
void set_gpu_count() {

  // If the value is specified don't worry about initilization
  if(getenv("GH_GPU_COUNT")) {
    gh_gpu_count = atoi(getenv("GH_GPU_COUNT"));
    return;
  } 
  else {
    nvmlReturn_t nvml_err;

    // Init NVML
    nvml_err = nvmlInit();
    if(nvml_err != NVML_SUCCESS) {
      EXIT_PRINT("NVML Failure: %s\n", nvmlErrorString(nvml_err));
    }

    // Get number of GPUs
    nvmlDeviceGetCount(&gh_gpu_count);
    if(nvml_err != NVML_SUCCESS) {
      EXIT_PRINT("NVML Failure: %s\n", nvmlErrorString(nvml_err));
    }

    // Cleanup NVML
    nvml_err = nvmlShutdown();
    if(nvml_err != NVML_SUCCESS) {
      EXIT_PRINT("NVML Failure: %s\n", nvmlErrorString(nvml_err));
    }
  }
}

// Check for any useful environment variables
void check_environment_variables() {
  // Check if debug should be enabled
  if(getenv("GH_DEBUG")) {
    gh_debug = true;
  }

  if(getenv("GH_TIMEOUT")) {
    watchdog_timeout = atoi(getenv("GH_TIMEOUT"));
  }

  if(getenv("GH_GPU_COUNT")) {
    gh_gpu_count = atoi(getenv("GH_GPU_COUNT"));
  }

}

// Initialize the watchdog timer for the first time
void init_watchdog() {
  // Set handler for SIGUSR1
  struct sigaction action;
  struct sigaction *old_action = NULL;
  memset(&action, 0, sizeof(action));
  action.sa_handler = &watchdog_handler;

  int err = sigaction(SIGUSR1, &action, old_action);
  if(old_action != NULL) {
    EXIT_PRINT("Check GPU: SIGPROF already set\n");
  }
  if(err != 0) {
    EXIT_PRINT("Failed to set SIGPROF handler: %s\n", strerror(errno));
  }

  // Create timer to fire SIGUSR1
  struct sigevent sev;
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIGUSR1;
  sev.sigev_value.sival_ptr = &watchdog_id;
  err = timer_create(CLOCK_REALTIME, &sev, &watchdog_id);
  if(err != 0) {
    EXIT_PRINT("Failed to create watchdog timer: %s\n", strerror(errno));
  }

  // Default watchdog timeout to 30 seconds
  if(watchdog_timeout == 0) {
    watchdog_timeout = 30;
  }
}

// Arm the watchdog timer
void arm_watchdog() {
  DEBUG_PRINT("Arming Watchdog timer\n");

  // Set the singleshot watchdog timer
  watchdog_time.it_interval.tv_sec = 0;
  watchdog_time.it_interval.tv_nsec = 0;
  watchdog_time.it_value.tv_sec = watchdog_timeout;
  watchdog_time.it_value.tv_nsec = 0;

  // Set the timer
  int err = timer_settime(watchdog_id, 0, &watchdog_time, NULL);
  if(err != 0) {
    EXIT_PRINT("Failed to arm watchdog timer: %s\n", strerror(errno));
  }
}

// Disarm the watchdog timer
void disarm_watchdog() {
  DEBUG_PRINT("Disarming Watchdog timer\n");

  // Set the singleshot watchdog timer
  watchdog_time.it_interval.tv_sec = 0;
  watchdog_time.it_interval.tv_nsec = 0;
  watchdog_time.it_value.tv_sec = 0;
  watchdog_time.it_value.tv_nsec = 0;

  // Set the ti mer
  int err = timer_settime(watchdog_id, 0, &watchdog_time, NULL);
  if(err != 0) {
    EXIT_PRINT("Failed to unset watchdog timer: %s\n", strerror(errno));
  }
}

// Initialize the health checker, this should only be called once
void initialize() {
  check_environment_variables();

  // Prep the watchdog timer
  init_watchdog();

  // Determine GPU count if not explicitly set
  if(gh_gpu_count == 0) {
    // This can potentially fail so we watchdog it
    arm_watchdog();
    set_gpu_count();
    disarm_watchdog();
  }

  initialized = true;
}

// Attempt to initialize NVML, If this succeeds the GPU should be in OK shape
void gpu_health(int sig) {

  // Preform initialization step
  if(!initialized) {
    passed_last_test = false;
    initialize();
    passed_last_test = true;
  }

  // Start singleshot watchdog timer
  arm_watchdog();
  
  // If the GPU is locked up very tight the nvml_* funcitons hang
  // so we test if our last test ever finished
  if(!passed_last_test) {
    fprintf(stderr, "GPU Health Failure: GPU likely hung\n");
    kill_job();
  }

  passed_last_test = false;
  nvmlReturn_t nvml_err;

  // Init NVML
  DEBUG_PRINT("Calling nvmlInit\n");
  nvml_err = nvmlInit();
  if(nvml_err != NVML_SUCCESS) {
    EXIT_PRINT("NVML Failure: %s\n", nvmlErrorString(nvml_err));
  }

  // Attempt to aquire a handle to all devices
  // This should fail in the GPU is in a "bad" state
  for(int i=0; i<gh_gpu_count; i++) {
    DEBUG_PRINT("Attemping to get handle to GPU %d of %d\n", i, gh_gpu_count-1);
    nvmlDevice_t device_handle;
    nvml_err = nvmlDeviceGetHandleByIndex(i, &device_handle);
    if(nvml_err != NVML_SUCCESS) {
      EXIT_PRINT("NVML Failure: %s\n", nvmlErrorString(nvml_err));
    }
  }

  // The tests have passed(or died trying to pass)
  passed_last_test = true;

  // Cleanup NVML
  DEBUG_PRINT("Calling nvmlShutdown\n");
  nvml_err = nvmlShutdown();
  if(nvml_err != NVML_SUCCESS) {
    EXIT_PRINT("NVML Failure: %s\n", nvmlErrorString(nvml_err));
  }

  // Disarm the watchdog timer
  disarm_watchdog();

  DEBUG_PRINT("GPU check passed\n");
}
