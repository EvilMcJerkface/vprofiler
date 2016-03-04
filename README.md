# VProfiler

VProfiler is a profiling tool that can efficiently and rigorously decompose the variance of the overall execution time of an application by instrumenting its source code, and identifying the major functions that contribute the most to the overall variance.


## Installation Guide
- - -

### 0. Package Overview

VProfiler consists of two parts: 1) Execution time tracer 2) Factor selector

The execution time tracer needs to be integrated into and compiled as part of the target system. It's located in folder 'ExecutionTimeTracer'.

The factor selector can be run locally. It's located in folder 'FactorSelector'.

### 1. Dependencies

The execution time tracer is mainly written in C++, but it has both a C++ interface and a C interface. Therefore, to use it, you need:

* C++ compiler
* Linux


The factor selector is written in Java and Python. To run it, you need the following:

* Java 1.7+
* Python 2.7+

### 2. Integration of Execution Time Tracer

The execution time tracer needs to be integrated into the target software system. It's implemented in C++, but has both a C and C++ interface. To integrate it into the target system, you need to do the following steps:

1. Put the header file in your include path so that `trace_tool.h` can be found and the functions defined in it can be used.

2. Add appropriate targets in your Makefile to compile `trace_tool.cc` separately into a `.o` file, and link it into the final binary file.

3. In the target system, add function call `SESSION_START()` to mark the start of a session, and function call `SESSION_END(successful)` to mark the end of it. If `false` is passed into `SESSION_END`, the session will be ignored.

   A *session* is one unit of work. Depending on the target system, the definition of a *session* will be different. For example, in a DBMS, a *session* will be a transaction.

### 3. Running Factor Selector

To building factor selector, simply do a `make` at the top level folder.

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
