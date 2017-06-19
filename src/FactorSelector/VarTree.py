class Node(object):
    def __init__(self, parent, contribution, perct):
        self._children = []
        self._parent = parent
        self._contribution = contribution
        self._perct = perct

    @property
    def children(self):
        return self._children

    @property
    def parent(self):
        return self._parent
    @parent.setter
    def parent(self, parent):
        self._parent = parent

    @property
    def contribution(self):
        return self._contribution
    @contribution.setter
    def contribution(self, contribution):
        self._contribution = contribution

    @property
    def perct(self):
        return self._perct
    @perct.setter
    def perct(self, perct):
        self._perct = perct

    @property
    def depth(self):
        depth = 0
        node = self._parent
        while node != None:
            depth += 1
            node = node.parent
        return depth

    def addChild(self, child):
        self._children.append(child)

class VarNode(Node):
    def __init__(self, function, parent, contribution, perct):
        super(VarNode, self).__init__(parent, contribution, perct)
        self._func = function

    @property
    def func(self):
        return self._func
    @func.setter
    def func(self, func):
        self._func = func

class CovNode(Node):
    def __init__(self, func1, func2, parent, contribution, perct):
        super(CovNode, self).__init__(parent, contribution, perct)
        self._func1 = func1
        self._func2 = func2

    @property
    def func(self):
        return self._func1 + ',' + self._func2
    @func.setter
    def func(self, func1, func2):
        self._func1 = func1
        self._func2 = func2

class Tree(object):
    def __init__(self, func):
        self._root = VarNode(func, None, 0, 100)

    @property
    def root(self):
        return self._root

    @root.setter
    def root(self, root):
        self._root = root

    def getLeaves(self):
        leaves = []
        nodesToLookAt = [self._root]
        while len(nodesToLookAt) > 0:
            node = nodesToLookAt.pop(0)
            if len(node.children) == 0:
                if node.perct > 5:
                    leaves.append(node)
            else:
                nodesToLookAt += node.children
        return leaves

    def selectFactors(self, k):
        leaves = self.getLeaves()
        leaves.sort(key=lambda x: x.perct, reverse=True)
        num_selected = min(k, len(leaves))
        return leaves[:num_selected]
