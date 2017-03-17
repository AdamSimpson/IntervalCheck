### File Progress
The `File Progress` plugin for `IntervalCheck` queries a specified set of files for progress and if neccesary kills the process/job. When a hang is detected the process will use ALPS low level interface to send `SIGKILL` to all the processes in the job.

#### Tuning
`FP_DEBUG`              : Enable debug information if set (default unset)

`FP_INITIAL_SKIPS`      : Set the integer number of `IC_INTERVAL` length intervals to skip (default: 1)

`FP_INTERVAL_STRIDE`    : Set the number of `IC_INTERVAL` length intervals to wait between checking the file (default: 1)

`FP_FILE`               : File name to check progress of (default ./$PBS_JOBID.OU)

`FP_MIN_BYTES`          : Minimum allowable filesizes in bytes (default: 0)

`FP_MIN_LINES`          : Minimum allowable line count (default: 0)

`FP_MIN_LINES_PROGRESS` : Minimum lines added to files since last check (default: 0)

`FP_MIN_BYTES_PROGRESS` : Minimum bytes added to files since last check (default: 0)

`FP_ONE_SHOT`           : Perform only a single check after `FP_INITIAL_SKIPS` (default unset)

`FP_SINGLE_PROCESS`     : Only check on the file on a single node if set (default unset)
