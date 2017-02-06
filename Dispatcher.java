import org.apache.commons.cli.*;

public class Dispatcher {
    final String sourceFilePath;
    final String execScriptPath;
    final String compilationScriptPath;
    final String traceToolPath;
    final String topLvlFunc;
    final String dataDir;
    final String heightsFile;

    final Integer numIterations;
    final Integer numFactors;

    // Make this line prettier
    private Dispatcher(String _sourceFilePath, String _execScriptPath, 
                       String _compilationScriptPath, String _traceToolPath, 
                       String _topLvlFunc, String _dataDir,
                       String _heightsFile, Integer _numIterations, 
                       Integer _numFactors) {
        sourceFilePath = _sourceFilePath;
        execScriptPath = _execScriptPath;
        compilationScriptPath = _compilationScriptPath;
        traceToolPath = _traceToolPath;
        topLvlFunc = _topLvlFunc;
        dataDir = _dataDir;
        heightsFile = _heightsFile;

        numIterations = _numIterations;
        numFactors = _numFactors;
    }

    public void Run() throws Exception {
        /* Annotate command                                                               */
        ProcessBuilder pb = new ProcessBuilder("annotate.sh", sourceFilePath, topLvlFunc, traceToolPath);

        final Process annotateProcess = pb.start();
        annotateProcess.waitFor();

        pb.command(compilationScriptPath);
        final Process buildProcess = pb.start();
        buildProcess.waitFor();

        pb.command(execScriptPath);
        final Process execScript = pb.start();
        execScript.waitFor();

        pb.command("factor_selector.sh", topLvlFunc, dataDir, heightsFile, Integer.toString(numIterations), topLvlFunc);
        final Process factorSelector = pb.start();
        factorSelector.waitFor();
    }

    /* Creates new Dispatcher from result of command line parsing.  Returns NewDispatcher *
     * object if initialization is successful, throws IllegalArgumentError if failure.    */ 
    public static Dispatcher NewDispatcher(CommandLine cmd) {
        String sourceFile, topLvlFunc, execScript, preNumIter, preFactors, 
               compScript, traceTool, dataDir, heightPath;
        Integer numFactors, numIter;
        boolean failure;

        sourceFile = cmd.getOptionValue("s", null);
        execScript = cmd.getOptionValue("e", null);
        compScript = cmd.getOptionValue("c", null);
        traceTool  = cmd.getOptionValue("t", null);
        dataDir    = cmd.getOptionValue("d", null);
        heightPath = cmd.getOptionValue("h", null);

        topLvlFunc = cmd.getOptionValue("f", null);

        preFactors = cmd.getOptionValue("k", null);
        preNumIter = cmd.getOptionValue("n", null);

        /* Check not strictly necessary (all opts set to required earlier),               *
         * but good sanity check.                                                         */
        failure = Utils.AnyNull(sourceFile, execScript, compScript, traceTool, 
                                topLvlFunc, dataDir, heightPath, preNumIter, preFactors);

        if (failure) {
            throw new IllegalArgumentException();
        }

        numIter = Integer.parseInt(preNumIter);
        numFactors = Integer.parseInt(preFactors);

        return new Dispatcher(sourceFile, execScript, compScript, traceTool, 
                              topLvlFunc, dataDir, heightPath, numIter, numFactors);
    }
}
