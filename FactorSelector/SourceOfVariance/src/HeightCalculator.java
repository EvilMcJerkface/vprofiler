import java.io.*;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

public class HeightCalculator
{
    private static HashMap<String, String> virtualFunctions = new HashMap<String, String>();
    private static HashMap<String, Node<String>> nodes;
    private static List<Node<String>> path;

    public static void main(String[] args) throws IOException
    {
        final String rootFunction = "voltdb::VoltDBEngine::executePlanFragments(int32_t,int64_t*,int64_t*," +
                "voltdb::ReferenceSerializeInputBE&,int64_t,int64_t,int64_t,int64_t,int64_t)";

        virtualFunctions.put("handler::ha_index_read_idx_map(uchar*,uint,const uchar*,key_part_map,ha_rkey_function)",
                "ha_innobase::index_read(uchar*,const uchar*,uint,ha_rkey_function)");
        virtualFunctions.put("handler::ha_write_row(uchar*)", "ha_innobase::write_row(uchar*)");
        virtualFunctions.put("handler::ha_update_row(const uchar*,uchar*)", "ha_innobase::update_row(const uchar*,uchar*)");

        Node<String> root = new Node<>(rootFunction, 1);
        BufferedReader reader = new BufferedReader(new FileReader("subgraph.voltdb"));
        List<String> graph = new ArrayList<>();
        String line;
        System.out.println("Reading file...");
        while ((line = reader.readLine()) != null)
        {
            graph.add(line);
        }
        reader.close();
        System.out.println(graph.size() + " lines read, constructing tree...");
        List<String> relatedLines = new ArrayList<>();

        nodes = new HashMap<>();
        path = new ArrayList<>();
        path.add(root);
        addChildren(rootFunction, root, graph, relatedLines);

        System.out.println("Tree constructed. Height is " + root.height());

        System.out.println("Calculating height for each node...");
        HashMap<String, Integer> heights = new HashMap<>();
        calculateHeights(root, heights);
        System.out.println("Calculation done. Writing to file...");

        PrintWriter writer = new PrintWriter(new FileWriter("heights.voltdb"));
        printHeights(root, writer, heights);
        writer.close();

//        writer = new PrintWriter(new FileWriter("subgraph"));
//        for (String relatedLine : relatedLines)
//        {
//            writer.println(relatedLine);
//        }
//        writer.close();

        System.out.println("Done");
    }

    private static void addChildren(String functionName, Node<String> functionNode, List<String> graph, List<String> relatedLines)
    {
        for (int index = 0, size = graph.size(); index < size; ++index)
        {
            String functionCall = graph.get(index);
            if (functionCall.startsWith("\"" + functionName + "\" -> "))
            {
                graph.remove(index);
                --index;
                --size;

                int indexOfLabel = functionCall.indexOf(" [label=");
                if (indexOfLabel > 0)
                {
                    functionCall = functionCall.substring(0, indexOfLabel);
                }
                relatedLines.add(functionCall);

                String[] functions = functionCall.split(" -> ");
                String callee = functions[1].replace("\"", "");

                if (virtualFunctions.keySet().contains(callee))
                {
                    callee = virtualFunctions.get(callee);
                }

                Node<String> child = getNode(callee);
                if (!path.contains(child))
                {
                    functionNode.addChild(child);
                }
            }
        }

        for (Node<String> child : functionNode.getChildren())
        {
            if (child.getNumberOfChildren() == 0)
            {
                path.add(child);
                addChildren(child.getData(), child, graph, relatedLines);
                path.remove(child);
            }
        }
    }

    private static Node<String> getNode(String functionName)
    {
        Node<String> node = nodes.get(functionName);
        if (node == null)
        {
            node = new Node<>(functionName, 1);
            nodes.put(functionName, node);
        }
        return node;
    }

    private static void calculateHeights(Node<String> node, HashMap<String, Integer> heights)
    {
        if (!heights.containsKey(node.getData()))
        {
            for (Node<String> child : node.getChildren())
            {
                calculateHeights(child, heights);
            }
            heights.put(node.getData(), node.height());
        }
    }

    private static void printHeights(Node<String> node, PrintWriter writer, HashMap<String, Integer> heights)
    {
        if (heights.get(node.getData()) == null)
        {
            return;
        }

        if (node.getData().startsWith("std::") ||
                node.getData().startsWith("_Alloc>") ||
                node.getData().startsWith("I>::") ||
                node.getData().startsWith("I_P_List") ||
                node.getData().startsWith("base_list"))
        {
            return;
        }

        String functionName = node.getData();
        int parenthesesIndex = functionName.indexOf("(");
        functionName = functionName.substring(0, parenthesesIndex);
//        int colonIndex = functionName.lastIndexOf(":");
//        int start = colonIndex < 0 ? 0 : colonIndex + 1;
//        functionName = functionName.substring(start);
        int height = heights.get(node.getData());
        writer.println(functionName + "," + height + "," + (24 - height));
        heights.remove(node.getData());

        for (Node<String> child : node.getChildren())
        {
            printHeights(child, writer, heights);
        }
    }
}
