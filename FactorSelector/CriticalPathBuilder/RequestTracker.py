from datetime import datetime
from bisect import bisect_left
from OperationEnum import Operation
from pdb import set_trace

class ThreadRequests:
    def __init__(self, val):
        self.requests = [val]

        self.keys = None

    def GetKeys(self):
        if self.keys == None:
            self.keys = [x[1] for x in self.requests]

        return self.keys

class RequestTracker:
    def __init__(self):
        # This dictionary maps a threadID to a list of tuples
        # (RequestType, RequestTimeEnd, objID). The tuples are ordered by time.
        self.threadRequests = {}

        self.allowedRequests = [Operation.MUTEX_LOCK, Operation.CV_WAIT,
                                Operation.QUEUE_DEQUEUE, Operation.MESSAGE_RECEIVE]

    # Don't do anything if this is not a request for some type of object
    def AddLogRow(self, row):
        set_trace()
        opID = int(row[0])
        if opID in self.allowedRequests:
            requestTimeEnd = datetime.fromtimestamp(int(row[4]) / 1e9)
            objID = row[1]
            toInsert = (opID, requestTimeEnd, objID)

            threadID = row[2]
            if threadID not in self.threadRequests:
                self.threadRequests[threadID] = ThreadRequests(toInsert)
            else:
                self.threadRequests[threadID].requests.append(toInsert)


    def FindPrecedingRequest(self, threadID, timestamp):
        opType, requestTimeEnd, objID = (None, None, None)
        idx = bisect_left(self.threadRequests[threadID], timestamp) - 1

        if idx != -1:
            opType, requestTimeEnd, objID = self.threadRequests[threadID][idx]

        return opType, requestTimeEnd, objID
