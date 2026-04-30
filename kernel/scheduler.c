#include "scheduler.h"
#include "screen.h"
#include "platform.h"

#define MAX_TASKS 16
#define TASK_NAME_MAX 32

typedef struct task_entry {
    unsigned int id;
    const char* name;
    const char* status;
    const char* type;
    unsigned int runtime_ticks;
    unsigned int priority;
} task_entry_t;

static task_entry_t tasks[MAX_TASKS];
static char task_names[MAX_TASKS][TASK_NAME_MAX];
static int task_count = 0;
static int active_task_index = 0;
static unsigned int scheduler_ticks = 0;
static unsigned int scheduler_yield_count = 0;
static unsigned int next_task_id = 4;
static int scheduler_state_is_ready(const char* state);
static int scheduler_state_is_running(const char* state);
static int scheduler_state_is_blocked(const char* state);
static int scheduler_state_is_killed(const char* state);
static int scheduler_state_is_valid(const char* state);

#define SCHED_LOG_MAX 8

static const char* sched_log_from[SCHED_LOG_MAX];
static const char* sched_log_to[SCHED_LOG_MAX];
static int sched_log_count = 0;

static void scheduler_copy_task_name(int index, const char* name) {
    int i = 0;

    if (index < 0 || index >= MAX_TASKS) {
        return;
    }

    if (name == 0) {
        task_names[index][0] = '\0';
        return;
    }

    while (i < TASK_NAME_MAX - 1 && name[i] != '\0') {
        task_names[index][i] = name[i];
        i++;
    }

    task_names[index][i] = '\0';
}

static void scheduler_add_task(unsigned int id, const char* name, const char* status, const char* type) {
    int index = task_count;

    if (task_count >= MAX_TASKS) {
        return;
    }

    scheduler_copy_task_name(index, name);

    tasks[index].id = id;
    tasks[index].name = task_names[index];
    tasks[index].status = status;
    tasks[index].type = type;
    tasks[index].runtime_ticks = 0;
    tasks[index].priority = 1;

    task_count++;
}

static int scheduler_task_is_runnable(int index) {
    if (index < 0 || index >= task_count) {
        return 0;
    }

    if (tasks[index].status == 0) {
        return 0;
    }

    if (index == 0) {
        return 1;
    }

    if (scheduler_state_is_blocked(tasks[index].status)) {
        return 0;
    }

    if (scheduler_state_is_killed(tasks[index].status)) {
        return 0;
    }

    return 1;
}

static int scheduler_state_is_ready(const char* state) {
    if (state == 0) {
        return 0;
    }

    return state[0] == 'r' &&
           state[1] == 'e' &&
           state[2] == 'a' &&
           state[3] == 'd' &&
           state[4] == 'y' &&
           state[5] == '\0';
}

static int scheduler_state_is_running(const char* state) {
    if (state == 0) {
        return 0;
    }

    return state[0] == 'r' &&
           state[1] == 'u' &&
           state[2] == 'n' &&
           state[3] == 'n' &&
           state[4] == 'i' &&
           state[5] == 'n' &&
           state[6] == 'g' &&
           state[7] == '\0';
}

static int scheduler_state_is_valid(const char* state) {
    if (state == 0) {
        return 0;
    }

    if (state[0] == 'r' &&
        state[1] == 'e' &&
        state[2] == 'a' &&
        state[3] == 'd' &&
        state[4] == 'y' &&
        state[5] == '\0') {
        return 1;
    }

    if (state[0] == 'r' &&
        state[1] == 'u' &&
        state[2] == 'n' &&
        state[3] == 'n' &&
        state[4] == 'i' &&
        state[5] == 'n' &&
        state[6] == 'g' &&
        state[7] == '\0') {
        return 1;
    }

    if (state[0] == 'b' &&
        state[1] == 'l' &&
        state[2] == 'o' &&
        state[3] == 'c' &&
        state[4] == 'k' &&
        state[5] == 'e' &&
        state[6] == 'd' &&
        state[7] == '\0') {
        return 1;
    }

    if (state[0] == 'k' &&
        state[1] == 'i' &&
        state[2] == 'l' &&
        state[3] == 'l' &&
        state[4] == 'e' &&
        state[5] == 'd' &&
        state[6] == '\0') {
        return 1;
    }

    return 0;
}

