# coding=utf-8

import os
import sys
import numpy as np
import cmath


def cov(variable1, variable2):
    cov_matrix = np.cov(np.vstack((variable1, variable2)))
    return cov_matrix[0, 1]


def extract_function(function_name, function_names, function_call_time):
    index = function_names.index(function_name)
    function = function_call_time[index]
    output_file = open(function_name, 'w')
    for index in range(len(function)):
        output_file.write(str(function[index]) + '\n')


def l2norm(nums):
    square_sum = 0
    for num in nums:
        square_sum += num * num
    return cmath.sqrt(square_sum)


for caller in ['block_4k_log', 'block_8k_log', 'block_16k_log', 'block_32k_log', 'block_64k_log']:
    path = '/Users/sens/Research/predictability/predictability/post/' + \
        caller + '/latency1'
    os.chdir(path)
    function_names = []
    function_names.insert(0, 'imaginary_function')
    function_names.append('latency')
    function_call_time = []
    function_call_time_file = open('tpcc', 'r')
    statistics = open('statistics', 'w')
    current_function_index = -1
    for line in function_call_time_file:
        if ',' not in line:
            continue

        if len(line) > 1:
            function_index, time = line.split(',')
            function_index = int(function_index)
            time = int(time)
            if function_index != current_function_index:
                # new function, ignore the first transaction
                function_call_time.append([])
            else:
                function_call_time[function_index].append(time)
            current_function_index = function_index

    # remove the last two elements
    for index in range(len(function_names)):
        function_call_time[index] = function_call_time[index][10:-10]
        statistics.write(function_names[index] + ', ' + str(np.mean(function_call_time[
                         index])) + ', ' + str(np.var(function_call_time[index])) + '\n')
    statistics.write('99th percentile, ' +
                     str(np.percentile(function_call_time[-1], 99)) + '\n')
    statistics.write('Throughput, ' +
                     str(len(function_call_time[-1]) / 600) + '\n')
    latencies = function_call_time[-1]
    print '"' + caller + ',' + str(np.mean(latencies)) + ',' + str(np.var(latencies)) + ',' + \
        str(np.percentile(latencies, 99)) + \
        ',' + str(l2norm(latencies)) + '",'

    variance_tree = '/Users/sens/Developer/Java/VProfiler/SourceOfVariance/variance_tree_post'
    function_combination = open('function_combination', 'w')
    break_down_file = open('break_down', 'w')
    break_down_absolute = open('break_down_absolute', 'w')
    break_down_total = open('break_down_total', 'w')
    function_names_file = open('function_names', 'w')
    function_names_file.write(str(function_names[1:-1]))
    imaginary_records = function_call_time[0]
    size = len(imaginary_records)
    total_variance = np.var(imaginary_records)
    variance_of_latency = np.var(function_call_time[-1])
    function_names_file.write('\nTotal: ' + str(total_variance) + '\n')
    function_names_file.write('Variance of Latency: ' +
                              str(variance_of_latency) + '\n')
    function_names_file.close()

    for index in range(1, len(function_names) - 1):
        records = function_call_time[index]
        for index in range(size):
            imaginary_records[index] -= records[index]

    tree_file = open(variance_tree, 'a')
    tree_file = sys.stdout
    tree_file.write('\n\n')
    count = 0
    variance_break_down = []
    length = len(function_names)
    sum_of_variance = 0
    for index1 in range(length - 1):
        for index2 in range(index1 + 1):
            function_name1 = function_names[index1]
            function_name2 = function_names[index2]
            if index1 == index2:
                variance = np.var(function_call_time[index1])
                sum_of_variance += variance
                variance_break_down.append((function_name1, variance))
                if variance / variance_of_latency > 0:
                    tree_file.write(caller + ' -> ' + function_name1 +
                                    ' -> ' + str(variance / variance_of_latency) + '\n')
            else:
                covariance = cov(function_call_time[
                                 index1], function_call_time[index2])
                sum_of_variance += covariance
                variance_break_down.append(
                    (function_name1 + ',' + function_name2, 2 * covariance))
                if 2 * covariance / variance_of_latency > 0:
                    tree_file.write(caller + ' -> ' + function_name1 + ',' +
                                    function_name2 + ' -> ' + str(2 * covariance / variance_of_latency) + '\n')
    break_down_list = sorted(
        variance_break_down, key=lambda variance: variance[1])
    break_down_list.reverse()
    for function_variance in break_down_list:
        function_combination.write(function_variance[0] + '\n')
        break_down_absolute.write(str(function_variance[1]) + '\n')
        break_down_file.write(
            str(function_variance[1] / sum_of_variance) + '\n')
        break_down_total.write(
            str(function_variance[1] / variance_of_latency) + '\n')
    function_combination.close()
    break_down_absolute.close()
    break_down_file.close()
    break_down_total.close()

# extract_function('LWLockAcquireOrWait', function_names, function_call_time)
# latency = open('latency', 'w');
# latencies = function_call_time[-1]
# for index in range(len(latencies)):
#   latency.write(str(latencies[index]) + '\n')
