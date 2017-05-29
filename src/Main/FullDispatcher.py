from DispatcherBase import Dispatcher

class Full(Dispatcher):
    def __init__(self):
        self.barredOptions = {}
        self.requiredOptions = { 'build_script': None,
                                 'run_script':   None  }

        self.annotator = Annotator()
        self.breakdown = Breakdown()

    @classmethod
    def Parse(self, options):
        success = Dispatcher.Parse.im_func(options)

        return success and self.annotator.Parse(options) \
               and self.breakdown.Parse(option)

    def Dispatch(self):
        self.annotator.Dispatch()

        subprocess.call([self.requiredOptions['build_script']], stdout = subprocess.STDOUT,
                        stderr = subprocess.STDOUT)

        subprocess.call([self.requiredOptions['run_script'], self.breakdown.requiredOptions['data_dir']],
                        stdout = subprocess.STDOUT, stderr = subprocess.STDOUT)

        self.breakdown.Dispatch()
