#include <string>
#include <vector>

class WrapperGenState {
    public:
        WrapperGenState():
            internalCallPrefix(""), argumentIndices(), usesResult(false) {}

        WrapperGenState(std::string _internalCallPrefix,
                           std::vector<int> _argIdxs,
                           bool _usesResult): internalCallPrefix(_internalCallPrefix),
                                              argumentIndices(_argIdxs),
                                              usesResult(_usesResult) {}
        
        // Prefix for call to VProfiler state, prefix will look something like:
        // on_read(
        std::string internalCallPrefix;

        // The indices of the arguments of the function being wrapped which need
        // to be passed to the call to alter VProfiler state.  For example, if 
        // if mknod with prototype:
        //
        // int mknod(const char *pathname, mode_t mode, dev_t dev)
        //
        // is being wrapped, the internal call will look like:
        //
        // on_mknod(pathname, mode)
        //
        // Here, argumentIndices' state will be [0, 1] as on_mknod needs the 0th
        // and 1st arguments of mknod passed to it in that order.
        std::vector<int> argumentIndices; 

        // True if the inner call to VProfiler state uses the result of the wrapped
        // call.
        bool usesResult;
};