static int scheduler_state_is_blocked(const char* state) {
    if (state == 0) {
        return 0;
    }

    return state[0] == 'b' &&
           state[1] == 'l' &&
           state[2] == 'o' &&
           state[3] == 'c' &&
           state[4] == 'k' &&
           state[5] == 'e' &&
           state[6] == 'd' &&
           state[7] == '\0';
}

static int scheduler_state_is_killed(const char* state) {
    if (state == 0) {
        return 0;
    }

    return state[0] == 'k' &&
           state[1] == 'i' &&
           state[2] == 'l' &&
           state[3] == 'l' &&
           state[4] == 'e' &&
           state[5] == 'd' &&
           state[6] == '\0';
}

static void scheduler_switch_to_next_runnable(void) {
    if (task_count <= 0) {
        active_task_index = 0;
        return;
    }

    for (int i = 0; i < task_count; i++) {
        active_task_index++;

        if (active_task_index >= task_count) {
            active_task_index = 0;
        }

        if (scheduler_task_is_runnable(active_task_index)) {
            for (int j = 0; j < task_count; j++) {
                if (j == active_task_index) {
                    tasks[j].status = "running";
                } else if (scheduler_task_is_runnable(j)) {
                    tasks[j].status = "ready";
                }
            }

            return;
        }
    }

    active_task_index = 0;

    for (int j = 0; j < task_count; j++) {
        if (j == 0) {
            tasks[j].status = "running";
        } else if (!scheduler_state_is_blocked(tasks[j].status) &&
                   !scheduler_state_is_killed(tasks[j].status)) {
            tasks[j].status = "ready";
        }
    }
}

static void scheduler_log_switch(const char* from, const char* to) {
    if (sched_log_count < SCHED_LOG_MAX) {
        sched_log_from[sched_log_count] = from;
        sched_log_to[sched_log_count] = to;
        sched_log_count++;
        return;
    }

    for (int i = 1; i < SCHED_LOG_MAX; i++) {
        sched_log_from[i - 1] = sched_log_from[i];
        sched_log_to[i - 1] = sched_log_to[i];
    }

    sched_log_from[SCHED_LOG_MAX - 1] = from;
    sched_log_to[SCHED_LOG_MAX - 1] = to;
}

void scheduler_init(void) {
    task_count = 0;
    active_task_index = 0;
    scheduler_ticks = 0;
    scheduler_yield_count = 0;
    sched_log_count = 0;

    for (int i = 0; i < SCHED_LOG_MAX; i++) {
        sched_log_from[i] = 0;
        sched_log_to[i] = 0;
    }

    scheduler_add_task(0, "idle", "running", "kernel");
    scheduler_add_task(1, "shell", "ready", "system");
    scheduler_add_task(2, "intent", "ready", "system");
    scheduler_add_task(3, "timer", "ready", "kernel");
}

void scheduler_tick(void) {
    scheduler_ticks++;

    if (task_count > 0 && active_task_index >= 0 && active_task_index < task_count) {
        tasks[active_task_index].runtime_ticks++;
    }
}

