from abc import ABCMeta

# Abstract base class for synchronization objects
class SynchronizationObject:
    __metaclass__ = ABCMeta

    @abstractmethod
    def GetDependenceRelation(obj):
        pass

class OwnershipTimeInterval:
    def __init__(self, startTime):
        self.startTime = startTime
        self.endTime = None

        # True if a request for ownership of an object was placed while
        # ownership was held by another entity.
        self.requestsWhileHeld = False

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
            valToAppend = OwnershipTimeInterval(threadID, self.ownershipTimeSeries[-1].endTime)
        else:
            valToAppend = OwnershipTimeInterval(threadID, startTime)

        self.ownershipTimeSeries.append(valToAppend)

    def SetLatestOwnershipHadAcquireReq(self, val):
        self.ownershipTimeSeries[-1].otherThreadTriedToAcquire = val

    # The END TIMES :OOOOO
    def SetLatestOwnershipEndTime(self, endTime):
        self.ownershipTimeSeries[-1].endTime = endTime

    def RegisterObjectAcquisitionRequest(self, timestamp):
        if len(self.ownershipTimeSeries) > 0:
            if self.ownershipTimeSeries[-1].startTime <= timestamp and
              (self.ownershipTimeSeries[-1].endTime == None or
               timestamp <= self.ownershipTimeSeries[-1].endTime):
                  self.ownershipTimeSeries[-1].otherThreadTriedToAcquire = True

    def GetDependenceRelation(self, timestamp):
        # Just loop over everything for now.  If too slow, can write something else.

        result = None

        for timeInterval in self.ownershipTimeSeries:
            if timeInterval.startTime <= timestamp and timestamp <= timeInterval.endTime:
                result = timeInterval.threadID
                break

        return result

# User should provide function which returns a unique ID of the event so we can
# track it
class EventCreationObject(SynchronizationObject):
    def __init__(self):
        # Map from eventID to the threadID of the thread which created event eventID
        self.eventCreationRelationships = {}

    def AddEventCreationRelationship(self, eventID, threadID):
        self.eventCreationRelationships[eventID] = threadID

    # Returns the threadID of the thread which created event eventID.  Returns None
    # if no event with eventID was found.
    def GetEventCreator(self, eventID):
        return self.eventCreationRelationships.get(eventID)
