from abc import ABCMeta
import subprocess

class Dispatcher(ABCMeta):
    def __init__(self):
        self.disallowedOptions = {}

    @classmethod
    def ParseOptions(self, options):
        for barredOpt, barredOptVal in self.disallowedOptions.iteritems():
            if getattr(options, barredOpt) == barredOptVal:
                return False

        for reqOpt, _ in self.requiredOptions.iteritems():
            self.requiredOptions[reqOpt] = getattr(options, reqOpt)
            if self.requiredOptions[reqOpt] == '':
                return False

        return True

    @abstractmethod
    def Dispatch(self, options):
        pass

