import csv
import sys
from nanotime import nanotime
from os import listdir

sys.path.append('CriticalPathBuilder/')
from CriticalPathBuilder import CriticalPathBuilder

class FunctionRecord:
    def __init__(self, startTime, endTime, threadID):
        self.startTime = startTime
        self.endTime = endTime
        self.threadID = threadID

class LatencyAggregator:
    def __init__(self, pathPrefix):
        # This will, at the end of execution, be a 2D with the first dimension
        # mapping to a function, and the second being a list of latencies.
        # Then, effectively, the list is a map of
        # FunctionID -> list of (SemanticIntervalID -> FunctionLatency)
        self.functionLatencies = []

        # Map from semantic interval ID to SemanticInterval object
        self.semanticIntervals = {}

        # Maps a semantic interval ID to a list of function ids, which
        # then map to the instances of that function for the given
        # semantic interval.
        self.semanticIntervalFunctionInstances = {}

        # Driver for building critical paths
        self.criticalPathBuilder = CriticalPathBuilder(pathPrefix, "SynchronizationLog_")

    def __Parse(self, pathPrefix, numFunctions):
        pathPrefix += '/' if pathPrefix[-1] != '/' else ''
        tpccFiles = [pathPrefix + f for f in listdir(pathPrefix) if 'tpcc' in f]

        for filename in tpccFiles:
            with open(filename, 'rb') as latencyLogFile:
                latencyReader = csv.reader(latencyLogFile)

                for row in latencyReader:
                    if len(row) == 5:
                        functionIndex = int(row[0])
                        threadID = row[1]
                        semIntervalID = int(row[2])
                        startTime = nanotime(int(row[3]))
                        endTime = nanotime(int(row[4]))

                        if semIntervalID not in self.semanticIntervals:
                            self.semanticIntervals[semIntervalID] = [[] for x in range(numFunctions)]

                        self.semanticIntervals[semIntervalID][functionIndex].append(FunctionRecord(startTime, endTime, threadID))

    def __AggregateForSemanticInterval(self, semanticIntervalID, functionInstances):
        #semIntervalInfo = functionInstances[len(functionInstances) - 1]
        semIntervalInfo = functionInstances[0][0]
        criticalPath = self.criticalPathBuilder.Build(semIntervalInfo.startTime, \
                                                      semIntervalInfo.endTime,   \
                                                      semIntervalInfo.threadID)

        # Holy nesting, batman.  Don't see another way to do this though.
        for functionID in range(len(functionInstances)):
            for functionInstance in functionInstances[functionID]:
                haveAppended = False
                for interval in criticalPath.search(functionInstance.startTime, functionInstance.endTime):
                    if interval.data == functionInstance.threadID:
                        latency = min(interval.end, functionInstance.endTime) - max(interval.begin, functionInstance.startTime)
                        if not haveAppended:
                            self.functionLatencies[functionID].append(latency)
                        # What was this supposed to do?
                        else:
                            self.functionLatencies[len(self.functionLatencies) - 1] += latency


    def GetLatencies(self, pathPrefix, numFunctions):
        self.__Parse(pathPrefix, numFunctions)
        self.functionLatencies = [[] for x in range(numFunctions)]

        for semanticIntervalID, functionInstances in self.semanticIntervals.items():
            self.__AggregateForSemanticInterval(semanticIntervalID, functionInstances)

        return [[int(latency) for latency in latencies] for latencies in self.functionLatencies]

