import org.apache.commons.cli.*;
import Utils;

public class Dispatcher {
    final String sourceFilePath;
    final String execScriptPath;
    final String topLvlFunc;

    final Integer numIterations;
    final Integer numFactors;

    private Dispatcher(String _sourceFilePath, String _execScriptPath, String _topLvlFunc,
                       Integer _numIterations, Integer _numFactors) {
        sourceFilePath = _sourceFilePath;
        execScriptPath = _execScriptPath;
        topLvlFunc = _topLvlFunc;

        numIterations = _numIterations;
        numFactors = _numFactors;
    }

    /* Creates new Dispatcher from result of command line parsing.  Returns NewDispatcher *
     * object if initialization is successful, null otherwise                             */ 
    public static Dispatcher NewDispatcher(CommandLine cmd) {
        String sourceFile, topLvlFunc, execScript, preNumIter, preFactors;
        Integer numFactors, numIter;
        boolean failure;

        sourceFile = cmd.getOptionValue("s", null);
        topLvlFunc = cmd.getOptionValue("f", null);
        execScript = cmd.getOptionValue("e", null);

        preFactors = cmd.getOptionValue("k", null);
        preNumIter = cmd.getOptionValue("n", null);

        /* Check not strictly necessary (all opts set to required earlier),               *
         * but good sanity check                                                          */
        failure = Utils.AnyNull(sourceFile, topLvlFunc, execScript, preNumIter, preFactors);

        if (failure) {
            return null;
        }

        numFactors = Integer.parseInt(preFactors);
        numIter = Integer.parseInt(preNumIter);

        return new Dispatcher(sourceFile, topLvlFunc, execScript, numFactors, numIter);
    }
}