void scheduler_yield(void) {
    int old_index = active_task_index;
    const char* from = scheduler_get_active_task();

    scheduler_yield_count++;

    if (task_count > 0) {
        int found = 0;

        for (int i = 0; i < task_count; i++) {
            active_task_index++;

            if (active_task_index >= task_count) {
                active_task_index = 0;
            }

            if (scheduler_task_is_runnable(active_task_index)) {
                found = 1;
                break;
            }
        }

        if (!found) {
            active_task_index = old_index;
        }
    }

    if (old_index >= 0 && old_index < task_count) {
        if (scheduler_task_is_runnable(old_index)) {
            tasks[old_index].status = "ready";
        }
    }

    if (active_task_index >= 0 && active_task_index < task_count) {
        if (scheduler_task_is_runnable(active_task_index)) {
            tasks[active_task_index].status = "running";
        }
    }

    const char* to = scheduler_get_active_task();

    scheduler_log_switch(from, to);

    platform_print("scheduler yield.\n");
    platform_print("switch ");
    platform_print(from);
    platform_print(" -> ");
    platform_print(to);
    platform_print("\n");
}

unsigned int scheduler_get_ticks(void) {
    return scheduler_ticks;
}

unsigned int scheduler_get_yields(void) {
    return scheduler_yield_count;
}

const char* scheduler_get_mode(void) {
    return "cooperative";
}

const char* scheduler_get_active_task(void) {
    if (task_count == 0) {
        return "none";
    }

    if (active_task_index < 0 || active_task_index >= task_count) {
        return "invalid";
    }

    return tasks[active_task_index].name;
}

void scheduler_info(void) {
    platform_print("Scheduler:\n");

    platform_print("  ticks:  ");
    platform_print_uint(scheduler_ticks);
    platform_print("\n");

    platform_print("  yields: ");
    platform_print_uint(scheduler_yield_count);
    platform_print("\n");

    platform_print("  tasks:  ");
    platform_print_uint((unsigned int)task_count);
    platform_print("\n");

    platform_print("  mode:   ");
    platform_print(scheduler_get_mode());
    platform_print(" prototype\n");

    platform_print("  active: ");
    platform_print(scheduler_get_active_task());
    platform_print("\n");
}

void scheduler_log(void) {
    platform_print("Scheduler log:\n");

    if (sched_log_count == 0) {
        platform_print("  empty\n");
        return;
    }

    for (int i = 0; i < sched_log_count; i++) {
        platform_print("  switch ");
        platform_print(sched_log_from[i]);
        platform_print(" -> ");
        platform_print(sched_log_to[i]);
        platform_print("\n");
    }
}

void scheduler_runqueue(void) {
    platform_print("Run queue:\n");

    for (int i = 0; i < task_count; i++) {
        platform_print("  ");
        platform_print_uint(tasks[i].id);
        platform_print(" ");
        platform_print(tasks[i].name);
        platform_print(" ");
        platform_print(tasks[i].status);

        if (scheduler_task_is_runnable(i)) {
            platform_print(" runnable");
        } else {
            platform_print(" skipped");
        }

        platform_print("\n");
    }
}

void scheduler_set_task_state(unsigned int id, const char* state) {
    if (!scheduler_state_is_valid(state)) {
        platform_print("invalid task state: ");
        platform_print(state);
        platform_print("\n");
        platform_print("allowed: ready, running, blocked, killed\n");
        return;
    }

    for (int i = 0; i < task_count; i++) {
        if (tasks[i].id == id) {
            int was_active = (i == active_task_index);

            tasks[i].status = state;

            if (scheduler_state_is_blocked(state) && was_active) {
                scheduler_switch_to_next_runnable();
            } else if (state[0] == 'r' &&
                state[1] == 'u' &&
                state[2] == 'n' &&
                state[3] == 'n' &&
                state[4] == 'i' &&
                state[5] == 'n' &&
                state[6] == 'g' &&
                state[7] == '\0') {
                active_task_index = i;

                for (int j = 0; j < task_count; j++) {
                    if (j != i && scheduler_task_is_runnable(j)) {
                        tasks[j].status = "ready";
                    }
                }
            }

            platform_print("task state changed:\n");

            platform_print("  id:     ");
            platform_print_uint(id);
            platform_print("\n");

            platform_print("  name:   ");
            platform_print(tasks[i].name);
            platform_print("\n");

            platform_print("  state:  ");
            platform_print(tasks[i].status);
            platform_print("\n");

            if (scheduler_state_is_blocked(state) && was_active) {
                platform_print("active switched to: ");
                platform_print(scheduler_get_active_task());
                platform_print("\n");
            }

            return;
        }
    }

    platform_print("task not found: ");
    platform_print_uint(id);
    platform_print("\n");
}

