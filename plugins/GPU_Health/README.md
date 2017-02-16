### GPU Health
The `GPU Health` plugin for `IntervalCheck` queries the GPU health and if neccessary kills the process/job. This can be used on most systems that allow job control from the compute nodes

#### Tuning
`GH_BATCH_KILL`    : The batch kill command line application to invoke on failure(default unset)

`GH_BATCH_ID_VAR`  : The environment variable to query for arguments to `GH_BATHC_KILL`(e.g. the jobID)(default unset)

`GH_GPU_COUNT`     : Bypass gpu count detection by specifying the number of GPU's to query per node if set(default unset)

`GH_DEBUG`         : Enable debug information if set(default unset)

`GH_TIMEOUT`       : The maximum time in seconds to wait for the health check to finish(default 30)
