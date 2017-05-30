import subprocess
import csv

class Restorer:
    def __init__(self, backupDir):
        self.backupDir = backupDir

    def Run(self, filenamesListFname):
        with open(filenamesListFname, 'rb') as fnamesList:
            filenameReader = csv.reader(fnamesList, delimiter='\t')

            for line in filenameReader:
                subprocess.call(['cp', line[0], line[1]])
                subprocess.call(['rm', line[0]])

        subprocess.call(['rm', filenamesListFname])
