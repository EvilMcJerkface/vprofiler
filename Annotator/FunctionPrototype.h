#ifndef FUNCTIONPROTOTYPE_H
#define FUNCTIONPROTOTYPE_H

#include <string>
#include <vector>

// Should put whether this is a CXXMethodDecl in here. Should also 
// ensure variable name of function parameters are included in the
// functionPrototype.  Also, a vector of these variable names would
// be useful later in the annotation process.

class FunctionPrototype {
    public:
        FunctionPrototype():
        functionPrototype(""), innerCallPrefix(""), paramVars(), returnType("") {}

        std::string functionPrototype;
        std::string innerCallPrefix;

        std::vector<std::string> paramVars;

        std::string returnType;
};

#endif
