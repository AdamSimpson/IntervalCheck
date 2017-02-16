# IntervalCheck
`IntervalCheck` is a utility that calls runtime specified user created functions at a specified interval

## Install
`IntervalCheck` includes a Smithy formula to automate deployment, for centers not
using Smithy the build looks as follow:

```
$ mkdir build
$ cmake -DCMAKE_INSTALL_PREFIX=/path/to/install ..
$ make
$ make install
```

The artificact `libIntervalCheck.so` will be created under `{PREFIX}/lib`

## Run
`IntervalCheck` works best with dynamically linked applications, in this case all that is needed to enable the timer is to set `LD_PRELOAD` to the path of `libIntervalCheck.so`.

To specify a function for `IntervalCheck` to call a dynamic library containing the function, with signature `void (void)`, must be created. `LD_PRELOAD` must preload this library, and the function name must be provided to `IntervalCheck`. An example of how to achieve this is given below:


**Step 1:** Create a function

```
$ cat code.c

#include "stdio.h"

void foo() {
  printf("Hi from foo()\n");
}

void bar() {
  printf("Hi from bar()\n");
}
```

**Step 2:** Compile the function into a dynamic library, `libcode.so`

```
$ gcc -shared -fpic code.c -o libcode.so
```

**Step 3:** `LD_PRELOAD` the interval check library as well as our custom library, provide the path as needed

```
$ export LD_PRELOAD=./libIntervalCheck.so:./libcode.so
```

**Step 4:** Tell `IntervalCheck` to call `foo()` and `bar()`, which is contained in the `LD_PRELOAD` library `libcode.so`
```
$ export IC_FUNCTIONS="foo:bar"
```

**Step 4:** Run your dynamically linked application as normal!

```
$ cat a.c
int main() {
  do{} while(1);
  return 0;
}
```

```
$ gcc a.c
```

```
$ ./a.out
Hi from foo()
Hi from bar()
Hi from foo()
Hi from bar()
...

```

## Tuning
The following environment variables can be used to fine time `IntervalTimer` 

`TMPDIR`           : Set the path to create temporary lock files in(default /tmp)

`IC_INTERVAL`      : The interval in seconds between running the specified functions(default 600)

`IC_UNSET_PRELOAD` : Unset the `LD_PRELOAD` variable on `IntervalCheck` initialization if set(default unset)

`IC_DEBUG`         : Enable debug information if set(default unset)

`IC_PER_NODE`      : Only run one instance of `IntervalCheck` per node if set(default set)

`IC_CALLBACKS`     : Colon seperated list of function names to be called by `IntervalCheck`
