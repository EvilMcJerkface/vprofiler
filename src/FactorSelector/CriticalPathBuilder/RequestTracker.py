from nanotime import nanotime
from bisect import bisect_left
from OperationEnum import Operation

class Request:
    def __init__(self, objID, opID, timeStart = None, timeEnd = None):
        self.objID = objID
        self.opID = opID

        self.timeStart = timeStart
        self.timeEnd = timeEnd

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
        if self.timeTrackIdx < len(self.requests): # and \
            self.requests[self.timeTrackIdx].AddFunctionTimes(funcStart, funcEnd)

            self.timeTrackIdx += 1

class RequestTracker:
    def __init__(self):
        # This dictionary maps a threadID to a list of tuples
        # (RequestType, RequestTimeEnd, objID). The tuples are ordered by time.
        self.threadRequests = {}

        self.allowedRequests = [Operation.MUTEX_LOCK, Operation.CV_WAIT,
                                Operation.QUEUE_DEQUEUE, Operation.MESSAGE_RECEIVE]

    def AddOperation(self, operation):
        opID = Operation(int(operation[3]))
        objID = operation[2]
        toInsert = Request(objID, opID)

        threadID = operation[0]
        if threadID not in self.threadRequests:
            self.threadRequests[threadID] = ThreadRequests(toInsert)
        else:
            self.threadRequests[threadID].requests.append(toInsert)

    def AddFunctionTime(self, funcTime):
        threadID = funcTime[0]

        # Index 1 is the semantic interval ID.  Don't actually think this is
        # necessary for RequestTracker.

        startTime = nanotime(int(funcTime[2]))
        endTime = nanotime(int(funcTime[3]))

        self.threadRequests[threadID].AddFunctionTime(startTime, endTime)

    def InitSynchObjAgg(self, objAggregator):
        for threadID, threadRequests in self.threadRequests.items():
            for request in threadRequests.requests:
                objAggregator.AddOwnership(threadID, request.opID, request.objID, \
                                           request.timeStart, request.timeEnd)

    def GetLastAddedOperationForTID(self, threadID):
        idx = self.threadRequests[threadID].timeTrackIdx - 1
        return self.threadRequests[threadID].requests[idx]

    def FindPrecedingRequest(self, threadID, timestamp):
        request = None

        # Should this be:
        # idx = bisect_left(self.threadRequests[threadID].GetKeys(), timestamp) - 1
        if not threadID in self.threadRequests:
            return request
        idx = bisect_left(self.threadRequests[threadID].GetKeys(), timestamp) - 1

        if idx != -1:
            while idx > -1:
                if self.threadRequests[threadID].requests[idx].opID in self.allowedRequests:
                    request = self.threadRequests[threadID].requests[idx]
                    break

                idx -= 1
            #opType = self.threadRequests[threadID].requests[idx].opID
            #requestTimeEnd = self.threadRequests[threadID].requests[idx].timeEnd
            #objID = self.threadRequests[threadID].requests[idx].objID

        #return opType, requestTimeEnd, objID
        return request
