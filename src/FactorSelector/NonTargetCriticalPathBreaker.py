import csv
import os
import re
import numpy as np

def mtxTranspose(mtx):
    return [list(item) for item in zip(*mtx)]

# Input:
# criticalPaths: a dictionary mapping transaction IDs to critical paths
# functionLog_dir: a string denoting the directory holding all function log files
# functionName_path: a string denoting the filepath holding the list of function names
#
# Output:
# resultArr: a 2d arrat, where the first index is transaction id.
#	     Each id maps to a row containing how much the transaction
#            overlapped with each function, along with the queueing time
#            and the execution time of that transaction
def NonTargetCriticalPathBreak(criticalPaths, functionLog_dir, functionName_path):
    # Obtain a list of all function log files
    functionLogPaths = []
    for item in os.listdir(functionLog_dir):
        if os.path.isfile(os.path.join(functionLog_dir, item)) and \
           re.match("FunctionLog", item):
            functionLogPaths.append(item)

    # Obtain a list of all of the function names
    functionNames = []
    with open(functionName_path) as names:
        reader = csv.reader(names)
        for row in reader:
            functionNames.append(row[0])

    # Initialize the result array
    resultArr = []

    # Iterate through every critical path, every interval in that path, every
    # log file, and every row in that log file
    for txid, cpath in criticalPaths.items():
        # Keep track of the overall start time of the transaction,
        # the overall end time, the time that the most recent interval ended,
        # and the amount of time that the transaction spent in between intervals
        startTime = 0
        endTime = 0
        lastIntervalEndTime = 0
        queueingTime = 0

        # Initialize the result row for this transaction
        # At first, the row has 0 time for each function
        resultRow = []
        for j in range(len(functionNames)):
            resultRow.append(0)

        for i, interval in enumerate(sorted(cpath)):
            for log_path in functionLogPaths:
                with open(log_path) as log_csv:
                    log = csv.reader(log_csv)

                    # Compare the row to the current interval.
                    # If the intervals overlap and the thread and transaction match,
                    # then add the overlap time to the transaction's row in the
                    # function's column
                    for row in log:
                        if txid == row[2] and interval.data == row[1]:
                            overlap = min(row[4], interval.end) - max(row[3], interval.begin)

                            if overlap > 0:
                                resultRow[row[0]] += overlap

                # If this is the first interval, set the start time of the transaction.
                # Otherwise, increment the queueing time by the difference between this
                # interval's start time and the last interval's end time
                if i == 0:
                    startTime = interval.begin
                else:
                    queueingTime += interval.begin - lastIntervalEndTime

                # If this is the last interval, set the end time of the transaction.
                # Otherwise, record the end time of this interval to calculate queueing time
                if i == len(cpath) - 1:
                    endTime = interval.end
                else:
                    lastIntervalEndTime = interval.end


        latency = endTime - startTime

        # Attach the queueing time and the execution time to the row,
        # then append it to the final result
        resultRow.append(queueingTime)
        resultRow.insert(0, latency)

        # Here we use latency as the exec time of the virtual parent function
        resultRow.append(latency)
        resultArr.append(resultRow)

    # Remove functions from the result that aren't on the critical path
    for i in reversed(range(len(functionNames))):
        allZeros = True
        for j in range(len(resultArr)):
            if resultArr[j][i] != 0:
                allZeros = False

        if allZeros is True:
            for row in resultArr:
                del row[i]

            del functionNames[i]

    functionNames.append('virtual_parent')

    return mtxTranspose(resultArr), functionNames

