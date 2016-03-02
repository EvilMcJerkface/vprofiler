import java.io.*;
import java.util.LinkedList;
import java.util.List;

/**
 */
public class SubgraphGen {
    public static void main(String[] args) throws IOException {
        PrintWriter writer = new PrintWriter(new FileWriter("subgraph.voltdb"));
        List<String> functions = new LinkedList<>();
        functions.add("voltdb::VoltDBEngine::executePlanFragments(int32_t,int64_t*,int64_t*," +
                "voltdb::ReferenceSerializeInputBE&,int64_t,int64_t,int64_t,int64_t,int64_t)");
        List<String> lines = readLines("full.graph.voltdb");
        for (int index = 0; index < functions.size(); ++index) {
            String function = functions.get(index);
            for (String line : lines) {
                if (!line.startsWith("\"" + function + "\" -> ")) {
                    continue;
                }
                String calledFunction = getCalledFunction(line);
                if (!(calledFunction.startsWith("TraceTool::") ||
                        calledFunction.startsWith(("TRACE_")) ||
                        calledFunction.equals(function) ||
                        functions.contains(calledFunction))) {
                    functions.add(calledFunction);
                    writer.println('"' + function + "\" -> \"" + calledFunction + '"');
                }
            }
        }
        writer.close();
    }

    private static List<String> readLines(String file) throws IOException {
        List<String> lines = new LinkedList<>();
        BufferedReader reader = new BufferedReader(new FileReader(file));
        String line;
        while((line = reader.readLine()) != null) {
            lines.add(line);
        }
        reader.close();
        return lines;
    }

    private static String getCalledFunction(String line) {
        String calledStart = "-> \"";
        int startIndex = line.indexOf(calledStart);
        int endIndex = line.indexOf("\" [");
        return line.substring(startIndex + calledStart.length(), endIndex);
    }
}
