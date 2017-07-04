import os
import subprocess

from FactorSelector import VarBreaker
from DispatcherBase import Dispatcher

class Breakdown(Dispatcher):
    def __init__(self):
        self.disallowedOptions = { 'annotate': True,
                                   'restore':  True  }
        self.optionalOptions = {}
        self.requiredOptions = { 'num_factors':   None }

    def Dispatch(self, funcNamesFile, dataDir, varTree, selectedNode):
        VarBreaker.breakDown(funcNamesFile, dataDir, selectedNode)
        selectedFuncs = varTree.selectFactors(self.requiredOptions['num_factors'])
        index = 0
        for node in selectedFuncs:
            print '[' + str(index) + '] ' + node.func + ':' + str(node.perct)
            index += 1
        return selectedFuncs
