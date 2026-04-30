#include "identity.h"
#include "platform.h"

static int identity_init_done = 0;
static int identity_ready = 0;
static int identity_broken = 0;

static const char* identity_mode = "local-placeholder";
static const char* identity_public_key = "not-generated";

void identity_init(void) {
    identity_init_done = 1;
    identity_ready = 1;
    identity_broken = 0;
}

int identity_is_ready(void) {
    if (!identity_init_done) {
        return 0;
    }

    if (!identity_ready) {
        return 0;
    }

    if (identity_broken) {
        return 0;
    }

    return 1;
}

int identity_doctor_ok(void) {
    return identity_is_ready();
}

const char* identity_get_state(void) {
    return identity_is_ready() ? "ready" : "broken";
}

const char* identity_get_mode(void) {
    return identity_mode;
}

const char* identity_get_public_key(void) {
    return identity_public_key;
}

void identity_status(void) {
    platform_print("Identity layer:\n");

    platform_print("  state:      ");
    platform_print(identity_get_state());
    platform_print("\n");

    platform_print("  mode:       ");
    platform_print(identity_get_mode());
    platform_print("\n");

    platform_print("  public key: ");
    platform_print(identity_get_public_key());
    platform_print("\n");

    platform_print("  init:       ");
    platform_print(identity_init_done ? "ok\n" : "missing\n");
}

void identity_doctor(void) {
    platform_print("Identity doctor:\n");

    platform_print("  init:   ");
    platform_print(identity_init_done ? "ok\n" : "missing\n");

    platform_print("  ready:  ");
    platform_print(identity_ready ? "ok\n" : "bad\n");

    platform_print("  broken: ");
    platform_print(identity_broken ? "yes\n" : "no\n");

    platform_print("  result: ");
    platform_print(identity_doctor_ok() ? "ready\n" : "blocked\n");
}

void identity_check(void) {
    platform_print("Identity check:\n");

    platform_print("  layer: ");
    platform_print(identity_doctor_ok() ? "ok\n" : "broken\n");
}

void identity_break(void) {
    identity_broken = 1;
    identity_ready = 0;

    platform_print("identity layer broken.\n");
}

void identity_fix(void) {
    identity_init_done = 1;
    identity_ready = 1;
    identity_broken = 0;

    platform_print("identity layer fixed.\n");
}