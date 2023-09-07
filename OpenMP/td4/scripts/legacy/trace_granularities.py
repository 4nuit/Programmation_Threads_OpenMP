"""
"   usage: python3 trace_granularities.py -h
"
"   This tool gather statistics of an hybrid MPI+OpenMP(tasks) trace
"""
import json
import sys
from utils import *

def main():
    if len(sys.argv) != 2:
        print("error usage : {} [TRACE_DIR_SRC]".format(sys.argv[0]))
        return
    src = sys.argv[1]
    (_, _, _, g, _) = parse_traces(sys.argv[1])
    for granularity in g:
        print(granularity)

if __name__ == "__main__":
    main()


