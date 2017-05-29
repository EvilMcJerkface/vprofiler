from csv import reader

numOpsInTimeLog = {}
numOpsInOpLog = {}

with open('OperationLog.log', 'rb') as opLogFile:
    opLogReader = reader(opLogFile)

    for op in opLogReader:
        if len(op) == 4:
            if op[0] not in numOpsInOpLog:
                numOpsInOpLog[op[0]] = 0

            numOpsInOpLog[op[0]] += 1

with open('SynchronizationTimeLog.log', 'rb') as timesLogFile:
    timesLogReader = reader(timesLogFile)

    for op in timesLogReader:
        if len(op) == 4:
            if op[0] not in numOpsInTimeLog:
                numOpsInTimeLog[op[0]] = 0

            numOpsInTimeLog[op[0]] += 1

for tid, numOccurs in numOpsInOpLog.items():
    print('Num logs for tid ' + tid + ' in operation log: ' + str(numOccurs))
    print('Num logs for tid ' + tid + ' in time log: ' + str(numOpsInTimeLog[tid]))
