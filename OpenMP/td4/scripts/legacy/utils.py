import heapq
import json
import hashlib
import math
import os
import struct
import sys
import time
import queue

import catapult
import progress

# Number of digits to keep with times (in s.)
# Note that time are often measures with microsecond precision
FLOAT_PRECISION = 6

MAGIC         = "task"
HEADER_SIZE   = 16
BYTE_ORDER    = 'little' # or 'big' (endianess)

MPC_OMP_TASK_PROP = [
    'UNDEFERRED',
    'UNTIED',
    'EXPLICIT',
    'IMPLICIT',
    'INITIAL',
    'INCLUDED',
    'FINAL',
    'MERGED',
    'MERGEABLE',
    'DEPEND',
    'PRIORITY',
    'UP',
    'GRAINSIZE',
    'IF',
    'NOGROUP',
    'HAS_FIBER',
    'PERSISTENT',
    'CONTROL_FLOW'
]

MPC_OMP_TASK_STATUSES = [
    'STARTED',
    'COMPLETED',
    'BLOCKING',
    'BLOCKED',
    'UNBLOCKED',
    'IN_BLOCKED_LIST',
    'CANCELLED',
]

MPC_OMP_TASK_LABEL_MAX_LENGTH = 64

# record sizes from C's sizeof()
RECORD_GENERIC_SIZE         = 16
RECORD_DEPENDENCY_SIZE      = 24
RECORD_SCHEDULE_SIZE        = 72
RECORD_CREATE_SIZE          = 112
RECORD_DELETE_SIZE          = 32
RECORD_SEND_SIZE            = 48
RECORD_RECV_SIZE            = 48
RECORD_ALLREDUCE_SIZE       = 40
RECORD_RANK_SIZE            = 24
RECORD_BLOCKED_SIZE         = 24
RECORD_UNBLOCKED_SIZE       = 24

MPC_OMP_TASK_TRACE_TYPE_BEGIN           = 0
MPC_OMP_TASK_TRACE_TYPE_END             = 1
MPC_OMP_TASK_TRACE_TYPE_DEPENDENCY      = 2
MPC_OMP_TASK_TRACE_TYPE_SCHEDULE        = 3
MPC_OMP_TASK_TRACE_TYPE_CREATE          = 4
MPC_OMP_TASK_TRACE_TYPE_DELETE          = 5
MPC_OMP_TASK_TRACE_TYPE_SEND            = 6
MPC_OMP_TASK_TRACE_TYPE_RECV            = 7
MPC_OMP_TASK_TRACE_TYPE_ALLREDUCE       = 8
MPC_OMP_TASK_TRACE_TYPE_RANK            = 9
MPC_OMP_TASK_TRACE_TYPE_BLOCKED         = 10
MPC_OMP_TASK_TRACE_TYPE_UNBLOCKED       = 11
MPC_OMP_TASK_TRACE_TYPE_COUNT           = 12

""" Hash a string to a 16-digits integer """
def hash_string(string):
    return int(hashlib.sha1(string.encode("utf-8")).hexdigest(), 16) % (2**64)

def records_fix(records):

    print("Fixing records timing...")
    progress.init(2 * sum(len(records[pid]) for pid in records))

    for pid in records:

        records_other = {}
        records_create = {}
        records_dependencies = []
        uid_to_persistent_uid = {}
        persistent_uid_to_uids = {}

        # fill record tables
        for record in records[pid]:
            progress.update()

            # creating a new task
            if isinstance(record, RecordCreate):
                assert(record.uid not in records_create)
                records_create[record.uid] = record

                # if we are creating a persistent, track uids
                if record.persistent_uid != 0:
                    if record.persistent_uid not in persistent_uid_to_uids:
                        persistent_uid_to_uids[record.persistent_uid] = []
                    persistent_uid_to_uids[record.persistent_uid].append(record.uid)
                    uid_to_persistent_uid[record.uid] = {
                        'uid': record.persistent_uid,
                        'index': len(persistent_uid_to_uids[record.persistent_uid]) - 1
                    }

            elif isinstance(record, RecordDependency):
                records_dependencies.append(record)

        # if a successor task uid is less than a predecessor task,
        # it means they are both persistents, and that the predecessor completed before the successor
        # was reset. In such case, we have to fix the dependeny record 'in' uid
        for record in records_dependencies:
            if  record.in_uid < record.out_uid and                                                      \
                'PERSISTENT' in record_to_properties_and_statuses(records_create[record.out_uid]) and   \
                'PERSISTENT' in record_to_properties_and_statuses(records_create[record.in_uid]):
                persistent = uid_to_persistent_uid[record.in_uid]
                uids = persistent_uid_to_uids[persistent['uid']]
                uid = uids[persistent['index'] + 1]
                record.in_uid = uid

        # if another event happened before the task creation, shift the task creation back in time
        for record in records[pid]:
            progress.update()

            if isinstance(record, RecordCreate):
                if (record.uid in records_other) and (records_other[record.uid].time < record.time):
                    record.time = records_other[record.uid].time

            elif isinstance(record, RecordRank) or isinstance(record, RecordIgnore):
                pass

            # if another event for a task occures before its creation, save time
            else:
                uid = -1
                if isinstance(record, RecordDependency):
                    uid = record.in_uid
                elif any(isinstance(record, x) for x in (RecordDelete, RecordRecv, RecordSend, RecordSchedule, RecordAllreduce, RecordBlocked, RecordUnblocked)):
                    uid = record.uid
                else:
                    print(record)
                    assert(0)

                # find 1st event that happen for this task
                if (uid not in records_other) or (record.time < records_other[uid].time):
                    records_other[uid] = record
                    if (uid in records_create) and (record.time < records_create[uid].time):
                        records_create[uid].time = record.time

    progress.deinit()

    print("Sorting records...")

    # sort records
    def record_order(record):
        orders = {
            RecordRank:        -1,
            RecordIgnore:       0,
            RecordCreate:       0,
            RecordDependency:   1,
            RecordSchedule:     2,
            RecordBlocked:      3,
            RecordUnblocked:    4,
            RecordSend:         5,
            RecordRecv:         6,
            RecordAllreduce:    7,
            RecordDelete:       8,
        }
        schedule_id = 0
        if hasattr(record, 'schedule_id'):
            schedule_id = record.schedule_id
        return (record.time, orders[type(record)], schedule_id)

    progress.init(len(records))
    for pid in records:
        progress.update()
        records[pid] = sorted(records[pid], key=record_order)
    progress.deinit()

# convert process PID to MPI Rank
def pid_to_rank(processes, pid):
    comm = 0    # MPI_COMM_WORLD
    rank = pid
    if ('p2c2r' in processes['convert']) and (pid in processes['convert']['p2c2r']):
        if comm in processes['convert']['p2c2r'][pid]:
            rank = processes['convert']['p2c2r'][pid][comm]
    return rank