void scheduler_create_task(const char* name) {
    if (name == 0 || name[0] == '\0') {
        platform_print("usage: taskcreate <name>\n");
        return;
    }

    if (task_count >= MAX_TASKS) {
        platform_print("task table full.\n");
        return;
    }

    scheduler_add_task(next_task_id, name, "ready", "user");

    platform_print("task created:\n");
    platform_print("  id:   ");
    platform_print_uint(next_task_id);
    platform_print("\n");

    platform_print("  name: ");
    platform_print(name);
    platform_print("\n");

    platform_print("  state: ready\n");

    next_task_id++;
}

void scheduler_kill_task(unsigned int id) {
    if (id == 0) {
        platform_print("cannot kill idle task.\n");
        return;
    }

    for (int i = 0; i < task_count; i++) {
        if (tasks[i].id == id) {
            int was_active = (i == active_task_index);

            if (scheduler_state_is_killed(tasks[i].status)) {
                platform_print("task already killed: ");
                platform_print_uint(id);
                platform_print("\n");
                return;
            }

            tasks[i].status = "killed";

            platform_print("task killed:\n");
            platform_print("  id:   ");
            platform_print_uint(id);
            platform_print("\n");
            platform_print("  name: ");
            platform_print(tasks[i].name);
            platform_print("\n");

            if (was_active) {
                scheduler_switch_to_next_runnable();
                platform_print("active switched to: ");
                platform_print(scheduler_get_active_task());
                platform_print("\n");
            }

            return;
        }
    }

    platform_print("task not found: ");
    platform_print_uint(id);
    platform_print("\n");
}

void scheduler_sleep_task(unsigned int id) {
    if (id == 0) {
        platform_print("cannot sleep idle task.\n");
        return;
    }

    for (int i = 0; i < task_count; i++) {
        if (tasks[i].id == id) {
            if (scheduler_state_is_killed(tasks[i].status)) {
                platform_print("cannot sleep killed task: ");
                platform_print_uint(id);
                platform_print("\n");
                return;
            }

            scheduler_set_task_state(id, "blocked");
            return;
        }
    }

    platform_print("task not found: ");
    platform_print_uint(id);
    platform_print("\n");
}

void scheduler_wake_task(unsigned int id) {
    if (id == 0) {
        platform_print("idle task is always runnable.\n");
        return;
    }

    for (int i = 0; i < task_count; i++) {
        if (tasks[i].id == id) {
            if (scheduler_state_is_killed(tasks[i].status)) {
                platform_print("cannot wake killed task: ");
                platform_print_uint(id);
                platform_print("\n");
                return;
            }

            scheduler_set_task_state(id, "ready");
            return;
        }
    }

    platform_print("task not found: ");
    platform_print_uint(id);
    platform_print("\n");
}

void scheduler_set_task_priority(unsigned int id, unsigned int priority) {
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].id == id) {
            tasks[i].priority = priority;

            platform_print("task priority changed:\n");
            platform_print("  id:   ");
            platform_print_uint(id);
            platform_print("\n");
            platform_print("  name: ");
            platform_print(tasks[i].name);
            platform_print("\n");
            platform_print("  prio: ");
            platform_print_uint(priority);
            platform_print("\n");
            return;
        }
    }

    platform_print("task not found: ");
    platform_print_uint(id);
    platform_print("\n");
}

