from abc import ABCMeta

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

class OwnableObject(SynchronizationObject):
    def __init__(self, objID):
        self.objID = objID
        self.ownershipTimeSeries = []

        # Only to be set true after synchronization file has been
        # completely parsed
        self.isInitialized = False

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

    def GetDependenceRelation(self, timestamp):
        # Just loop over everything for now.  If too slow, can write something else.

        result = None

        for timeInterval in self.ownershipTimeSeries:
            if timeInterval.startTime <= timestamp and timestamp <= timeInterval.endTime:
                result = timeInterval.threadID
                break

        return result

