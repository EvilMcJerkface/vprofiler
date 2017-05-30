from DispatcherBase import Dispatcher

class Restore(Dispatcher):
    def __init__(self):
        self.disallowedOptions = {}
        self.requiredOptions = {}

        super(Restore, self).__init__(self.disallowedOptions, self.requiredOptions)

    def Dispatch(self):
        print('Error: Restore mode not yet implemented')
