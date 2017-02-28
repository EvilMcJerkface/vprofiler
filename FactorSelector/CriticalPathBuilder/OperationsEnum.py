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
