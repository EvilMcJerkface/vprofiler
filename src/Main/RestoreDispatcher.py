from DispatcherBase import Dispatcher

class Restore(Dispatcher):
    def __init__(self):
        self.disallowedOptions = {}
        self.requiredOptions = {'backup_dir': None}

    def Dispatch(self, restoreTracer, restoreSynchro):
        subprocess.call(['vprof-restore', '--backup_dir', self.requiredOptions['backup_dir'],
                         '-t' if restoreTracer else '', '-s' if restoreSynchro else ''],
                         stderr = subprocess.STDOUT)

        if restoreTracer and restoreSynchro:
            subprocess.call(['rm', '-rf', self.requiredOptions['backup_dir'])

