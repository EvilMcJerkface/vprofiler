from DispatcherBase import Dispatcher

class Annotator(Dispatcher):
    def __init__(self):
        self.disallowedOptions = { 'breakdown': True,
                                   'restore':   True  }

        self.requiredOptions = { 'source_dir': None,
                                 'parallelism_functions': None,
                                 'compilation_db': None,
                                 'target_fxn': None }

    def Dispatch(self, instrumentSynchro = True, targetFxn = ''):
        if targetFxn:
            self.requiredOptions['target_fxn'] = targetFxn

        if instrumentSynchro:
            print('Instrumenting synchronization events')
            subprocess.call(['EventAnnotator', '-s', self.requiredOptions['source_dir'],
                  '-f', self.requiredOptions['parallelism_functions'], self.requiredOptions['compilation_db']],
                  stderr = subprocess.STDOUT)

        print('Instrumenting user source code')
        subprocess.call(['TraceInstrumentor', '-s', self.requiredOptions['source_dir'],
              '-f', self.requiredOptions['target_fxn'], self.requiredOptions['compilation_db']],
              stderr = subprocess.STDOUT)
