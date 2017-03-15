from abc import ABCMeta, abstractmethod

# Abstract base class for synchronization objects
class SynchronizationObject:
    __metaclass__ = ABCMeta

    @abstractmethod
    def GetDependenceRelation(obj):
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
                if self.ownershipTimeSeries[i].otherThreadTriedToAcquire:
                    resultIdx = i - 1
                break

        if resultIdx == -1:
            return None, None
        else:
            return self.ownershipTimeSeries[resultIdx].endTime, \
                   self.ownershipTimeSeries[resultIdx].threadID

# User should provide function which returns a unique ID of the event so we can
# track it
class EventCreationObject(SynchronizationObject):
    def __init__(self):
        # Map from eventID to the threadID of the thread which created event eventID
        self.eventCreationRelationships = {}

    def AddEventCreationRelationship(self, eventID, threadID, creationTime):
        self.eventCreationRelationships[eventID] = (threadID, creationTime)

    # Returns the threadID of the thread which created event eventID.  Returns None
    # if no event with eventID was found.
    def GetDependenceRelation(self, eventID):
        result = self.eventCreationRelationships.get(eventID)

        if result == None:
            result = (None, None)

        return result[0], result[1]
