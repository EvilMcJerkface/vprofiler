#! /usr/bin/env python
# coding=utf-8

import os
import sys
import cmath
import getopt
import numpy as np
import csv

from LatencyAggregator import LatencyAggregator

# Note for TODO. Filter by semantic interval ID AFTER we get critical path.
# Thus, when checking whether a factor should be in the variance tree, first
# check if it's the correct semantic interval ID, then check whether it's part
# of the critical path.  In other words, we don't need to check whether a factor
# is part of the critical path when building the critical path, only after it is
# constructed.
from CriticalPathBuilder import CriticalPathBuilder


def cov(variable1, variable2):
    """ Return the covariance of two lists """
    cov_matrix = np.cov(np.vstack((variable1, variable2)))
    return cov_matrix[0, 1]


def l2norm(nums):
    """ Compute the L2 Norm of nums """
    square_sum = 0
    for num in nums:
        square_sum += num * num
    return cmath.sqrt(square_sum)


def collect_exec_time(function_file, path):
    """ Read function execution time data from file """
    functions = open(path + function_file, 'r')
    functions.readline()
    function_names = [function.strip() for function in functions]
    function_names.insert(0, function_file)
    function_names.append('latency')

    latencyAggregator = LatencyAggregator(path + "OperationLog.log", path + "SynchronizationTimeLog.log")
    function_exec_time = latencyAggregator.GetLatencies(path + "tpcc", len(function_names))
#    function_exec_time = []
#    function_exec_time_file = open('tpcc', 'r')
#    current_function_index = -1
#    for line in function_exec_time_file:
#        # Skip transaction starting time
#        if ',' not in line:
#            continue
#        # Skip empty lines
#        if len(line) > 1:
#            function_index, time = line.split(',')
#            function_index = int(function_index)
#            time = int(time)
#            if function_index != current_function_index:
#                # new function, ignore the first transaction
#                function_exec_time.append([])
#            else:
#                function_exec_time[function_index].append(time)
#            current_function_index = function_index
    return function_names, function_exec_time

def break_down(function_file, path, var_tree_file):
    """ Break down variance into variances and covariances """
    function_names, function_exec_time = collect_exec_time(function_file, path)
    caller = function_names[0]
    function_names[0] = 'img_' + function_names[0]

    latency_data = function_exec_time[-1]
    imaginary_records = function_exec_time[0]
    variance_of_latency = np.var(latency_data)
    std_of_latency = np.std(latency_data)
    mean_of_latency = np.mean(latency_data)
    perc_of_latency = np.percentile(latency_data, 99)
    print 'Num requests: ' + str(len(latency_data))
    print mean_of_latency, std_of_latency, variance_of_latency, perc_of_latency
    print std_of_latency / mean_of_latency, perc_of_latency / mean_of_latency
    print np.var(imaginary_records) / np.var(latency_data)
    size = len(imaginary_records)
    for index in range(size):
        imaginary = imaginary_records[index]
        for i in range(1, len(function_names) - 1):
            records = function_exec_time[i]
            imaginary -= records[index]
            if imaginary < 0:
                for exec_time in function_exec_time:
                    print exec_time[index]
                print imaginary
            assert imaginary >= 0
        imaginary_records[index] = imaginary

    if not os.path.exists(var_tree_file):
        tree_file = open(var_tree_file, 'w')
    else:
        tree_file = open(var_tree_file, 'a')
    tree_file.write('\n\n')

    variance_break_down = []
    length = len(function_names)
    sum_of_variance = 0
    for index1 in range(length - 1):
        for index2 in range(index1 + 1):
            function_name1 = function_names[index1]
            function_name2 = function_names[index2]
            if index1 == index2:
                variance = np.var(function_exec_time[index1])
                sum_of_variance += variance
                variance_break_down.append((function_name1, variance))
                if variance / variance_of_latency > 2e-3:
                    tree_file.write('"' + caller + '" -> "' + function_name1 +
                                    '" -> "' + str(variance / variance_of_latency) + '"\n')
            else:
                covariance = cov(function_exec_time[index1],
                                 function_exec_time[index2])
                sum_of_variance += covariance
                variance_break_down.append(
                    (function_name1 + ',' + function_name2, 2 * covariance))
                if 2 * covariance / variance_of_latency > 1e-3:
                    # Missing quotation mark after first ->?
                    tree_file.write('"' + caller + '" -> ' + function_name1 + '",' +
                                    function_name2 + '" -> "' +
                                    str(2 * covariance / variance_of_latency) + '"\n')


def usage():
    """ Print usage """
    print 'Usage: var_breaker.py -f <function names file generated from SourceAnnotator>\n' +\
          '                      -d <dir of data files generated from ExectuionTimeTracer>\n' +\
          '                      -v <variance tree file>\n' +\
          '                      -h print help message'


def main():
    """ Main function """
    try:
        opts, _ = getopt.getopt(sys.argv[1:], "hf:d:v:", ["help", "output="])
    except getopt.GetoptError as err:
        # print help information and exit:
        print str(err)  # will print something like "option -a not recognized"
        usage()
        sys.exit(2)
    function_file = ''
    path = ''
    var_tree_file = ''
    for opt, arg in opts:
        if opt == '-h':
            usage()
            sys.exit()
        elif opt == '-f':
            function_file = arg
        elif opt == '-d':
            path = arg
        elif opt == '-v':
            var_tree_file = arg
        else:
            print opt
            assert False, 'Invalid option'
    if function_file == '' or path == '' or var_tree_file == '':
        usage()
        sys.exit()
    break_down(function_file, path, var_tree_file)


if __name__ == '__main__':
    main()
