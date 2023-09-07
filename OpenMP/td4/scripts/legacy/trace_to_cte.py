"""
"   Converts an MPI+OpenMP(tasks) trace to a Chrome Trace Engine (CTE) json trace,
"   to be read in chrome, at about:tracing
"""
import json
import getopt
import sys
from utils import *

def usage():
    print('usage: ' + sys.argv[0] + ' {-h|--help} [[-i|--input=]TRACE] [[-o|--output=]TRACE] [{-s|--schedule} {-c|--creation} {-r|--records}] {-d --dependencies} {-a --communications} {--color}')

def main():
    if len(sys.argv) < 2:
        usage()
        sys.exit(1)

    config = {
        'input': 'traces',
        'output': 'traces',
        'schedule': False,
        'creation': False,
        'records': False,
        'dependencies': False,
        'communications': False,
        'color': False
    }

    try:
        opts, args = getopt.getopt(sys.argv[1:], 'hi:o:scrdab', ['help', 'input=', 'output=', 'schedule', 'creation', 'records', 'dependencies', 'communications', 'color'])
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
        elif opt in ('-s', '--schedule'):
            config['schedule'] = True
        elif opt in ('-c', '--creation'):
            config['creation'] = True
        elif opt in ('-r', '--records'):
            config['records'] = True
        elif opt in ('-d', '--dependencies'):
            config['dependencies'] = True
        elif opt in ('-a', '--communications'):
            config['communications'] = True
        elif opt in ('-b', '--color'):
            config['color'] = True
        else:
            usage()
            sys.exit(1)

    if not config['schedule'] and not config['creation'] and not config['records']:
        usage();
        sys.exit(1)

    # parse traces
    src = config['input']
    (stats, cte, gg, g, records, _) = parse_traces(src, config=config)

    # write stat file
    dst = "{}-stats.json".format(config['output'])
    with open(dst, "w") as f:
        print("writing `{}` to disk...".format(dst))
        json.dump(stats, f, indent=4)
        f.write("\n")
        print("`{}` converted to `{}`".format(src, dst))

    # write cte json file
    dst = "{}.json".format(config['output'])
    with open(dst, "w") as f:
        print("writing `{}` to disk...".format(dst))
        json.dump(cte, f)
        f.write("\n")
        print("`{}` converted to `{}`".format(src, dst))

    # write records to file
    if config['records']:
        dst = "{}-records.txt".format(config['output'])
        with open(dst, "w") as f:
            print("writing `{}` to disk...".format(dst))
            for pid in records:
                for record in records[pid]:
                    print(record, file=f)
            print("`{}` converted to `{}`".format(src, dst))

if __name__ == "__main__":
    main()
