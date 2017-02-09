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

#define DEBUG_PRINT(str, args...) do {                                              \
if(gh_debug == 1) {                                                                 \
  printf("CG DEBUG: %s: %d: %s\n" str, __FILENAME__, __LINE__, __func__, ##args); } \
} while(0)

static bool initialized = false;
static unsigned int gh_gpu_count = -1;
static bool gh_debug = false;

// Determine the number of GPU's available on the system
void set_gpu_count() {
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

// Check for any useful environment variables
void check_environment_variables() {
  // Check if debug should be enabled
  if(getenv("CG_DEBUG")) {
    gh_debug = true;
  }
}

// Initialize the health checker, this should only be called once
void initialize() {
  set_gpu_count();
  check_environment_variables();
  initialized = true;
}

void gpu_health(int sig) {

  // Preform initialization step
  if(!initialized) {
    initialize();
  }

  nvmlReturn_t nvml_err;

  // Init NVML
  nvml_err = nvmlInit();
  if(nvml_err != NVML_SUCCESS) {
    EXIT_PRINT("NVML Failure: %s\n", nvmlErrorString(nvml_err));
  }

  // Attempt to aquire a handle to all devices
  // This should fail in the GPU is in a "bad" state
  for(int i=0; i<gh_gpu_count; i++) {
    nvmlDevice_t device_handle;
    nvml_err = nvmlDeviceGetHandleByIndex(i, &device_handle);
    if(nvml_err != NVML_SUCCESS) {
      EXIT_PRINT("NVML Failure: %s\n", nvmlErrorString(nvml_err));
    }
  }

  // Cleanup NVML
  nvml_err = nvmlShutdown();
  if(nvml_err != NVML_SUCCESS) {
    EXIT_PRINT("NVML Failure: %s\n", nvmlErrorString(nvml_err));
  }

  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  DEBUG_PRINT("GPU check passed at %s\n", asctime(tm));
}
