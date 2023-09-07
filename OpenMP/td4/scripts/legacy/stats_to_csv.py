"""
"   usage: python3 trace_stats.py -h
"
" This tool convert a json statistic file to a csv format
"
" {'about': {'n-processes': 1, 'n-threads-total': 16}, 'graph': {'tasks': {'n': {'total': 22210, 'compute': 22203, 'communication': {'total': 7, 'allreduce': 7, 'send': 0, 'recv': 0}}, 'degrees': {'max': {'total': 7, 'in': 2, 'out': 6}, 'min': {'total': 0, 'in': 0, 'out': 0}, 'med': {'total': 0, 'in': 0, 'out': 0}, 'avg': {'total': 0.0, 'in': 0.0, 'out': 0.0}}}, 'arcs': {'total': 95, 'local': 95, 'remote': 0}, 'granularity (s.)': {'max': 0.000223, 'min': 0.0, 'med': 0.0, 'avg': 1e-06}}, 'time': {'flat (s.)': {'total': 1.695088, 'max-process-time': 0.105943, 'graph-gen': 0.105921, 'in-task': {'total': 0.012197, 'compute': 0.012124, 'communication': {'total': 7.3e-05, 'recv': 0.0, 'send': 0.0, 'allreduce': 7.3e-05}}, 'out-task': {'total': 1.682891, 'overhead': 1.197398, 'famine': {'total': 0.485493, 'blocked': 0.0, 'unblocked': 0.485493}}}, 'proportion (%)': {'in-task': {'total': 0.71955, 'compute': 0.715243, 'communication': {'total': 0.004307, 'recv': 0.0, 'send': 0.0, 'allreduce': 0.004307}}, 'out-task': {'total': 99.28045, 'overhead': 70.639282, 'famine': {'total': 28.641168, 'blocked': 0.0, 'unblocked': 28.641168}}}}}
"
"""
import json
import os.path
import sys
from utils import *

def main():
    keys=(
        ('graph', 'tasks', 'n', 'compute'),
        ('graph', 'tasks', 'n', 'communication', 'allreduce'),
        ('graph', 'tasks', 'n', 'communication', 'send'),
        ('graph', 'tasks', 'n', 'communication', 'recv'),
        ('graph', 'tasks', 'degrees', 'min', 'total'),
        ('graph', 'tasks', 'degrees', 'min', 'in'),
        ('graph', 'tasks', 'degrees', 'min', 'out'),
        ('graph', 'tasks', 'degrees', 'med', 'total'),
        ('graph', 'tasks', 'degrees', 'med', 'in'),
        ('graph', 'tasks', 'degrees', 'med', 'out'),
        ('graph', 'tasks', 'degrees', 'max', 'total'),
        ('graph', 'tasks', 'degrees', 'max', 'in'),
        ('graph', 'tasks', 'degrees', 'max', 'out'),
        ('graph', 'tasks', 'degrees', 'avg', 'total'),
        ('graph', 'tasks', 'degrees', 'avg', 'in'),
        ('graph', 'tasks', 'degrees', 'avg', 'out'),
        ('graph', 'arcs', 'local'),
        ('graph', 'arcs', 'remote'),
        ('graph', 'granularity (s.)', 'min'),
        ('graph', 'granularity (s.)', 'med'),
        ('graph', 'granularity (s.)', 'max'),
        ('graph', 'granularity (s.)', 'avg'),
        ('time', 'flat (s.)', 'max-process-time'),
        ('time', 'flat (s.)', 'graph-gen'),
        ('time', 'proportion (%)', 'in-task', 'compute'),
        ('time', 'proportion (%)', 'in-task', 'communication', 'allreduce'),
        ('time', 'proportion (%)', 'in-task', 'communication', 'send'),
        ('time', 'proportion (%)', 'in-task', 'communication', 'recv'),
        ('time', 'proportion (%)', 'out-task', 'overhead'),
        ('time', 'proportion (%)', 'out-task', 'famine', 'blocked'),
        ('time', 'proportion (%)', 'out-task', 'famine', 'unblocked'),
    )
    if len(sys.argv) == 2 and sys.argv[1] == '-l':
        labels=tuple(".".join(key) for key in keys)
        print(":".join(["{}"] * len(keys)).format(*labels))
        return
    elif len(sys.argv) != 3:
        print("error usage : " + sys.argv[0] + " [TRACE_JSON_SRC] [TRACE_CSV_DST] {-l} {-h}")
        return
    src = sys.argv[1]
    if not os.path.isfile(src):
        print("invalid file " + src)
        return
    dst = sys.argv[2]
    with open(src, 'r') as f:
        stats = json.load(f)
    with open(dst, 'w') as f:
        def retrieve(stats, key):
            for s in key:
                stats = stats[s]
            return stats
        attribs = tuple(retrieve(stats, key) for key in keys)
        f.write(":".join(["{}"] * len(attribs)).format(*attribs))
        f.write("\n")

if __name__ == "__main__":
    main()


