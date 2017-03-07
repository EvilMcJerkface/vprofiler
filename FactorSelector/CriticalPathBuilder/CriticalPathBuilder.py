from csv import reader
from collections import deque
from intervaltree import IntervalTree
from RequestTracker import RequestTracker
from SynchronizationObjectAggregator import SynchronizationObjectAggregator
from pdb import set_trace

# TODO need to add support for try_locks
# TODO need to add support for state transitions like the following for thread1
# executing semanticID1 -> executing semanticID2 -> executing semanticID1
class CriticalPathBuilder:
    def __init__(self, synchroOpLogName, synchroFxnTimeLogName):
        self.synchObjAgg = SynchronizationObjectAggregator()
        self.requestTracker = RequestTracker()
        self.blockedEdgeStack = deque()

        # Error if we can't open this or the next file.
        with open(synchroOpLogName, 'rb') as synchroOpLogFile:
            synchroOpLogReader = reader(synchroOpLogFile)

            for operation in synchroOpLogReader:
                if len(operation) == 4 and '' not in operation:
                    self.requestTracker.AddOperation(operation)

        with open(synchroFxnTimeLogName, 'rb') as synchroFxnTimeFile:
            synchroFxnTimeReader = reader(synchroFxnTimeFile)

            for functionTime in synchroFxnTimeReader:
                if len(functionTime) == 4 and '' not in operation:
                    print(functionTime)
                    self.requestTracker.AddFunctionTime(functionTime)

                    threadID = functionTime[0]
                    self.synchObjAgg.AddOperation(threadID, \
                    self.requestTracker.GetLastAddedOperationForTID(threadID))

        set_trace()


    def __BuildHelper(self, segmentEndTime, currThreadID, timeSeries):
        opType, requestTime, objID = requestTracker.FindPrecedingRequest(currThreadID, segmentEndTime)

        unblockedSegment = -1
        i = 0
        for potentialBlockedEdge in self.blockedEdgeStack:
            if requestTime.endTime <= potentialBlockedEdge[0]:
                unblockedSegment = i

            i += 1

        leftTimeBound = None
        nextThreadID = None
        # This thread was blocking a thread earlier in the critical path.
        if unblockedSegment != -1:
            leftTimeBound = self.blockedEdgeStack[unblockedSegment][0]
            timeSeries.appendleft((currThreadID, leftTimeBound, segmentEndTime))

            nextThreadID = self.blockedEdgeStack[unblockedSegment][1]

            # We've finished the critical path for the semantic interval
            if nextThreadID == -1:
                return timeSeries

            for j in range(unblockedSegment + 1):
                self.blockedEdgeStack.pop_front()

        # We're blocked by some other thread
        else:
            self.blockedEdgeStack.appendleft(requestTime.startTime, currThreadID)
            timeSeries.appendleft((currThreadID, requestTime.endTime, segmentEndTime))

            leftTimeBound, nextThreadID = self.synchObjAgg.GetDependenceEdge(requestTime, objID, opType)

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

        timeSeries = self.__BuildHelper(semIntStartTime, semIntEndTime, endingThreadID, deque())

        self.blockedEdgeStack.clear()
        return IntervalTree.from_tuples(timeSeries)