def parse_traces(traces, config={'schedule': False, 'creation': False, 'dependencies': False, 'communications': False}):

    # parse records (TODO : maybe this can be optimized)
    records = traces_to_records(traces)

    # fix records
    records_fix(records)

    # generate global graph and 'processes' array
    (processes, gg) = records_to_gg(records)

    # cte trace format
    cte = {
        'traceEvents': []
    }

    # stats dict
    stats = {}

    # number of threads (total)
    n_threads = 0

    # timers
    total_time      = 0
    intask_time     = 0
    outtask_time    = 0
    idle_time       = 0
    recv_time       = 0
    send_time       = 0
    allreduce_time  = 0

    asynchronous_total_time    = 1e-64
    asynchronous_overlap_time  = 1e-64
    asynchronous_idle_time     = 1e-64

    # granularity
    granularities = {}

    # parallelism infos
    readiness = {}

    # the process with the longest lifespan
    max_process_total_time = -math.inf

    # the timestamp for the 1st and last created task
    first_task_time = math.inf
    last_task_time  = 0

    # asynchronous task progression are tracked through task pause/resume mechanism
    # if a task is blocked, asynchronous[pid]['pending'][uid] is set
    # if a task blocked and unblocked, asynchronous[pid]['completed'][uid] is set
    asynchrony_per_pid = {}

    # blocked tasks over time
    blocked_tasks_over_time_per_pid = {}

    # replay the scheduling to compute stats
    for pid in records:

        print("Scheduling process {} of rank {}".format(pid, pid_to_rank(processes, pid)))

        # find first and last time a thread worked
        ti = records[pid][0].time
        tf = records[pid][-1].time

        # initialize asynchronous map
        asynchrony = {
            'pending': {},
            'completed': {},
        }
        asynchrony_per_pid[pid] = asynchrony

        # blocked tasks over time
        blocked_tasks_over_time = []
        blocked_tasks_over_time_per_pid[pid] = blocked_tasks_over_time

        # tasks[uid] => record : RecordCreate of task with given id
        tasks = {}

        # readyqueue[uid] => `True` : the task is ready for schedule
        readyqueue = {}
        readiness[pid] = {'avg': 0, 'max': 0, 'nschedules': 0}

        def update_readiness_counters():
            ntasks_ready = len(readyqueue) + 1
            readiness[pid]['max'] = max(readiness[pid]['max'], ntasks_ready)
            readiness[pid]['avg'] += ntasks_ready
            readiness[pid]['nschedules'] += 1

        # per-task schedules (x2, contains start, pause, resume, complete)
        schedules = {}

        # successors[uid] => [uid1, uid2, ..., uidn] : successors of 'uid'
        successors = {}

        # predecessors[uid] => n : number of remaining predecessors
        predecessors = {}

        # bind[tid] => [uid1, uid1, uid2, uid2, ..., uidn, uidn] : per-thread schedules
        bind = {}
        for record in records[pid]:
            if record.tid not in bind:
                bind[record.tid] = []

        progress.init(len(records[pid]))

        # for each record on this process
        for i in range(len(records[pid])):
            progress.update()
            record = records[pid][i]

            # creating a new task
            if isinstance(record, RecordCreate):
                tasks[record.uid] = {'create': record, 'delete': None}
                if record.uid not in predecessors:
                    predecessors[record.uid] = 0
                predecessors[record.uid] += record.npredecessors
                properties = record_to_properties_and_statuses(record)
                if (predecessors[record.uid] == 0) and ('INITIAL' not in properties):
                    readyqueue[record.uid] = True
                first_task_time = min(first_task_time, record.time)
                last_task_time  = max(last_task_time, record.time)

            # deleting a task
            elif isinstance(record, RecordDelete):
                assert(record.uid in tasks)
                properties = record_to_properties_and_statuses(record)

                # task got cancelled
                if 'CANCELLED' in properties:
                    if record.uid in readyqueue:
                        del readyqueue[record.uid]
                    if record.uid in asynchrony['pending']:
                        del asynchrony['pending'][record.uid]
                tasks[record.uid]['delete'] = record

            # dependency completion
            elif isinstance(record, RecordDependency):
                assert(record.in_uid in tasks)
                assert(record.out_uid in tasks)
                assert(record.in_uid not in readyqueue)

                if record.in_uid not in predecessors:
                    predecessors[record.in_uid] = 0
                predecessors[record.in_uid] -= 1

                if predecessors[record.in_uid] == 0:
                    readyqueue[record.in_uid] = True

                # add successors
                if record.out_uid not in successors:
                    successors[record.out_uid] = []
                successors[record.out_uid].append(record.in_uid)

            # TODO : stacking multiple tasks on the same thread is likely unsupported
            # task scheduling
            elif isinstance(record, RecordSchedule):
                # append schedule time to the granularity tracker list
                uuid = task_uuid(pid, record.uid)
                if uuid not in granularities:
                    granularities[uuid] = []
                granularities[uuid].append(record.time)

                # a task was just paused or completed, increment the overlap accumulators
                def increment_asynchronous_overlap():
                    for uid in asynchrony['pending']:
                        assert(len(schedules[record.uid]) % 2 == 1)
                        asynchrony['pending'][uid]['time']['overlap'] += record.time - max(asynchrony['pending'][uid]['blocked'].time, schedules[record.uid][-1].time)

                def increment_asynchronous_idle(duration):
                    for uid in asynchrony['pending']:
                        asynchrony['pending'][uid]['time']['idle'] += duration

                # update various lists depending on the schedule details
                properties = record_to_properties_and_statuses(record)

                # a task completed
                if 'COMPLETED' in properties:
                    assert(len(bind[record.tid]) > 0)
                    #assert(record.uid == bind[record.tid][-1].uid)
                    del bind[record.tid][-1]
                    increment_asynchronous_overlap()

                # a task unblocked and resumed
                elif 'UNBLOCKED' in properties:
                    update_readiness_counters()

                    # add it to the core binding list
                    bind[record.tid].append(record)

                    # a task may unblocked before fully blocking,
                    # in such case, no pause/resume happened
                    if record.uid in asynchrony['pending']:
                        if record.uid not in asynchrony['completed']:
                            asynchrony['completed'][record.uid] = []
                        asynchrony['pending'][record.uid]['unblocked'] = record
                        asynchrony['completed'][record.uid].append(asynchrony['pending'][record.uid])
                        del asynchrony['pending'][record.uid]
                        blocked_tasks_over_time.append((record.time, -1))

                # a task blocked and paused
                elif 'BLOCKING' in properties:
                    assert(record.uid == bind[record.tid][-1].uid)
                    increment_asynchronous_overlap()
                    del bind[record.tid][-1]
                    assert(record.uid not in asynchrony['pending'])
                    asynchrony['pending'][record.uid] = {
                        'blocked':      record,
                        'unblocked':    None,
                        'time': {
                            'overlap':  0.0,
                            'idle':     0.0,
                        }
                    }
                    blocked_tasks_over_time.append((record.time, 1))

                # a task started or resumed
                elif 'BLOCKED' not in properties:
                    assert(record.uid not in predecessors or predecessors[record.uid] == 0)
                    if record.uid in readyqueue:    # on 1st run
                        del readyqueue[record.uid]
                        update_readiness_counters()
                    bind[record.tid].append(record)

                # register task scheduling
                if record.uid not in schedules:
                    schedules[record.uid] = []
                schedules[record.uid].append(record)

                # if there is no other tasks ready
                if len(readyqueue) == 0:

                    # and if we just ended or paused a task
                    if ('COMPLETED' in properties) or (('BLOCKING' in properties) and ('UNBLOCKED' not in properties)):
                        # we are entering an idle period : find its duration
                        nrecord = None
                        for j in range(i + 1, len(records[pid])):
                            nrecord = records[pid][j]
                            if isinstance(nrecord, RecordSchedule):
                                if nrecord.tid == record.tid:
                                    break
                            nrecord = None
                        if nrecord is None:
                            duration = tf - record.time
                        else:
                            duration = nrecord.time - record.time
                        idle_time += duration
                        # TODO : increment idle time with the correct amount
                        increment_asynchronous_idle(duration)

            # mark send tasks
            elif isinstance(record, RecordSend):
                if record.uid in tasks:
                    tasks[record.uid]['create'].send = True

            # mark recv tasks
            elif isinstance(record, RecordRecv):
                if record.uid in tasks:
                    tasks[record.uid]['create'].recv = True

            # mark allreduce tasks
            elif isinstance(record, RecordAllreduce):
                if record.uid in tasks:
                    tasks[record.uid]['create'].allreduce = True


        progress.deinit()

        # CHROME TRACE ENGINE TRACE GENERATION
        # AND STATS COMPUTATION FOR THIS PROCESS

        # compute in-task and out-task time
        process_total_time      = (tf - ti) * len(bind)
        process_intask_time     = 0
        process_send_time       = 0
        process_recv_time       = 0
        process_allreduce_time  = 0
        process_outtask_time    = 0
        max_process_total_time  = max(max_process_total_time, tf - ti)
        graph_gen_time          = 0

        # number of recv, send and allreduce tasks
        nsend = 0
        nrecv = 0
        nallreduce = 0

        # for each tasks scheduled
        for uid in schedules:
            lst = schedules[uid]
            assert(len(lst) % 2 == 0)

            task = tasks[uid]['create']

            # count communications
            if task.send:
                nsend += 1
            elif task.recv:
                nrecv += 1
            elif task.allreduce:
                nallreduce += 1

            # trace schedules
            for i in range(0, len(lst), 2):
                r1 = lst[i + 0]
                r2 = lst[i + 1]
                assert(r1.tid == r2.tid)
                assert(r1.time <= r2.time)

                # statistics in-task time
                dt = r2.time - r1.time
                process_intask_time += dt
                if task.send:
                    process_send_time += dt
                elif task.recv:
                    process_recv_time += dt
                elif task.allreduce:
                    process_allreduce_time += dt

                # cte : task schedule
                if config['schedule'] and not task.label.startswith("EMPTY"):
                    event = {
                        'name': task.label,
                        'cat':  'task-schedule',
                        'ph':   'X',
                        'ts':   r1.time,
                        'dur':  r2.time - r1.time,
                        #'dur':  max(r2.time - r1.time, 1),
                        'pid':  pid_to_rank(processes, pid),
                        'tid':  r1.tid,
                        'args': {
                            'uid': uid,
                            'ts': r1.time,
                            'created': task.time,
                            'priority': r1.priority,
                            'properties_begin': record_to_properties_and_statuses(r1),
                            'properties_end': record_to_properties_and_statuses(r2),
                            'color': task.color,
                            'hwcounters_begin': r1.hwcounters,
                            'hwcounters_end': r2.hwcounters,
                       }
                    }
                    if config['color']:
                        event['cname'] = catapult.colors[task.color % len(catapult.colors)]
                    if task.recv or task.send or task.allreduce:
                        event['args']['mpi'] = []
                        if task.recv:
                            event['args']['mpi'].append('recv')
                        if task.send:
                            event['args']['mpi'].append('send')
                        if task.allreduce:
                            event['args']['mpi'].append('allreduce')
                    cte['traceEvents'].append(event)

            # task block-resume events
            if len(lst) > 2:
                for i in range(0, len(lst) - 2, 2):
                    r0 = lst[i + 0]
                    r1 = lst[i + 1]
                    r2 = lst[i + 2]
                    r3 = lst[i + 3]
                    p1 = record_to_properties_and_statuses(r1)
                    p2 = record_to_properties_and_statuses(r2)
                    if ('BLOCKING' in p1) and ('UNBLOCKED' in p2):
                        cat = 'block-resume'
                    else:
                        cat = 'yield'
                    identifier = "{}-{}-{}-{}-{}".format(cat, pid, r1.tid, r2.tid, uid)
                    event = {
                        'name' : identifier,
                        'cat': cat,
                        'ph': 's',
                        'ts': r1.time - min(0.01, 0.01 * (r1.time - r0.time)),
                        'pid': pid_to_rank(processes, pid),
                        'tid': r1.tid,
                        'id': hash_string(identifier)
                    }
                    cte['traceEvents'].append(event)
                    event = {
                        'name' : identifier,
                        'cat': cat,
                        'ph': 't',
                        'ts': r2.time + min(0.01, 0.01 * (r3.time - r2.time)),
                        'pid': pid_to_rank(processes, pid),
                        'tid': r2.tid,
                        'id': hash_string(identifier)
                    }
                    cte['traceEvents'].append(event)

        # 'uid' in 'schedules' ends here

        # Print task creation and control dependency
        if config['creation']:
            for uid in tasks:
                task = tasks[uid]['create']
                event = {
                    'name': task.label,
                    'cat':  'task-create',
                    'ph':   'X',
                    'ts':   task.time,
                    'dur':  1,
                    'pid':  pid_to_rank(processes, task.pid),
                    'tid':  task.tid,
                    'args': {
                        'uid': task.uid,
                        'ts': task.time,
                        'priority': task.priority,
                        'properties': record_to_properties_and_statuses(task)
                   }
                }
                cte['traceEvents'].append(event)

                if config['dependencies']:
                    if task.parent_uid in tasks:
                        # creation
                        parent = tasks[task.parent_uid]['create']
                        cat = 'task-create-control'
                        identifier = "{}-{}-{}".format(cat, task.uid, parent.uid)
                        event = {
                            'name' : identifier,
                            'cat': cat,
                            'ph': 's',
                            'ts': parent.time + 0.5,
                            'pid': pid_to_rank(processes, parent.pid),
                            'tid': parent.tid,
                            'id': hash_string(identifier)
                        }
                        cte['traceEvents'].append(event)
                        event = {
                            'name' : identifier,
                            'cat': cat,
                            'ph': 't',
                            'ts': task.time + 0.5,
                            'pid': pid_to_rank(processes, task.pid),
                            'tid': task.tid,
                            'id': hash_string(identifier)
                        }
                        cte['traceEvents'].append(event)

                # task deletion
                delete = tasks[uid]['delete']
                if delete:
                    event = {
                        'name': tasks[uid]['create'].label,
                        'cat':  'task-delete',
                        'ph':   'X',
                        'ts':   delete.time,
                        'dur':  1,
                        'pid':  pid_to_rank(processes, delete.pid),
                        'tid':  delete.tid,
                        'args': {
                            'uid': delete.uid,
                            'ts': delete.time,
                            'priority': delete.priority,
                            'properties': record_to_properties_and_statuses(delete)
                       }
                    }
                    cte['traceEvents'].append(event)

                    if config['dependencies']:
                        if task.parent_uid in tasks:
                            parent = tasks[task.parent_uid]['delete']
                            cat = 'task-delete-control'
                            identifier = "{}-{}-{}".format(cat, task.uid, parent.uid)
                            event = {
                                'name' : identifier,
                                'cat': cat,
                                'ph': 's',
                                'ts': delete.time + 0.5,
                                'pid': pid_to_rank(processes, delete.pid),
                                'tid': delete.tid,
                                'id': hash_string(identifier)
                            }
                            cte['traceEvents'].append(event)

                            event = {
                                'name' : identifier,
                                'cat': cat,
                                'ph': 't',
                                'ts': parent.time + 0.5,
                                'pid': pid_to_rank(processes, parent.pid),
                                'tid': parent.tid,
                                'id': hash_string(identifier)
                            }
                            cte['traceEvents'].append(event)

        # cte dependencies
        if config['schedule'] and config['dependencies']:
            for uid1 in successors:
                lst = successors[uid1]
                for uid2 in lst:
                    r1 = schedules[uid1][-1]
                    r2 = schedules[uid2][0] # TODO : this may fail is task with 'uid2' was cancelled
                    identifier = "dependency-{}-{}-{}-{}-{}".format(pid, r1.tid, r2.tid, uid1, uid2)
                    event = {
                        'name' : identifier,
                        'cat': 'dependencies',
                        'ph': 's',
                        'ts': r1.time,
                        'pid': pid_to_rank(processes, pid),
                        'tid': r1.tid,
                        'id': hash_string(identifier)
                    }
                    cte['traceEvents'].append(event)

                    event = {
                        'name' : identifier,
                        'cat': 'dependencies',
                        'ph': 't',
                        'ts': r2.time + 0.000001,
                        'pid': pid_to_rank(processes, pid),
                        'tid': r2.tid,
                        'id': hash_string(identifier)
                    }
                    cte['traceEvents'].append(event)

        # cte thread
        if config['schedule'] or config['creation']:
            for tid in bind:
                cte["traceEvents"].append({
                    'name': "thread_name",
                    'ph': "M",
                    'pid': pid_to_rank(processes, pid),
                    'tid': tid,
                    'args': {
                        'name': "omp thread {}".format(tid)
                    }
                })

        # STATS COMPUTATION

        # time spent outside tasks
        process_outtask_time = process_total_time - process_intask_time

        # append to process timers to global ones
        total_time      += process_total_time
        intask_time     += process_intask_time
        send_time       += process_send_time
        recv_time       += process_recv_time
        allreduce_time  += process_allreduce_time
        outtask_time    += process_outtask_time

        # increment number of thread counter
        n_threads += len(bind)

        # reduce asynchronous timers
        process_asynchronous_total_time = 0.0
        for uid in asynchrony['completed']:
            runs = asynchrony['completed'][uid]
            for run in runs:
                process_asynchronous_total_time += run['unblocked'].time - run['blocked'].time
                asynchronous_overlap_time       += run['time']['overlap']
                asynchronous_idle_time          += run['time']['idle']
        process_asynchronous_total_time *= len(bind)
        asynchronous_total_time += process_asynchronous_total_time

        # coherency check
        for uid in predecessors:
           assert(predecessors[uid] == 0)
        assert(len(readyqueue) == 0)
        assert(len(asynchrony['pending']) == 0)

    # 'pid' loop ends here

    asynchronous_overhead_time = asynchronous_total_time - asynchronous_overlap_time - asynchronous_idle_time

    assert(all(len(granularities[uuid]) % 2 == 0 for uuid in granularities))

    # compute granularities
    # TODO : add a filter functionality, doing it by hand right now
    # add task name to the banlist so their granularity is ignored
    # from the granularity metrics
    g = []
    banlist=[
        #"GPU",
        #"CalcKinematicsForElems",
        #"IntegrateStressForElems1"
    ]
    for uuid in granularities:
        node = gg.nodes[uuid]
        if any([banword in node.label for banword in banlist]):
            continue
        grain = sum(granularities[uuid][i+1] - granularities[uuid][i] for i in range(0, len(granularities[uuid]) - 1, 2))
        g.append(grain)
    g = sorted(g)

    # compute granularities by name
    g_by_label = {}
    for uuid in granularities:
        node = gg.nodes[uuid]
        if node.label not in g_by_label:
            g_by_label[node.label] = []
        grain = sum(granularities[uuid][i+1] - granularities[uuid][i] for i in range(0, len(granularities[uuid]) - 1, 2))
        g_by_label[node.label].append(grain)
    for label in g_by_label:
        g_by_label[label] = sorted(g_by_label[label])
   # for label in g_by_label:
   #     grains = g_by_label[label]
   #     print("{} grain boundaries ({}, {}, {})".format(label, grains[0], grains[len(grains) // 2], grains[-1]))

    stats['about'] = {
        'n-processes': len(records),
        'n-threads-total': n_threads
    }

    # graph stats
    narcs = sum(len(node.successors) for node in gg.nodes.values())
    narcs_remote = 0

    # graph nodes degree stats
    degrees = {
        'max': {
            'total': 0,
            'in': 0,
            'out': 0
        },
        'min': {
            'total': math.inf,
            'in': math.inf,
            'out': math.inf
        },
        'med': {
            'total': [],
            'in': [],
            'out': []
        },
        'avg': {
            'total': 0,
            'in': 0,
            'out': 0
        }
    }

    for pid in gg.graphs:
        graph = gg.graphs[pid]
        for uid in graph.nodes:
            node = graph.nodes[uid]
            in_degree = len(node.predecessors)
            out_degree = len(node.successors)
            degree = in_degree + out_degree

            degrees['max']['in']    = max(degrees['max']['in'],    in_degree)
            degrees['max']['out']   = max(degrees['max']['out'],   out_degree)
            degrees['max']['total'] = max(degrees['max']['total'], degree)

            degrees['min']['in']    = min(degrees['min']['in'],    in_degree)
            degrees['min']['out']   = min(degrees['min']['out'],   out_degree)
            degrees['min']['total'] = min(degrees['min']['total'], degree)

            degrees['med']['in'].append(in_degree)
            degrees['med']['out'].append(out_degree)
            degrees['med']['total'].append(degree)

    degrees['avg']['in']    = round(sum(degrees['med']['in'])    / len(degrees['med']['in']), 1)
    degrees['avg']['out']   = round(sum(degrees['med']['out'])   / len(degrees['med']['out']), 1)
    degrees['avg']['total'] = round(sum(degrees['med']['total']) / len(degrees['med']['total']), 1)

    degrees['med']['in']    = degrees['med']['in'][len(degrees['med']['in']) // 2]
    degrees['med']['out']   = degrees['med']['out'][len(degrees['med']['out']) // 2]
    degrees['med']['total'] = degrees['med']['total'][len(degrees['med']['total']) // 2]

    # add communication event
    c = gg.communications
    for comm in c:
        for src in c[comm]:
            for dst in c[comm][src]:
                for count in c[comm][src][dst]:
                    for dtype in c[comm][src][dst][count]:
                        for tag in c[comm][src][dst][count][dtype]:
                            x = c[comm][src][dst][count][dtype][tag]
                            narcs_remote += len(x['send'])
                            # length may be different if a task was cancelled
                            # assert(len(x['send']) == len(x['recv']))
                            end = min(len(x['send']), len(x['recv']))
                            for i in range(end):
                                send = x['send'][i]
                                recv = x['recv'][i]
                                if config['schedule'] and config['communications']:
                                    identifier = "communication-{}-{}-{}-{}-{}-{}-{}-{}".format(send.comm, send.pid, send.dst, send.count, send.dtype, send.tag, send.uid, recv.uid)
                                    event = {
                                        'name' : identifier,
                                        'cat': 'communication',
                                        'ph': 's',
                                        'ts':  send.time,
                                        'pid': pid_to_rank(processes, send.pid),
                                        'tid': send.tid,
                                        'id': hash_string(identifier)
                                    }
                                    cte['traceEvents'].append(event)

                                    sched = gg.graphs[recv.pid].nodes[recv.uid].last_sched
                                    event = {
                                        'name' : identifier,
                                        'cat': 'communication',
                                        'ph': 't',
                                        'ts': sched.time,
                                        'pid': pid_to_rank(processes, sched.pid),
                                        'tid': sched.tid,
                                        'id': hash_string(identifier)
                                    }
                                    cte['traceEvents'].append(event)

    for pid in readiness:
        readiness[pid]['avg'] = round(readiness[pid]['avg'] / readiness[pid]['nschedules'], 1)

    stats['records'] = {
        'total': sum(len(records[pid]) for pid in records),
        'parallelism': readiness
    },

    stats['graph'] = {
        'tasks': {
            'n': {
                'total': len(gg.nodes),
                'compute': len(gg.nodes) - nsend - nrecv - nallreduce,
                'communication': {
                    'total': nsend + nrecv + nallreduce,
                    'allreduce': nallreduce,
                    'send': nsend,
                    'recv': nrecv
                },
            },
            'degrees': degrees
        },
        'arcs': {
            'total': narcs,
            'local': narcs - narcs_remote,
            'remote': narcs_remote
        },
        'granularity (s.)': {
            'max': round(1e-6 * g[-1],           FLOAT_PRECISION),
            'min': round(1e-6 * g[0],            FLOAT_PRECISION),
            'med': round(1e-6 * g[len(g) // 2],  FLOAT_PRECISION),
            'avg': round(1e-6 * sum(g) / len(g), FLOAT_PRECISION)
        }
    }

    stats['time'] = {
        'flat (s.)': {
            'total': round(1e-6 * total_time, FLOAT_PRECISION),
            'max-process-time': round(1e-6 * max_process_total_time, FLOAT_PRECISION),
            'graph-gen': round(1e-6 * (last_task_time - first_task_time), FLOAT_PRECISION),
            'in-task': {
                'total': round(1e-6 * intask_time, FLOAT_PRECISION),
                'compute': round(1e-6 * (intask_time - recv_time - send_time - allreduce_time), FLOAT_PRECISION),
                'communication': {
                    'total': round(1e-6 * (recv_time + send_time + allreduce_time), FLOAT_PRECISION),
                    'recv':         round(1e-6 * recv_time, FLOAT_PRECISION),
                    'send':         round(1e-6 * send_time, FLOAT_PRECISION),
                    'allreduce':    round(1e-6 * allreduce_time, FLOAT_PRECISION),
                },
            },
            'out-task': {
                'total':    round(1e-6 * outtask_time,                 FLOAT_PRECISION),
                'overhead': round(1e-6 * (outtask_time - idle_time),   FLOAT_PRECISION),
                'idle':     round(1e-6 * idle_time,                    FLOAT_PRECISION),
            },
            'asynchronous': {
                'all': {
                    'total':    round(1e-6 * asynchronous_total_time,      FLOAT_PRECISION),
                    'overlap':  round(1e-6 * asynchronous_overlap_time,    FLOAT_PRECISION),
                    'idle':     round(1e-6 * asynchronous_idle_time,       FLOAT_PRECISION),
                    'overhead': round(1e-6 * asynchronous_overhead_time,   FLOAT_PRECISION),
                },
                'recv': {},
                'send': {},
            },
        },
        'proportion (%)': {
            'in-task': {
                'total':    round(intask_time / total_time * 100.0, FLOAT_PRECISION),
                'compute':  round((intask_time - recv_time - send_time - allreduce_time) / total_time * 100.0, FLOAT_PRECISION),
                'communication': {
                    'total': round((recv_time + send_time + allreduce_time) / total_time * 100.0, FLOAT_PRECISION),
                    'recv':         round(recv_time / total_time * 100.0, FLOAT_PRECISION),
                    'send':         round(send_time / total_time * 100.0, FLOAT_PRECISION),
                    'allreduce':    round(allreduce_time / total_time * 100.0, FLOAT_PRECISION)
                },
            },
            'out-task': {
                'total':    round(outtask_time / total_time * 100.0, FLOAT_PRECISION),
                'overhead': round((outtask_time - idle_time) / total_time * 100.0, FLOAT_PRECISION),
                'idle':     round(idle_time / total_time * 100.0, FLOAT_PRECISION),
            },
            'asynchronous': {
                'all': {
                    'overlap':  round(asynchronous_overlap_time / asynchronous_total_time * 100.0,  FLOAT_PRECISION),
                    'idle':     round(asynchronous_idle_time / asynchronous_total_time * 100.0,     FLOAT_PRECISION),
                    'overhead': round(asynchronous_overhead_time / asynchronous_total_time * 100.0, FLOAT_PRECISION),
                },
                'recv': {},
                'send': {},
            }
        }
    }

    return (stats, cte, gg, g, records, blocked_tasks_over_time_per_pid)


class Header:

    def __init__(self, buff):
        # see omptg_record.h (omptg_trace_file_header_t)
        self.magic   = buff[0:4].decode("ascii")
        self.version = int.from_bytes(buff[4:8],    byteorder=BYTE_ORDER, signed=False)
        self.pid     = int.from_bytes(buff[8:12],   byteorder=BYTE_ORDER, signed=False)
        self.tid     = int.from_bytes(buff[12:16],  byteorder=BYTE_ORDER, signed=False)
        assert(self.magic == MAGIC)

    def __repr__(self):
        return str(self)

    def __str__(self):
        return "Header(magic={}, version={}, pid={}, tid={})".format(self.magic, self.version, self.pid, self.tid)

def ensure_task(tasks, uid):
    if uid not in tasks:
        tasks[uid] = {
            'create': None,
            'delete': None,
            'schedules':    [],
            'successors':   [],
            'predecessors': [],
            'sends':        [],
            'recvs':        [],
            'allreduces':   []
        }

# see mpcomp_task_trace.h
class Record:

    def __init__(self, pid, tid, t, typ, f):
        self.pid = pid
        self.tid = tid
        self.time = t
        self.typ  = typ
        self.parse(f.read(self.size() - RECORD_GENERIC_SIZE))

    def parse(self, f):
        print("`{}` should implement `parse` method".format(type(self).__name__), file=stderr)
        assert(0)

    def attributes(self):
        print("`{}` should implement `attributes` method".format(type(self).__name__), file=stderr)
        assert(0)

    def size(self):
        print("`{}` should implement `size` method".format(type(self).__name__), file=stderr)
        assert(0)

    def __repr__(self):
        return str(self)

    def __str__(self):
        return "{}(pid={}, tid={}, time={}, {})".format(type(self).__name__, self.pid, self.tid, self.time, self.attributes())

class RecordIgnore(Record):

    def parse(self, f):
        pass

    def attributes(self):
        return "time={}".format(self.time)

    def store(self, processes, pid):
        pass

    def size(self):
        return RECORD_GENERIC_SIZE

class RecordDependency(Record):

    def parse(self, buff):
        self.out_uid    = int.from_bytes(buff[0:4], byteorder=BYTE_ORDER, signed=False)
        self.in_uid     = int.from_bytes(buff[4:8], byteorder=BYTE_ORDER, signed=False)

    def attributes(self):
        return "out_uid={}, in_uid={}".format(self.out_uid, self.in_uid)

    def store(self, processes, pid):
        tasks = processes['by-pids'][pid]['tasks']
        ensure_task(tasks, self.in_uid)
        ensure_task(tasks, self.out_uid)
        tasks[self.in_uid]['predecessors'].append(self.out_uid)
        tasks[self.out_uid]['successors'].append(self.in_uid)

    def size(self):
        return RECORD_DEPENDENCY_SIZE


class RecordSchedule(Record):

    def parse(self, buff):
        self.uid                = int.from_bytes(buff[0:4],     byteorder=BYTE_ORDER, signed=False)
        self.priority           = int.from_bytes(buff[4:8],     byteorder=BYTE_ORDER, signed=False)
        self.properties         = int.from_bytes(buff[8:12],    byteorder=BYTE_ORDER, signed=False)
        self.schedule_id        = int.from_bytes(buff[12:16],   byteorder=BYTE_ORDER, signed=False)
        self.statuses           = int.from_bytes(buff[16:20],   byteorder=BYTE_ORDER, signed=False)
        self.hwcounters = []
        self.hwcounters.append(   int.from_bytes(buff[24:32],   byteorder=BYTE_ORDER, signed=False))
        self.hwcounters.append(   int.from_bytes(buff[32:40],   byteorder=BYTE_ORDER, signed=False))
        self.hwcounters.append(   int.from_bytes(buff[40:48],   byteorder=BYTE_ORDER, signed=False))
        self.hwcounters.append(   int.from_bytes(buff[48:56],   byteorder=BYTE_ORDER, signed=False))

    def attributes(self):
        return "uid={}, priority={}, properties/statuses={}, schedule_id={}, hwcounters={}".format(
            self.uid,
            self.priority,
            record_to_properties_and_statuses(self),
            self.schedule_id,
            self.hwcounters
        )

    def store(self, processes, pid):
        tasks = processes['by-pids'][pid]['tasks']
        ensure_task(tasks, self.uid)
        tasks[self.uid]['schedules'].append(self)

    def size(self):
        return RECORD_SCHEDULE_SIZE

class RecordCreate(Record):

    def parse(self, buff):
        self.send = False
        self.recv = False
        self.allreduce = False

        self.uid                = int.from_bytes(buff[0:4],     byteorder=BYTE_ORDER, signed=False)
        self.persistent_uid     = int.from_bytes(buff[4:8],     byteorder=BYTE_ORDER, signed=False)
        self.properties         = int.from_bytes(buff[8:12],    byteorder=BYTE_ORDER, signed=False)
        self.statuses           = int.from_bytes(buff[12:16],   byteorder=BYTE_ORDER, signed=False)

        self.npredecessors      = 0

        x = 16
        y = MPC_OMP_TASK_LABEL_MAX_LENGTH;
        self.label              = buff[x:x+y].decode("ascii").replace('\0', '')
        self.color              = int.from_bytes(buff[x+y+0:x+y+4],   byteorder=BYTE_ORDER, signed=False)
        self.parent_uid         = int.from_bytes(buff[x+y+4:x+y+8],   byteorder=BYTE_ORDER, signed=False)
        self.omp_priority       = int.from_bytes(buff[x+y+8:x+y+12],  byteorder=BYTE_ORDER, signed=False)

    def attributes(self):
        return "uid={}, persistent_uid={}, properties/statuses={}, npredecessors={}, label={}, parent_uid={}, omp_priority={}".format(
            self.uid,
            self.persistent_uid,
            record_to_properties_and_statuses(self),
            self.npredecessors,
            self.label,
            self.parent_uid,
            self.omp_priority)

    def store(self, processes, pid):
        tasks = processes['by-pids'][pid]['tasks']
        ensure_task(tasks, self.uid)
        tasks[self.uid]['create'] = self

    def size(self):
        return RECORD_CREATE_SIZE

class RecordDelete(Record):

    def parse(self, buff):
        self.uid            = int.from_bytes(buff[0:4],     byteorder=BYTE_ORDER, signed=False)
        self.priority       = int.from_bytes(buff[4:8],     byteorder=BYTE_ORDER, signed=False)
        self.properties     = int.from_bytes(buff[8:12],    byteorder=BYTE_ORDER, signed=False)
        self.statuses       = int.from_bytes(buff[12:16],   byteorder=BYTE_ORDER, signed=False)

    def attributes(self):
        return "uid={}, priority={}, properties={}".format(
            self.uid, self.priority, record_to_properties_and_statuses(self))

    def store(self, processes, pid):
        tasks = processes['by-pids'][pid]['tasks']
        ensure_task(tasks, self.uid)
        tasks[self.uid]['delete'] = self

    def size(self):
        return RECORD_DELETE_SIZE

class RecordCallback(Record):

    def parse(self, buff):
        self.status = int.from_bytes(buff[0:4], byteorder=BYTE_ORDER, signed=False)
        self.when   = int.from_bytes(buff[4:8], byteorder=BYTE_ORDER, signed=False)

    def attributes(self):
        return "status={}, when={}".format(self.status, self.when)

    def store(self, processes, pid):
        tasks = processes['by-pids'][pid]['tasks']
        pass

    def size(self):
        return RECORD_CALLBACK_SIZE

class RecordSend(Record):

    def parse(self, buff):
        self.uid        = int.from_bytes(buff[0:4],   byteorder=BYTE_ORDER, signed=False)
        self.count      = int.from_bytes(buff[4:8],   byteorder=BYTE_ORDER, signed=False)
        self.dtype      = int.from_bytes(buff[8:12],  byteorder=BYTE_ORDER, signed=False)
        self.dst        = int.from_bytes(buff[12:16], byteorder=BYTE_ORDER, signed=False)
        self.tag        = int.from_bytes(buff[16:20], byteorder=BYTE_ORDER, signed=False)
        self.comm       = int.from_bytes(buff[20:24], byteorder=BYTE_ORDER, signed=False)
        self.completed  = int.from_bytes(buff[24:28], byteorder=BYTE_ORDER, signed=False)

    def attributes(self):
        return "uid={}, count={}, dtype={}, dst={}, tag={}, comm={}, completed={}".format(self.uid, self.count, self.dtype, self.dst, self.tag, self.comm, self.completed)

    def store(self, processes, pid):
        tasks = processes['by-pids'][pid]['tasks']
        #assert(self.uid in tasks)
        if self.uid in tasks:
            tasks[self.uid]['sends'].append(self)
        else:
            pass # Isend is not within an explicit task

    def size(self):
        return RECORD_SEND_SIZE

class RecordRecv(Record):

    def parse(self, buff):
        self.uid        = int.from_bytes(buff[0:4],   byteorder=BYTE_ORDER, signed=False)
        self.count      = int.from_bytes(buff[4:8],   byteorder=BYTE_ORDER, signed=False)
        self.dtype      = int.from_bytes(buff[8:12],  byteorder=BYTE_ORDER, signed=False)
        self.src        = int.from_bytes(buff[12:16], byteorder=BYTE_ORDER, signed=False)
        self.tag        = int.from_bytes(buff[16:20], byteorder=BYTE_ORDER, signed=False)
        self.comm       = int.from_bytes(buff[20:24], byteorder=BYTE_ORDER, signed=False)
        self.completed  = int.from_bytes(buff[24:28], byteorder=BYTE_ORDER, signed=False)

    def attributes(self):
        return "uid={}, count={}, dtype={}, src={}, tag={}, comm={}, completed={}".format(self.uid, self.count, self.dtype, self.src, self.tag, self.comm, self.completed)

    def store(self, processes, pid):
        tasks = processes['by-pids'][pid]['tasks']
        #assert(self.uid in tasks)
        if self.uid in tasks:
            tasks[self.uid]['recvs'].append(self)
        else:
            pass # Irecv is not within an explicit task

    def size(self):
        return RECORD_SEND_SIZE

class RecordAllreduce(Record):

    def parse(self, buff):
        self.uid        = int.from_bytes(buff[0:4],   byteorder=BYTE_ORDER, signed=False)
        self.count      = int.from_bytes(buff[4:8],   byteorder=BYTE_ORDER, signed=False)
        self.dtype      = int.from_bytes(buff[8:12],  byteorder=BYTE_ORDER, signed=False)
        self.op         = int.from_bytes(buff[12:16], byteorder=BYTE_ORDER, signed=False)
        self.comm       = int.from_bytes(buff[16:20], byteorder=BYTE_ORDER, signed=False)
        self.completed  = int.from_bytes(buff[24:28], byteorder=BYTE_ORDER, signed=False)

    def attributes(self):
        return "uid={}, count={}, dtype={}, op={}, comm={}, completed={}".format(self.uid, self.count, self.dtype, self.op, self.comm, self.completed)

    def store(self, processes, pid):
        tasks = processes['by-pids'][pid]['tasks']
        assert(self.uid in tasks)
        tasks[self.uid]['allreduces'].append(self)

    def size(self):
        return RECORD_ALLREDUCE_SIZE

class RecordRank(Record):

    def parse(self, buff):
        self.comm = int.from_bytes(buff[0:4],   byteorder=BYTE_ORDER, signed=False)
        self.rank = int.from_bytes(buff[4:8],   byteorder=BYTE_ORDER, signed=False)

    def attributes(self):
        return "comm={}, rank={}".format(self.comm, self.rank)

    def store(self, processes, pid):
        convert = processes['convert']
        if "p2c2r" not in convert:
            convert["p2c2r"] = {}
            convert["c2r2p"] = {}
        if pid not in convert["p2c2r"]:
            convert["p2c2r"][pid] = {}
        if self.comm not in convert["p2c2r"][pid]:
            convert["p2c2r"][pid][self.comm] = self.rank
        if self.comm not in convert["c2r2p"]:
            convert["c2r2p"][self.comm] = {}
        if self.rank not in convert["c2r2p"][self.comm]:
            convert["c2r2p"][self.comm][self.rank] = pid

    def size(self):
        return RECORD_RANK_SIZE

class RecordBlocked(Record):

    def parse(self, buff):
        self.uid = int.from_bytes(buff[0:4], byteorder=BYTE_ORDER, signed=False)

    def attributes(self):
        return "uid={}".format(self.uid)

    def store(self, processes, pid):
        pass

    def size(self):
        return RECORD_BLOCKED_SIZE

class RecordUnblocked(Record):

    def parse(self, buff):
        self.uid = int.from_bytes(buff[0:4], byteorder=BYTE_ORDER, signed=False)

    def attributes(self):
        return "uid={}".format(self.uid)

    def store(self, processes, pid):
        pass

    def size(self):
        return RECORD_UNBLOCKED_SIZE

RECORD_CLASS = {
    MPC_OMP_TASK_TRACE_TYPE_BEGIN:          RecordIgnore,
    MPC_OMP_TASK_TRACE_TYPE_END:            RecordIgnore,
    MPC_OMP_TASK_TRACE_TYPE_DEPENDENCY:     RecordDependency,
    MPC_OMP_TASK_TRACE_TYPE_SCHEDULE:       RecordSchedule,
    MPC_OMP_TASK_TRACE_TYPE_CREATE:         RecordCreate,
    MPC_OMP_TASK_TRACE_TYPE_DELETE:         RecordDelete,
    MPC_OMP_TASK_TRACE_TYPE_SEND:           RecordSend,
    MPC_OMP_TASK_TRACE_TYPE_RECV:           RecordRecv,
    MPC_OMP_TASK_TRACE_TYPE_ALLREDUCE:      RecordAllreduce,
    MPC_OMP_TASK_TRACE_TYPE_RANK:           RecordRank,
    MPC_OMP_TASK_TRACE_TYPE_BLOCKED:        RecordBlocked,
    MPC_OMP_TASK_TRACE_TYPE_UNBLOCKED:      RecordUnblocked,
}

# ---------------------------------------------------- #

def record_to_properties_and_statuses(record):
    lst = []

    # task properties
    for i in range(len(MPC_OMP_TASK_PROP)):
        if record.properties & (1 << i):
            lst.append(MPC_OMP_TASK_PROP[i])

    # task statuses
    for i in range(len(MPC_OMP_TASK_STATUSES)):
        if record.statuses & (1 << i):
            lst.append(MPC_OMP_TASK_STATUSES[i])

    return lst

def trace_to_records(records, path):
    total_size = os.path.getsize(path)
    with open(path, "rb") as f:
        buff = f.read(HEADER_SIZE)
        assert(len(buff) == HEADER_SIZE)
        header = Header(buff)
        if header.pid not in records:
            records[header.pid] = []
        while True:
            buff = f.read(RECORD_GENERIC_SIZE)
            if buff == b'':
                break
            assert(len(buff) == RECORD_GENERIC_SIZE)
            t   = int.from_bytes(buff[0:8],     byteorder=BYTE_ORDER, signed=False)
            typ = int.from_bytes(buff[8:12],    byteorder=BYTE_ORDER, signed=False)
            assert(typ in RECORD_CLASS)
            record = RECORD_CLASS[typ](header.pid, header.tid, t, typ, f)
            records[header.pid].append(record)

def traces_to_records(src):
    records = {}
    num_files = len(os.listdir(src))
    print("Converting raw trace to records...")
    progress.init(num_files)
    for f in os.listdir(src):
        progress.update()
        trace_to_records(records, "{}/{}".format(src, f))
    progress.deinit()
    return records

def records_to_processes(records):

    processes = {
        "by-pids": {},
        "convert": {}
    }

    print("Converting records to per-core scheduling...")
    progress.init(sum(len(records[pid]) for pid in records))

    # sort record by processes and threads
    for pid in records:
        processes["by-pids"][pid] = {'threads': [], 'tasks': {}}
        process = processes["by-pids"][pid]
        for record in records[pid]:
            progress.update()
            if record.tid not in process['threads']:
                process['threads'].append(record.tid)
            record.store(processes, pid)

    progress.deinit()

    return processes

def dotprint(depth, s):
    print('{}{}'.format(' ' * depth * 4, s))

##### GRAPH DATA STRUCTURES #####

COLOR_GRADIENT_1 = [180, 0, 0]
COLOR_GRADIENT_2 = [255, 255, 255]

def task_uuid(pid, uid):
    return 'Tx{}x{}'.format(pid, uid)

# a task node
class Node:
    def __init__(self, graph, uid, schedule_id, task):
        self.graph = graph
        self.critical = False
        self.critical_index = -1
        self.uuid = task_uuid(graph.pid, uid)
        self.uid = uid
        self.successors = task['successors']
        self.predecessors = task['predecessors']
        self.label = task['create'].label if task['create'] is not None else ""
        self.priority = task['schedules'][0].priority
        self.omp_priority = task['create'].omp_priority if task['create'] is not None else 0
        self.time = sum([task['schedules'][i + 1].time - task['schedules'][i].time for i in range(0, len(task['schedules']), 2)])
        self.schedule_id = schedule_id
        self.last_sched = task['schedules'][-2]

    def dump(self, gg, g, dumped, show_label=False, show_priority=False, show_omp_priority=False, show_time=False, gradient=False):
        if self.uuid in dumped:
            return
        dumped[self.uuid] = True
        s = ''
        if show_label:
            s = self.label
        if show_priority:
            s += "\\n{}".format(self.priority)
        if show_omp_priority:
            s += "\\n{}".format(self.omp_priority)
        if show_time:
            s += "\\n{}".format(self.time / 1e6)
        if gradient:
            if self.graph.last_schedule_id != 0:
                f = self.schedule_id / float(self.graph.last_schedule_id)   # linear
                f = f**2
            else:
                f = 1
            rgb = tuple(int((1.0 - f) * component[0] + f * component[1]) for component in zip(COLOR_GRADIENT_1, COLOR_GRADIENT_2))
            color = '#%02x%02x%02x' % rgb
            shape = 'circle'
            if self.label.startswith('send'):
                shape = 'diamond'
            dotprint(2, '{} [style=filled, fillcolor="{}", label="{}", shape="{}"];'.format(self.uuid, color, s, shape))
        else:
            dotprint(2, '{} [label="{}"];'.format(self.uuid, s))
        for suid in self.successors:
            snode = g.nodes[suid]
            snode.dump(gg, g, dumped, show_label=show_label, show_priority=show_priority, show_omp_priority=show_omp_priority, show_time=show_time, gradient=gradient)
            s = '{} -> {}'.format(self.uuid, snode.uuid)
            if self.critical and snode.critical and abs(self.critical_index - snode.critical_index) == 1:
                s += ' [color=red]'
            s += ';'
            dotprint(2, s)

    def set_critical(self, index):
        self.critical = True
        self.critical_index = index

    def __str__(self):
        return 'Node(label={}, time={}, uid={}, pid={})'.format(self.label, self.time, self.uid, self.graph.pid)

    def __repr__(self):
        return str(self)

# a process task graph
class Graph:
    def __init__(self, pid, process):
        self.pid = pid
        self.nodes = {}
        self.roots = []
        self.leaves = []
        self.last_schedule_id = 0

        # for each task of the process
        tasks = process['tasks']
        for uid in tasks:
            task = tasks[uid]

            # ignore initial tasks
            if (task['create'] and task['create'].parent_uid == 4294967295):
                continue

            # if task was never scheduled, then it was cancelled
            if len(task['schedules']) == 0:
                properties = record_to_properties_and_statuses(task['delete'])
                print("task is:");
                print(task)
                # assert('CANCELLED' in properties)
                continue

            # retrieve first schedule id for this task
            schedule_id = min(x.schedule_id for x in task['schedules'])

            # create graph node
            node = Node(self, uid, schedule_id, task)
            self.nodes[uid] = node
            if len(node.successors) == 0:
                self.leaves.append(node)
            if len(node.predecessors) == 0:
                self.roots.append(node)

            # set the last schedule id of the graph
            self.last_schedule_id = max(schedule_id, self.last_schedule_id)

    def dump(self, gg, show_label=False, show_priority=False, show_omp_priority=False, show_time=False, gradient=False):
        dotprint(1, 'subgraph cluster_P{}'.format(self.pid))
        dotprint(1, '{')

        dotprint(2, 'label="Process {}";'.format(self.pid))
        dotprint(2, 'color="#aaaaaa";')

        dumped = {}
        for node in self.roots:
            node.dump(gg, self, dumped, show_label=show_label, show_priority=show_priority, show_omp_priority=show_omp_priority, show_time=show_time, gradient=gradient)

        dotprint(1, '}')

# the global graph
class GlobalGraph:
    def __init__(self):
        self.graphs = {}
        self.nodes = {}
        self.send_to_recv = {}
        self.recv_to_send = {}
        self.roots = []
        self.leaves = []

    def add_process(self, pid, process):
        graph = Graph(pid, process)
        self.graphs[pid] = graph
        for uid in graph.nodes:
            node = graph.nodes[uid]
            self.nodes[node.uuid] = node

    def finalize(self, processes):
        for pid in self.graphs:
            graph = self.graphs[pid]

            # retrieve leaves and roots
            for leaf in graph.leaves:
                if leaf not in self.send_to_recv:
                    self.leaves.append(leaf)
            for root in graph.roots:
                if root not in self.recv_to_send:
                    self.roots.append(root)

        # link inter-process nodes
        self.communications = {}
        c = self.communications
        for pid in processes["by-pids"]:
            tasks = processes["by-pids"][pid]['tasks']
            for uid in tasks:
                task = tasks[uid]
                for s in task['sends']:
                    if s.comm not in c:
                        c[s.comm] = {}
                    rank = pid_to_rank(processes, pid)
                    if rank    not in c[s.comm]:
                        c[s.comm][rank] = {}
                    if s.dst   not in c[s.comm][rank]:
                        c[s.comm][rank][s.dst] = {}
                    if s.count not in c[s.comm][rank][s.dst]:
                        c[s.comm][rank][s.dst][s.count] = {}
                    if s.dtype not in c[s.comm][rank][s.dst][s.count]:
                        c[s.comm][rank][s.dst][s.count][s.dtype] = {}
                    if s.tag   not in c[s.comm][rank][s.dst][s.count][s.dtype]:
                        c[s.comm][rank][s.dst][s.count][s.dtype][s.tag] = {'send': [], 'recv': []}
                    c[s.comm][rank][s.dst][s.count][s.dtype][s.tag]['send'].append(s)

                for r in task['recvs']:
                    if r.comm  not in c:
                        c[r.comm] = {}
                    if r.src   not in c[r.comm]:
                        c[r.comm][r.src] = {}
                    rank = pid_to_rank(processes, pid)
                    if rank    not in c[r.comm][r.src]:
                        c[r.comm][r.src][rank] = {}
                    if r.count not in c[r.comm][r.src][rank]:
                        c[r.comm][r.src][rank][r.count] = {}
                    if r.dtype not in c[r.comm][r.src][rank][r.count]:
                        c[r.comm][r.src][rank][r.count][r.dtype] = {}
                    if r.tag   not in c[r.comm][r.src][rank][r.count][r.dtype]:
                        c[r.comm][r.src][rank][r.count][r.dtype][r.tag] = {'send': [], 'recv': []}
                    c[r.comm][r.src][rank][r.count][r.dtype][r.tag]['recv'].append(r)

        for comm in c:
            for src in c[comm]:
                for dst in c[comm][src]:
                    for count in c[comm][src][dst]:
                        for dtype in c[comm][src][dst][count]:
                            for tag in c[comm][src][dst][count][dtype]:
                                x = c[comm][src][dst][count][dtype][tag]
                                # TODO : this condition may only be verified if we
                                # trace all the ranks involved in all the communications
                                # assert(len(x['send']) == len(x['recv']))
                                for i in range(min(len(x['send']), len(x['recv']))):
                                    send = x['send'][i]
                                    recv = x['recv'][i]
                                    assert(send.pid in self.graphs)
                                    assert(recv.pid in self.graphs)
                                    snode = self.graphs[send.pid].nodes[send.uid]
                                    rnode = self.graphs[recv.pid].nodes[recv.uid]

                                    if snode not in self.send_to_recv:
                                        self.send_to_recv[snode] = []
                                    self.send_to_recv[snode].append(rnode)

                                    if rnode not in self.recv_to_send:
                                        self.recv_to_send[rnode] = []
                                    self.recv_to_send[rnode].append(snode)

    # compute local process critical path
    def compute_critical(self):
        print("computing critical path...",  file=sys.stderr)
        predecessors = {}
        d = {uuid: -math.inf for uuid in self.nodes}
        q = queue.Queue()
        last_time_dump = time.time()

        for pid in self.graphs:
            graph = self.graphs[pid]
            for node in graph.roots:
                if "deps" in node.label:    # work-around for HPCCG
                    continue
                d[node.uuid] = node.time
                q.put(node)
                while not q.empty():
                    node = q.get()
                    successors = [node.graph.nodes[uid] for uid in node.successors]
                    if node in self.send_to_recv:
                        successors += self.send_to_recv[node]
                    for snode in successors:
                        if d[snode.uuid] < d[node.uuid] + snode.time:
                            d[snode.uuid] = d[node.uuid] + snode.time
                            predecessors[snode.uuid] = node.uuid
                            q.put(snode)
        path = []
        node = max(self.leaves, key=lambda leaf: d[leaf.uuid])
        while True:
            path.insert(0, node)
            if node.uuid in predecessors:
                node = self.nodes[predecessors[node.uuid]]
            else:
                break
        return path

    def dump(self, show_label=False, show_priority=False, show_omp_priority=False, show_time=False, gradient=False):
        dotprint(0, 'digraph G')
        dotprint(0, '{')

        # print legend
        dotprint(1, 'subgraph cluster_LEGEND')
        dotprint(1, '{')

        legend = 'LABEL'
        if show_priority:
            legend += '\\nPRIORITY (internal)'
        if show_omp_priority:
            legend += '\\nOMP_PRIORITY'
        if show_time:
            legend += '\\nTIME (s.)'
        dotprint(2, 'LEGEND [label="{}"];'.format(legend))
        dotprint(2, 'label="Legend";')
        dotprint(2, 'color="#aaaaaa";')
        dotprint(1, '}')

        # print subgraphs
        for pid in self.graphs:
            graph = self.graphs[pid]
            graph.dump(self, show_label=show_label, show_priority=show_priority, show_omp_priority=show_omp_priority, show_time=show_time, gradient=gradient)

        # print inter-graph dependencies
        for snode in self.send_to_recv:
            for rnode in self.send_to_recv[snode]:
                s = '{} -> {}'.format(snode.uuid, rnode.uuid)
                attr = []
                attr.append('style=dotted')
                if snode.critical and rnode.critical:
                    attr.append('color=red')
                s += ' [{}] ;'.format(','.join(attr))
                dotprint(1, s)
        dotprint(0, '}')

def records_to_gg(records):
    gg = GlobalGraph()
    processes = records_to_processes(records)

    print("Generating global task dependency graph...")
    progress.init(len(processes["by-pids"]))
    pidID = 0
    for pid in processes["by-pids"]:
        progress.update()
        pidID = pidID + 1
        gg.add_process(pid, processes["by-pids"][pid])
    progress.deinit()
    gg.finalize(processes)
    return (processes, gg)

def traces_to_gg(traces):
    records = traces_to_records(traces)
    records_fix(records)
    (processes, gg) = records_to_gg(records)
    return gg

def blumofe_leiserson_check(trace):
    (stats, _, gg, _, _, _) = parse_traces(trace)

    # T1
    T1 = 0.
    for node in gg.nodes.values():
        T1 += node.time
    T1 *= 1e-6

    # Too
    cpath = gg.compute_critical()
    Too = 0.
    for node in cpath:
        Too += node.time
    Too *= 1e-6

    # P
    P = stats['about']['n-threads-total']

    # TP
    TP = stats['time']['flat (s.)']['max-process-time']

    return (Too, TP, T1, P, cpath)

