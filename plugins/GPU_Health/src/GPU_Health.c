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
  printf("GH DEBUG: %s: %d: %s\n" str, __FILENAME__, __LINE__, __func__, ##args); } \
} while(0)

static bool initialized = false;
static unsigned int gh_gpu_count = -1;
static bool gh_debug = false;
static bool passed_last_test = true;

// Kill the batch job
// This is required as the hangs can make the process non responsive to SIGKILL
void kill_job() {
  char kill_command[1024];

  // Kill LSF job, this kills the entire job and not just the job step
  // e.g. GH_BATCH_KILL=appkill , GH_BATCH_ID_VAR=APPID
  sprintf(kill_command, "%s %s", getenv("GH_BATCH_KILL"), getenv(getenv("GH_BATCH_ID_VAR")));
  DEBUG_PRINT("Sending batch kill command %s\n", kill_command);

  system(kill_command);
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
}

// Initialize the health checker, this should only be called once
void initialize() {
  check_environment_variables();

  if(getenv("GH_GPU_COUNT")) {
    gh_gpu_count = atoi(getenv("GH_GPU_COUNT"));
  } else { 
    set_gpu_count();
  }

  initialized = true;
}

void gpu_health(int sig) {

  // Preform initialization step
  if(!initialized) {
    passed_last_test = false;
    initialize();
    passed_last_test = true;
  }

  // If the GPU is locked up very tight the nvml_* funcitons hang
  // so we test if our last test ever finished
  if(!passed_last_test) {
    fprintf(stderr, "GPU Health Failure: GPU likely hung\n");
    kill_job();
    sleep(60);
    raise(SIGKILL);
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

  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  DEBUG_PRINT("GPU check passed at %s\n", asctime(tm));
}
