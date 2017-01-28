import org.apache.commons.cli.*;

public class Dispatcher {
    final String sourceFilePath;
    final String execScriptPath;
    final String compilationScriptPath;
    final String traceToolPath;
    final String topLvlFunc;

    final Integer numIterations;
    final Integer numFactors;

    // Make this line prettier
    private Dispatcher(String _sourceFilePath, String _execScriptPath, 
                       String _compilationScriptPath, String _traceToolPath, 
                       String _topLvlFunc, Integer _numIterations, 
                       Integer _numFactors) {
        sourceFilePath = _sourceFilePath;
        execScriptPath = _execScriptPath;
        compilationScriptPath = _compilationScriptPath;
        traceToolPath = _traceToolPath;
        topLvlFunc = _topLvlFunc;

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
        for (int i = 0; i < numIterations; i++) {
            Process execScriptPath = pb.start();
            execScriptPath.waitFor();
        }

        // TODO Run factor selection. Need to figure out how exactly that's run first.
    }

    /* Creates new Dispatcher from result of command line parsing.  Returns NewDispatcher *
     * object if initialization is successful, throws IllegalArgumentError if failure.    */ 
    public static Dispatcher NewDispatcher(CommandLine cmd) {
        String sourceFile, topLvlFunc, execScript, preNumIter, preFactors, 
               compScript, traceTool;
        Integer numFactors, numIter;
        boolean failure;

        sourceFile = cmd.getOptionValue("s", null);
        execScript = cmd.getOptionValue("e", null);
        compScript = cmd.getOptionValue("c", null);
        traceTool  = cmd.getOptionValue("t", null);

        topLvlFunc = cmd.getOptionValue("f", null);

        preFactors = cmd.getOptionValue("k", null);
        preNumIter = cmd.getOptionValue("n", null);

        /* Check not strictly necessary (all opts set to required earlier),               *
         * but good sanity check.                                                         */
        failure = Utils.AnyNull(sourceFile, execScript, compScript, traceTool, 
                                topLvlFunc, preNumIter, preFactors);

        if (failure) {
            throw new IllegalArgumentException();
        }

        numIter = Integer.parseInt(preNumIter);
        numFactors = Integer.parseInt(preFactors);

        return new Dispatcher(sourceFile, execScript, compScript, traceTool, 
                              topLvlFunc, numIter, numFactors);
    }
}
