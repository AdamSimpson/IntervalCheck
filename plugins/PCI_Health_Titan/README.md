### Titan PCI Health Check
The `Titan PCI Health` plugin for `IntervalCheck` queries the PCI/GPU health and if neccesary kills the process/job. When a hang is detected the process will use ALPS low level interface to send `SIGKILL` to all the processes in the job.

#### Tuning
`GH_GPU_COUNT`     : Bypass gpu count detection by specifying the number of GPU's to query per node if set(default unset)

`GH_DEBUG`         : Enable debug information if set(default unset)

`GH_TIMEOUT`       : The maximum time in seconds to wait for the health check to finish(default 30)
