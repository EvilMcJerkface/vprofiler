#ifndef VPROFFRONTENDFACTORY_H
#define VPROFFRONTENDFACTORY_H

// Clang libs
#include "clang/Tooling/Tooling.h"

// STL libs
#include <unordered_map>
#include <string>
#include <memory>

// VProf libs
#include "FunctionPrototype.h"
#include "VProfFrontendAction.h"

class VProfFrontendActionFactory : public clang::tooling::FrontendActionFactory {
    private:
        // Maps qualified function name to vprof wrapper name.
        std::shared_ptr<std::unordered_map<std::string, std::string>> functionNameMap;

        // Maps qualified function name to FunctionPrototype object.
        std::shared_ptr<std::unordered_map<std::string, FunctionPrototype>> prototypeMap;
    public:
        VProfFrontendActionFactory(std::shared_ptr<std::unordered_map<std::string, 
                                   std::string>> _functionNameMap,
                                   std::shared_ptr<std::unordered_map<std::string,
                                   FunctionPrototype>> _prototypeMap):
                                   functionNameMap(_functionNameMap),
                                   prototypeMap(_prototypeMap) {}

        // Creates a VProfFrontendAction to be used by clang tool.
        virtual VProfFrontendAction *create() {
            return new VProfFrontendAction(functionNameMap, prototypeMap);
        }
};

// This is absurdly long, but not sure how to break lines up to make more
// readable
std::unique_ptr<VProfFrontendActionFactory> newVProfFrontendActionFactory(std::shared_ptr<std::unordered_map<std::string, std::string>> functionNameMap,
                                                                          std::shared_ptr<std::unordered_map<std::string, FunctionPrototype>> prototypeMap) {
    return std::unique_ptr<VProfFrontendActionFactory>(new VProfFrontendActionFactory(functionNameMap, prototypeMap));
}

#endif
