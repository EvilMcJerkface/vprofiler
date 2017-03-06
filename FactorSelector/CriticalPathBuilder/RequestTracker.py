import numpy as np

from bisect import bisect_left
from OperationEnum import Operation
from pdb import set_trace

class Request:
    def __init__(self, objID, opID):
        self.objID = objID
        self.opID = opID

        self.timeStart = None
        self.timeEnd = None

    def AddFunctionTimes(self, timeStart, timeEnd):
        self.timeStart = timeStart
        self.timeEnd = timeEnd

# Building of a ThreadRequests works as follows.
# 1) Iterate through operation log.  Add each row in the OperationLog file as a
# partially complete ThreadRequest.  Each row represents a single request by a thread.
# 2) Iterate through synchronization time log.  Keep a counter where the counter
# is the operation to which the function time corresponds.  Increment the counter on
# each function time addition.
class ThreadRequests:
    def __init__(self, val):
        self.requests = [val]

        self.keys = None

        # Counter to be able to find the index at which we must insert the next
        # function time end.
        self.timeTrackIdx = 0

    def GetKeys(self):
        if self.keys == None:
            self.keys = [x.timeEnd for x in self.requests]

        return self.keys

    def AddFunctionTime(self, funcStart, funcEnd):
        self.requests[self.timeTrackIdx].AddFunctionTimes(funcStart, funcEnd)

        self.timeTrackIdx += 1

class RequestTracker:
    def __init__(self):
        # This dictionary maps a threadID to a list of tuples
        # (RequestType, RequestTimeEnd, objID). The tuples are ordered by time.
        self.threadRequests = {}

        self.allowedRequests = [Operation.MUTEX_LOCK, Operation.CV_WAIT,
                                Operation.QUEUE_DEQUEUE, Operation.MESSAGE_RECEIVE]

    # Don't do anything if this is not a request for some type of object
    def AddOperation(self, operation):
        opID = int(operation[3])
        if opID in self.allowedRequests:
            objID = operation[2]
            toInsert = Request(objID, opID)


            threadID = operation[0]
            if threadID not in self.threadRequests:
                self.threadRequests[threadID] = ThreadRequests(toInsert)
            else:
                self.threadRequests[threadID].requests.append(toInsert)

    def AddFuntionTime(self, funcTime):
        threadID = funcTime[0]
        startTime = np.datetime64(funcTime[1], 'ns')
        endTime = np.datetime64(funcTime[2], 'ns')

        self.threadRequests[threadID].AddFunctionTime(startTime, endTime)

    def InitSynchObjAgg(self, objAggregator):
        for threadID, threadRequests:
            for request in threadRequests.requests:
                objAggregator.AddOwnership(threadID, request.opID, request.objID \
                                           request.timeStart, request.timeEnd)

    def GetLastAddedOperationForTID(self, threadID):
        idx = self.threadRequests[threadID].timeTrackIdx
        return self.threadRequests[threadID].requests[idx]

    def FindPrecedingRequest(self, threadID, timestamp):
        opType, requestTimeEnd, objID = (None, None, None)
        # Should this be:
        # idx = bisect_left(self.threadRequests[threadID].GetKeys(), timestamp) - 1
        idx = bisect_left(self.threadRequests[threadID], timestamp) - 1

        if idx != -1:
            opType, requestTimeEnd, objID = self.threadRequests[threadID][idx]

        return opType, requestTimeEnd, objID
