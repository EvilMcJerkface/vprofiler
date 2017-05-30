import subprocess

class Dispatcher(object):
    def __init__(self, disallowedOptions, requiredOptions):
        self.disallowedOptions = disallowedOptions
        self.requiredOptions = requiredOptions

    def ParseOptions(self, options):
        for barredOpt, barredOptVal in self.disallowedOptions.iteritems():
            if getattr(options, barredOpt) == barredOptVal:
                return False

        for reqOpt, _ in self.requiredOptions.iteritems():
            self.requiredOptions[reqOpt] = getattr(options, reqOpt)
            if not self.requiredOptions[reqOpt]:
                return False

        return True

    def Dispatch(self, options):
        pass

