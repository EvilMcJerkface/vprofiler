import org.apache.commons.cli.*;

public class VProfiler {
    public static void main(String[] args) {
        Options options = new Options();

        Option sourceFileOpt = new Option("s", "sourceFile", true, "Path to source file");
        sourceFileOpt.setRequired(true);
        options.addOption(sourceFileOpt);

        Option topLvlFuncOpt = new Option("f", "functionOpt", true, "Top level function of semantic interval");
        topLvlFuncOpt.setRequired(true);
        options.addOption(topLvlFuncOpt);

        Option traceToolOpt = new Option("t", "traceTool", true, "Path to trace_tool.cc");
        traceToolOpt.setRequired(true);
        options.addOption(traceToolOpt);

        Option compilationScriptOpt = new Option("c", "compilationScript", true, "Path to script which rebuilds source. Be sure to include trace_tool.cc within your build process.");
        compilationScriptOpt.setRequired(true);
        options.addOption(compilationScriptOpt);

        Option execScriptOpt = new Option("e", "execScript", true, "Path to script which runs the project being profiled");
        execScriptOpt.setRequired(true);
        options.addOption(execScriptOpt);

        Option dataDirOpt = new Option("d", "dataDir", true, "Directory which will contain the latency data produced by the execution script");
        dataDirOpt.setRequired(true);
        options.addOption(dataDirOpt);

        Option heightFileOpt = new Option("h", "heightsFile", true, "The path to the generated heights file");
        heightFileOpt.setRequired(true);
        options.addOption(heightFileOpt);

        Option numFactorsOpt = new Option("k", "numFactors", true, "Number of factors to select");
        numFactorsOpt.setRequired(true);
        options.addOption(numFactorsOpt);


      /*Option numIterOpt = new Option("n", "numIterations", true, "Number of times to run the execution script");
        numIterOpt.setRequired(false);
        options.addOption(numIterOpt);*/

        CommandLineParser clParser = new DefaultParser();
        HelpFormatter helpFormatter = new HelpFormatter();

        try {
            CommandLine cmd;
            Dispatcher dispatcher;

            cmd = clParser.parse(options, args);
            dispatcher = Dispatcher.NewDispatcher(cmd);
            dispatcher.Run();

            System.exit(0);
            return;
        }
        catch (ParseException|IllegalArgumentException e) {
            System.out.println(e.getMessage());
            helpFormatter.printHelp("VProfiler", options);

            System.exit(1);
            return;
        }
        catch (Exception e) {
            System.out.println(e.getMessage());

            System.exit(1);
            return;
        }
    }
}
