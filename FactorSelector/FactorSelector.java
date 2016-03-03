import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.*;

/**
 */
public class FactorSelector
{
    private static HashMap<String, String> virtualFunctions = new HashMap<>();
    private static HashMap<String, Integer> heights;
    private static int maxHeight = -1;
    private static List<String> selectedFactors;
    private static String varTreeFile;
    private static String heightsFile;
    private static int k;
    private static String rootFunction;

    public static void main(String[] args)
    {
        if (args.length < 4) {
            System.out.println("Usage: java FactorSelector <variance tree file> <heights file> <k(number of functions to select)> <root function> [selected function 1] [selected function 2]...");
            System.exit(1);
        }

        varTreeFile = args[0];
        heightsFile = args[1];
        k = Integer.parseInt(args[2]);
        rootFunction = args[3];

        for (int index = 4; index < args.length; ++index) {
            selectedFactors.add(args[index]);
        }

        readHeights();
        setSelectedFactors();

        Tree<String> tree = readVarianceTree(varTreeFile);
        List<Node<String>> factors = factorSelection(tree, k, 0.05);

        for (Node<String> factor : factors)
        {
            System.out.println(factor.getData() + "," + factor.getValue() + "," + factor.getResponsibility());
        }
    }

    private static void readHeights()
    {
        heights = new HashMap<>();

        try
        {
            BufferedReader reader = new BufferedReader(new FileReader(heightsFile));
            String line;
            while((line = reader.readLine()) != null)
            {
                String[] functionHeight = line.split(",");
                String functionName = functionHeight[0].toLowerCase();
                int indexOfColon = functionName.lastIndexOf(":");
                functionName = functionName.substring(indexOfColon + 1);
                int height = Integer.parseInt(functionHeight[1]);
                heights.put(functionName, height);

                if (height > maxHeight)
                {
                    maxHeight = height;
                }
            }
        } catch (IOException e)
        {
            e.printStackTrace();
        }
    }

    private static Tree<String> readVarianceTree(String variance_tree_file)
    {
        // virtualFunctions.put("ha_index_read_map", "index_read");
        // virtualFunctions.put("ha_index_read_idx_map", "index_read");
        // virtualFunctions.put("ha_write_row", "write_row");
        // virtualFunctions.put("ha_update_row", "update_row");

        Node<String> root = new Node<>(rootFunction, 1);
        BufferedReader reader;
        try
        {
            reader = new BufferedReader(new FileReader(variance_tree_file));
            List<String> graph = new ArrayList<>();
            String line;
            while ((line = reader.readLine()) != null)
            {
                graph.add(line);
            }
            reader.close();
            addChildren(rootFunction, root, graph);
        } catch (IOException e)
        {
            e.printStackTrace();
        }

        Tree<String> tree = new Tree<>();
        tree.setRootElement(root);
        return tree;
    }

    private static void setSelectedFactors()
    {
        selectedFactors = new ArrayList<>();
    }

    private static void addChildren(String functionName, Node<String> functionNode, List<String> graph)
    {
        for (int index = 0, size = graph.size(); index < size; ++index)
        {
            String functionCall = graph.get(index);
            if (functionCall.startsWith("\"" + functionName + "\" -> "))
            {
                graph.remove(index);
                --index;
                --size;

                String[] functions = functionCall.split(" -> ");
                String caller = functions[0].replace("\"", "");
                String callee = functions[1].replace("\"", "");

                if (virtualFunctions.keySet().contains(callee))
                {
                    callee = virtualFunctions.get(callee);
                }

                Node<String> child = new Node<>(callee, 1);
                double contribution = Double.parseDouble(functions[2]);
                int height = getHeight(callee, heights.get(ripDash(caller)));
                if (height != -1)
                {
                    if (selectedFactors.contains(callee))
                    {
                        child.setContribution(Double.MAX_VALUE);
                    }
                    else
                    {
                        child.setContribution(contribution);
                    }
                    child.setValue(contribution);
                    child.setResponsibility(responsibility(contribution, height));

                    functionNode.addChild(child);

                    if (child.getContribution() / functionNode.getContribution() > 0.85 ||
                            (functionNode.getContribution() - child.getContribution()) < 0.05)
                    {
                        functionNode.setContribution(0);
                    }
                }
            }
        }

        for (Node<String> child : functionNode.getChildren())
        {
            if (child.getNumberOfChildren() == 0)
            {
                addChildren(child.getData(), child, graph);
            }
        }
    }

    private static int getHeight(String function, int parentHeight)
    {
        if (!function.contains(","))
        {
            if (function.contains("img_"))
            {
                return parentHeight - 1;
            }
            else
            {
                function = ripDash(function);
                if (heights.get(function) == null)
                {
                    return -1;
                }
                else
                {
                    return heights.get(function);
                }
            }
        }
        else
        {
            String[] functions = function.split(",");
            return Math.max(getHeight(functions[0], parentHeight), getHeight(functions[1], parentHeight));
        }
    }

    private static List<Node<String>> factorSelection(Tree<String> tree, int numberOfFactors, double threshold)
    {
        HashMap<String, Node<String>> functionNameToFactor = new HashMap<>();
        List<Node<String>> nodesInTree = tree.toList();

        for (Node<String> node : nodesInTree)
        {
            String function = node.getData();
            function = ripDash(function);
            Node<String> factor = functionNameToFactor.get(function);
            if (factor == null)
            {
                factor = new Node<>(function, 1);
                factor.setContribution(node.getContribution());
                factor.setValue(node.getValue());
                factor.setResponsibility(node.getResponsibility());
                functionNameToFactor.put(function, factor);
            }
            else
            {
                factor.addContribution(node.getContribution());
                factor.addValue(node.getValue());
                factor.addResponsibility(node.getResponsibility());
            }
        }

        List<Node<String>> factors = new ArrayList<>(functionNameToFactor.size());
        for (String functionName : functionNameToFactor.keySet())
        {
            factors.add(functionNameToFactor.get(functionName));
        }
        System.out.println();

        for (int index = 0, size = factors.size(); index < size; ++index)
        {
            Node node = factors.get(index);
            if (node.getContribution() < threshold)
            {
                factors.remove(index);
                --index;
                --size;
            }
        }

        Collections.sort(factors, new Comparator<Node<String>>()
        {
            @Override
            public int compare(Node<String> node1, Node<String> node2)
            {
                int compare_result = Double.compare(node1.getResponsibility(), node2.getResponsibility());
                if (compare_result < 0)
                {
                    return 1;
                }
                else if (compare_result > 0)
                {
                    return -1;
                }
                else
                {
                    return 0;
                }
            }
        });

        for (int index = factors.size() - 1; index >= numberOfFactors; --index)
        {
            factors.remove(index);
        }

        return factors;
    }

    private static String ripDash(String function)
    {
        int indexOfDash = function.indexOf("-");
        indexOfDash = indexOfDash > 0 ? indexOfDash : function.length();
        function = function.substring(0, indexOfDash);
        return function;
    }

    private static double responsibility(double contribution, double height)
    {
        if (contribution == Double.MAX_VALUE)
        {
            return Double.MAX_VALUE;
        }

        return contribution * specificness(height);
    }

    private static double specificness(double height)
    {
        return Math.pow(maxHeight - height, 2);
    }
}
