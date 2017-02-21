# libclang and libTooling have issues finding system headers when a binary
# utilizing the libraries is invoked in a directory besides that in which
# the clang/clang++ executable resides.  To avoid headaches with setting
# up proper include paths, this script copies the EventAnnotator binary
# to the directory containing clang's binaries.  Users should always
# invoke the event annotator from this script instead of from the
# EventAnnotator binary itself.

# TODO add usage information

CLANG_BIN_PATH=$(which clang)
CLANG_BIN_DIR=$(dirname "${CLANG_BIN_PATH}")

if [ ! -f ${CLANG_BIN_DIR}/EventAnnotator ]; then
    if [ -z "${CLANG_BIN_PATH}" ]; then
        echo "Clang not found. Exiting."
        exit 1
    elif [ ! -f EventAnnotator ]; then
        echo "EventAnnotator not found.  Has the build been made?  Exiting."
        exit 1
    fi


    cp EventAnnotator $CLANG_BIN_DIR
fi

${CLANG_BIN_DIR}/EventAnnotator "$@"
