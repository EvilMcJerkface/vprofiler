from os import listdir
from csv import reader
from collections import deque
from intervaltree import IntervalTree
from RequestTracker import RequestTracker
from SynchronizationObjectAggregator import SynchronizationObjectAggregator
from progressbar import ProgressBar

# TODO need to add support for try_locks
# TODO need to add support for state transitions like the following for thread1
# executing semanticID1 -> executing semanticID2 -> executing semanticID1
class CriticalPathBuilder:
    def __init__(self, pathPrefix, logName):
        self.synchObjAgg = SynchronizationObjectAggregator()
        self.requestTracker = RequestTracker()
        self.blockedEdgeStack = deque()

        pathPrefix += '/' if pathPrefix[-1] != '/' else ''

        logFiles = [pathPrefix + f for f in listdir(pathPrefix) if logName in f]
        for synchroLogName in logFiles:
            # Error if we can't open this or the next file.
            with open(synchroLogName, 'rb') as synchroLogFile:
                numLines = sum(1 for line in synchroLogFile)
                print(synchroLogName)
                pbar = ProgressBar(max_value = numLines).start()

                synchroLogFile.seek(0)
                synchroLogReader = reader(synchroLogFile)

                for i, log in enumerate(synchroLogReader):
                    if log[0] == '0':
                        self.requestTracker.AddOperation(log[1:])
                    else:
                        self.requestTracker.AddFunctionTime(log[1:])

                        threadID = log[1]
                        self.synchObjAgg.AddOperation(threadID, \
                        self.requestTracker.GetLastAddedOperationForTID(threadID))

                    pbar.update(i + 1)


    def __BuildHelper(self, segmentEndTime, currThreadID, timeSeries):
        precedingRequest = self.requestTracker.FindPrecedingRequest(currThreadID, segmentEndTime)

        unblockedSegment = -1

        if precedingRequest == None:
            unblockedSegment = len(self.blockedEdgeStack) - 1
        else:
            i = 0
            for potentialBlockedEdge in self.blockedEdgeStack:
                if precedingRequest.timeEnd <= potentialBlockedEdge[0]:
                    unblockedSegment = i

                i += 1

        leftTimeBound = None
        nextThreadID = None

        if unblockedSegment == -1:
            self.blockedEdgeStack.appendleft((precedingRequest.timeStart, currThreadID))
            leftTimeBound, nextThreadID = self.synchObjAgg.GetDependenceEdge(precedingRequest)

            if leftTimeBound is None or nextThreadID is None:
                leftTimeBound = precedingRequest.timeStart
                nextThreadID = currThreadID

            i = 0
            for potentialBlockedEdge in self.blockedEdgeStack:
                if leftTimeBound <= potentialBlockedEdge[0]:
                    unblockedSegment = i

                i += 1

        # This thread was blocking a thread earlier in the critical path.
        if unblockedSegment != -1:
            leftTimeBound = self.blockedEdgeStack[unblockedSegment][0]
            if leftTimeBound != segmentEndTime:
                timeSeries.appendleft((leftTimeBound, segmentEndTime, currThreadID))

            nextThreadID = self.blockedEdgeStack[unblockedSegment][1]

            # We've finished the critical path for the semantic interval
            if nextThreadID == -1:
                return timeSeries

            for j in range(unblockedSegment + 1):
                self.blockedEdgeStack.popleft()

        # We're blocked by some other thread
        else:
            if precedingRequest.timeEnd != segmentEndTime:
                timeSeries.appendleft((precedingRequest.timeEnd, segmentEndTime, currThreadID))

            if leftTimeBound is None or nextThreadID is None:
                leftTimeBound = requestTime
                nextThreadID = currThreadID

        return self.__BuildHelper(leftTimeBound, nextThreadID, timeSeries)


    # Returns a list of tuples, (threadID, startTime, endTime) where each tuple
    # represents a single segment of the critical path.  Note that if a tuple
    # represents an event creation wait-for relationship (the time between when
    # an event is created and picked up), its threadID will be -1.
    def Build(self, semIntStartTime, semIntEndTime, endingThreadID):
        # Take threadID of -1 to indicate that whichever thread is executing
        # at the end of the semantic interval is the final thread in the
        # critical path's time series.
        self.blockedEdgeStack.append((semIntStartTime, -1))

        timeSeries = self.__BuildHelper(semIntEndTime, endingThreadID, deque())

        self.blockedEdgeStack.clear()
        return IntervalTree.from_tuples(timeSeries)

