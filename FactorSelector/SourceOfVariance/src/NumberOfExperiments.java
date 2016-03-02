import java.io.*;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;

/**
 */
public class NumberOfExperiments
{
    private static HashMap<String, String> virtualFunctions = new HashMap<String, String>();
    private static HashMap<String, Node<String>> nodes;
    private static List<String> path;
    private static int[] numberOfFunctionsLeft;

    public static void main(String[] args) throws IOException
    {
        final String rootFunction = "dispatch_command(enum_server_command,THD*,char*,uint)";

        virtualFunctions.put("handler::ha_index_read_idx_map(uchar*,uint,const uchar*,key_part_map,ha_rkey_function)",
                "ha_innobase::index_read(uchar*,const uchar*,uint,ha_rkey_function)");
        virtualFunctions.put("handler::ha_write_row(uchar*)", "ha_innobase::write_row(uchar*)");
        virtualFunctions.put("handler::ha_update_row(const uchar*,uchar*)", "ha_innobase::update_row(const uchar*,uchar*)");

        String[][] functionsToMonitor = {
//                {"dispatch_command(enum_server_command,THD*,char*,uint)"},
                {"mysql_parse(THD*,char*,uint,Parser_state*)"},
                {"mysql_execute_command(THD*)"},
                {"execute_sqlcom_select(THD*,TABLE_LIST*)", "mysql_insert(THD*,TABLE_LIST*,List<Item>&,List<List<Item> >&,List<Item>&,List<Item>&,enum_duplicates,bool)", "mysql_update(THD*,TABLE_LIST*,List<Item>&,List<Item>&,Item*,uint,ORDER*,ha_rows,enum_duplicates,bool,ha_rows*,ha_rows*)"},
                {"handle_select(THD*,select_result*,ulong)", "write_record(THD*,TABLE*,COPY_INFO*,COPY_INFO*)"},
                {"mysql_select(THD*,TABLE_LIST*,uint,List<Item>&,Item*,SQL_I_List<st_order>*,SQL_I_List<st_order>*,Item*,ulonglong,select_result*,SELECT_LEX_UNIT*,SELECT_LEX*)", "ha_innobase::write_row(uchar*)"},
                {"mysql_execute_select(THD*,SELECT_LEX*,bool)", "row_insert_for_mysql(unsigned char*,row_prebuilt_t*)"},
                {"JOIN::optimize()", "row_ins_step(que_thr_t*)", "row_mysql_handle_errors(dberr_t*,trx_t*,que_thr_t*,trx_savept_t*)"},
                {"make_join_statistics(JOIN*,TABLE_LIST*,Item*,Key_use_array*,bool)", "row_ins(ins_node_t*,que_thr_t*)", "lock_wait_suspend_thread(que_thr_t*)"},
                {"join_read_const_table(JOIN_TAB*,POSITION*)", "row_ins_index_entry_step(ins_node_t*,que_thr_t*)"},
                {"join_read_const_table(JOIN_TAB*,POSITION*)", "row_ins_index_entry(dict_index_t*,dtuple_t*,que_thr_t*)"},
                {"ha_innobase::index_read(uchar*,const uchar*,uint,ha_rkey_function)", "row_ins_sec_index_entry(dict_index_t*,dtuple_t*,que_thr_t*)"},
                {"row_search_for_mysql(unsigned char*,ulint,row_prebuilt_t*,ulint,ulint)", "row_ins_sec_index_entry_low(ulint,ulint,dict_index_t*,mem_heap_t*,mem_heap_t*,dtuple_t*,trx_id_t,que_thr_t*)"},
                {"btr_pcur_open_with_no_init_func(dict_index_t*,const dtuple_t*,ulint,ulint,btr_pcur_t*,ulint,const char*,ulint,mtr_t*)", "btr_cur_search_to_nth_level(dict_index_t*,ulint,const dtuple_t*,ulint,ulint,btr_cur_t*,ulint,const char*,ulint,mtr_t*)"},
                {"btr_cur_search_to_nth_level(dict_index_t*,ulint,const dtuple_t*,ulint,ulint,btr_cur_t*,ulint,const char*,ulint,mtr_t*)", "buf_page_get_gen(ulint,ulint,ulint,ulint,buf_block_t*,ulint,const char*,ulint,mtr_t*)"},
                {"buf_page_get_gen(ulint,ulint,ulint,ulint,buf_block_t*,ulint,const char*,ulint,mtr_t*)", "btr_search_guess_on_hash(dict_index_t*,btr_search_t*,const dtuple_t*,ulint,ulint,btr_cur_t*,ulint,mtr_t*)", "buf_page_make_young_if_needed(buf_page_t*)"},
                {"buf_page_make_young_if_needed(buf_page_t*)", "buf_page_get_known_nowait(ulint,buf_block_t*,ulint,const char*,ulint,mtr_t*)", "buf_page_make_young(buf_page_t*)"},
                {"buf_page_make_young(buf_page_t*)"}
        };

        Node<String> root = new Node<String>(rootFunction, 7);
        BufferedReader reader = new BufferedReader(new FileReader("/Users/sens/Developer/Java/subgraph"));
        List<String> graph = new ArrayList<String>();
        String line;
        System.out.println("Reading file...");
        while ((line = reader.readLine()) != null)
        {
            graph.add(line);
        }
        reader.close();
        System.out.println(graph.size() + " lines read, constructing tree...");
        List<String> relatedLines = new ArrayList<String>();

        nodes = new HashMap<String, Node<String>>();
        path = new ArrayList<String>();
        path.add(root.getData());
        addChildren(rootFunction, root, graph, relatedLines);
        System.out.println("Tree constructed. Height is " + root.height() + ", size is " + root.size() + ", leaves " + root.getLeaves());

//        List<Node<String>> listOfNodes = new ArrayList<Node<String>>();
//        listOfNodes.add(root);
//        for (int index = 0; index < listOfNodes.size(); ++index)
//        {
//            Node node = listOfNodes.get(index);
//            if (node.getNumberOfChildren() > 0)
//            {
////                System.out.println(node.getData());
//                listOfNodes.addAll(node.getChildren());
//            }
//        }

        int[] numberOfFunctions = {1, 2, 5, 10, 20, 50, 100};
        long[] totalNumberOfExperiments = {0, 0, 0, 0, 0, 0, 0};
        countNumberOfExperiments(root, numberOfFunctions, totalNumberOfExperiments);
        for (int index = 0; index < numberOfFunctions.length; ++index)
        {
            System.out.print(totalNumberOfExperiments[index] + ",");
        }


//        int[] numberOfFunctions = {1, 2, 5, 10, 20, 50, 100};
//        numberOfFunctionsLeft = zeroes(numberOfFunctions.length);
//        int[] totalNumberOfExperiments = zeroes(numberOfFunctions.length);
//        for (String[] functions : functionsToMonitor)
//        {
//            for (int index = 0; index < functions.length; ++index)
//            {
//                Node<String> node = nodes.get(functions[index]);
//                for (int indexOfNumber = 0; indexOfNumber < numberOfFunctions.length; ++indexOfNumber)
//                {
//                    totalNumberOfExperiments[indexOfNumber] += getNumberOfExperiments(node.getNumberOfChildren(), numberOfFunctions, indexOfNumber, index == functions.length - 1);
//                }
//            }
//        }
//
//        for (int experiments : totalNumberOfExperiments)
//        {
//            System.out.print(experiments + ",");
//        }
    }

