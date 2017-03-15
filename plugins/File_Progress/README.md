### File Progress
The `File Progress` plugin for `IntervalCheck` queries a specified set of files for progress and if neccesary kills the process/job. When a hang is detected the process will use ALPS low level interface to send `SIGKILL` to all the processes in the job.

#### Tuning
`FP_DEBUG`              : Enable debug information if set(default unset)

`FP_INITIAL_SKIPS`      : Set the initial number of intervals to skip(default: 1)

`FP_INTERVAL_STRIDE`    : Set the number of `IC_INTERVAL` length intervals to wait between checking the file

`FP_FILES`              : Comma seperated list of files to check

`FP_MIN_BYTES`          : Comma seperated list of Minimum filesizes in bytes(default: 0)

`FP_MIN_LINES`          : Comma seperated list of minimum line count(default: 0)

`FP_MIN_LINES_PROGRESS` : Comma seperated list of minimum lines added to files since last check(default: 0)

`FP_MIN_BYTES_PROGRESS` : Comma seperated list of minimum bytes added to files since last check(default: 0)

`FP_ONE_SHOT`           : Enable a single check after the intial wait time(default unset)
