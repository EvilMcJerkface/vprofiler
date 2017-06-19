import subprocess
from Restorer import Restorer
from DispatcherBase import Dispatcher

class Restore(Dispatcher):
    def __init__(self):
        self.disallowedOptions = {}
        self.optionalOptions = {}
        self.requiredOptions = {}

    def RestoreFromBackup(self, backup):
        restorer = Restorer.Restorer(backup)
        restorer.Run('TracerFilenames')

    def Dispatch(self, backup, callerbackup, syncbackup):
        restorer = Restorer.Restorer(backup)
        restorer.Run('TracerFilenames')
        restorer = Restorer.Restorer(callerbackup)
        restorer.Run('CallerFilenames')
        restorer = Restorer.Restorer(syncbackup)
        restorer.Run('SynchronizationFilenames')

