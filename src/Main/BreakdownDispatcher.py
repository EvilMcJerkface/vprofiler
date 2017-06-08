import os
import subprocess

from FactorSelector import VarBreaker
from DispatcherBase import Dispatcher

class Breakdown(Dispatcher):
    def __init__(self):
        self.disallowedOptions = { 'annotate': True,
                                   'restore':  True  }

        self.requiredOptions = { 'heights_file':  None,
                                 'num_factors':   None,
                                 'root_fxn':      None }
        # cwd = os.path.dirname(os.path.realpath(__file__))
        # self.installPrefix = cwd[:cwd.rfind('Main')]

    def Dispatch(self, funcNamesFile, dataDir, varTree, selectedNode):
        VarBreaker.breakDown(funcNamesFile, dataDir, selectedNode)
        selectedFuncs = varTree.selectFactors(self.requiredOptions['num_factors'])
        index = 0
        for node in selectedFuncs:
            print '[' + str(index) + '] ' + node.func + ':' + str(node.perct)
            index += 1
        return selectedFuncs
        # subprocess.call(['var_breaker', '-f', funcNamesFile, '-d', dataDir,
        #                 '-v', self.varTree], stderr=subprocess.STDOUT)
        # selectedOut = subprocess.check_output(['java', '-cp', self.installPrefix,'FactorSelector',
        #                                       self.varTree, self.requiredOptions['heights_file'],
        #                                       self.requiredOptions['num_factors'],
        #                                       self.requiredOptions['root_fxn']],
        #                                       stderr=subprocess.STDOUT)
        # print selectedOut
        # selectedFunctions = []
        # print 'Selected function:'
        # index = 0
        # for line in selectedOut.split('\n'):
        #     if len(line) <= 1:
        #         continue
        #     selectedFunctions.append(line.split('|'))
        #     funcName = line.split('|')[0]
        #     print '[' + str(index) + '] ' + funcName
        # index += 1
        # return selectedFunctions
