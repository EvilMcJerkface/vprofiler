import org.apache.commons.cli.*;

public class VProfiler {
    public static void main(String[] args) {
        Options options = new Options();

        //Option interfaceOpt = new Option("i", "interfaceFile", true, "File denoting synchronization interfaces");

        Option sourceFileOpt = new Option("s", "sourceFile", true, "Path to source file");
        sourceFileOpt.setRequired(true);
        options.addOption(sourceFileOpt);

        Option topLvlFuncOpt = new Option("f", "functionOpt", true, "Top level function of semantic interval");
        topLvlFuncOpt.setRequired(true);
        options.addOption(topLvlFuncOpt);

        // Assume we can find path of trace_tool.cc

        Option execScriptOpt = new Option("e", "execScript", true, "Path to script which runs the project being profiled");
        execScriptOpt.setRequired(true);
        options.addOption(execScriptOpt);

        Option numFactorsOpt = new Option("k", "numFactors", true, "Number of factors to select");
        numFactorsOpt.setRequired(true);
        options.addOption(numFactorsOpt);

        Option numIterOpt = new Option("n", "numIterations", true, "Number of times to run the execution script");
        numIterOpt.setRequired(false);
        options.addOption(numIterOpt);

        CommandLineParser clParser = new DefaultParser();
        HelpFormatter helpFormatter = new HelpFormatter();
        CommandLine cmd;

        try {
            cmd = clParser.parse(options, args);
        }
        catch {
            System.out.println(e.getMessage());
            helpFormatter.printHelp("VProfiler", options);

            System.exit(1);
            return;
        }

       Dispatcher dispatcher = new Dispatcher.NewDispatcher(cmd);

    }
}
