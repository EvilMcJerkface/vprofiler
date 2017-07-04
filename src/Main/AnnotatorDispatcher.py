import shutil
import subprocess
import os
from DispatcherBase import Dispatcher

class Annotator(Dispatcher):
    def __init__(self):
        self.disallowedOptions = { 'breakdown': True,
                                   'restore':   True  }
        self.optionalOptions = { 'root_funcs': None }
        self.requiredOptions = { 'source_dir': None,
                                 'parallelism_functions': None,
                                 'compilation_db': None }

        self.syncbackup = '/tmp/vprof/syncbackup'
        self.callerbackup = '/tmp/vprof/callerbackup'
        self.backup = '/tmp/vprof/backup'
        self.errorLogName = '/tmp/vprof/annotator.log'
        self.createIfNotExists(self.syncbackup)
        self.createIfNotExists(self.callerbackup)
        self.createIfNotExists(self.backup)
        self.errorLog = open(self.errorLogName, 'a')

    def createIfNotExists(self, dirname):
        if not os.path.exists(dirname):
            os.makedirs(dirname)

    def getSourceDir(self):
        sourceDir = self.requiredOptions['source_dir']
        if not sourceDir.endswith('/'):
            sourceDir += '/'
        return sourceDir

    def getFuncNameAndTypes(self, nodeString):
        nodeString = nodeString.split(' [')[0]
        nodeString = nodeString.replace('"', '')
        nodeString = nodeString.replace(',', '|')
        if '()' in nodeString:
            nodeString = nodeString.replace('()', '')
        else:
            nodeString = nodeString.replace('(', '|')
            nodeString = nodeString.replace(')', '')
        return nodeString

    def AnnotateWithoutTarget(self, callGraph, funcNamesFile, backup):
        funcFile = open(funcNamesFile, 'w')
        funcFile.close()
        print 'Instrumenting thread entry functions...'
        subprocess.call(['TracerInstrumentor -s %s -b %s -n %s -r %s %s' %
                         (self.getSourceDir(), backup, funcNamesFile,
                          self.optionalOptions['root_funcs'], self.requiredOptions['compilation_db'])],
                        stderr=self.errorLog, shell=True)
        print 'Instrumentation done.'

    def AnnotateTargetFunc(self, selectedNode, funcNamesFile, backup, callerbackup):
        print 'Instrumenting target function ' + selectedNode.func + '...'
        if selectedNode.parent:
            subprocess.call(['TracerInstrumentor -s %s -f %s -c %s -t %d -b %s -e %s -n %s %s' %
                             (self.getSourceDir(), selectedNode.func, selectedNode.parent.func,
                              selectedNode.depth, backup, callerbackup, funcNamesFile,
                              self.requiredOptions['compilation_db'])],
                            stderr=self.errorLog, shell=True)
        else:
            subprocess.call(['TracerInstrumentor -s %s -f %s -b %s -n %s %s' %
                             (self.getSourceDir(), selectedNode.func, backup, funcNamesFile,
                              self.requiredOptions['compilation_db'])],
                            stderr=self.errorLog, shell=True)

        print 'Instrumentation done.'

    def Dispatch(self, instrumentSynchro=True, targetFunc=''):
        print 'Instrumenting synchronization APIs...'
        subprocess.call(['EventAnnotator -s %s -f %s -b %s %s' % (self.getSourceDir(), self.requiredOptions['parallelism_functions'],
                        self.syncbackup, self.requiredOptions['compilation_db'])], stderr=self.errorLog, shell=True)
        cwd = os.path.dirname(os.path.realpath(__file__))
        self.createIfNotExists('./vprof_files')
        shutil.copyfile(cwd + '/../ExecutionTimeTracer/trace_tool.cc', './vprof_files/trace_tool.cc')
        shutil.copyfile(cwd + '/../ExecutionTimeTracer/trace_tool.h', './vprof_files/trace_tool.h')
        shutil.move('./VProfEventWrappers.cc', './vprof_files/VProfEventWrappers.cc')
        shutil.move('./VProfEventWrappers.h', './vprof_files/VProfEventWrappers.h')
        print 'Instrumentation done, please integrate the files in vprof_files to your application.'
