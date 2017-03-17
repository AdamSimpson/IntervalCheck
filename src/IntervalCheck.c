#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define EXIT_PRINT(str, args...) do { fprintf(stderr, "ERROR Interval Check: %s:%d:%s(): " str, \
		                               __FILE__, __LINE__, __func__, ##args); \
	                              exit(EXIT_FAILURE); } while(0)

#define DEBUG_PRINT(str, args...) do {                                              \
if(ic_debug == 1) {                                                                 \
  printf("IC DEBUG: %s: %d: %s: " str, __FILENAME__, __LINE__, __func__, ##args); } \
} while(0)

static bool ic_debug = false;
static bool ic_per_node = false;

static void *dl_handle = NULL;
static char *lock_file_name;
static int lock_fd = -1;

typedef void (*ic_callback_t)(void);

#define MAX_CALLBACKS 1024
static ic_callback_t callbacks[MAX_CALLBACKS];
static int callback_count = 0;

// Handler called by alarm at specified interval
void alarm_handler(int sig) {
  DEBUG_PRINT("Handling callbacks\n");

  // Call all requested callbacks
  for(int i=0; i<callback_count; i++) {
    (*callbacks[i])();
  }
}

void setup_timer() {
  bool create_timer = true;

  // Create only one timer per node if specified
  if(ic_per_node) {
    // Determine absolute lock file path
    lock_file_name = (char*)malloc(sizeof(char)*1024);
    char tmp_dir[756] = "/tmp";
    if(getenv("TMPDIR")){
      strncpy(tmp_dir, getenv("TMPDIR"), 756);
    }
    sprintf(lock_file_name, "%s/interval_check.lock", tmp_dir);

    // Create lock file if one doesn't exist
    lock_fd = open(lock_file_name, O_CREAT|O_RDWR, S_IRUSR | S_IWUSR); 
    // We don't check for a failure to create the file
    // As a maximum of one process per node will be able to create it

    // Attempt to aquire lock on file
    struct flock lock_struct;
    memset(&lock_struct, 0, sizeof(lock_struct));

    lock_struct.l_type = F_WRLCK;
    lock_struct.l_whence = SEEK_SET;
    lock_struct.l_pid = getpid();

    int locked = fcntl(lock_fd, F_SETLK, &lock_struct);

    // If lock failed we aren't the first process on the node
    // to reach this point so no timer is created
    if(locked == -1) {
      create_timer = false;
    }
  }

  if (create_timer == true) {
    int err;
    struct itimerval timer;

    // Set handler for timer signal SIGALRM
    struct sigaction action;
    struct sigaction *old_action = NULL;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &alarm_handler;

    err = sigaction(SIGALRM, &action, old_action);
    if(old_action != NULL) {
      EXIT_PRINT("Check GPU: SIGALRM already set\n");
    }
    if(err != 0) {
      EXIT_PRINT("Failed to set SIGALM handler: %s\n", strerror(errno));
    }

    // Check if a ITIMER_REAL already is set
    getitimer(ITIMER_REAL, &timer);
    if(timer.it_interval.tv_sec  != 0 ||
       timer.it_interval.tv_usec != 0 ||
       timer.it_value.tv_sec     != 0 ||
       timer.it_value.tv_usec    != 0) {
      DEBUG_PRINT("WARNING: ITIMER_REAL already set, overwriting\n");
    }

    // Set the timer interval
    if(getenv("IC_INTERVAL")) {
      timer.it_interval.tv_sec = atoi(getenv("IC_INTERVAL"));
    }
    else { // Default to 5 minutes
      timer.it_interval.tv_sec = 60*5;
    }
    timer.it_interval.tv_usec = 0;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 1; // If the initial value is 0 the timer won't begin

    // Set the timer
    err = setitimer(ITIMER_REAL, &timer, NULL);
    if(err != 0) {
      EXIT_PRINT("Failed to set timer: %s\n", strerror(errno));
    }

  }
}

void process_environment_variables() {
  // Check if LD_PRELOAD should be unset, this can be helpful on Cray's
  if (getenv("IC_UNSET_PRELOAD")) {
    unsetenv("LD_PRELOAD");
  }

  // Check if debug should be enabled
  if(getenv("IC_DEBUG")) {
    ic_debug = true;
  }

  // Check if a single check should be performed per node
  if(getenv("IC_PER_NODE")) {
    ic_per_node = true;
  }

  // All callbacks must be loaded and visible to the process
  dl_handle = dlopen(0,RTLD_NOW|RTLD_GLOBAL);
  if(!dl_handle) {
    EXIT_PRINT("Error: %s\n", dlerror());
  }

  callback_count = 0;
  char *callback_name;
  
  char *names_env;
  if(getenv("IC_CALLBACKS")) {
    names_env = strdup(getenv("IC_CALLBACKS"));
  } else {
    EXIT_PRINT("IC_CALLBACKS not defined\n");
  }

  while ((callback_name = strsep(&names_env, ":"))) {
    // Check if we've reached the maximum number of callbacks
    if(callback_count == MAX_CALLBACKS) {
      EXIT_PRINT("Callback count exceeded: %d\n", MAX_CALLBACKS);
    }

    // Get function pointer to callback_name and append it to our list of callbacks
    callbacks[callback_count] = (ic_callback_t)dlsym(dl_handle, callback_name);

    // Check to make sure we have a valid function pointer
    if(callbacks[callback_count] != NULL) {
      callback_count++;
      DEBUG_PRINT("Added function %s\n", callback_name);
    } else {
      EXIT_PRINT("Callback Function not found: %s\n", callback_name);
    }
  }
  free(names_env);
}

void destroy_timer() {
  // Stop the timer
  struct itimerval timer;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 0;
  setitimer(ITIMER_REAL, &timer, NULL);

  // Return the alarm handler to default
  signal(SIGALRM, SIG_DFL);

  // Close and delete lock file
  close(lock_fd);
  remove(lock_file_name); 
  free(lock_file_name);

  // dlclose dl_handle
  dlclose(dl_handle);
}

// Entry point into the application
// This will be run as soon as the library is loaded
__attribute__ ((__constructor__))
void IC_init() {
  process_environment_variables();
  setup_timer();
}

// Exit point
// Called when the library is unloaded
__attribute__((destructor))
void IC_finalize() {
  destroy_timer();
}
