import java.util.ArrayList;
import java.util.List;

/**
 * Represents a node of the Tree<T> class. The Node<T> is also a container, and
 * can be thought of as instrumentation to determine the location of the type T
 * in the Tree<T>.
 */
public class Node<T> {

    private T data;
    private List<Node<T>> children;
    private int height;
    private long size;
    private double responsibility;
    private double contribution;
    private double value;
    private int heightInCallGraph;
    private long leaves;
    private long[] numberOfExperiments;

    /**
     * Default constructor.
     */
    public Node() {
        super();
    }

    /**
     * Convenience constructor to create a Node<T> with an instance of T.
     * @param data an instance of T.
     */
    public Node(T data, int number) {
        this();
        setData(data);
        height = -1;
        size = 1;
        leaves = -1;
        numberOfExperiments = new long[number];
        for (int index = 0; index < number; ++index)
        {
            numberOfExperiments[index] = 0;
        }
    }

    /**
     * Return the children of Node<T>. The Tree<T> is represented by a single
     * root Node<T> whose children are represented by a List<Node<T>>. Each of
     * these Node<T> elements in the List can have children. The getChildren()
     * method will return the children of a Node<T>.
     * @return the children of Node<T>
     */
    public List<Node<T>> getChildren() {
        if (this.children == null) {
            return new ArrayList<Node<T>>();
        }
        return this.children;
    }

    /**
     * Sets the children of a Node<T> object. See docs for getChildren() for
     * more information.
     * @param children the List<Node<T>> to set.
     */
    public void setChildren(List<Node<T>> children) {
        this.children = children;
    }

    /**
     * Returns the number of immediate children of this Node<T>.
     * @return the number of immediate children.
     */
    public int getNumberOfChildren() {

        if (children == null) {
            return 0;
        }
        return children.size();
    }

    /**
     * Adds a child to the list of children for this Node<T>. The addition of
     * the first child will create a new List<Node<T>>.
     * @param child a Node<T> object to set.
     */
    public void addChild(Node<T> child) {
        if (children == null) {
            children = new ArrayList<Node<T>>();
        }
        children.add(child);
    }

    /**
     * Inserts a Node<T> at the specified position in the child list. Will     * throw an ArrayIndexOutOfBoundsException if the index does not exist.
     * @param index the position to insert at.
     * @param child the Node<T> object to insert.
     * @throws IndexOutOfBoundsException if thrown.
     */
    public void insertChildAt(int index, Node<T> child) throws IndexOutOfBoundsException {
        if (index == getNumberOfChildren()) {
            // this is really an append
            addChild(child);
        } else {
            children.get(index); //just to throw the exception, and stop here
            children.add(index, child);
        }
    }

    /**
     * Remove the Node<T> element at index index of the List<Node<T>>.
     * @param index the index of the element to delete.
     * @throws IndexOutOfBoundsException if thrown.
     */
    public void removeChildAt(int index) throws IndexOutOfBoundsException {
        children.remove(index);
    }

    public T getData() {
        return this.data;
    }

    public void setData(T data) {
        this.data = data;
    }

    public int height() {
        if (height == -1)
        {
            if (children == null || children.size() == 0)
            {
                height = 0;
            }
            else
            {
                int maximum = 0;
                for (Node child : children)
                {
                    int height = child.height();
                    if (height > maximum)
                    {
                        maximum = height;
                    }
                    size += child.size;
                }
                height = maximum + 1;
            }
        }
        return height;
    }

    public long getLeaves()
    {
        if (leaves == -1)
        {
            if (children == null || children.size() == 0)
            {
                leaves = 1;
            }
            else
            {
                for (Node child : children)
                {
                    leaves += child.getLeaves();
                }
            }
        }

        return leaves;
    }

    public long size()
    {
        return size;
    }

    public void setContribution(double contribution)
    {
        this.contribution = contribution;
    }

    public double getContribution()
    {
        return contribution;
    }

    public void setValue(double value)
    {
        this.value = value;
    }

    public double getValue()
    {
        return value;
    }

    public void addContribution(double contribution)
    {
        if (this.contribution != Double.MAX_VALUE)
        {
            this.contribution += contribution;
        }
    }

    public void addValue(double value)
    {
        this.value += value;
    }

    public void addResponsibility(double responsibility)
    {
        this.responsibility += responsibility;
    }

    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("{").append(getData().toString()).append(",[");
        int i = 0;
        for (Node<T> e : getChildren()) {
            if (i > 0) {
                sb.append(",");
            }
            sb.append(e.getData().toString());
            i++;
        }
        sb.append("]").append("}");
        return sb.toString();
    }

    public double getResponsibility()
    {
        return responsibility;
    }

    public void setResponsibility(double responsibility)
    {
        this.responsibility = responsibility;
    }

    public int getHeightInCallGraph()
    {
        return heightInCallGraph;
    }

    public void setHeightInCallGraph(int heightInCallGraph)
    {
        this.heightInCallGraph = heightInCallGraph;
    }

    public long getNumberOfExperiments(int index, int numberOfFunctions)
    {
        if (children == null || children.size() == 0)
        {
            numberOfExperiments[index] = 0;
            return 0;
        }
        else if (numberOfExperiments[index] > 0)
        {
            return numberOfExperiments[index];
        }
        else
        {
            numberOfExperiments[index] = (children.size() - 1) / numberOfFunctions + 1;
            for (Node<T> child : children)
            {
                numberOfExperiments[index] += child.getNumberOfExperiments(index, numberOfFunctions);
            }
        }
        return numberOfExperiments[index];
    }
}