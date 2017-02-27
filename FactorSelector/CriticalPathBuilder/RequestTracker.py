from datetime import datetime
from OperationEnum import Operation

class RequestTracker:
    def __init__(self):
        # This dictionary maps a threadID to a list of tuples
        # (RequestType, RequestTimeEnd, objID). The tuples are ordered by time.
        self.threadRequests = {}

        self.allowedRequests = [Operation.MUTEX_LOCK, Operation.CV_WAIT,
                                Operation.QUEUE_DEQUEUE, Operation.MESSAGE_RECEIVE]

    # Don't do anything if this is not a request for some type of object
    def AddLogRow(self, ):
        opID = int(row[0])
        if opID in self.allowedRequests:
            requestTimeEnd = datetime.fromtimestamp(int(row[4]) / 1e9)
            objID = row[1]
            toInsert = (opID, requestTimeEnd, objID)

            threadID = row[2]
            if threadID not in self.threadRequests:
                self.threadRequests[threadID] = [toInsert]
            else:
                self.threadRequests[threadID].append(toInsert)
