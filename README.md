# VProfiler

VProfiler is a profiling tool that can efficiently and rigorously decompose the variance of the overall execution time of an application by instrumenting its source code, and identifying the major functions that contribute the most to the overall variance.


## Installation Guide

### 0. Package Overview

VProfiler consists of three parts: 1) Synchronization call annotator 2) Execution time tracer 2) Factor selector

The files overwritten by the synchronization call annotator and the generated wrapper files must be integrated into and compiled as part of the target system.  It's located in the folder 'Annotator'.

The execution time tracer needs to be integrated into and compiled as part of the target system. It's located in folder 'ExecutionTimeTracer'.

The factor selector can be run locally. It's located in folder 'FactorSelector'.

### 1. Dependencies

The execution time tracer is mainly written in C++, but it has both a C++ interface and a C interface. Therefore, to use it, you need:

* C++ compiler
* Linux

The synchronization annotator uses clang to process files and rewrite them.  Therefore, it needs:
* Clang
* llvm (Dependency of clang)
* C++ compiler supporting c++11

The factor selector is mainly written in Java and Python. To run it, you need the following:

* Java 1.7+
* Python 2.7+
* Linux (for the shell scripts)

### 2. Clang and LLVM Installation
Clang and LLVM installation can be a bit hairy.  Refer to this [Clang/LLVM installation guide](https://clang.llvm.org/get_started.html) for instruction on how to install clang/llvm.  A nice script to automatically install clang/llvm for you can be found [here](https://github.com/rsmmr/install-clang).

### 3. Integration of Execution Time Tracer and Generated Files

The execution time tracer needs to be integrated into the target software system. It's implemented in C++, but has both a C and C++ interface. To integrate it into the target system, you need to do the following steps:

1. Put the header files in your include path so that `trace_tool.h` and `VProfEventWrappers.h` can be found and the functions defined in it can be used.

2. Add appropriate targets in your Makefile to compile `trace_tool.cc` and `VProfEventWrappers.cpp` into separate `.o` files (something like `trace_tool.o` and `VProfEventWrappers`.o), and link them into the final binary file.

3. In the target system, add function call `SESSION_START()` to mark the start of a session, and function call `SESSION_END(successful)` to mark the end of it. If `false` is passed into `SESSION_END`, the session will be ignored.

   A *session* is one unit of work. Depending on the target system, the definition of a *session* will be different. For example, in a DBMS, a *session* will be a transaction, and so `SESSION_START` should mark the start of a transaction while `SESSION_END` marks the end (commit, abort or rollback) or a transaction.

### 4. Building Factor Selector

To building factor selector, simply do a `make` at the top level folder.

### 5. Usage Guide

#### Annotator Usage

VProfiler offers an abstraction to attempt to determine which particular operations influenced the predictability of the user's execution.  This abstraction uses the idea that only one thread finishes the segment of execution between `SESSION_START` and `SESSION_END`, and so VProfiler determines the particular operations which influenced the predictability by tracking calls to synchronization APIs.  VProfiler will run correctly without this step, but the results may not be as accurate.
VProfiler uses clang's libtooling to instrument code. To instrument the source code properly, VProfiler must know the compilation options used for each file in the source tree.  The mechanism it uses for this is a compilation database represented by a `compile_commands.json` file.  This file details exactly how each file is compiled in the source tree so that VProfiler can instrument exactly what the compiler sees at compile time of the user's system.  To use VProfiler, then, the user
must generate a `compile_commands.json` file for their system.  A guide on how to create `compile_commands.json` can be found later in the usage guide.

1. Create a file where each line is of the format
`<fully qualified function name> <function type>`
where fully qualified function name includes the templated type if one exists.  Function type specifies what type of synchronization function the function name is. Acceptable values are as follows:

* MUTEX\_LOCK
* MUTEX\_UNLOCK
* CV\_WAIT
* CV\_SIGNAL
* CV\_BROADCAST
* QUEUE\_ENQUEUE
* QUEUE\_DEQUEUE
* MESSAGE\_SEND
* MESSAGE\_RECEIVE

2. Run the following:
    ```
    ./EventAnnotator.sh -f <function_filename> -s <source_tree_base_dir> compile_commands.json
    ```
Here, function\_filename is the name of the file created in step 1.  source\_tree\_base\_dir is the base directory of the source tree that the user is attempting to instrument.  Note that all arguments are required and that they must follow the order given above.

##### Generating compile_commands.json
There are a number of options for generating `compile_commands.json`.  If you are using cmake, add **-DCMAKE_EXPORT_COMPILE_COMMANDS=on** to the command you usually run cmake with.  If you are using a build system besides cmake, look at [Bear](https://github.com/rizsotto/Bear), a system for generating `compile_commands.json` with various build systems.

#### Factor Selector Usage

The factor selector runs in the following loop:

1. It instruments the function whose execution time variance needs to be broken down using SourceAnnotator.

2. Using the instrumented source code, it recompiles the target software system, and runs experiments on it to collect execution time data of the function.

3. After the experiment, the execution time tracer will generate some data files, which will be copied (or downloaded) for further analysis. var_breaker breaks down the variance of this function into variances and covariances of its child functions and expand the variance tree.

4. Based on the expanded variance, the factor selector selects the top k most interesting functions and present them to the user.

Therefore, to use the factor selector, you need to do the following steps (make sure you are in directory FactorSelector):

1. Generate a static call graph of the target software system using tools like [CodeViz](http://www.csn.ul.ie/~mel/projects/codeviz/). Based on the static call graph, generate a file containing the heights of each function in the graph, each line in the form `func_name,height`.

2. Identify the source code file in which the target function is defined. The target function is the one whose execution time variance needs to be further broken down. At first the target function is the entry point function which performs the unit of work (e.g, `dispatch_command` in MySQL and `PostgresMain` in Postgres). Instrument the source code file using SourceAnnotator.

   ```
   ./annotate.sh <source code file> <target function name> <location of trace_tool.cc file in the target system>
   ```

   This will generate an instrumented source code file in directory FactorSelector.

3. Using the instrumented source code file, recompile the target software system and run an experiment on it. The execution time tracer will automatically generate execution time data at this step, which is located in directory `latency`.

4. Using the data generate in the previous step, do a factor selection to select the top k most interesting function.

   ```
   ./select_factor.sh <target function name> <dir of data files generated from ExectuionTimeTracer> <heights file> <k> <root function> [selected functions]
   ```

   Here `k` is the number of sources of variances you want to find. `root function` is the entry point function for the unit of work. `selected functions` is a list of functions separated by commas, which you've decided as sources of variance, if any.

   The factor selector will show you a list of functions which are most responsible for the variance. You need to look into these functions and decide whether or not you need to further break down their variances. If so, go back to step 2. If you've already selected any function as one source of variance, include its name in the list of `selected functions`.
