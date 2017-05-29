from datetime import datetime
from OperationEnum import Operation
from SynchronizationObject import OwnableObject, EventCreationObject

class SynchronizationObjectAggregator:
    def __init__(self):
        self.objectMap = {}

        self.messages = {}

    def AddOperation(self, threadID, operation):
        objID = operation.objID
        # These values represent mutexes and condition variables
        if opID <= Operation.CV_SIGNAL:
            opID = operation.opID
            operationTimeStart = operation.timeStart
            operationTimeEnd = operation.timeEnd

            # If this conditional is true then we know it is a thread acquiring
            # ownership of an object and it's the first thread to own the object.
            if objID not in self.objectMap:
                self.objectMap[objID] = OwnableObject()

            if opID == Operation.MUTEX_UNLOCK or \
               opID == Operation.CV_BROADCAST or \
               opID == Operation.CV_SIGNAL:
                self.objectMap[objID].SetLatestOwnershipEndTime(operationTimeEnd)
            else:
                self.objectMap[objID].RegisterObjectAcquisitionRequest(operationTimeStart)
                self.objectMap[objID].StartNewOwnership(threadID, operationTimeEnd)

        else:
            if objID not in self.objectMap:
                self.objectMap[objID] = QueueObject()

            self.objectMap[objID].AddOperation(operation)


    # Get ending ending time of segment we have an dependence edge to, as
    # well as the thread ID of the thread adjacent to the dependence edge
    # which comes first in the time series.
    def GetDependenceEdge(self, request):
        if request.opID == Operation.MUTEX_LOCK or request.opID == Operation.CV_WAIT:
            return self.objectMap[request.objID].GetDependenceRelation(request.timeEnd)
        else:
            return self.objectMap[request.objID].GetDependenceRelation(request)
