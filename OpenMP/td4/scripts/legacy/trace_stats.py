"""
"   usage: python3 trace_stats.py -h
"
"   This tool gather statistics of an hybrid MPI+OpenMP(tasks) trace
"""
import json
import getopt
import sys
from utils import *

def usage():
    print('usage: ' + sys.argv[0] + ' {-h|--help} [[-i|--input=]TRACE] {[-s|--stats=]STATS_FILE} {[-b|--blocked]BLOCKED_TASKS_FILE}')

def main():
    config = {
        'input': 'traces',
        'stats': None,
        'blocked': None
    }
    try:
        opts, args = getopt.getopt(sys.argv[1:], 'hi:s:b:', ['help', 'input=', 'stats', 'blocked'])
    except getopt.GetoptError:
        usage()
        sys.exit(1)

    for opt, arg in opts:
        if opt in ('-h', '--help'):
            usage()
            sys.exit(0)
        elif opt in ('-i', '--input'):
            config['input'] = arg
        elif opt in ('-s', '--stats'):
            config['stats'] = arg
        elif opt in ('-b', '--blocked'):
            config['blocked'] = arg
        else:
            usage()
            sys.exit(1)

    f = sys.stdout
    if config['stats'] is not None:
        f = open(config['stats'], 'w')

    (stats, _, _, _, _, blocked) = parse_traces(config['input'])

    if config['stats'] is not None:
        print("writing `{}` to disk...".format(config['stats']))

    json.dump(stats, f, indent=4)
    f.write("\n")

    if config['stats'] is not None:
        f.close()

    if config['blocked'] is not None:
        with open(config['blocked'], 'w') as f:
            f.write("pid time (un)blocked\n")
            for pid in blocked:
                f.write('\n'.join(["{} {} {}".format(pid, time, x) for (time, x) in blocked[pid]]))
                f.write('\n')

if __name__ == "__main__":
    main()


