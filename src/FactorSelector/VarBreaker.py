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
    funcNames = [function.strip() for function in functions]

    latencyAggregator = LatencyAggregator(dataDir)
    funcExecTime = latencyAggregator.GetLatencies(dataDir, len(funcNames) + 2)

    funcNames.append('synchronization wait time')
    funcNames.append('latency')
    return funcNames, funcExecTime

def breakDown(functionFile, dataDir, nodeToBreak):
    """ Break down variance into variances and covariances """
    funcNames, funcExecTime = collectExecTime(functionFile, dataDir)
    # print len(funcNames)
    # print len(funcExecTime)
    # index = 0
    # for execTime in funcExecTime:
    #     print str(index) + ',' + str(len(execTime)) + ':' + str(execTime[:10])
    #     index += 1
    # print ''
    caller = funcNames[0]
    funcNames[0] = 'img_' + funcNames[0]

    latencyData = funcExecTime[-1]
    imaginaryRecords = funcExecTime[0]
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
    for index1 in range(length - 1):
        for index2 in range(index1 + 1):
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
