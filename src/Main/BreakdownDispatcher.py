from DispatcherBase import Dispatcher

class Breakdown(Dispatcher):
    def __init__(self):
        self.disallowedOptions = { 'annotate': True,
                                   'restore':  True  }

        self.requiredOptions = { 'target_fxn':    None,
                                 'data_dir':      None,
                                 'heights_file':  None,
                                 'k':             None,
                                 'root_fxn':      None,
                                 'selected_fxns': None  }

    # TODO will need to change where this calls
    def Dispatch(self):
        subprocess.call(['select_factor.sh', self.requiredOptions['target_fxn'],
                         self.requiredOptions['data_dir'], self.requiredOptions['heights_file'],
                         self.requiredOptions['k'], self.requiredOptions['root_fxn'],
                         self.requiredOptions['selected_fxns']], stderr = subprocess.STDOUT)
