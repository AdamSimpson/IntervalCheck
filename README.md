# IntervalCheck
`IntervalCheck` is a utility that calls arbitrary functions at a specified interval

## Install:
`IntervalCheck` includes a Smithy formula to automate deployment, for centers not
using Smithy the build looks as follow:

```
$ mkdir build
$ cmake -DCMAKE_INSTALL_PREFIX=/path/to/install ..
$ make
$ make install
```

The artificact `libIntervalCheck.so` will be created under `{PREFIX}/lib`

## Run:
`IntervalCheck` works best with dynamically linked MPI applications, in this case all  that is needed is to set `LD_PRELOAD` to the full path of `libCheckGPU.so`. If a GPU error is reported a message will be printed to `stderr` and the application will terminate. The following environment variables can be used to tune CheckGPU:

`CG_INTERVAL` : Set the number of seconds between GPU checks, defaults to 600(5 minutes)

`CG_DEBUG`    : Enable debug information printed to `stderr`

`CG_NO_MPI`   : Enable GPU check on non MPI applications

`CG_PPN`      : Override the automatically detected number of MPI processes launched per node

`CG_CPU_MEM`  : Print CPU memory usage information if `CG_DEBUG` enabled

`CG_GPU_MEM`  : Print GPU memory usage information if `CG_DEBUG` enabled
