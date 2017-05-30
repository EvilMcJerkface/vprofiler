import csv

from DispatcherBase import Dispatcher
from AnnotatorDispatcher import Annotator
from BreakdownDispatcher import Breakdown
from RestoreDispatcher import Restore

class Full(Dispatcher):
    def __init__(self):
        self.disallowedOptions = {}
        self.requiredOptions = { 'build_script': None,
                                 'run_script':   None  }


        super(Full, self).__init__(self.disallowedOptions, self.requiredOptions)

        self.annotator = Annotator()
        self.breakdown = Breakdown()
        self.restore = Restore()

    def ParseOptions(self, options):
        success = super(Full, self).ParseOptions(options)

        return success and self.annotator.ParseOptions(options) \
               and self.breakdown.ParseOptions(option)

    def __AreDone(self):
        inputCorrect = False
        allowedVals = ['y', 'n']

        while True:
            inVal = raw_input('Are these factors sufficient? [y/n] ')

            if inVal.lower() in allowedVals:
                break
            else:
                'Input value not valid'

        return True if inVal.lower() == 'y' else False

    def __GetNextTargetFxn(self):
        fxnOptions = []

        with open('.breakdownOut', 'rb') as breakdownOut:
            breakdownReader = csv.reader(breakdownOut, delimiter = ' ')

            for line in breakdownReader:
                fxnOptions.append(line[1])


        done = False
        nextTarg = -1
        while not done:
            while True:
                nextTargStr = raw_input('Number of next target function: ')

                if nextTargStr.isdigit() and int(nextTargStr) <= len(fxnOptions) and int(nextTargStr) > 0:
                    break
                else:
                    print('Input invalid')

            nextTarg = int(nextTargStr)

            targOkay = ''
            while True:
                targOkay = raw_input('Instrument ' + fxnOptions[nextTarg] + ' ? [y/n]').lower()

                if targOkay in ['y', 'n']:
                    break
                else:
                    print('Input invalid')

            done = True if targOkay = 'y' else False

        return fxnOptions[nextTarg]


    def Dispatch(self):
        instrumentSynchro = True
        targetFxn = ''

        while True:
            self.annotator.Dispatch(instrumentSynchro, targetFxn)

            subprocess.call([self.requiredOptions['build_script']], stdout = subprocess.STDOUT,
                            stderr = subprocess.STDOUT)

            subprocess.call([self.requiredOptions['run_script'], self.breakdown.requiredOptions['data_dir']],
                            stdout = subprocess.STDOUT, stderr = subprocess.STDOUT)

            self.breakdown.Dispatch()

            if self.__AreDone():
                break
            else:
                instrumentSynchro = False
                self.restore.Run(restoreTracer = True, restoreSynchro = False)

                targetFxn = self.__GetNextTargetFxn()

        subprocess.call(['rm', '.breakdownOut'])
        self.restore.Run(restoreTracer = True, restoreSynchro = True)
