"""
VProfiler invokes the other components to annotate the target system and
analyze the output.
"""
import argparse
import os
import shutil
import subprocess
import sys


MSG_LENGTH = 80
MSG_HEADER = '/' * MSG_LENGTH
MSG_FOOTER = '/' * MSG_LENGTH


def print_msg(msg):
    """ Print message shown in between the header and footer
    """
    msg = ' ' + msg + ' '
    msg_len = len(msg)
    num_left_asterisks = (MSG_LENGTH - msg_len) / 2
    num_right_asterisks = MSG_LENGTH - (msg_len + num_left_asterisks)
    print '/' * num_left_asterisks + msg + '/' * num_right_asterisks

def parse_args():
    """ Parse arguments
    """
    parser = argparse.ArgumentParser()
    parser.add_argument('-q', '--queuefile', help='File with queueing APIs')
    parser.add_argument('-s', '--source', help='Path of the source repository')
    parser.add_argument('-c', '--compiledb', help='Path of compile_commands.json file')
    parser.add_argument('-r', '--runningsript', help='Script to compile and run the application')
    parser.add_argument('-g', '--graph', help='Call graph file of the repository')
    parser.add_argument('-p', '--processor',
                        help='High level function for request processing', default='')
    args = parser.parse_args()
    return args


def annotate(args):
    """ Run the EventAnnotator
    """
    envcopy = os.environ.copy()
    try:
        clang_path = subprocess.check_output(['which', 'clang'], env=envcopy)[:-1]
    except subprocess.CalledProcessError:
        sys.exit('clang not found. Please make sure that clang is in your PATH.')
    annotator = 'Annotator/EventAnnotator'
    if not os.path.exists(annotator):
        sys.exit('EventAnnotator binary not found. Do a make to build the binaries.')
        return ''
    clang_dir = os.path.dirname(clang_path)
    shutil.copy2(annotator, clang_dir)
    os.chdir(clang_dir)
    print 'Instrumenting queueing APIs'
    subprocess.call(['./EventAnnotator', '-s', args.source,
                     '-f', args.queuefile, args.compiledb], env=envcopy)
    print 'Instrumentation done'


def read_call_graph(args):
    return []


def compute_heights(call_graph):
    return {}


def get_trx_func(args):
    return None


def get_roots(call_graph):
    return []


def instrument_target_function(func_name):
    """ Instrument the target function
    """
    pass


def main():
    """ Main function
    """
    args = parse_args()
    annotate(args)
    call_graph = read_call_graph(args)
    heights = compute_heights(call_graph)
    root_functions = get_roots(call_graph)


if __name__ == '__main__':
    main()
