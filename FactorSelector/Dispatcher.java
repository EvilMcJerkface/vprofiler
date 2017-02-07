import org.apache.commons.cli.*;

public class Dispatcher {
    final String sourceFilePath;
    final String execScriptPath;
    final String compilationScriptPath;
    final String topLvlFunc;
    final String dataDir;
    final String heightsFile;
    final String traceToolPath;

    //final Integer numIterations;
    final Integer numFactors;

    // Make this line prettier
    private Dispatcher(String _sourceFilePath, String _execScriptPath, 
                       String _compilationScriptPath, String _topLvlFunc, 
                       String _dataDir, String _heightsFile, 
                       String _traceToolPath, Integer _numFactors) {
        sourceFilePath = _sourceFilePath;
        execScriptPath = _execScriptPath;
        compilationScriptPath = _compilationScriptPath;
        topLvlFunc = _topLvlFunc;
        dataDir = _dataDir;
        heightsFile = _heightsFile;
        traceToolPath = _traceToolPath;

        numFactors = _numFactors;
    }

    public void Run() throws Exception {
        ProcessBuilder pb = new ProcessBuilder("/bin/bash", "/home/jiamin/vprofTest/annotate.sh", sourceFilePath, topLvlFunc, traceToolPath);
        
        // Redirect IO to parent process
        pb.redirectOutput(ProcessBuilder.Redirect.INHERIT);
        pb.redirectError(ProcessBuilder.Redirect.INHERIT); 
        pb.redirectInput(ProcessBuilder.Redirect.INHERIT);

        final Process annotateProcess = pb.start();
        annotateProcess.waitFor();

        pb.command("/bin/bash", compilationScriptPath);
        final Process buildProcess = pb.start();
        buildProcess.waitFor();

        pb.command("/bin/bash", execScriptPath);
        final Process execScript = pb.start();
        execScript.waitFor();

        pb.command("/bin/bash", "select_factor.sh", topLvlFunc, dataDir, heightsFile, Integer.toString(numFactors), topLvlFunc);
        final Process factorSelector = pb.start();
        factorSelector.waitFor();
    }

    /* Creates new Dispatcher from result of command line parsing.  Returns NewDispatcher *
     * object if initialization is successful, throws IllegalArgumentError if failure.    */ 
    public static Dispatcher NewDispatcher(CommandLine cmd) {
        String sourceFile, topLvlFunc, execScript, preFactors, 
               compScript, traceTool, dataDir, heightPath, tracePath;
        Integer numFactors;
        boolean failure;

        sourceFile = cmd.getOptionValue("s", null);
        execScript = cmd.getOptionValue("e", null);
        compScript = cmd.getOptionValue("c", null);
        dataDir    = cmd.getOptionValue("d", null);
        heightPath = cmd.getOptionValue("h", null);
        tracePath  = cmd.getOptionValue("t", null);

        topLvlFunc = cmd.getOptionValue("f", null);

        preFactors = cmd.getOptionValue("k", null);

        /* Check not strictly necessary (all opts set to required earlier),               *
         * but good sanity check.                                                         */
        failure = Utils.AnyNull(sourceFile, execScript, compScript, 
                                topLvlFunc, dataDir, heightPath, 
                                tracePath, preFactors);

        if (failure) {
            throw new IllegalArgumentException();
        }

        numFactors = Integer.parseInt(preFactors);

        return new Dispatcher(sourceFile, execScript, compScript, 
                              topLvlFunc, dataDir, heightPath, 
                              tracePath, numFactors);
    }
}
