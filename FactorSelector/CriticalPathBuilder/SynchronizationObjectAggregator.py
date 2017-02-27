from datetime import datetime
from OperationsEnum import Operation

class SynchronizationObjectAggregator:
    def __init__(self):
        self.objectMap = {}

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
