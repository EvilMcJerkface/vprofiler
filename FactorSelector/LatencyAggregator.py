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

    def __AggregateSemanticIntervalWaitTime(self, timeSeries):
        self.functionLatencies[-1].append(0)
        prevInterval = None

        for interval in timeSeries:
            if prevInterval != None:
                self.functionLatencies[-1][-1] = interval.begin - prevInterval.end

            prevInterval = interval

    def __AggregateForSemanticInterval(self, semanticIntervalID, functionInstances):
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
            for functionInstance in functionInstances[functionID]:
                haveAppended = False
                for interval in criticalPath.search(functionInstance.startTime, functionInstance.endTime):
                    if interval.data == functionInstance.threadID:
                        execIntervalStartTime = max(interval.begin, functionInstance.startTime)
                        execIntervalEndTime = min(interval.end, functionInstance.endTime)

                        timeSeriesTuples.append((execIntervalStartTime, execIntervalEndTime))

                        latency = execIntervalEndTime - execIntervalStartTime

                        if not haveAppended:
                            self.functionLatencies[functionID].append(latency)
                        # What was this supposed to do?
                        else:
                            # I think this is wrong! Shouldn't it be:
                            # self.functionLatencies[len(self.functionLatencies) - 1][-1] += latency
                            self.functionLatencies[functionID][-1] += latency

        semIntTimeSeries = IntervalTree.from_tuples(timeSeriesTuples)

        self.__AggregateSemanticIntervalWaitTime(timeSeriesTuples)

    def GetLatencies(self, pathPrefix, numFunctions):
        self.__Parse(pathPrefix, numFunctions)
        self.functionLatencies = [[] for x in range(numFunctions)]

        for semanticIntervalID, functionInstances in self.semanticIntervals.items():
            self.__AggregateForSemanticInterval(semanticIntervalID, functionInstances)

        return [[int(latency) for latency in latencies] for latencies in self.functionLatencies]

