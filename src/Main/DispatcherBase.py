import subprocess

class Dispatcher(object):
    def __init__(self, disallowedOptions, optionalOptions, requiredOptions):
        self.disallowedOptions = disallowedOptions
        self.optionalOptions = optionalOptions
        self.requiredOptions = requiredOptions

    def ParseOptions(self, options):
        for barredOpt, barredOptVal in self.disallowedOptions.iteritems():
            if getattr(options, barredOpt) == barredOptVal:
                print 'Barred opt ' + barredOpt + " found"
                return False

        for reqOpt, _ in self.requiredOptions.iteritems():
            self.requiredOptions[reqOpt] = getattr(options, reqOpt)
            if not self.requiredOptions[reqOpt]:
                print 'Required opt ' + reqOpt + " not found"
                return False

        for optionalOpt, _ in self.optionalOptions.iteritems():
            self.optionalOptions[optionalOpt] = getattr(options, optionalOpt)

        return True

    def Dispatch(self, options):
        pass

