#include "lang.h"

static language_t current_lang = LANG_EN;

static const char* messages_en[MSG_COUNT] = {
    "System ready",
    "Permission denied",
    "Module not found",
    "Intent running",
    "Doctor check passed",
    "Current language",
    "Language set to English",
    "Language set to Chinese"
};

static const char* messages_zh[MSG_COUNT] = {
    "[ZH] System ready",
    "[ZH] Permission denied",
    "[ZH] Module not found",
    "[ZH] Intent running",
    "[ZH] Doctor check passed",
    "[ZH] Current language",
    "[ZH] Language set to English",
    "[ZH] Language set to Chinese"
};

void lang_init(void) {
    current_lang = LANG_EN;
}

void lang_set(language_t lang) {
    if (lang == LANG_ZH) {
        current_lang = LANG_ZH;
        return;
    }

    current_lang = LANG_EN;
}

language_t lang_get_current(void) {
    return current_lang;
}

const char* lang_get(message_id_t id) {
    if (id < 0 || id >= MSG_COUNT) {
        return "";
    }

    if (current_lang == LANG_ZH) {
        return messages_zh[id];
    }

    return messages_en[id];
}

const char* lang_get_current_name(void) {
    if (current_lang == LANG_ZH) {
        return "zh";
    }

    return "en";
}