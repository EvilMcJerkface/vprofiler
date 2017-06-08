import os
import shutil
import csv

class Restorer:
    def __init__(self, backupDir):
        self.backupDir = backupDir
        if not self.backupDir.endswith('/'):
            self.backupDir += '/'

    def Run(self, filenamesListFname, doDelete=False):
        if not os.path.exists(self.backupDir + filenamesListFname):
            return
        with open(self.backupDir + filenamesListFname, 'rb') as fnamesList:
            filenameReader = reversed(list(csv.reader(fnamesList, delimiter='\t')))

            for line in filenameReader:
                shutil.copyfile(line[0], line[1])
                if doDelete:
                    os.remove(line[0])

        if doDelete:
            os.remove(self.backupDir + filenamesListFname)
