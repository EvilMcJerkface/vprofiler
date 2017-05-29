class SemanticInterval:
    def __init__(self, startTime):
        self.startTime = startTime

    # Note that this function takes the overall latency of the semantic interval
    # as its argument to set the end time, not the end time itself.  Then, end
    # time is set as follows:
    # endTime = startTime + latency
    def SetEndTime(self, latency):
        self.endTime = self.startTime + latency

    def SetEndingThreadID(self, threadID):
        self.endingThreadID = threadID
