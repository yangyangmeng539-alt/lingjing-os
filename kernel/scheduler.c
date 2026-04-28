#include "scheduler.h"
#include "screen.h"

#define MAX_TASKS 8

typedef struct task_entry {
    unsigned int id;
    const char* name;
    const char* status;
    const char* type;
    unsigned int runtime_ticks;
} task_entry_t;

static task_entry_t tasks[MAX_TASKS];
static int task_count = 0;
static unsigned int scheduler_ticks = 0;

static void scheduler_print_uint(unsigned int value) {
    char buffer[16];
    int index = 0;

    if (value == 0) {
        screen_put_char('0');
        return;
    }

    while (value > 0 && index < 16) {
        buffer[index] = (char)('0' + (value % 10));
        value /= 10;
        index++;
    }

    for (int i = index - 1; i >= 0; i--) {
        screen_put_char(buffer[i]);
    }
}

static void scheduler_add_task(unsigned int id, const char* name, const char* status, const char* type) {
    if (task_count >= MAX_TASKS) {
        return;
    }

    tasks[task_count].id = id;
    tasks[task_count].name = name;
    tasks[task_count].status = status;
    tasks[task_count].type = type;
    tasks[task_count].runtime_ticks = 0;
    task_count++;
}

void scheduler_init(void) {
    task_count = 0;
    scheduler_ticks = 0;

    scheduler_add_task(0, "idle", "running", "kernel");
    scheduler_add_task(1, "shell", "ready", "system");
    scheduler_add_task(2, "intent", "ready", "system");
}

void scheduler_tick(void) {
    scheduler_ticks++;

    if (task_count > 0) {
        tasks[0].runtime_ticks++;
    }
}

unsigned int scheduler_get_ticks(void) {
    return scheduler_ticks;
}

const char* scheduler_get_mode(void) {
    return "cooperative";
}

const char* scheduler_get_active_task(void) {
    return "idle";
}

void scheduler_info(void) {
    screen_print("Scheduler:\n");

    screen_print("  ticks:  ");
    scheduler_print_uint(scheduler_ticks);
    screen_print("\n");

    screen_print("  tasks:  ");
    scheduler_print_uint((unsigned int)task_count);
    screen_print("\n");

    screen_print("  mode:   ");
    screen_print(scheduler_get_mode());
    screen_print(" prototype\n");

    screen_print("  active: ");
    screen_print(scheduler_get_active_task());
    screen_print("\n");
}

void scheduler_list_tasks(void) {
    screen_print("Tasks:\n");

    for (int i = 0; i < task_count; i++) {
        screen_print("  ");
        scheduler_print_uint(tasks[i].id);
        screen_print("    ");
        screen_print(tasks[i].name);
        screen_print("    ");
        screen_print(tasks[i].status);
        screen_print("    ");
        screen_print(tasks[i].type);
        screen_print("    ticks ");
        scheduler_print_uint(tasks[i].runtime_ticks);
        screen_print("\n");
    }
}

void scheduler_task_info(unsigned int id) {
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].id == id) {
            screen_print("Task:\n");

            screen_print("  id:     ");
            scheduler_print_uint(tasks[i].id);
            screen_print("\n");

            screen_print("  name:   ");
            screen_print(tasks[i].name);
            screen_print("\n");

            screen_print("  status: ");
            screen_print(tasks[i].status);
            screen_print("\n");

            screen_print("  type:   ");
            screen_print(tasks[i].type);
            screen_print("\n");

            screen_print("  ticks:  ");
            scheduler_print_uint(tasks[i].runtime_ticks);
            screen_print("\n");

            return;
        }
    }

    screen_print("task not found: ");
    scheduler_print_uint(id);
    screen_print("\n");
}

void scheduler_check_tasks(void) {
    int active_count = 0;
    int broken_count = 0;

    screen_print("Task check:\n");

    for (int i = 0; i < task_count; i++) {
        screen_print("  ");
        screen_print(tasks[i].name);
        screen_print("    ");

        if (tasks[i].status == 0 || tasks[i].name == 0 || tasks[i].type == 0) {
            screen_print("broken\n");
            broken_count++;
        } else {
            screen_print("ok\n");
            active_count++;
        }
    }

    screen_print("summary:\n");

    screen_print("  total:  ");
    scheduler_print_uint((unsigned int)task_count);
    screen_print("\n");

    screen_print("  active: ");
    scheduler_print_uint((unsigned int)active_count);
    screen_print("\n");

    screen_print("  broken: ");
    scheduler_print_uint((unsigned int)broken_count);
    screen_print("\n");

    screen_print("  ticks:  ");
    scheduler_print_uint(scheduler_ticks);
    screen_print("\n");
}

int scheduler_has_broken_tasks(void) {
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].status == 0 || tasks[i].name == 0 || tasks[i].type == 0) {
            return 1;
        }
    }

    return 0;
}

int scheduler_task_count(void) {
    return task_count;
}