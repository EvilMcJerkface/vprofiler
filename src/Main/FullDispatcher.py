import subprocess
import os
import shutil

from FactorSelector import VarTree
from DispatcherBase import Dispatcher
from AnnotatorDispatcher import Annotator
from BreakdownDispatcher import Breakdown
from RestoreDispatcher import Restore

class Full(Dispatcher):
    def __init__(self):
        self.disallowedOptions = {}
        self.optionalOptions = {'target_fxn': None}
        self.requiredOptions = { 'build_script': None,
                                 'run_script':   None  }


        super(Full, self).__init__(self.disallowedOptions, self.optionalOptions, self.requiredOptions)

        self.annotator = Annotator()
        self.breakdown = Breakdown()
        self.restore = Restore()

    def ParseOptions(self, options):
        success = super(Full, self).ParseOptions(options)

        return success and self.annotator.ParseOptions(options) \
               and self.breakdown.ParseOptions(options)

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

    def __GetNextTargetFxn(self, selectedFunctions):
        funcNames = []

        for selectedFunction in selectedFunctions:
            funcNames.append(selectedFunction.func)

        done = False
        nextTarg = -1
        if len(funcNames) == 1:
            nextTarg = 0
            done = True
        while not done:
            while True:
                nextTargStr = raw_input('Number of next target function: ')

                if nextTargStr.isdigit() and int(nextTargStr) <= len(funcNames) and int(nextTargStr) >= 0:
                    break
                else:
                    print 'Input invalid'

            nextTarg = int(nextTargStr)

            targOkay = ''
            while True:
                targOkay = raw_input('Instrument ' + funcNames[nextTarg] + ' ? [y/n]').lower()

                if targOkay in ['y', 'n']:
                    break
                else:
                    print 'Input invalid'

            done = True if targOkay == 'y' else False

        return selectedFunctions[nextTarg]

    def Dispatch(self):
        instrumentSynchro = True
        syncbackup = '/tmp/vprof/syncbackup'
        callerbackup = '/tmp/vprof/callerbackup'
        backup = '/tmp/vprof/backup'
        funcNamesFile = '/tmp/vprof/funcNames'
        dataDir = "/tmp/vprof/"
        varTree = VarTree.Tree(self.optionalOptions['target_fxn'])
        selectedNode = varTree.root

        while True:
            self.annotator.AnnotateTargetFunc(selectedNode, funcNamesFile, backup, callerbackup)

            subprocess.call([self.requiredOptions['build_script']])

            if os.path.exists(dataDir + 'latency'):
                shutil.rmtree(dataDir + 'latency')
            subprocess.call([self.requiredOptions['run_script'], dataDir])

            selectedFunctions = self.breakdown.Dispatch(funcNamesFile, dataDir + 'latency/',
                                                        varTree, selectedNode)

            if self.__AreDone():
                break
            else:
                # instrumentSynchro = False
                # self.restore.Run(restoreTracer = True, restoreSynchro = False)
                selectedNode = self.__GetNextTargetFxn(selectedFunctions)
                self.restore.RestoreFromBackup(backup)

        print 'Restoring annotated files...'
        self.restore.Dispatch(backup, callerbackup, syncbackup)
        shutil.rmtree(backup)
        shutil.rmtree(callerbackup)
        shutil.rmtree(syncbackup)
        print 'Files restored.'
        print 'Done'
