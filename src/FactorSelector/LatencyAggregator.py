import csv
import sys
from nanotime import nanotime
from intervaltree import IntervalTree
from os import listdir
from NonTargetCriticalPathBreaker import NonTargetCriticalPathBreak

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
        self.criticalPathBuilder = CriticalPathBuilder.CriticalPathBuilder(pathPrefix, "SynchronizationLog_")

    def __Parse(self, pathPrefix, numFunctions):
        pathPrefix += '/' if pathPrefix[-1] != '/' else ''
        functionLogFiles = [pathPrefix + f for f in listdir(pathPrefix) if 'FunctionLog' in f]

        for filename in functionLogFiles:
            with open(filename, 'rb') as latencyLogFile:
                latencyReader = csv.reader(latencyLogFile)

                for row in latencyReader:
                    if len(row) == 5:
                        functionIndex = int(row[0])
                        threadID = row[1]
                        semIntervalID = row[2]
                        startTime = nanotime(int(row[3]))
                        endTime = nanotime(int(row[4]))

                        if semIntervalID not in self.semanticIntervals:
                            self.semanticIntervals[semIntervalID] = [[] for x in range(numFunctions)]

                        self.semanticIntervals[semIntervalID][functionIndex].append(FunctionRecord(startTime, endTime, threadID))

    def __GetCriticalPaths(self, pathPrefix):
        criticalPaths = {}
        pathPrefix += '/' if pathPrefix[-1] != '/' else ''
        functionLogFiles = [pathPrefix + f for f in listdir(pathPrefix) if 'FunctionLog' in f]

        for filename in functionLogFiles:
            with open(filename, 'rb') as latencyLogFile:
                latencyReader = csv.reader(latencyLogFile)

                for row in latencyReader:
                    if len(row) == 5:
                        functionIndex = int(row[0])
                        # Done with all semantic interval latencies
                        if functionIndex > 0:
                            break

                        threadID = row[1]
                        semIntervalID = row[2]
                        startTime = nanotime(int(row[3]))
                        endTime = nanotime(int(row[4]))
                        criticalPath = self.criticalPathBuilder.Build(startTime, endTime, threadID)
                        criticalPaths[semIntervalID] = criticalPath

    def __AggregateForSemanticInterval(self, semanticIntervalID, functionInstances):
        if len(functionInstances[0]) == 0:
            functionInstances[0] = list(functionInstances[-1])
        semIntervalInfo = functionInstances[0][0]
        criticalPath = self.criticalPathBuilder.Build(semIntervalInfo.startTime, \
                                                      semIntervalInfo.endTime,   \
                                                      semIntervalInfo.threadID)

        timeSeriesTuples = []

        # I think in this for loop we should make a time series for the semantic interval.  Then
        # after this loop is done w/ execution, go through each interval and add the value of
        # postInteval.startTime - preInterval.endTime to the wait time record.

        # Holy nesting, batman.  Don't see another way to do this though.
        for functionID in range(len(functionInstances)):
            haveAppended = False
            for functionInstance in functionInstances[functionID]:
                for interval in criticalPath.search(functionInstance.startTime, functionInstance.endTime):
                    if interval.data != functionInstance.threadID:
                        continue
                    execIntervalStartTime = max(interval.begin, functionInstance.startTime)
                    execIntervalEndTime = min(interval.end, functionInstance.endTime)

                    timeSeriesTuples.append((execIntervalStartTime, execIntervalEndTime))

                    latency = execIntervalEndTime - execIntervalStartTime

                    if not haveAppended:
                        self.functionLatencies[functionID].append(latency)
                        haveAppended = True
                    # What was this supposed to do?
                    else:
                        # I think this is wrong! Shouldn't it be:
                        # self.functionLatencies[len(self.functionLatencies) - 1][-1] += latency
                        self.functionLatencies[functionID][-1] += latency
            if len(self.functionLatencies[functionID]) < len(self.functionLatencies[0]):
                self.functionLatencies[functionID].append(0)

        # semIntTimeSeries = IntervalTree.from_tuples(timeSeriesTuples)

    def GetLatencies(self, pathPrefix, numFunctions):
        self.__Parse(pathPrefix, numFunctions)
        self.functionLatencies = [[] for _ in range(numFunctions)]

        for semanticIntervalID, functionInstances in self.semanticIntervals.items():
            self.__AggregateForSemanticInterval(semanticIntervalID, functionInstances)

        return [[int(latency) for latency in latencies] for latencies in self.functionLatencies]

    def GetLatenciesNonTarget(self, pathPrefix, funcNamesFile):
        criticalPaths = self.__GetCriticalPaths(pathPrefix)
        return NonTargetCriticalPathBreak(criticalPaths, pathPrefix, funcNamesFile)
