"""
"   usage: python3 main.py -h
"
"   This tool gather statistics of an hybrid MPI+OpenMP(tasks) trace
"""
import json
import getopt
import sys

from passes.cte.google_cte              import GoogleCtePass

from passes.communication_duration      import CommunicationDurationPass
from passes.communication_predecessors  import CommunicationPredecessorsPass
from passes.critical                    import CriticalPass
from passes.dump                        import DumpPass
from passes.tdg_dot                     import DotPass
from passes.tdg_stats                   import TDGStatsPass
from passes.overlap                     import OverlapPass
from passes.timing                      import TimingPass
from passes.workload                    import WorkloadPass

from passes.papi.accumulate             import AccumulatePAPIPass
from passes.papi.cache_hit              import CacheHitPAPIPass
from passes.papi.ins_cycle              import InsCyclePAPIPass

from scheduler import *

def usage():
    print('usage: ' + sys.argv[0] + ' {-h|--help} [[-i|--input=]TRACE] {[-o|--output=]OUTPUT} {-p|--progress}')

def main():
    config = {
        'input':  'traces',
        'output': 'traces',
        'progress': False,
    }
    try:
        opts, args = getopt.getopt(sys.argv[1:], 'hi:o:p', ['help', 'input=', 'output=', 'process'])
    except getopt.GetoptError:
        usage()
        sys.exit(1)

    for opt, arg in opts:
        if opt in ('-h', '--help'):
            usage()
            sys.exit(0)
        elif opt in ('-i', '--input'):
            config['input'] = arg
        elif opt in ('-o', '--output'):
            config['output'] = arg
        elif opt in ('-p', '--progress'):
            config['progress'] = True
        else:
            usage()
            sys.exit(1)

    # TODO : modify this: add a precedence constraint,
    # right now, this is done implicitely by the list order
    passes = [
        WorkloadPass(),
        #AccumulatePAPIPass(),
        #CacheHitPAPIPass(),
        #InsCyclePAPIPass(),
        #DumpPass(),
        #CommunicationDurationPass(),
        #OverlapPass(),
        #CommunicationPredecessorsPass(),
        TimingPass(),
        CriticalPass(),
        TDGStatsPass(),
        DotPass(),
        GoogleCtePass(),
    ]

    for inspector in passes:
        inspector.on_start(config)

    parse_traces(config['input'], config['progress'], passes)
    for inspector in passes:
        inspector.on_end(config)

if __name__ == "__main__":
    main()


