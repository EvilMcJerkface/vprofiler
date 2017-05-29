import argparse

##################################
# Set command line options begin #
##################################

parser = argparse.Parser(prog="vprof",
                         description="VProfiler - a system for identifying sources of variance within a codebase",
                         epilog="For more help, visit https://github.com/mozafari/vprofiler")

parser.add_argument('-a', '--annotate',
                    action='store_true',
                    help='Setting this flag tells vprofiler to annotate your code')

parser.add_argument('-b', '--breakdown',
                    action='store_true',
                    help='Setting this flag tells vprofiler to determine which parts of your code most attribute to variance')

parser.add_argument('-r', '--restore', action='store_true',
                    help='Setting this flag tells vprofiler to restore your source tree to its un-annotated state. ' \
                          'That is, the state of the source tree before vprofiler performed annotations')

parser.add_argument('-o', '--originals_dir',
                    help='This directory contains all the un-annotated version of each file that vprofiler annotated')

parser.add_argument('-s', '-source_dir',
                    help='This directory is the root of your source tree.  This must be set when using the option -a')

parser.add_argument('-p', '-parallelism_functions',
                    help='A file listing the parallelism functions which should be profiled.  For specifics ' \
                            'on the format of this file see https://github.com/mozafari/vprofiler')

parser.add_argument('-c', '--compilation_db',
                    help='The compilation database vprofiler will use when instrumenting source code.')

parser.add_argument('-m', '--main_file',
                    help='The file with the semantic interval to be profiled')

parser.add_argument('-t', '--target_fxn',
                    help='The name of the top level function in which the semantic interval begins')

# For finding path to trace_tool.cc, maybe make some dir like /lib/vprof/ and put trace_tool
# there instead of having a separate command line option?

parser.add_argument('-d', '--data_dir',
                    help='Directory containing the data files created by vprofiler')

parser.add_argument('-h', '--heights_file',
                    help='File containing each function in the call graph and its ' \
                         'corresponding height in the graph')

parser.add_argument('-k', help='The number of factors to be selected')

##################################
# Set command line options end   #
##################################

args = parser.parse_args()

# TODO
# Logic here not correct (say user selects annotate & breakdown). Fix this
if args.annotate:
    Annotator(args)
elif args.breakdown:
    Breakdown(args)
elif args.restore:
    Restore(args)
else:
    print 'ERROR: Must select one of annotate, breakdown, or restore'
