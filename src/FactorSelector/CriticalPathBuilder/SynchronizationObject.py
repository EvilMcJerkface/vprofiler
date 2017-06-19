from abc import ABCMeta, abstractmethod
from OperationEnum import Operation

# Abstract base class for synchronization objects
class SynchronizationObject:
    __metaclass__ = ABCMeta

    @abstractmethod
    def GetDependenceRelation(self, obj):
        pass

class OwnershipTimeInterval:
    def __init__(self, threadID, startTime):
        self.threadID = threadID

        self.startTime = startTime
        self.endTime = None

        self.otherThreadTriedToAcquire = False

# Represents mutexes, condition variables, and semaphores (semaphores might actually need more changes)
# Probably need extra changes for rwlocks
class OwnableObject(SynchronizationObject):
    def __init__(self):
        self.ownershipTimeSeries = []

    def StartNewOwnership(self, threadID, startTime):
        valToAppend = None

        if len(self.ownershipTimeSeries) > 0 and self.ownershipTimeSeries[-1].otherThreadTriedToAcquire:
            self.ownershipTimeSeries[-1].endTime = startTime

        self.ownershipTimeSeries.append(OwnershipTimeInterval(threadID, startTime))

    def SetLatestOwnershipHadAcquireReq(self, val):
        self.ownershipTimeSeries[-1].otherThreadTriedToAcquire = val

    # The END TIMES :OOOOO
    def SetLatestOwnershipEndTime(self, endTime):
        self.ownershipTimeSeries[-1].endTime = endTime

    def RegisterObjectAcquisitionRequest(self, timestamp):
        if len(self.ownershipTimeSeries) > 0:
            if self.ownershipTimeSeries[-1].startTime <= timestamp and \
              (self.ownershipTimeSeries[-1].endTime is None or \
               timestamp <= self.ownershipTimeSeries[-1].endTime):
                  self.ownershipTimeSeries[-1].otherThreadTriedToAcquire = True

    def GetDependenceRelation(self, timestamp):
        # Just loop over everything for now.  If too slow, can write something else.

        resultIdx = -1
        for i in range(len(self.ownershipTimeSeries)):
            if self.ownershipTimeSeries[i].startTime == timestamp:
                if self.ownershipTimeSeries[i - 1].otherThreadTriedToAcquire:
                    resultIdx = i - 1
                break

        if resultIdx == -1:
            return None, None
        else:
            return self.ownershipTimeSeries[resultIdx].endTime, \
                   self.ownershipTimeSeries[resultIdx].threadID

# Maybe have something where it's a map from event -> event that created it.
# While parsing, maintain queue modeling the actual queue in user's code.
# When we have an operation which pops from the queue, create an entry in the
# event -> event creator map.  Then, for GetDependenceEdge, just return the
# event stored at dependenceEdges[recipientEvent] where recipientEvent is the
# event whose creator we are trying to find.
class QueueObject(SynchronizationObject):
    def __init__(self):
        # Map from eventID to the threadID of the thread which created event eventID
        self.eventCreationRelationships = {}
        self.eventQueue = []

    def AddOperation(self, operation):
        if operation.opID == Operation.MESSAGE_SEND or \
           operation.opID == Operation.QUEUE_ENQUEUE:
            self.eventQueue.append(operation)

        # If the operation is MESSAGE_RECEIVE or QUEUE_DEQUEUE
        else:
            creatingEvent = self.eventQueue.pop(0)
            self.eventCreationRelationships[operation] = creatingEvent

    # Returns the threadID of the thread which created event eventID.  Returns None
    # if no event with eventID was found.
    def GetDependenceRelation(self, operation):
        result = self.eventCreationRelationships.get(operation)

        if result is None:
            return None, None

        return result.endTime, result.threadID
