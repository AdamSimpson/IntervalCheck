#include <sys/time.h>
#include <sys/resource.h>
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
if(cg_debug == 1) {                                                                 \
  printf("CG DEBUG: %s: %d: %s\n" str, __FILENAME__, __LINE__, __func__, ##args); } \
} while(0)

static bool ic_debug = false;
static bool ic_per_node = false;

static int lock_fd = -1;

const static int MAX_CALLBACKS = 1024;
typedef void (*ic_callback_t)(void);
ic_callback_t callbacks[MAX_CALLBACKS];
static int callback_count = 0;

// Handler called by alarm at specified interval
void alarm_handler(int sig) {
  // Call all requested callbacks
  for(int i=0; i<callback_count; i++) {
    (*callbacks[i])();
  }
}

void setup_timer() {
  bool create_timer = true;

  // Create only one timer per node if specified
  if(ic_per_node) {
    /* TODO: Don't hardcode /tmp */

    // Create lock file if one doesn't exist
    lock_fd = open("/tmp/interval_check.lock", O_CREAT); 
    if(lock_fd == -1) {
      EXIT_PRINT("Failed to create lock file: %s\n", strerror(errno));
    }    
    
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
    if( getitimer(ITIMER_REAL, &timer) ) {
    }

    // Set the timer interval
    if(getenv("CG_INTERVAL")) {
      timer.it_interval.tv_sec = atoi(getenv("CG_INTERVAL"));
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
  // Unset LD_PRELOAD as it can upset Cray's
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
  void* dl_handle = dlopen(0,RTLD_NOW|RTLD_GLOBAL);
  if(!dl_handle) {
    EXIT_PRINT("Error: %s\n", dlerror());
  }

  callback_count = 0;
  char *callback_name;
  char *names_env = strdup(getenv("IC_CALLBACKS"));

  while ((callback_name = strsep(&names_env, ":"))) {
    // Get function pointer to callback_name and append it to our list of callbacks
    callbacks[callback_count] = (ic_callback_t)dlsym(dl_handle, callback_name);

    if(callbacks[callback_count]) {
      callback_count++;
      if(callback_count == MAX_CALLBACKS) {
        EXIT_PRINT("Callback count exceeded: %d\n", MAX_CALLBACKS);
      }
    } else {
      EXIT_PRINT("Callback Function not found: %s\n", callback_name);
    }
  }
}

//void destroy_timer() {
  // Free timer
  // Free callback strings
  // Delete lockfile
  // dlclose dl_handle
//}

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
//  destroy_timer();
}
