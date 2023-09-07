import json
import math

from . import ipass
"""
"   This pass computes in-tasks and out-tasks times.
"       - in-tasks is simply the sum of tasks workload
"       - out-tasks time is 'total-time . N_THREADS - in-tasks'
"           - idle is computed by replaying the schedule and tracking ready tasks
"             and how they are scheduled onto cores :
              this is the logic running behind this pass
"           - overhead is 'out-tasks - idle'
"""

class TimingPass(ipass.Pass):

    def __init__(self):
        pass

    def __repr__(self):
        return str(self)

    def __str__(self):
        return "TimingPass(...)"

    def on_start(self, config):
        pass

    def on_end(self, config):
        pass

    # return the time elapsed since the last active time of the thread 'tid'
    def last_active_time(self, env, tid):
        if tid not in self.last_active:
            return env['t0']
        return self.last_active[tid]

    # return the last time a thread started idling
    def last_idle_start(self, env, tid):
        if tid not in self.idle_start:
            return env['t0']
        return self.idle_start[tid]

    def on_process_inspection_start(self, env):
        env['time'] = {
            'in-tasks':     0,
            'out-tasks':    0,
            'idle':         0,
            'overhead':     0,
            'graph_gen':    0,
            'n-threads':    len(env['bound']),
        }
        self.first_task_schedule = None
        self.last_task_schedule = None
        self.first_task_create = +math.inf     # first task creation time
        self.last_task_create  = -math.inf     # last  task creation time
        self.last_active = {}           # last time a thread was executing a task
        self.idle_start = {}            # last when a thread became

    def on_process_inspection_end(self, env):

        # end of the gantt
        for tid in env['bound']:
            dt = env['tf'] - self.last_active_time(env, tid)
            env['time']['out-tasks'] += dt
            idle_start = self.last_idle_start(env, tid)
            if idle_start != +math.inf:
                dt = env['tf'] - self.last_idle_start(env, tid)
                env['time']['idle'] += dt

        env['time']['total']        = 1e-6 * (env['tf'] - env['t0'])
        env['time']['in-tasks']     = 1e-6 * sum(env['workload'][uid] for uid in env['workload'])
        env['time']['out-tasks']   *= 1e-6
        env['time']['graph_gen']    = 1e-6 * (self.last_task_create - self.first_task_create)
        env['time']['makespan']     = 1e-6 * (self.last_task_schedule.time - self.first_task_schedule.time)

        # check, should be also equal to 'total - in-tasks'
        out_tasks_check = len(env['bound']) * env['time']['total'] - env['time']['in-tasks']
        assert(abs(1.0 - out_tasks_check / env['time']['out-tasks']) < 1e-5)
        assert(len(env['bound']) * env['time']['total'] - env['time']['in-tasks'] - env['time']['idle'] - env['time']['overhead'] < 1e-5)

        # induce overhead from idle and out-tasks times
        env['time']['idle']        *= 1e-6
        env['time']['overhead']     = env['time']['out-tasks'] - env['time']['idle']

        # maximum of 6 digits floating point precision
        for key in env['time']:
            env['time'][key] = round(env['time'][key], 6)
        print(json.dumps(env['time'], indent=4))

    def on_task_create(self, env):
        self.first_task_create = min(self.first_task_create, env['record'].time)
        self.last_task_create  = max(self.last_task_create,  env['record'].time)

    def on_task_delete(self, env):
        pass

    def on_task_dependency(self, env):
        pass

    def a_task_is_ready(self, env):
        task = env['record']
        for tid in env['bound']:
            idle_start = self.last_idle_start(env, tid)
            if idle_start != +math.inf:
                dt = task.time - idle_start
                env['time']['idle'] += dt
                self.idle_start[tid] = +math.inf

    def on_task_ready(self, env):
        assert(len(env['readyqueue']) > 0)
        self.a_task_is_ready(env)

    def on_task_started(self, env):
        task = env['record']
        if self.first_task_schedule == None:
            self.first_task_schedule = task
        dt = task.time - self.last_active_time(env, task.tid)
        env['time']['out-tasks'] += dt
        if len(env['readyqueue']) == 0:
            for tid in env['bound']:
                if len(env['bound'][tid]) == 0:
                    assert(tid not in self.idle_start or self.idle_start[tid] == math.inf)
                    self.idle_start[tid] = task.time

    def on_task_completed(self, env):
        task = env['record']
        self.last_active[task.tid] = task.time
        if len(env['readyqueue']) == 0:
            assert(self.idle_start[task.tid] == +math.inf)
            self.idle_start[task.tid] = task.time
        self.last_task_schedule = task

    def on_task_blocked(self, env):
        pass

    def on_task_unblocked(self, env):
        self.a_task_is_ready(env)

    def on_task_paused(self, env):
        task = env['record']
        self.last_active[task.tid] = task.time
        if len(env['readyqueue']) == 0:
            for tid in env['bound']:
                if len(env['bound'][tid]) == 0:
                    if tid not in self.idle_start or self.idle_start[tid] == math.inf:
                        self.idle_start[tid] = min(task.time, self.idle_start[tid])

    def on_task_resumed(self, env):
        task = env['record']
        dt = task.time - self.last_active_time(env, task.tid)
        env['time']['out-tasks'] += dt
        self.idle_start[task.tid] = +math.inf
        if len(env['readyqueue']) == 0:
            for tid in env['bound']:
                if len(env['bound'][tid]) == 0:
                    self.idle_start[tid] = min(task.time, self.idle_start[tid])