void scheduler_clear_log(void) {
    for (int i = 0; i < SCHED_LOG_MAX; i++) {
        sched_log_from[i] = 0;
        sched_log_to[i] = 0;
    }

    sched_log_count = 0;

    platform_print("Scheduler log cleared.\n");
}

void scheduler_reset(void) {
    scheduler_yield_count = 0;
    active_task_index = 0;
    next_task_id = 4;

    for (int i = 0; i < SCHED_LOG_MAX; i++) {
        sched_log_from[i] = 0;
        sched_log_to[i] = 0;
    }

    sched_log_count = 0;

    task_count = 0;

    scheduler_add_task(0, "idle", "running", "kernel");
    scheduler_add_task(1, "shell", "ready", "system");
    scheduler_add_task(2, "intent", "ready", "system");
    scheduler_add_task(3, "timer", "ready", "kernel");

    platform_print("Scheduler reset.\n");
    platform_print("active task: ");
    platform_print(scheduler_get_active_task());
    platform_print("\n");
}

void scheduler_list_tasks(void) {
    platform_print("Tasks:\n");

    for (int i = 0; i < task_count; i++) {
        platform_print("  ");
        platform_print_uint(tasks[i].id);
        platform_print("    ");
        platform_print(tasks[i].name);
        platform_print("    ");
        platform_print(tasks[i].status);
        platform_print("    ");
        platform_print(tasks[i].type);
        platform_print("    prio ");
        platform_print_uint(tasks[i].priority);
        platform_print("    ticks ");
        platform_print_uint(tasks[i].runtime_ticks);
        platform_print("\n");
    }
}

void scheduler_task_info(unsigned int id) {
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].id == id) {
            platform_print("Task:\n");

            platform_print("  id:     ");
            platform_print_uint(tasks[i].id);
            platform_print("\n");

            platform_print("  name:   ");
            platform_print(tasks[i].name);
            platform_print("\n");

            platform_print("  status: ");
            platform_print(tasks[i].status);
            platform_print("\n");

            platform_print("  type:   ");
            platform_print(tasks[i].type);
            platform_print("\n");

            platform_print("  prio:   ");
            platform_print_uint(tasks[i].priority);
            platform_print("\n");

            platform_print("  ticks:  ");
            platform_print_uint(tasks[i].runtime_ticks);
            platform_print("\n");

            return;
        }
    }

    platform_print("task not found: ");
    platform_print_uint(id);
    platform_print("\n");
}

void scheduler_check_tasks(void) {
    int broken_count = 0;
    int running_count = 0;
    int ready_count = 0;
    int blocked_count = 0;
    int killed_count = 0;

    platform_print("Task check:\n");

    for (int i = 0; i < task_count; i++) {
        platform_print("  ");
        platform_print_uint(tasks[i].id);
        platform_print(" ");
        platform_print(tasks[i].name);
        platform_print(" ");

        if (tasks[i].name == 0 || tasks[i].status == 0 || tasks[i].type == 0) {
            platform_print("broken\n");
            broken_count++;
            continue;
        }

        if (!scheduler_state_is_valid(tasks[i].status)) {
            platform_print("broken\n");
            broken_count++;
            continue;
        }

        if (scheduler_state_is_running(tasks[i].status)) {
            platform_print("running\n");
            running_count++;
        } else if (scheduler_state_is_ready(tasks[i].status)) {
            platform_print("ready\n");
            ready_count++;
        } else if (scheduler_state_is_blocked(tasks[i].status)) {
            platform_print("blocked\n");
            blocked_count++;
        } else if (scheduler_state_is_killed(tasks[i].status)) {
            platform_print("killed\n");
            killed_count++;
        }
    }

    platform_print("summary:\n");

    platform_print("  running: ");
    platform_print_uint((unsigned int)running_count);
    platform_print("\n");

    platform_print("  ready:   ");
    platform_print_uint((unsigned int)ready_count);
    platform_print("\n");

    platform_print("  blocked: ");
    platform_print_uint((unsigned int)blocked_count);
    platform_print("\n");

    platform_print("  killed:  ");
    platform_print_uint((unsigned int)killed_count);
    platform_print("\n");

    platform_print("  broken:  ");
    platform_print_uint((unsigned int)broken_count);
    platform_print("\n");

    platform_print("  result:  ");
    platform_print(broken_count == 0 ? "ok\n" : "broken\n");
}

