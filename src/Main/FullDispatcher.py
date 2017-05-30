from DispatcherBase import Dispatcher
from AnnotatorDispatcher import Annotator
from BreakdownDispatcher import Breakdown

class Full(Dispatcher):
    def __init__(self):
        self.disallowedOptions = {}
        self.requiredOptions = { 'build_script': None,
                                 'run_script':   None  }


        super(Full, self).__init__(self.disallowedOptions, self.requiredOptions)

        self.annotator = Annotator()
        self.breakdown = Breakdown()

    def ParseOptions(self, options):
        success = super(Full, self).ParseOptions(options)

        return success and self.annotator.ParseOptions(options) \
               and self.breakdown.ParseOptions(option)

    def Dispatch(self):
        self.annotator.Dispatch()

        subprocess.call([self.requiredOptions['build_script']], stdout = subprocess.STDOUT,
                        stderr = subprocess.STDOUT)

        subprocess.call([self.requiredOptions['run_script'], self.breakdown.requiredOptions['data_dir']],
                        stdout = subprocess.STDOUT, stderr = subprocess.STDOUT)

        self.breakdown.Dispatch()
