#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include "alps/libalpslli.h"
#include <fcntl.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define EXIT_PRINT(str, args...) do { fprintf(stderr, "ERROR Check File: %s:%d:%s(): " str, \
		                               __FILE__, __LINE__, __func__, ##args); \
	                              exit(EXIT_FAILURE); } while(0)

#define SIGKILL_PRINT(str, args...) do { fprintf(stderr, "ERROR Check File: %s:%d:%s(): " str, \
                                               __FILE__, __LINE__, __func__, ##args); \
                                      raise(SIGKILL); } while(0)
#define DEBUG_PRINT(str, args...) do {                                              \
if(fp_debug == 1) {                                                                 \
  time_t t = time(NULL);                                                            \
  struct tm *tm = localtime(&t);                                                    \
  char *time = asctime(tm);                                                         \
  time[strlen(time) - 1] = 0;                                                       \
  printf("FP DEBUG(%s): %s: %d: %s: " str, time,  __FILENAME__, __LINE__, __func__, ##args); } \
} while(0)

#define FP_MAX_PATH_LENGTH 2048

static bool fp_initialized = false;
static bool fp_debug = false;
static unsigned long fp_initial_skips = 1;
static unsigned long fp_interval_stride = 1;
static char fp_file[FP_MAX_PATH_LENGTH];
static unsigned long fp_min_bytes = 0;
static unsigned long fp_min_lines = 0;
static unsigned long fp_min_lines_progress = 0;
static unsigned long fp_min_bytes_progress = 0;
static unsigned long fp_one_shot = 0;
static bool fp_single_process = false;
static bool fp_master_process = false;
static int lock_fd = -1;

// Return the number of bytes in file_name
long long file_bytes(const char* file_name) {
  struct stat st;
  int err = stat(file_name, &st);
  if(err != 0) {
    SIGKILL_PRINT("Error stating %s: %s\n", file_name, strerror(errno));
  }
  return (long long)st.st_size;  
}

// Return the number of lines file_name
long long file_lines(const char* file_name) {
  // Open File and count number of new lines
  FILE *file = fopen(file_name, "r");
  if(file == NULL) {
    SIGKILL_PRINT("Error opening %s: %s\n", file_name, strerror(errno));
  }
  int c;
  long long new_lines = 0;
  do {
    c = fgetc(file);
    if(c == '\n') {
      new_lines++;
    }
  } while(c != EOF);
  fclose(file);

  return new_lines;
}

// Kill the batch job
// This is required as the hangs can make the process non responsive to SIGKILL
void kill_job() {
  // Get App ID
  int alps_id = atoi(getenv("ALPS_APP_ID"));

  DEBUG_PRINT("Sending low level ALPS request to fail\n");

  // Contact ALPS to tell it we need to die
  int32_t buf[2] = { SIGKILL, -1 };
  int lli_ret = alps_app_lli_put_simple_request(ALPS_APP_LLI_ALPS_REQ_SIGNAL, buf, sizeof(buf));
  if (0 != lli_ret) {
    SIGKILL_PRINT("Failed to send low level ALPS message, Attempting to SIGKILL process\n");
  }

}

// Check for any useful environment variables
void check_environment_variables() {
  if(getenv("FP_DEBUG")) {
    fp_debug = true;
  }

  // Initial number of intervals to skip
  if(getenv("FP_INITIAL_SKIPS")) {
    fp_initial_skips = strtoul(getenv("FP_INITIAL_SKIPS"), NULL, 0);
  }

  // Number of intervals between checking file progress, after intial number of skipped intervals
  if(getenv("FP_INTERVAL_STRIDE")) {
    fp_interval_stride = strtoul(getenv("FP_INITIAL_STRIDE"), NULL, 0);
  }

  // File to check
  if(getenv("FP_FILE")) {
    strcpy(fp_file, getenv("FP_FILE"));
  } else {
    // Default to the temporary file created by PBS
    sprintf(fp_file, "%s.OU", getenv("PBS_JOBID"));
  }

  // Minimum filesize in bytes
  if(getenv("FP_MIN_BYTES")) {
    fp_min_bytes = strtoul(getenv("FP_MIN_BYTES"), NULL, 0);
  }

  // Minimum line count
  if(getenv("FP_MIN_LINES")) {
    fp_min_lines = strtoul(getenv("FP_MIN_LINES"), NULL, 0);
  }

  // Minimum lines added to file since last check
  if(getenv("FP_MIN_LINES_PROGRESS")) {
    fp_min_lines_progress = strtoul(getenv("FP_MIN_LINES_PROGRESS"), NULL, 0);
  }

  // Minimum bytes added to file since last check
  if(getenv("FP_MIN_BYTES_PROGRESS")) {
    fp_min_bytes_progress = strtoul(getenv("FP_MIN_BYTES_PROGRESS"), NULL, 0);
  }

  // Enable a single check after which FileProgress will do nothing
  if(getenv("FP_ONE_SHOT")) {
    fp_one_shot = true;
  }

  // Only preform the check from a single process
  if(getenv("FP_SINGLE_PROCESS")) {
    fp_single_process = true;
  } 
}

void initialize() {
  check_environment_variables();

  if(fp_single_process) {
    // Create lock file if one doesn't exist, the pwd is expected to be shared amongst the processes
    lock_fd = open("file_progress.lock", O_CREAT|O_RDWR, S_IRUSR | S_IWUSR);
    // We don't check for a failure to create the file
    // As a maximum of one process will be able to create it

    // Attempt to aquire lock on file
    struct flock lock_struct;
    memset(&lock_struct, 0, sizeof(lock_struct));

    lock_struct.l_type = F_WRLCK;
    lock_struct.l_whence = SEEK_SET;
    lock_struct.l_pid = getpid();

    int locked = fcntl(lock_fd, F_SETLK, &lock_struct);
    
    if(locked == -1) {
      fp_master_process = true;
    }
  }

  fp_initialized = true;
}

void file_progress() {
  if(!fp_initialized) {
    initialize();
  }

  // Number of total file interval checks since start
  static unsigned long long interval_count = 0;

  // If the file check has been performed
  static bool fired = false;

  // Determine if we should check the file
  bool should_check = false;
  if(fp_single_process && fp_single_process) { // We  won the lock file race
    if(interval_count >= fp_initial_skips) { // Skip initial intervals
      if((interval_count - fp_initial_skips) % fp_interval_stride == 0) { // Stride the file check against the interval
        if(!(fp_one_shot && fired)) { // If we're in oneshot mode only fire once
          should_check = true;
        }
      }
    }
  }

  // Preform the requested file checks if neccessary
  if(should_check) {

    if(fp_min_bytes > 0) {
      long long bytes = file_bytes(fp_file);
      if(bytes < fp_min_bytes) {
        EXIT_PRINT("%s contains %lld bytes which is less than the required %lu", fp_file, bytes, fp_min_bytes);
        kill_job();
      }
      DEBUG_PRINT("%s contains %lld bytes\n", fp_file, bytes);
    }

    if(fp_min_lines > 0) {
      long long lines = file_lines(fp_file);
      if(lines < fp_min_lines) {
        EXIT_PRINT("%s contains %lld bytes which is less than the required %lu", fp_file, lines, fp_min_lines);
        kill_job();
      }
      DEBUG_PRINT("%s contains %lld lines\n", fp_file, lines);
    }

    if(fp_min_lines_progress > 0) {
      static int previous_lines = 0;
      long long lines = file_lines(fp_file);
      if(lines - previous_lines < fp_min_lines_progress) {
        EXIT_PRINT("%s only added %lld lines but needed to add %lld \n", fp_file, lines - previous_lines, fp_min_lines_progress);
        kill_job();
      }
      DEBUG_PRINT("%s contained %lld lines and now contains %lld\n", fp_file, lines, previous_lines);

      previous_lines = lines;
    }

    if(fp_min_bytes_progress > 0) {
      static int previous_bytes = 0;
      long long bytes = file_bytes(fp_file);
      if(bytes - previous_bytes < fp_min_bytes_progress) {
        EXIT_PRINT("%s only added %lld bytes but needed to add %lld \n", fp_file, bytes - previous_bytes, fp_min_bytes_progress);
        kill_job();
      }
      DEBUG_PRINT("%s contained %lld bytes and now contains %lld\n", fp_file, bytes, previous_bytes);

      previous_bytes = bytes;
    }

    fired = true;
  }
 
  interval_count++;
}
