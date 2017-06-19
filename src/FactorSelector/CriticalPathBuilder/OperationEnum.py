from enum import Enum

class Operation(Enum):
    MUTEX_LOCK      = 0
    MUTEX_UNLOCK    = 1
    CV_WAIT         = 2
    CV_BROADCAST    = 3
    CV_SIGNAL       = 4
    QUEUE_ENQUEUE   = 5
    QUEUE_DEQUEUE   = 6
    MESSAGE_SEND    = 7
    MESSAGE_RECEIVE = 8
    SWITCH_SI       = 9

    def __eq__(self, other):
        if self.__class__ == other.__class__:
            return self.value == other.value
        return NotImplemented

    def __le__(self, other):
        if self.__class__ == other.__class__:
            return self.value <= other.value
        return NotImplemented