    private static int[] zeroes(int size)
    {
        int[] array = new int[size];
        for (int index = 0; index < size; ++index)
        {
            array[index] = 0;
        }

        return array;
    }

    private static void countNumberOfExperiments(Node<String> node, int[] numberOfFunctionsToMonitor, long[] totalNumberOfExperiments)
    {
        for (int index = 0; index < numberOfFunctionsToMonitor.length; ++index)
        {
            totalNumberOfExperiments[index] += (node.getNumberOfChildren() - 1) / numberOfFunctionsToMonitor[index];
        }
        for (Node<String> child : node.getChildren())
        {
            for (int index = 0; index < numberOfFunctionsToMonitor.length; ++index)
            {
                totalNumberOfExperiments[index] += (child.size() - 1) / numberOfFunctionsToMonitor[index];
            }
        }
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

                relatedLines.add(functionCall);

                String[] functions = functionCall.split(" -> ");
                String callee = functions[1].replace("\"", "");
//                int lastIndexOfColon = callee.lastIndexOf(":");
//                int firstIndexOfParentheses = callee.indexOf("(");
//                lastIndexOfColon = lastIndexOfColon == -1 ? 0 : lastIndexOfColon;
//                callee = callee.substring(lastIndexOfColon, firstIndexOfParentheses);

                if (virtualFunctions.keySet().contains(callee))
                {
                    callee = virtualFunctions.get(callee);
                }

                Node<String> child = getNode(callee);
                if (!path.contains(child.getData()))
                {
                    functionNode.addChild(child);
                }
            }
        }

        for (Node<String> child : functionNode.getChildren())
        {
            if (child.getNumberOfChildren() == 0)
            {
                path.add(child.getData());
                addChildren(child.getData(), child, graph, relatedLines);
                path.remove(child.getData());
            }
        }
    }

    private static Node<String> getNode(String functionName)
    {
//        return new Node<String>(functionName);
        Node<String> node = nodes.get(functionName);
        if (node == null)
        {
            node = new Node<String>(functionName, 7);
            nodes.put(functionName, node);
        }
        return node;
    }

    private static int getNumberOfExperiments(int numberOfChildFunctions, int[] numberOfFunctionsToMonitor, int index, boolean forceMonitor)
    {
        numberOfFunctionsLeft[index] = (numberOfChildFunctions + numberOfFunctionsLeft[index]) % numberOfFunctionsToMonitor[index];
        int numberOfExperiments = (numberOfChildFunctions + numberOfFunctionsLeft[index]) / numberOfFunctionsToMonitor[index];

        if (numberOfExperiments == 0 && forceMonitor)
        {
            numberOfExperiments = 1;
            numberOfFunctionsLeft[index] = 0;
        }

        return numberOfExperiments;
    }
}
