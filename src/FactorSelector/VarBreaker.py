#! /usr/bin/env python
# coding=utf-8

import argparse
import os
import sys
import numpy as np

import VarTree
from LatencyAggregator import LatencyAggregator

# Note for TODO. Filter by semantic interval ID AFTER we get critical path.
# Thus, when checking whether a factor should be in the variance tree, first
# check if it's the correct semantic interval ID, then check whether it's part
# of the critical path.  In other words, we don't need to check whether a factor
# is part of the critical path when building the critical path, only after it is
# constructed.
from CriticalPathBuilder import CriticalPathBuilder


def cov(var1, var2):
    """ Return the covariance of two lists """
    covMatrix = np.cov(np.vstack((var1, var2)))
    return covMatrix[0, 1]


def collectExecTime(functionFile, dataDir):
    """ Read function execution time data from file """
    functions = open(functionFile, 'r')
    # Order of function names:
    # 0: parent function
    # 1 to n - 1: child functions
    funcNames = [function.strip() for function in functions]

    latencyAggregator = LatencyAggregator(dataDir)
    # Order of exec time data:
    # 0: latency
    # 1 to n - 1: child functions
    # n: parent function
    funcExecTime = latencyAggregator.GetLatencies(dataDir, len(funcNames) + 2)

    # Reorder function names
    funcNames.append('SyncWaitTime')
    funcNames.append(funcNames[0])
    funcNames[0] = 'latency'
    return funcNames, funcExecTime

def collectExecTimeNontarget(functionFile, dataDir):
    latencyAggregator = LatencyAggregator(dataDir)
    funcExecTime, funcNames = latencyAggregator.GetLatenciesNonTarget(dataDir, functionFile)
    return funcNames, funcExecTime


def breakDown(functionFile, dataDir, nodeToBreak):
    """ Break down variance into variances and covariances """
    if nodeToBreak.func is None:
        funcNames, funcExecTime = collectExecTimeNontarget(functionFile, dataDir)
        names = funcNames[-1].split('_')
        nodeToBreak.func = names[1]
        nodeToBreak.parent = VarTree.VarNode(names[0], None, 0, 100)
    else:
        funcNames, funcExecTime = collectExecTime(functionFile, dataDir)

    # print len(funcNames)
    # print len(funcExecTime)
    # index = 0
    # for execTime in funcExecTime:
    #     print funcNames[index] + ',' + str(len(execTime)) + ':' + str(execTime[:10])
    #     index += 1
    # print ''
    caller = funcNames[-1]
    funcNames[-1] = 'img_' + funcNames[-1]

    latencyData = funcExecTime[0]
    imaginaryRecords = funcExecTime[-1]
    varLatency = np.var(latencyData)
    size = len(imaginaryRecords)
    for index in range(size):
        imaginary = imaginaryRecords[index]
        for i in range(1, len(funcNames) - 1):
            records = funcExecTime[i]
            imaginary -= records[index]
            if imaginary < 0:
                for execTime in funcExecTime:
                    print execTime[index]
                print imaginary
            assert imaginary >= 0
        imaginaryRecords[index] = imaginary

    if nodeToBreak.func == '':
        nodeToBreak.func = caller
        nodeToBreak.contribution = varLatency
        nodeToBreak.perct = 100

    length = len(funcNames)
    for index1 in range(1, length):
        for index2 in range(1, index1 + 1):
            funcName1 = funcNames[index1]
            funcName2 = funcNames[index2]
            if index1 == index2:
                variance = np.var(funcExecTime[index1])
                if variance / varLatency > 2e-3:
                    perct = 100 * variance / varLatency
                    varNode = VarTree.VarNode(funcName1, nodeToBreak, variance, perct)
                    nodeToBreak.addChild(varNode)
            else:
                covariance = cov(funcExecTime[index1],
                                 funcExecTime[index2])
                if 2 * covariance / varLatency > 1e-3:
                    perct = 200 * covariance / varLatency
                    covNode = VarTree.CovNode(funcName1, funcName2,
                                              nodeToBreak, variance, perct)
                    nodeToBreak.addChild(covNode)