void scheduler_doctor(void) {
    int broken_count = 0;
    int running_count = 0;
    int ready_count = 0;
    int blocked_count = 0;
    int killed_count = 0;

    for (int i = 0; i < task_count; i++) {
        if (tasks[i].name == 0 || tasks[i].status == 0 || tasks[i].type == 0) {
            broken_count++;
            continue;
        }

        if (!scheduler_state_is_valid(tasks[i].status)) {
            broken_count++;
            continue;
        }

        if (scheduler_state_is_running(tasks[i].status)) {
            running_count++;
        } else if (scheduler_state_is_ready(tasks[i].status)) {
            ready_count++;
        } else if (scheduler_state_is_blocked(tasks[i].status)) {
            blocked_count++;
        } else if (scheduler_state_is_killed(tasks[i].status)) {
            killed_count++;
        }
    }

    platform_print("Task doctor:\n");

    platform_print("  total:   ");
    platform_print_uint((unsigned int)task_count);
    platform_print("\n");

    platform_print("  running: ");
    platform_print_uint((unsigned int)running_count);
    platform_print("\n");

    platform_print("  ready:   ");
    platform_print_uint((unsigned int)ready_count);
    platform_print("\n");

    platform_print("  blocked: ");
    platform_print_uint((unsigned int)blocked_count);
    platform_print("\n");

    platform_print("  killed:  ");
    platform_print_uint((unsigned int)killed_count);
    platform_print("\n");

    platform_print("  broken:  ");
    platform_print_uint((unsigned int)broken_count);
    platform_print("\n");

    platform_print("  result:  ");
    platform_print(broken_count == 0 ? "ok\n" : "broken\n");
}

void scheduler_validate(void) {
    platform_print("Scheduler validation:\n");

    platform_print("  active index: ");
    platform_print_uint((unsigned int)active_task_index);
    platform_print("\n");

    platform_print("  active task:  ");
    platform_print(scheduler_get_active_task());
    platform_print("\n");

    platform_print("  active state: ");

    if (active_task_index >= 0 && active_task_index < task_count && tasks[active_task_index].status != 0) {
        platform_print(tasks[active_task_index].status);
    } else {
        platform_print("unknown");
    }

    platform_print("\n");

    platform_print("  result:       ");

    if (scheduler_task_is_runnable(active_task_index)) {
        platform_print("ok\n");
    } else if (active_task_index == 0) {
        platform_print("ok\n");
    } else {
        platform_print("broken\n");
    }
}

void scheduler_fix(void) {
    platform_print("Scheduler fix:\n");

    if (task_count <= 0) {
        active_task_index = 0;
        platform_print("  no tasks.\n");
        return;
    }

    if (active_task_index < 0 || active_task_index >= task_count) {
        active_task_index = 0;
    }

    if (!scheduler_task_is_runnable(active_task_index)) {
        scheduler_switch_to_next_runnable();
    }

    platform_print("  active task: ");
    platform_print(scheduler_get_active_task());
    platform_print("\n");

    platform_print("  result: ok\n");
}

int scheduler_has_broken_tasks(void) {
    if (task_count <= 0) {
        return 1;
    }

    for (int i = 0; i < task_count; i++) {
        if (tasks[i].name == 0 || tasks[i].status == 0 || tasks[i].type == 0) {
            return 1;
        }

        if (!scheduler_state_is_valid(tasks[i].status)) {
            return 1;
        }
    }

    return 0;
}

int scheduler_task_count(void) {
    return task_count;
}