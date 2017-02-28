from datetime import datetime
from OperationsEnum import Operation

class SynchronizationObjectAggregator:
    def __init__(self):
        self.objectMap = {}

        self.messages = {}

    def AddLogRow(self, row):
        opID = int(row[0])
        objID = row[1]
        threadID = row[2]
        operationTimeStart = datetime.fromtimestamp(int(row[3]) / 1e9)
        operationTimeEnd = datetime.fromtimestamp(int(row[4]) / 1e9)

        # These values represent mutexes and condition variables
        if opID <= 4:
            # If this conditional is true then we know it is a thread acquiring
            # ownership of an object and it's the first thread to own the object.
            if objID not in self.objectMap:
                self.objectMap[objID] = OwnableObject()
            elif opID == Operation.MUTEX_UNLOCK or
                 opID == Operation.CV_BROADCAST or
                 opID == Operation.CV_SIGNAL:
                self.objectMap[objID].SetLatestOwnershipEndTime(operationTimeEnd)
            else:
                self.objectMap[objID].RegisterObjectAcquisitionRequest(operationTimeStart)
                self.objectMap[objID].StartNewOwnership(threadID, operationTimeEnd)

        elif opID == Operation.MESSAGE_SEND:
            self.messages[objID] = (threadID, operationTimeEnd)

        elif opID == Operation.QUEUE_ENQUEUE:
            eventID = row[5]

            if objID not in self.objectMap:
                self.objectMap[objID] = EventCreationObject()

            self.objectMap[objID].AddEventCreationRelationship(eventID, threadID)

    # Get ending ending time of segment we have an dependence edge to, as
    # well as the thread ID of the thread adjacent to the dependence edge
    # which comes first in the time series.
    #
    # Note that to use this function, it must be called with one of the following
    # configurations for the tuple (objID, requestTime, eventID)
    # (!None, !None, None) - This is a object ownership relation
    # (!None, None, !None) - This is an event queue relation
    # (None, None, !None)  - This is a messaging relation
    def GetDependenceEdge(self, opType, objID=None, requestTime=None, eventID=None):
        if opType == Operation.MUTEX_LOCK or opType == Operation.CV_WAIT:
            return self.objectMap[objID].GetDependenceRelation(requestTime)
        elif opType == Operation.QUEUE_DEQUEUE:
            return self.objectMap[objID].GetDependenceRelation(eventID)
        else:
            return self.messages[eventID]
