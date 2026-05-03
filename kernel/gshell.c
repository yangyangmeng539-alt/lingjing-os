#include "gshell.h"
#include "platform.h"
#include "graphics.h"
#include "bootinfo.h"
#include "module.h"
#include "capability.h"
#include "intent.h"
#include "framebuffer.h"
#include "health.h"
#include "scheduler.h"
#include "timer.h"
#include "memory.h"
#include "paging.h"
#include "syscall.h"
#include "user.h"
#include "ring3.h"
#include "security.h"
#include "identity.h"
#include "lang.h"
#include "version.h"

static int gshell_initialized = 0;
static int gshell_broken = 0;

static unsigned int gshell_render_count = 0;
static unsigned int gshell_blocked_count = 0;
static const char* gshell_last_view = "none";
static const char* gshell_mode = "metadata-only";
static const char* gshell_output_mode = "text-vga";
static const char* gshell_output_plan = "text-vga";
static const char* gshell_output_reason = "default VGA text output";
static unsigned int gshell_output_changes = 0;

#define GSHELL_INPUT_BUFFER_SIZE 64

static char gshell_input_buffer[GSHELL_INPUT_BUFFER_SIZE];
static unsigned int gshell_input_len = 0;
static unsigned int gshell_input_events = 0;
static unsigned int gshell_input_enters = 0;
static unsigned int gshell_input_backspaces = 0;
static const char* gshell_input_status_text = "READY";
static const char* gshell_command_name = "NONE";
static const char* gshell_command_result = "NO COMMAND";
static const char* gshell_command_view = "IDLE";
static unsigned int gshell_command_dispatches = 0;
static unsigned int gshell_command_unknown = 0;
static unsigned int gshell_kernel_bridge_refreshes = 0;
#define GSHELL_HISTORY_SIZE 4
#define GSHELL_HISTORY_TEXT_SIZE 64

static char gshell_history_commands[GSHELL_HISTORY_SIZE][GSHELL_HISTORY_TEXT_SIZE];
static char gshell_history_views[GSHELL_HISTORY_SIZE][GSHELL_HISTORY_TEXT_SIZE];
static unsigned int gshell_history_count = 0;
static unsigned int gshell_history_total = 0;
#define GSHELL_RESULT_LOG_SIZE 4
#define GSHELL_RESULT_LOG_TEXT_SIZE 64

static char gshell_result_log_commands[GSHELL_RESULT_LOG_SIZE][GSHELL_RESULT_LOG_TEXT_SIZE];
static char gshell_result_log_results[GSHELL_RESULT_LOG_SIZE][GSHELL_RESULT_LOG_TEXT_SIZE];
static unsigned int gshell_result_log_count = 0;
static unsigned int gshell_result_log_total = 0;
#define GSHELL_INPUT_VISIBLE_SIZE 40

static char gshell_input_visible_text[GSHELL_INPUT_VISIBLE_SIZE];
#define GSHELL_TERMINAL_LINES 3
#define GSHELL_TERMINAL_TEXT_SIZE 64

static char gshell_terminal_lines[GSHELL_TERMINAL_LINES][GSHELL_TERMINAL_TEXT_SIZE];
static unsigned int gshell_terminal_count = 0;
static unsigned int gshell_terminal_total = 0;

#define GSHELL_COMMAND_TEXT_SIZE 64

#define GSHELL_CMD_EMPTY       0
#define GSHELL_CMD_DASH        1
#define GSHELL_CMD_HEALTH      2
#define GSHELL_CMD_DOCTOR      3
#define GSHELL_CMD_HELP        4
#define GSHELL_CMD_CLEAR       5
#define GSHELL_CMD_HISTORY     6
#define GSHELL_CMD_HISTCLEAR   7
#define GSHELL_CMD_LOG         8
#define GSHELL_CMD_LOGCLEAR    9
#define GSHELL_CMD_MILESTONE   10
#define GSHELL_CMD_STATUS      11
#define GSHELL_CMD_VERSION     12
#define GSHELL_CMD_ABOUT       13
#define GSHELL_CMD_BRIDGE      14
#define GSHELL_CMD_REGISTRY    15
#define GSHELL_CMD_TEXTCMDS    16
#define GSHELL_CMD_SYSINFO     17
#define GSHELL_CMD_MEMINFO     18
#define GSHELL_CMD_PAGEINFO    19
#define GSHELL_CMD_SYSCALLS    20
#define GSHELL_CMD_RING3INFO   21
#define GSHELL_CMD_MODULEINFO  22
#define GSHELL_CMD_MODULECHECK 23
#define GSHELL_CMD_CAPINFO     24
#define GSHELL_CMD_INTENTINFO  25
#define GSHELL_CMD_TASKINFO    26
#define GSHELL_CMD_TASKLIST    27
#define GSHELL_CMD_SCHEDINFO   28
#define GSHELL_CMD_RUNQUEUE    29
#define GSHELL_CMD_SECURITYINFO 30
#define GSHELL_CMD_PLATFORMINFO 31
#define GSHELL_CMD_IDENTITYINFO 32
#define GSHELL_CMD_LANGINFO     33
#define GSHELL_CMD_BOOTINFO    34
#define GSHELL_CMD_FBINFO      35
#define GSHELL_CMD_GFXINFO     36
#define GSHELL_CMD_FONTINFO    37
#define GSHELL_CMD_FSINFO      38
#define GSHELL_CMD_DEVICEINFO  39
#define GSHELL_CMD_DRIVERINFO  40
#define GSHELL_CMD_IOINFO      41
#define GSHELL_CMD_PROCESSINFO 42
#define GSHELL_CMD_PROCINFO    43
#define GSHELL_CMD_RUNTIMEINFO 44
#define GSHELL_CMD_APPINFO     45
#define GSHELL_CMD_ELFINFO     46
#define GSHELL_CMD_LOADERINFO  47
#define GSHELL_CMD_BINARYINFO  48
#define GSHELL_CMD_ABIINFO     49
#define GSHELL_CMD_SANDBOXINFO 50
#define GSHELL_CMD_POLICYINFO  51
#define GSHELL_CMD_PERMISSIONINFO 52
#define GSHELL_CMD_GUARDINFO   53
#define GSHELL_CMD_CONTROLINFO 54
#define GSHELL_CMD_OWNERINFO   55
#define GSHELL_CMD_DECISIONINFO 56
#define GSHELL_CMD_RULEINFO    57
#define GSHELL_CMD_AUDITINFO   58
#define GSHELL_CMD_TRACEINFO   59
#define GSHELL_CMD_LOGINFO     60
#define GSHELL_CMD_EVENTINFO   61
#define GSHELL_CMD_NETWORKINFO 62
#define GSHELL_CMD_NETINFO     63
#define GSHELL_CMD_SOCKETINFO  64
#define GSHELL_CMD_PROTOCOLINFO 65
#define GSHELL_CMD_SERVICEINFO 66
#define GSHELL_CMD_SESSIONINFO 67
#define GSHELL_CMD_CHANNELINFO 68
#define GSHELL_CMD_BUSINFO     69
#define GSHELL_CMD_PACKAGEINFO 70
#define GSHELL_CMD_INSTALLINFO 71
#define GSHELL_CMD_LIFECYCLEINFO 72
#define GSHELL_CMD_LAUNCHINFO  73
#define GSHELL_CMD_OVERVIEWINFO 74
#define GSHELL_CMD_MILESTONEINFO 75
#define GSHELL_CMD_ROADMAPINFO 76
#define GSHELL_CMD_FINALINFO   77
#define GSHELL_CMD_CONTROLSTATUS 78
#define GSHELL_CMD_CONTROLSTRICT 79
#define GSHELL_CMD_CONTROLOPEN 80
#define GSHELL_CMD_BLOCKNET    81
#define GSHELL_CMD_ALLOWNET    82
#define GSHELL_CMD_CHECKNET    83
#define GSHELL_CMD_CAPSTATUS   84
#define GSHELL_CMD_BLOCKFILE   85
#define GSHELL_CMD_ALLOWFILE   86
#define GSHELL_CMD_BLOCKAI     87
#define GSHELL_CMD_ALLOWAI     88
#define GSHELL_CMD_CHECKFILE   89
#define GSHELL_CMD_CHECKAI     90
#define GSHELL_CMD_CHECKGUI    91
#define GSHELL_CMD_INTENTGATE  92
#define GSHELL_CMD_BLOCKVIDEO  93
#define GSHELL_CMD_ALLOWVIDEO  94
#define GSHELL_CMD_BLOCKAIINTENT 95
#define GSHELL_CMD_ALLOWAIINTENT 96
#define GSHELL_CMD_CHECKVIDEO  97
#define GSHELL_CMD_CHECKAIINTENT 98
#define GSHELL_CMD_CHECKNETINTENT 99
#define GSHELL_CMD_MODULEGATE  100
#define GSHELL_CMD_BLOCKEXTERNAL 101
#define GSHELL_CMD_ALLOWEXTERNAL 102
#define GSHELL_CMD_CHECKGUIMOD 103
#define GSHELL_CMD_CHECKNETMOD 104
#define GSHELL_CMD_CHECKAIMOD  105
#define GSHELL_CMD_CHECKCOREMOD 106
#define GSHELL_CMD_CHECKSHELLMOD 107
#define GSHELL_CMD_SANDBOXSTATUS 108
#define GSHELL_CMD_PROFILEOPEN 109
#define GSHELL_CMD_PROFILESAFE 110
#define GSHELL_CMD_PROFILELOCKED 111
#define GSHELL_CMD_CHECKSANDBOX 112
#define GSHELL_CMD_SANDBOXRESET 113
#define GSHELL_CMD_DECISIONSTATUS 114
#define GSHELL_CMD_DECISIONLOG 115
#define GSHELL_CMD_AUDITSTATUS 116
#define GSHELL_CMD_AUDITLAST   117
#define GSHELL_CMD_POLICYTRACE 118
#define GSHELL_CMD_DECISIONRESET 119
#define GSHELL_CMD_RULESTATUS  120
#define GSHELL_CMD_RULEALLOW   121
#define GSHELL_CMD_RULEASK     122
#define GSHELL_CMD_RULEDENY    123
#define GSHELL_CMD_CHECKRULE   124
#define GSHELL_CMD_RULERESET   125
#define GSHELL_CMD_CONTROLPANEL 126
#define GSHELL_CMD_POLICYPANEL 127
#define GSHELL_CMD_GATEPANEL   128
#define GSHELL_CMD_HEALTHPANEL 129
#define GSHELL_CMD_CONTROLRESET 130
#define GSHELL_CMD_GATECHECK   131
#define GSHELL_CMD_CONTROLFINAL 132
#define GSHELL_CMD_CONTROLHEALTH 133
#define GSHELL_CMD_POLICYHEALTH 134
#define GSHELL_CMD_GATESUMMARY 135
#define GSHELL_CMD_SANDBOXSUMMARY 136
#define GSHELL_CMD_RULESUMMARY 137
#define GSHELL_CMD_DECISIONSUMMARY 138
#define GSHELL_CMD_NEXTSTAGE   139
#define GSHELL_CMD_UNKNOWN     255

typedef struct GShellCommandRegistryEntry {
    const char* name;
    unsigned int id;
    const char* view;
    const char* result;
} GShellCommandRegistryEntry;

static const GShellCommandRegistryEntry gshell_command_registry[] = {
    { "dash",        GSHELL_CMD_DASH,        "DASH",        "DASH OK" },
    { "health",      GSHELL_CMD_HEALTH,      "HEALTH",      "HEALTH OK" },
    { "doctor",      GSHELL_CMD_DOCTOR,      "DOCTOR",      "DOCTOR OK" },
    { "status",      GSHELL_CMD_STATUS,      "STATUS",      "STATUS OK" },
    { "version",     GSHELL_CMD_VERSION,     "VERSION",     "VERSION OK" },
    { "about",       GSHELL_CMD_ABOUT,       "ABOUT",       "ABOUT OK" },
    { "bridge",      GSHELL_CMD_BRIDGE,      "BRIDGE",      "BRIDGE OK" },
    { "registry",    GSHELL_CMD_REGISTRY,    "REGISTRY",    "REGISTRY OK" },
    { "textcmds",    GSHELL_CMD_TEXTCMDS,    "TEXTCMDS",    "TEXTCMDS OK" },
    { "sysinfo",     GSHELL_CMD_SYSINFO,     "SYSINFO",     "SYSINFO OK" },
    { "meminfo",     GSHELL_CMD_MEMINFO,     "MEMINFO",     "MEMINFO OK" },
    { "pageinfo",    GSHELL_CMD_PAGEINFO,    "PAGEINFO",    "PAGEINFO OK" },
    { "syscalls",    GSHELL_CMD_SYSCALLS,    "SYSCALLS",    "SYSCALLS OK" },
    { "ring3info",   GSHELL_CMD_RING3INFO,   "RING3INFO",   "RING3 OK" },
    { "moduleinfo",  GSHELL_CMD_MODULEINFO,  "MODULEINFO",  "MODULE INFO OK" },
    { "modulecheck", GSHELL_CMD_MODULECHECK, "MODULECHECK", "MODULE CHECK OK" },
    { "capinfo",     GSHELL_CMD_CAPINFO,     "CAPINFO",     "CAP INFO OK" },
    { "intentinfo",  GSHELL_CMD_INTENTINFO,  "INTENTINFO",  "INTENT INFO OK" },
    { "taskinfo",    GSHELL_CMD_TASKINFO,    "TASKINFO",    "TASK INFO OK" },
    { "tasklist",    GSHELL_CMD_TASKLIST,    "TASKLIST",    "TASK LIST OK" },
    { "schedinfo",   GSHELL_CMD_SCHEDINFO,   "SCHEDINFO",   "SCHED INFO OK" },
    { "runqueue",    GSHELL_CMD_RUNQUEUE,    "RUNQUEUE",    "RUNQUEUE OK" },
    { "securityinfo", GSHELL_CMD_SECURITYINFO, "SECURITYINFO", "SECURITY INFO OK" },
    { "platforminfo", GSHELL_CMD_PLATFORMINFO, "PLATFORMINFO", "PLATFORM INFO OK" },
    { "identityinfo", GSHELL_CMD_IDENTITYINFO, "IDENTITYINFO", "IDENTITY INFO OK" },
    { "langinfo",     GSHELL_CMD_LANGINFO,     "LANGINFO",     "LANG INFO OK" },
    { "bootinfo",    GSHELL_CMD_BOOTINFO,    "BOOTINFO",    "BOOT INFO OK" },
    { "fbinfo",      GSHELL_CMD_FBINFO,      "FBINFO",      "FB INFO OK" },
    { "gfxinfo",     GSHELL_CMD_GFXINFO,     "GFXINFO",     "GFX INFO OK" },
    { "fontinfo",    GSHELL_CMD_FONTINFO,    "FONTINFO",    "FONT INFO OK" },
    { "fsinfo",      GSHELL_CMD_FSINFO,      "FSINFO",      "FS INFO OK" },
    { "deviceinfo",  GSHELL_CMD_DEVICEINFO,  "DEVICEINFO",  "DEVICE INFO OK" },
    { "driverinfo",  GSHELL_CMD_DRIVERINFO,  "DRIVERINFO",  "DRIVER INFO OK" },
    { "ioinfo",      GSHELL_CMD_IOINFO,      "IOINFO",      "IO INFO OK" },
    { "processinfo", GSHELL_CMD_PROCESSINFO, "PROCESSINFO", "PROCESS INFO OK" },
    { "procinfo",    GSHELL_CMD_PROCINFO,    "PROCINFO",    "PROC INFO OK" },
    { "runtimeinfo", GSHELL_CMD_RUNTIMEINFO, "RUNTIMEINFO", "RUNTIME INFO OK" },
    { "appinfo",     GSHELL_CMD_APPINFO,     "APPINFO",     "APP INFO OK" },
    { "elfinfo",     GSHELL_CMD_ELFINFO,     "ELFINFO",     "ELF INFO OK" },
    { "loaderinfo",  GSHELL_CMD_LOADERINFO,  "LOADERINFO",  "LOADER INFO OK" },
    { "binaryinfo",  GSHELL_CMD_BINARYINFO,  "BINARYINFO",  "BINARY INFO OK" },
    { "abiinfo",     GSHELL_CMD_ABIINFO,     "ABIINFO",     "ABI INFO OK" },
    { "sandboxinfo", GSHELL_CMD_SANDBOXINFO, "SANDBOXINFO", "SANDBOX INFO OK" },
    { "policyinfo",  GSHELL_CMD_POLICYINFO,  "POLICYINFO",  "POLICY INFO OK" },
    { "permissioninfo", GSHELL_CMD_PERMISSIONINFO, "PERMISSIONINFO", "PERMISSION INFO OK" },
    { "guardinfo",   GSHELL_CMD_GUARDINFO,   "GUARDINFO",   "GUARD INFO OK" },
    { "controlinfo", GSHELL_CMD_CONTROLINFO, "CONTROLINFO", "CONTROL INFO OK" },
    { "ownerinfo",   GSHELL_CMD_OWNERINFO,   "OWNERINFO",   "OWNER INFO OK" },
    { "decisioninfo", GSHELL_CMD_DECISIONINFO, "DECISIONINFO", "DECISION INFO OK" },
    { "ruleinfo",    GSHELL_CMD_RULEINFO,    "RULEINFO",    "RULE INFO OK" },
    { "auditinfo",   GSHELL_CMD_AUDITINFO,   "AUDITINFO",   "AUDIT INFO OK" },
    { "traceinfo",   GSHELL_CMD_TRACEINFO,   "TRACEINFO",   "TRACE INFO OK" },
    { "loginfo",     GSHELL_CMD_LOGINFO,     "LOGINFO",     "LOG INFO OK" },
    { "eventinfo",   GSHELL_CMD_EVENTINFO,   "EVENTINFO",   "EVENT INFO OK" },
    { "networkinfo", GSHELL_CMD_NETWORKINFO, "NETWORKINFO", "NETWORK INFO OK" },
    { "netinfo",     GSHELL_CMD_NETINFO,     "NETINFO",     "NET INFO OK" },
    { "socketinfo",  GSHELL_CMD_SOCKETINFO,  "SOCKETINFO",  "SOCKET INFO OK" },
    { "protocolinfo", GSHELL_CMD_PROTOCOLINFO, "PROTOCOLINFO", "PROTOCOL INFO OK" },
    { "serviceinfo", GSHELL_CMD_SERVICEINFO, "SERVICEINFO", "SERVICE INFO OK" },
    { "sessioninfo", GSHELL_CMD_SESSIONINFO, "SESSIONINFO", "SESSION INFO OK" },
    { "channelinfo", GSHELL_CMD_CHANNELINFO, "CHANNELINFO", "CHANNEL INFO OK" },
    { "businfo",     GSHELL_CMD_BUSINFO,     "BUSINFO",     "BUS INFO OK" },
    { "packageinfo", GSHELL_CMD_PACKAGEINFO, "PACKAGEINFO", "PACKAGE INFO OK" },
    { "installinfo", GSHELL_CMD_INSTALLINFO, "INSTALLINFO", "INSTALL INFO OK" },
    { "lifecycleinfo", GSHELL_CMD_LIFECYCLEINFO, "LIFECYCLEINFO", "LIFECYCLE INFO OK" },
    { "launchinfo",  GSHELL_CMD_LAUNCHINFO,  "LAUNCHINFO",  "LAUNCH INFO OK" },
    { "overviewinfo", GSHELL_CMD_OVERVIEWINFO, "OVERVIEWINFO", "OVERVIEW INFO OK" },
    { "milestoneinfo", GSHELL_CMD_MILESTONEINFO, "MILESTONEINFO", "MILESTONE INFO OK" },
    { "roadmapinfo", GSHELL_CMD_ROADMAPINFO, "ROADMAPINFO", "ROADMAP INFO OK" },
    { "finalinfo",   GSHELL_CMD_FINALINFO,   "FINALINFO",   "FINAL INFO OK" },
    { "controlstatus", GSHELL_CMD_CONTROLSTATUS, "CONTROLSTATUS", "CONTROL STATUS OK" },
    { "controlstrict", GSHELL_CMD_CONTROLSTRICT, "CONTROLSTATUS", "CONTROL STRICT OK" },
    { "controlopen", GSHELL_CMD_CONTROLOPEN, "CONTROLSTATUS", "CONTROL OPEN OK" },
    { "blocknet",    GSHELL_CMD_BLOCKNET,    "CONTROLSTATUS", "NETWORK BLOCKED" },
    { "allownet",    GSHELL_CMD_ALLOWNET,    "CONTROLSTATUS", "NETWORK ALLOWED" },
    { "checknet",    GSHELL_CMD_CHECKNET,    "CONTROLSTATUS", "NETWORK CHECK OK" },
    { "capstatus",   GSHELL_CMD_CAPSTATUS,   "CAPSTATUS",   "CAP STATUS OK" },
    { "blockfile",   GSHELL_CMD_BLOCKFILE,   "CAPSTATUS",   "FILE BLOCKED" },
    { "allowfile",   GSHELL_CMD_ALLOWFILE,   "CAPSTATUS",   "FILE ALLOWED" },
    { "blockai",     GSHELL_CMD_BLOCKAI,     "CAPSTATUS",   "AI BLOCKED" },
    { "allowai",     GSHELL_CMD_ALLOWAI,     "CAPSTATUS",   "AI ALLOWED" },
    { "checkfile",   GSHELL_CMD_CHECKFILE,   "CAPSTATUS",   "FILE CHECK OK" },
    { "checkai",     GSHELL_CMD_CHECKAI,     "CAPSTATUS",   "AI CHECK OK" },
    { "checkgui",    GSHELL_CMD_CHECKGUI,    "CAPSTATUS",   "GUI CHECK OK" },
    { "intentgate",  GSHELL_CMD_INTENTGATE,  "INTENTGATE",  "INTENT GATE OK" },
    { "blockvideo",  GSHELL_CMD_BLOCKVIDEO,  "INTENTGATE",  "VIDEO INTENT BLOCKED" },
    { "allowvideo",  GSHELL_CMD_ALLOWVIDEO,  "INTENTGATE",  "VIDEO INTENT ALLOWED" },
    { "blockaiintent", GSHELL_CMD_BLOCKAIINTENT, "INTENTGATE", "AI INTENT BLOCKED" },
    { "allowaiintent", GSHELL_CMD_ALLOWAIINTENT, "INTENTGATE", "AI INTENT ALLOWED" },
    { "checkvideo",  GSHELL_CMD_CHECKVIDEO,  "INTENTGATE",  "VIDEO CHECK OK" },
    { "checkaiintent", GSHELL_CMD_CHECKAIINTENT, "INTENTGATE", "AI INTENT CHECK OK" },
    { "checknetintent", GSHELL_CMD_CHECKNETINTENT, "INTENTGATE", "NET INTENT CHECK OK" },
    { "modulegate",  GSHELL_CMD_MODULEGATE,  "MODULEGATE",  "MODULE GATE OK" },
    { "blockexternal", GSHELL_CMD_BLOCKEXTERNAL, "MODULEGATE", "EXTERNAL MODULES BLOCKED" },
    { "allowexternal", GSHELL_CMD_ALLOWEXTERNAL, "MODULEGATE", "EXTERNAL MODULES ALLOWED" },
    { "checkguimod", GSHELL_CMD_CHECKGUIMOD, "MODULEGATE", "GUI MODULE CHECK OK" },
    { "checknetmod", GSHELL_CMD_CHECKNETMOD, "MODULEGATE", "NET MODULE CHECK OK" },
    { "checkaimod",  GSHELL_CMD_CHECKAIMOD,  "MODULEGATE", "AI MODULE CHECK OK" },
    { "checkcoremod", GSHELL_CMD_CHECKCOREMOD, "MODULEGATE", "CORE MODULE CHECK OK" },
    { "checkshellmod", GSHELL_CMD_CHECKSHELLMOD, "MODULEGATE", "SHELL MODULE CHECK OK" },
    { "sandboxstatus", GSHELL_CMD_SANDBOXSTATUS, "SANDBOXSTATUS", "SANDBOX STATUS OK" },
    { "profileopen", GSHELL_CMD_PROFILEOPEN, "SANDBOXSTATUS", "PROFILE OPEN OK" },
    { "profilesafe", GSHELL_CMD_PROFILESAFE, "SANDBOXSTATUS", "PROFILE SAFE OK" },
    { "profilelocked", GSHELL_CMD_PROFILELOCKED, "SANDBOXSTATUS", "PROFILE LOCKED OK" },
    { "checksandbox", GSHELL_CMD_CHECKSANDBOX, "SANDBOXSTATUS", "SANDBOX CHECK OK" },
    { "sandboxreset", GSHELL_CMD_SANDBOXRESET, "SANDBOXSTATUS", "SANDBOX RESET OK" },
    { "decisionstatus", GSHELL_CMD_DECISIONSTATUS, "DECISIONSTATUS", "DECISION STATUS OK" },
    { "decisionlog", GSHELL_CMD_DECISIONLOG, "DECISIONSTATUS", "DECISION LOG OK" },
    { "auditstatus", GSHELL_CMD_AUDITSTATUS, "DECISIONSTATUS", "AUDIT STATUS OK" },
    { "auditlast",   GSHELL_CMD_AUDITLAST,   "DECISIONSTATUS", "AUDIT LAST OK" },
    { "policytrace", GSHELL_CMD_POLICYTRACE, "DECISIONSTATUS", "POLICY TRACE OK" },
    { "decisionreset", GSHELL_CMD_DECISIONRESET, "DECISIONSTATUS", "DECISION RESET OK" },
    { "rulestatus",  GSHELL_CMD_RULESTATUS,  "RULESTATUS",  "RULE STATUS OK" },
    { "ruleallow",   GSHELL_CMD_RULEALLOW,   "RULESTATUS",  "RULE DEFAULT ALLOW" },
    { "ruleask",     GSHELL_CMD_RULEASK,     "RULESTATUS",  "RULE DEFAULT ASK" },
    { "ruledeny",    GSHELL_CMD_RULEDENY,    "RULESTATUS",  "RULE DEFAULT DENY" },
    { "checkrule",   GSHELL_CMD_CHECKRULE,   "RULESTATUS",  "RULE CHECK OK" },
    { "rulereset",   GSHELL_CMD_RULERESET,   "RULESTATUS",  "RULE RESET OK" },
    { "controlpanel", GSHELL_CMD_CONTROLPANEL, "CONTROLPANEL", "CONTROL PANEL OK" },
    { "policypanel", GSHELL_CMD_POLICYPANEL, "CONTROLPANEL", "POLICY PANEL OK" },
    { "gatepanel",   GSHELL_CMD_GATEPANEL,   "CONTROLPANEL", "GATE PANEL OK" },
    { "healthpanel", GSHELL_CMD_HEALTHPANEL, "CONTROLPANEL", "HEALTH PANEL OK" },
    { "controlreset", GSHELL_CMD_CONTROLRESET, "CONTROLPANEL", "CONTROL RESET OK" },
    { "gatecheck",   GSHELL_CMD_GATECHECK,   "CONTROLPANEL", "GATE CHECK OK" },
    { "controlfinal", GSHELL_CMD_CONTROLFINAL, "CONTROLFINAL", "CONTROL FINAL OK" },
    { "controlhealth", GSHELL_CMD_CONTROLHEALTH, "CONTROLFINAL", "CONTROL HEALTH OK" },
    { "policyhealth", GSHELL_CMD_POLICYHEALTH, "CONTROLFINAL", "POLICY HEALTH OK" },
    { "gatesummary", GSHELL_CMD_GATESUMMARY, "CONTROLFINAL", "GATE SUMMARY OK" },
    { "sandboxsummary", GSHELL_CMD_SANDBOXSUMMARY, "CONTROLFINAL", "SANDBOX SUMMARY OK" },
    { "rulesummary", GSHELL_CMD_RULESUMMARY, "CONTROLFINAL", "RULE SUMMARY OK" },
    { "decisionsummary", GSHELL_CMD_DECISIONSUMMARY, "CONTROLFINAL", "DECISION SUMMARY OK" },
    { "nextstage",   GSHELL_CMD_NEXTSTAGE,   "CONTROLFINAL", "NEXT STAGE OK" },
    { "help",        GSHELL_CMD_HELP,        "HELP",        "HELP OK" },
    { "history",     GSHELL_CMD_HISTORY,     "HISTORY",     "HISTORY OK" },
    { "histclear",   GSHELL_CMD_HISTCLEAR,   "HISTCLEAR",   "HIST CLEARED" },
    { "log",         GSHELL_CMD_LOG,         "LOG",         "LOG OK" },
    { "logclear",    GSHELL_CMD_LOGCLEAR,    "LOGCLEAR",    "LOG CLEARED" },
    { "clear",       GSHELL_CMD_CLEAR,       "CLEAR",       "CLEAR OK" },
    { "milestone",   GSHELL_CMD_MILESTONE,   "MILESTONE",   "MILESTONE OK" }
};

static const unsigned int gshell_command_registry_count =
    sizeof(gshell_command_registry) / sizeof(gshell_command_registry[0]);

static char gshell_command_normalized[GSHELL_COMMAND_TEXT_SIZE];
static unsigned int gshell_last_command_id = GSHELL_CMD_EMPTY;
static const char* gshell_parser_status_text = "READY";

static void gshell_copy_text(char* dst, const char* src, unsigned int max_len);
static void gshell_history_clear(void);
static void gshell_history_push(const char* command, const char* view);
static void gshell_result_log_clear(void);
static void gshell_result_log_push(const char* command, const char* result);
static void gshell_terminal_clear(void);
static void gshell_terminal_push(const char* text);

static int gshell_is_command_space(char c);
static char gshell_to_lower_char(char c);
static void gshell_parser_reset_text(void);
static void gshell_prepare_command_text(void);
static unsigned int gshell_parse_command_id(void);

void gshell_init(void) {
    unsigned int i = 0;

    gshell_initialized = 1;
    gshell_broken = 0;
    gshell_render_count = 0;
    gshell_blocked_count = 0;
    gshell_last_view = "none";
    gshell_mode = "metadata-only";

    gshell_output_mode = "text-vga";
    gshell_output_plan = "text-vga";
    gshell_output_reason = "default VGA text output";
    gshell_output_changes = 0;

    gshell_input_len = 0;
    gshell_input_events = 0;
    gshell_input_enters = 0;
    gshell_input_backspaces = 0;
    gshell_input_status_text = "READY";

    gshell_command_name = "NONE";
    gshell_command_result = "NO COMMAND";
    gshell_command_view = "IDLE";
    gshell_command_dispatches = 0;
    gshell_command_unknown = 0;
    gshell_kernel_bridge_refreshes = 0;

    gshell_last_command_id = GSHELL_CMD_EMPTY;
    gshell_parser_status_text = "READY";
    gshell_parser_reset_text();

    for (i = 0; i < GSHELL_INPUT_BUFFER_SIZE; i++) {
        gshell_input_buffer[i] = '\0';
    }

    gshell_history_clear();
    gshell_result_log_clear();
    gshell_terminal_clear();

    gshell_terminal_push("LINGJING GRAPHICAL TERMINAL READY");
    gshell_terminal_push("TYPE HELP");
}

int gshell_is_ready(void) {
    if (!gshell_initialized) {
        return 0;
    }

    if (gshell_broken) {
        return 0;
    }

    if (!graphics_is_ready()) {
        return 0;
    }

    if (!module_dependency_ok("gshell")) {
        return 0;
    }

    if (!capability_is_ready("gui.shell")) {
        return 0;
    }

    return 1;
}

int gshell_is_broken(void) {
    return gshell_broken;
}

int gshell_output_graphics_planned(void) {
    if (gshell_output_plan[0] == 'g') {
        return 1;
    }

    return 0;
}

const char* gshell_get_output_mode(void) {
    return gshell_output_mode;
}

const char* gshell_get_output_plan(void) {
    return gshell_output_plan;
}

unsigned int gshell_get_render_count(void) {
    return gshell_render_count;
}

const char* gshell_get_last_view(void) {
    return gshell_last_view;
}

const char* gshell_get_mode(void) {
    return gshell_mode;
}

void gshell_status(void) {
    platform_print("Graphical shell status:\n");

    platform_print("  initialized: ");
    platform_print(gshell_initialized ? "yes\n" : "no\n");

    platform_print("  broken:      ");
    platform_print(gshell_broken ? "yes\n" : "no\n");

    platform_print("  mode:        ");
    platform_print(gshell_mode);
    platform_print("\n");

    platform_print("  output:      ");
    platform_print(gshell_output_mode);
    platform_print("\n");

    platform_print("  output plan: ");
    platform_print(gshell_output_plan);
    platform_print("\n");

    platform_print("  graphics:    ");
    platform_print(graphics_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  display:     ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  capability:  ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  module deps: ");
    platform_print(module_dependency_ok("gshell") ? "ok\n" : "broken\n");

    platform_print("  last view:   ");
    platform_print(gshell_last_view);
    platform_print("\n");

    platform_print("  renders:     ");
    platform_print_uint(gshell_render_count);
    platform_print("\n");

    platform_print("  blocked:     ");
    platform_print_uint(gshell_blocked_count);
    platform_print("\n");

    platform_print("  ready:       ");
    platform_print(gshell_is_ready() ? "yes\n" : "no\n");
}

void gshell_check(void) {
    platform_print("Graphical shell check:\n");

    platform_print("  initialized: ");
    platform_print(gshell_initialized ? "ok\n" : "bad\n");

    platform_print("  graphics:    ");
    platform_print(graphics_is_ready() ? "ready\n" : "blocked\n");

    platform_print("  capability:  ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  module deps: ");
    platform_print(module_dependency_ok("gshell") ? "ok\n" : "broken\n");

    platform_print("  broken:      ");
    platform_print(gshell_broken ? "yes\n" : "no\n");

    platform_print("  result:      ");

    if (gshell_broken) {
        platform_print("broken\n");
        return;
    }

    if (!gshell_initialized) {
        platform_print("blocked\n");
        return;
    }

    if (!graphics_is_ready()) {
        platform_print("blocked\n");
        return;
    }

    if (!capability_is_ready("gui.shell")) {
        platform_print("blocked\n");
        return;
    }

    if (!module_dependency_ok("gshell")) {
        platform_print("blocked\n");
        return;
    }

    platform_print("ready\n");
}

void gshell_doctor(void) {
    platform_print("Graphical shell doctor:\n");

    platform_print("  layer:       ");
    platform_print(gshell_initialized ? "initialized\n" : "not initialized\n");

    platform_print("  mode:        metadata-only\n");

    platform_print("  output:      ");
    platform_print(gshell_output_mode);
    platform_print("\n");

    platform_print("  output plan: ");
    platform_print(gshell_output_plan);
    platform_print("\n");

    platform_print("  graphics:    ");
    platform_print(graphics_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  display:     ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  capability:  ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  module deps: ");
    platform_print(module_dependency_ok("gshell") ? "ok\n" : "broken\n");

    platform_print("  intent:      ");
    platform_print(intent_get_current_name());
    platform_print("\n");

    platform_print("  running:     ");
    platform_print(intent_is_running() ? "yes\n" : "no\n");

    platform_print("  self-output: ");
    if (gshell_output_plan[0] == 'g') {
        platform_print("planned\n");
    } else {
        platform_print("not planned\n");
    }

    platform_print("  broken:      ");
    platform_print(gshell_broken ? "yes\n" : "no\n");

    platform_print("  result:      ");

    if (gshell_broken) {
        platform_print("broken\n");
        return;
    }

    if (!gshell_initialized) {
        platform_print("blocked\n");
        return;
    }

    if (!graphics_is_ready()) {
        platform_print("blocked\n");
        return;
    }

    if (!capability_is_ready("gui.shell")) {
        platform_print("blocked\n");
        return;
    }

    if (!module_dependency_ok("gshell")) {
        platform_print("blocked\n");
        return;
    }

    platform_print("ready\n");
}

static void gshell_render_begin(const char* view) {
    gshell_last_view = view;
    gshell_render_count++;

    platform_print("Graphical shell render:\n");

    platform_print("  view:   ");
    platform_print(view);
    platform_print("\n");

    platform_print("  mode:   ");
    platform_print(gshell_mode);
    platform_print("\n");
}

static int gshell_render_guard(void) {
    if (!gshell_is_ready()) {
        gshell_blocked_count++;

        platform_print("  result: blocked\n");
        platform_print("  reason: graphical shell not ready\n");
        return 0;
    }

    return 1;
}

void gshell_panel(void) {
    gshell_render_begin("panel");

    if (!gshell_render_guard()) {
        return;
    }

    graphics_panel();

    platform_print("  title:  Lingjing OS Runtime\n");
    platform_print("  blocks: intent capability module task health doctor\n");

    platform_print("  intent: ");
    platform_print(intent_get_current_name());
    platform_print("\n");

    platform_print("  running:");
    platform_print(intent_is_running() ? " yes\n" : " no\n");

    platform_print("  gui.shell: ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  result: staged\n");
}

void gshell_runtime(void) {
    gshell_render_begin("runtime");

    if (!gshell_render_guard()) {
        return;
    }

    graphics_rect(16, 16, 360, 220, 0x00112233);
    graphics_text(28, 32, "Lingjing Runtime");
    graphics_text(28, 56, "intent -> capability -> module -> task");
    graphics_text(28, 80, "graphics backend: gated");

    platform_print("  runtime chain:\n");
    platform_print("    intent\n");
    platform_print("    capability\n");
    platform_print("    module\n");
    platform_print("    task context\n");
    platform_print("    health / doctor\n");

    platform_print("  current intent: ");
    platform_print(intent_get_current_name());
    platform_print("\n");

    platform_print("  running:        ");
    platform_print(intent_is_running() ? "yes\n" : "no\n");

    intent_context_report();

    platform_print("  result: staged\n");
}

void gshell_intent(void) {
    gshell_render_begin("intent");

    if (!gshell_render_guard()) {
        return;
    }

    graphics_rect(16, 16, 360, 180, 0x00002244);
    graphics_text(28, 32, "Intent Panel");
    graphics_text(28, 56, intent_get_current_name());

    platform_print("  current intent: ");
    platform_print(intent_get_current_name());
    platform_print("\n");

    platform_print("  running:        ");
    platform_print(intent_is_running() ? "yes\n" : "no\n");

    intent_context_report();

    platform_print("  result:         staged\n");
}

void gshell_system(void) {
    int deps_broken = module_has_broken_dependencies();

    gshell_render_begin("system");

    if (!gshell_render_guard()) {
        return;
    }

    graphics_rect(16, 16, 360, 200, 0x00003322);
    graphics_text(28, 32, "System Panel");

    platform_print("  module deps: ");
    platform_print(deps_broken ? "broken\n" : "ok\n");

    platform_print("  health:      ");
    platform_print(deps_broken ? "bad\n" : "ok\n");

    platform_print("  graphics:    ");
    platform_print(graphics_is_ready() ? "ready\n" : "blocked\n");

    platform_print("  gui.shell:   ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  result:      staged\n");
}

void gshell_dashboard(void) {
    int deps_broken = module_has_broken_dependencies();

    gshell_render_begin("dashboard");

    if (!gshell_render_guard()) {
        return;
    }

    graphics_rect(8, 8, 480, 300, 0x00101022);
    graphics_text(20, 24, "Lingjing OS Dashboard");
    graphics_text(20, 48, "Intent / Capability / Module / Task / Doctor");

    platform_print("Graphical runtime dashboard:\n");

    platform_print("  intent:      ");
    platform_print(intent_get_current_name());
    platform_print("\n");

    platform_print("  running:     ");
    platform_print(intent_is_running() ? "yes\n" : "no\n");

    platform_print("  module deps: ");
    platform_print(deps_broken ? "broken\n" : "ok\n");

    platform_print("  graphics:    ");
    platform_print(graphics_is_ready() ? "ready\n" : "blocked\n");

    platform_print("  gshell:      ");
    platform_print(gshell_is_ready() ? "ready\n" : "blocked\n");

    platform_print("  gui.shell:   ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  gui.graphics:");
    platform_print(capability_is_ready("gui.graphics") ? " ready\n" : " blocked\n");

    platform_print("  gui.fb:      ");
    platform_print(capability_is_ready("gui.framebuffer") ? "ready\n" : "blocked\n");

    intent_context_report();

    platform_print("  result:      staged\n");
}

void gshell_text_panel(void) {
    int deps_broken = module_has_broken_dependencies();

    gshell_render_begin("text-panel");

    if (!gshell_render_guard()) {
        return;
    }

    platform_clear();

    platform_print("+------------------------------------------------------------------------------+\n");
    platform_print("|                            Lingjing OS Runtime                               |\n");
    platform_print("+------------------------------------------------------------------------------+\n");
    platform_print("| mode: text-safe VGA shell                                                    |\n");
    platform_print("| view: graphical shell text panel                                             |\n");
    platform_print("+------------------------------------------------------------------------------+\n");
    platform_print("| Intent                                                                       |\n");
    platform_print("|   current: ");

    platform_print(intent_get_current_name());

    platform_print("\n");
    platform_print("|   running: ");

    platform_print(intent_is_running() ? "yes" : "no");

    platform_print("\n");
    platform_print("+------------------------------------------------------------------------------+\n");
    platform_print("| Runtime Chain                                                                |\n");
    platform_print("|   intent -> capability -> module -> task context -> health -> doctor         |\n");
    platform_print("+------------------------------------------------------------------------------+\n");
    platform_print("| System                                                                       |\n");
    platform_print("|   module deps: ");

    platform_print(deps_broken ? "broken" : "ok");

    platform_print("\n");
    platform_print("|   graphics:    ");

    platform_print(graphics_is_ready() ? "ready" : "blocked");

    platform_print("\n");
    platform_print("|   gui.shell:   ");

    platform_print(capability_is_ready("gui.shell") ? "ready" : "blocked");

    platform_print("\n");
    platform_print("+------------------------------------------------------------------------------+\n");
    platform_print("| Status                                                                       |\n");
    platform_print("|   this is not real framebuffer graphics                                      |\n");
    platform_print("|   this is a text-mode graphical shell prototype                              |\n");
    platform_print("+------------------------------------------------------------------------------+\n");

    platform_print("result: staged\n");
}

void gshell_text_dashboard(void) {
    int deps_broken = module_has_broken_dependencies();

    gshell_render_begin("text-dashboard");

    if (!gshell_render_guard()) {
        return;
    }

    platform_clear();

    platform_print("Lingjing OS Dashboard\n");
    platform_print("================================================================================\n");

    platform_print("Boot / Display\n");
    platform_print("  boot mode:      text-safe\n");
    platform_print("  framebuffer:    guarded\n");
    platform_print("  real graphics:  disabled\n");
    platform_print("  text UI:        enabled\n");
    platform_print("\n");

    platform_print("Runtime\n");
    platform_print("  intent:         ");
    platform_print(intent_get_current_name());
    platform_print("\n");

    platform_print("  running:        ");
    platform_print(intent_is_running() ? "yes" : "no");
    platform_print("\n");

    platform_print("  module deps:    ");
    platform_print(deps_broken ? "broken" : "ok");
    platform_print("\n");

    platform_print("  graphics:       ");
    platform_print(graphics_is_ready() ? "ready" : "blocked");
    platform_print("\n");

    platform_print("  gshell:         ");
    platform_print(gshell_is_ready() ? "ready" : "blocked");
    platform_print("\n");

    platform_print("\n");

    platform_print("Capabilities\n");
    platform_print("  gui.framebuffer:");
    platform_print(capability_is_ready("gui.framebuffer") ? " ready\n" : " blocked\n");

    platform_print("  gui.graphics:   ");
    platform_print(capability_is_ready("gui.graphics") ? "ready\n" : "blocked\n");

    platform_print("  gui.shell:      ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("\n");

    intent_context_report();

    platform_print("================================================================================\n");
    platform_print("result: staged\n");
}

void gshell_text_compact(void) {
    int deps_broken = module_has_broken_dependencies();

    gshell_render_begin("text-compact");

    if (!gshell_render_guard()) {
        return;
    }

    platform_clear();

    platform_print("Lingjing OS | text-mode graphical shell\n");
    platform_print("================================================================================\n");

    platform_print("intent: ");
    platform_print(intent_get_current_name());
    platform_print(" | run: ");
    platform_print(intent_is_running() ? "yes" : "no");
    platform_print(" | deps: ");
    platform_print(deps_broken ? "bad" : "ok");
    platform_print(" | gfx: ");
    platform_print(graphics_is_ready() ? "ready" : "bad");
    platform_print("\n");

    platform_print("cap: fb=");
    platform_print(capability_is_ready("gui.framebuffer") ? "ok" : "bad");
    platform_print(" gfx=");
    platform_print(capability_is_ready("gui.graphics") ? "ok" : "bad");
    platform_print(" shell=");
    platform_print(capability_is_ready("gui.shell") ? "ok" : "bad");
    platform_print("\n");

    platform_print("chain: intent -> capability -> module -> ctx -> health -> doctor\n");

    platform_print("display: ");
    platform_print(framebuffer_get_profile());
    platform_print(" | pixel-write: gated\n");

    platform_print("--------------------------------------------------------------------------------\n");

    if (intent_is_running()) {
        platform_print("ctx: active | intent=");
        platform_print(intent_get_current_name());
        platform_print(" | mods=gui,net,ai\n");

        platform_print("caps: gd=gui.display nh=net.http aa=ai.assist\n");
    } else {
        platform_print("ctx: inactive | intent=none | mods=none\n");
        platform_print("caps: none\n");
    }

    platform_print("--------------------------------------------------------------------------------\n");
    platform_print("cmd: gshell textcompact | gshell statusbar | health short | doctor short\n");
    platform_print("================================================================================\n");
    platform_print("result: staged\n");
}

void gshell_statusbar(void) {
    int deps_broken = module_has_broken_dependencies();

    gshell_render_begin("statusbar");

    if (!gshell_render_guard()) {
        return;
    }

    platform_print("LJ | intent=");
    platform_print(intent_get_current_name());
    platform_print(" | run=");
    platform_print(intent_is_running() ? "yes" : "no");
    platform_print(" | deps=");
    platform_print(deps_broken ? "bad" : "ok");
    platform_print(" | gfx=");
    platform_print(graphics_is_ready() ? "ready" : "blocked");
    platform_print(" | shell=");
    platform_print(capability_is_ready("gui.shell") ? "ready" : "blocked");
    platform_print("\n");
}

void gshell_output(void) {
    platform_print("GShell output:\n");

    platform_print("  current: ");
    platform_print(gshell_output_mode);
    platform_print("\n");

    platform_print("  planned: ");
    platform_print(gshell_output_plan);
    platform_print("\n");

    platform_print("  changes: ");
    platform_print_uint(gshell_output_changes);
    platform_print("\n");

    platform_print("  reason:  ");
    platform_print(gshell_output_reason);
    platform_print("\n");

    platform_print("  result:  ready\n");
}

void gshell_output_doctor(void) {
    platform_print("GShell output doctor:\n");

    platform_print("  current output: ");
    platform_print(gshell_output_mode);
    platform_print("\n");

    platform_print("  planned output: ");
    platform_print(gshell_output_plan);
    platform_print("\n");

    platform_print("  text-vga:       ");
    platform_print(gshell_output_mode[0] == 't' ? "active\n" : "inactive\n");

    platform_print("  graphics-self:  ");
    if (gshell_output_plan[0] == 'g') {
        platform_print("planned\n");
    } else {
        platform_print("not planned\n");
    }

    platform_print("  graphics:       ");
    platform_print(graphics_is_ready() ? "ready\n" : "blocked\n");

    platform_print("  text backend:   ");
    platform_print(graphics_text_backend_is_armed() ? "armed\n" : "disarmed\n");

    platform_print("  shell cap:      ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  can self output:");

    if (!graphics_is_ready()) {
        platform_print(" no\n");
        platform_print("  reason:         graphics not ready\n");
    } else if (!graphics_text_backend_is_armed()) {
        platform_print(" no\n");
        platform_print("  reason:         graphics text backend disarmed\n");
    } else {
        platform_print(" staged\n");
        platform_print("  reason:         self-output can be tested later\n");
    }

    platform_print("  result:         ready\n");
}

void gshell_output_text(void) {
    gshell_output_plan = "text-vga";
    gshell_output_reason = "user selected VGA text output";
    gshell_output_changes++;

    platform_print("GShell output text:\n");
    platform_print("  planned: text-vga\n");
    platform_print("  real switch: no\n");
    platform_print("  result:  staged\n");
}

void gshell_output_graphics(void) {
    gshell_output_plan = "graphics-self";
    gshell_output_reason = "user planned graphics shell self-output";
    gshell_output_changes++;

    platform_print("GShell output graphics:\n");
    platform_print("  planned: graphics-self\n");
    platform_print("  real switch: no\n");
    platform_print("  warning: requires framebuffer graphics mode later\n");
    platform_print("  result:  staged\n");
}

void gshell_self_check(void) {
    platform_print("GShell self-output check:\n");

    platform_print("  current output: ");
    platform_print(gshell_output_mode);
    platform_print("\n");

    platform_print("  planned output: ");
    platform_print(gshell_output_plan);
    platform_print("\n");

    platform_print("  display:        ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  graphics:       ");
    platform_print(graphics_is_ready() ? "ready\n" : "blocked\n");

    platform_print("  shell cap:      ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  text backend:   ");
    platform_print(graphics_text_backend_is_armed() ? "armed\n" : "disarmed\n");

    platform_print("  real gate:      ");
    platform_print(graphics_real_is_armed() ? "armed\n" : "disarmed\n");

    platform_print("  framebuffer:    ");
    platform_print(framebuffer_is_ready() ? "ready\n" : "not ready\n");

    platform_print("  self-output:    ");

    if (gshell_output_plan[0] != 'g') {
        platform_print("blocked\n");
        platform_print("  reason:         gshell graphics-self not planned\n");
        return;
    }

    if (!graphics_is_ready()) {
        platform_print("blocked\n");
        platform_print("  reason:         graphics layer not ready\n");
        return;
    }

    if (!capability_is_ready("gui.shell")) {
        platform_print("blocked\n");
        platform_print("  reason:         gui.shell capability blocked\n");
        return;
    }

    if (!graphics_text_backend_is_armed()) {
        platform_print("blocked\n");
        platform_print("  reason:         graphics text backend disarmed\n");
        return;
    }

    platform_print("staged\n");
    platform_print("  reason:         graphics-self output metadata ready\n");
}

void gshell_self_render(void) {
    gshell_render_begin("self-render");

    platform_print("GShell self-render dryrun:\n");

    platform_print("  planned output: ");
    platform_print(gshell_output_plan);
    platform_print("\n");

    platform_print("  display:        ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  backend:        graphics text dryrun\n");

    platform_print("  result:         ");

    if (gshell_output_plan[0] != 'g') {
        gshell_blocked_count++;
        platform_print("blocked\n");
        platform_print("  reason:         gshell graphics-self not planned\n");
        return;
    }

    if (!graphics_is_ready()) {
        gshell_blocked_count++;
        platform_print("blocked\n");
        platform_print("  reason:         graphics layer not ready\n");
        return;
    }

    if (!capability_is_ready("gui.shell")) {
        gshell_blocked_count++;
        platform_print("blocked\n");
        platform_print("  reason:         gui.shell capability blocked\n");
        return;
    }

    if (!graphics_text_backend_is_armed()) {
        gshell_blocked_count++;
        platform_print("blocked\n");
        platform_print("  reason:         graphics text backend disarmed\n");
        return;
    }

    graphics_text(16, 16, "Lingjing OS Runtime");
    graphics_text(16, 32, "intent capability module task health doctor");
    graphics_text(16, 48, "graphics-self-output staged");

    platform_print("  self render:    staged\n");
}

void gshell_runtime_check(void) {
    int deps_broken = module_has_broken_dependencies();

    platform_print("Graphical runtime check:\n");

    platform_print("  framebuffer: ");
    platform_print(capability_is_ready("gui.framebuffer") ? "ready\n" : "blocked\n");

    platform_print("  graphics:    ");
    platform_print(capability_is_ready("gui.graphics") ? "ready\n" : "blocked\n");

    platform_print("  display:        ");
    platform_print(framebuffer_get_profile());
    platform_print("\n");

    platform_print("  shell:       ");
    platform_print(capability_is_ready("gui.shell") ? "ready\n" : "blocked\n");

    platform_print("  module deps: ");
    platform_print(deps_broken ? "broken\n" : "ok\n");

    platform_print("  intent:      ");
    platform_print(intent_get_current_name());
    platform_print("\n");

    platform_print("  running:     ");
    platform_print(intent_is_running() ? "yes\n" : "no\n");

    platform_print("  result:      ");

    if (deps_broken) {
        platform_print("blocked\n");
        return;
    }

    if (!capability_is_ready("gui.framebuffer")) {
        platform_print("blocked\n");
        return;
    }

    if (!capability_is_ready("gui.graphics")) {
        platform_print("blocked\n");
        return;
    }

    if (!capability_is_ready("gui.shell")) {
        platform_print("blocked\n");
        return;
    }

    platform_print("ready\n");
}

static void gshell_copy_text(char* dst, const char* src, unsigned int max_len) {
    unsigned int i = 0;

    if (!dst || max_len == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[i] != '\0' && i + 1 < max_len) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static void gshell_history_clear(void) {
    unsigned int i = 0;

    gshell_history_count = 0;
    gshell_history_total = 0;

    for (i = 0; i < GSHELL_HISTORY_SIZE; i++) {
        gshell_history_commands[i][0] = '\0';
        gshell_history_views[i][0] = '\0';
    }
}

static void gshell_history_push(const char* command, const char* view) {
    unsigned int i = 0;

    if (!command || command[0] == '\0') {
        return;
    }

    if (gshell_history_count < GSHELL_HISTORY_SIZE) {
        gshell_copy_text(
            gshell_history_commands[gshell_history_count],
            command,
            GSHELL_HISTORY_TEXT_SIZE
        );

        gshell_copy_text(
            gshell_history_views[gshell_history_count],
            view,
            GSHELL_HISTORY_TEXT_SIZE
        );

        gshell_history_count++;
        gshell_history_total++;
        return;
    }

    for (i = 1; i < GSHELL_HISTORY_SIZE; i++) {
        gshell_copy_text(
            gshell_history_commands[i - 1],
            gshell_history_commands[i],
            GSHELL_HISTORY_TEXT_SIZE
        );

        gshell_copy_text(
            gshell_history_views[i - 1],
            gshell_history_views[i],
            GSHELL_HISTORY_TEXT_SIZE
        );
    }

    gshell_copy_text(
        gshell_history_commands[GSHELL_HISTORY_SIZE - 1],
        command,
        GSHELL_HISTORY_TEXT_SIZE
    );

    gshell_copy_text(
        gshell_history_views[GSHELL_HISTORY_SIZE - 1],
        view,
        GSHELL_HISTORY_TEXT_SIZE
    );

    gshell_history_total++;
}

static void gshell_result_log_clear(void) {
    unsigned int i = 0;

    gshell_result_log_count = 0;
    gshell_result_log_total = 0;

    for (i = 0; i < GSHELL_RESULT_LOG_SIZE; i++) {
        gshell_result_log_commands[i][0] = '\0';
        gshell_result_log_results[i][0] = '\0';
    }
}

static void gshell_result_log_push(const char* command, const char* result) {
    unsigned int i = 0;

    if (!command || command[0] == '\0') {
        return;
    }

    if (gshell_result_log_count < GSHELL_RESULT_LOG_SIZE) {
        gshell_copy_text(
            gshell_result_log_commands[gshell_result_log_count],
            command,
            GSHELL_RESULT_LOG_TEXT_SIZE
        );

        gshell_copy_text(
            gshell_result_log_results[gshell_result_log_count],
            result,
            GSHELL_RESULT_LOG_TEXT_SIZE
        );

        gshell_result_log_count++;
        gshell_result_log_total++;
        return;
    }

    for (i = 1; i < GSHELL_RESULT_LOG_SIZE; i++) {
        gshell_copy_text(
            gshell_result_log_commands[i - 1],
            gshell_result_log_commands[i],
            GSHELL_RESULT_LOG_TEXT_SIZE
        );

        gshell_copy_text(
            gshell_result_log_results[i - 1],
            gshell_result_log_results[i],
            GSHELL_RESULT_LOG_TEXT_SIZE
        );
    }

    gshell_copy_text(
        gshell_result_log_commands[GSHELL_RESULT_LOG_SIZE - 1],
        command,
        GSHELL_RESULT_LOG_TEXT_SIZE
    );

    gshell_copy_text(
        gshell_result_log_results[GSHELL_RESULT_LOG_SIZE - 1],
        result,
        GSHELL_RESULT_LOG_TEXT_SIZE
    );

    gshell_result_log_total++;
}

static void gshell_terminal_clear(void) {
    unsigned int i = 0;

    gshell_terminal_count = 0;
    gshell_terminal_total = 0;

    for (i = 0; i < GSHELL_TERMINAL_LINES; i++) {
        gshell_terminal_lines[i][0] = '\0';
    }
}

static void gshell_terminal_push(const char* text) {
    unsigned int i = 0;

    if (!text || text[0] == '\0') {
        return;
    }

    if (gshell_terminal_count < GSHELL_TERMINAL_LINES) {
        gshell_copy_text(
            gshell_terminal_lines[gshell_terminal_count],
            text,
            GSHELL_TERMINAL_TEXT_SIZE
        );

        gshell_terminal_count++;
        gshell_terminal_total++;
        return;
    }

    for (i = 1; i < GSHELL_TERMINAL_LINES; i++) {
        gshell_copy_text(
            gshell_terminal_lines[i - 1],
            gshell_terminal_lines[i],
            GSHELL_TERMINAL_TEXT_SIZE
        );
    }

    gshell_copy_text(
        gshell_terminal_lines[GSHELL_TERMINAL_LINES - 1],
        text,
        GSHELL_TERMINAL_TEXT_SIZE
    );

    gshell_terminal_total++;
}

static int gshell_text_equal(const char* a, const char* b) {
    unsigned int i = 0;

    if (!a || !b) {
        return 0;
    }

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }

        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

static int gshell_is_command_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char gshell_to_lower_char(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + 32);
    }

    return c;
}

static void gshell_parser_reset_text(void) {
    unsigned int i = 0;

    for (i = 0; i < GSHELL_COMMAND_TEXT_SIZE; i++) {
        gshell_command_normalized[i] = '\0';
    }
}

static void gshell_prepare_command_text(void) {
    unsigned int src_start = 0;
    unsigned int src_end = 0;
    unsigned int out = 0;
    char c = '\0';

    gshell_parser_reset_text();

    while (gshell_input_buffer[src_start] != '\0' &&
           gshell_is_command_space(gshell_input_buffer[src_start])) {
        src_start++;
    }

    src_end = src_start;

    while (gshell_input_buffer[src_end] != '\0') {
        src_end++;
    }

    while (src_end > src_start &&
           gshell_is_command_space(gshell_input_buffer[src_end - 1])) {
        src_end--;
    }

    while (src_start < src_end && out + 1 < GSHELL_COMMAND_TEXT_SIZE) {
        c = gshell_to_lower_char(gshell_input_buffer[src_start]);
        gshell_command_normalized[out] = c;
        out++;
        src_start++;
    }

    gshell_command_normalized[out] = '\0';
}

static unsigned int gshell_parse_command_id(void) {
    unsigned int i = 0;

    if (gshell_text_equal(gshell_command_normalized, "")) {
        return GSHELL_CMD_EMPTY;
    }

    for (i = 0; i < gshell_command_registry_count; i++) {
        if (gshell_text_equal(gshell_command_normalized, gshell_command_registry[i].name)) {
            return gshell_command_registry[i].id;
        }
    }

    return GSHELL_CMD_UNKNOWN;
}

static void gshell_dispatch_command(void) {
    unsigned int command_id = GSHELL_CMD_EMPTY;

    gshell_command_dispatches++;

    gshell_prepare_command_text();
    command_id = gshell_parse_command_id();
    gshell_last_command_id = command_id;

    if (command_id == GSHELL_CMD_EMPTY) {
        gshell_command_name = "EMPTY";
        gshell_command_result = "EMPTY";
        gshell_command_view = "IDLE";
        gshell_input_status_text = "ENTER ACCEPTED";
        gshell_parser_status_text = "EMPTY";
        gshell_terminal_push("EMPTY COMMAND");
        return;
    }

    if (command_id == GSHELL_CMD_DASH) {
        gshell_command_name = "DASH";
        gshell_command_result = "DASH OK";
        gshell_command_view = "DASH";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DASH -> REAL KERNEL DASH");
        return;
    }

    if (command_id == GSHELL_CMD_HEALTH) {
        gshell_command_name = "HEALTH";
        gshell_command_result = health_result_ok() ? "HEALTH OK" : "HEALTH BAD";
        gshell_command_view = "HEALTH";
        gshell_input_status_text = health_result_ok() ? "COMMAND OK" : "HEALTH BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(health_result_ok() ? "HEALTH -> RESULT OK" : "HEALTH -> RESULT BAD");
        return;
    }

    if (command_id == GSHELL_CMD_DOCTOR) {
        gshell_command_name = "DOCTOR";
        gshell_command_result = health_result_ok() ? "DOCTOR OK" : "DOCTOR BAD";
        gshell_command_view = "DOCTOR";
        gshell_input_status_text = health_result_ok() ? "COMMAND OK" : "DOCTOR BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(health_result_ok() ? "DOCTOR -> SYSTEM READY" : "DOCTOR -> SYSTEM BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_HELP) {
        gshell_command_name = "HELP";
        gshell_command_result = "HELP OK";
        gshell_command_view = "HELP";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("HELP -> COMMAND TABLE READY");
        return;
    }

    if (command_id == GSHELL_CMD_CLEAR) {
        gshell_command_name = "CLEAR";
        gshell_command_result = "CLEAR OK";
        gshell_command_view = "CLEAR";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_clear();
        gshell_terminal_push("TERMINAL CLEARED");
        return;
    }

    if (command_id == GSHELL_CMD_HISTORY) {
        gshell_command_name = "HISTORY";
        gshell_command_result = "HISTORY OK";
        gshell_command_view = "HISTORY";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("HISTORY -> VIEW READY");
        return;
    }

    if (command_id == GSHELL_CMD_HISTCLEAR) {
        gshell_history_clear();
        gshell_command_name = "HISTCLEAR";
        gshell_command_result = "HIST CLEARED";
        gshell_command_view = "HISTCLEAR";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("HISTORY CLEARED");
        return;
    }

    if (command_id == GSHELL_CMD_LOG) {
        gshell_command_name = "LOG";
        gshell_command_result = "LOG OK";
        gshell_command_view = "LOG";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LOG -> RESULT LOG READY");
        return;
    }

    if (command_id == GSHELL_CMD_LOGCLEAR) {
        gshell_result_log_clear();
        gshell_command_name = "LOGCLEAR";
        gshell_command_result = "LOG CLEARED";
        gshell_command_view = "LOGCLEAR";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RESULT LOG CLEARED");
        return;
    }

    if (command_id == GSHELL_CMD_MILESTONE) {
        gshell_command_name = "MILESTONE";
        gshell_command_result = "MILESTONE OK";
        gshell_command_view = "MILESTONE";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MILESTONE -> GSHELL READY");
        return;
    }

    if (command_id == GSHELL_CMD_STATUS) {
        gshell_command_name = "STATUS";
        gshell_command_result = "STATUS OK";
        gshell_command_view = "STATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("STATUS -> GSHELL STATUS READY");
        return;
    }

    if (command_id == GSHELL_CMD_VERSION) {
        gshell_command_name = "VERSION";
        gshell_command_result = "VERSION OK";
        gshell_command_view = "VERSION";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VERSION -> LINGJING VERSION READY");
        return;
    }

    if (command_id == GSHELL_CMD_ABOUT) {
        gshell_command_name = "ABOUT";
        gshell_command_result = "ABOUT OK";
        gshell_command_view = "ABOUT";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ABOUT -> USER CONTROL LAYER");
        return;
    }

    if (command_id == GSHELL_CMD_BRIDGE) {
        gshell_command_name = "BRIDGE";
        gshell_command_result = "BRIDGE OK";
        gshell_command_view = "BRIDGE";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BRIDGE -> COMMAND BRIDGE READY");
        return;
    }

    if (command_id == GSHELL_CMD_REGISTRY) {
        gshell_command_name = "REGISTRY";
        gshell_command_result = "REGISTRY OK";
        gshell_command_view = "REGISTRY";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("REGISTRY -> COMMAND TABLE READY");
        return;
    }

    if (command_id == GSHELL_CMD_TEXTCMDS) {
        gshell_command_name = "TEXTCMDS";
        gshell_command_result = "TEXTCMDS OK";
        gshell_command_view = "TEXTCMDS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TEXTCMDS -> TEXT COMMAND STATUS");
        return;
    }

    if (command_id == GSHELL_CMD_SYSINFO) {
        gshell_command_name = "SYSINFO";
        gshell_command_result = "SYSINFO OK";
        gshell_command_view = "SYSINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SYSINFO -> SYSTEM INFO READY");
        return;
    }

    if (command_id == GSHELL_CMD_MEMINFO) {
        gshell_command_name = "MEMINFO";
        gshell_command_result = "MEMINFO OK";
        gshell_command_view = "MEMINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MEMINFO -> MEMORY INFO READY");
        return;
    }

    if (command_id == GSHELL_CMD_PAGEINFO) {
        gshell_command_name = "PAGEINFO";
        gshell_command_result = "PAGEINFO OK";
        gshell_command_view = "PAGEINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PAGEINFO -> PAGING INFO READY");
        return;
    }

    if (command_id == GSHELL_CMD_SYSCALLS) {
        gshell_command_name = "SYSCALLS";
        gshell_command_result = "SYSCALLS OK";
        gshell_command_view = "SYSCALLS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SYSCALLS -> SYSCALL INFO READY");
        return;
    }

    if (command_id == GSHELL_CMD_RING3INFO) {
        gshell_command_name = "RING3INFO";
        gshell_command_result = "RING3 OK";
        gshell_command_view = "RING3INFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RING3INFO -> USER MODE READY");
        return;
    }

    if (command_id == GSHELL_CMD_MODULEINFO) {
        gshell_command_name = "MODULEINFO";
        gshell_command_result = "MODULE INFO OK";
        gshell_command_view = "MODULEINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MODULEINFO -> MODULE INFO READY");
        return;
    }

    if (command_id == GSHELL_CMD_MODULECHECK) {
        gshell_command_name = "MODULECHECK";
        gshell_command_result = "MODULE CHECK OK";
        gshell_command_view = "MODULECHECK";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MODULECHECK -> MODULE DEPS OK");
        return;
    }

    if (command_id == GSHELL_CMD_CAPINFO) {
        gshell_command_name = "CAPINFO";
        gshell_command_result = "CAP INFO OK";
        gshell_command_view = "CAPINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CAPINFO -> CAPABILITY INFO READY");
        return;
    }

    if (command_id == GSHELL_CMD_INTENTINFO) {
        gshell_command_name = "INTENTINFO";
        gshell_command_result = "INTENT INFO OK";
        gshell_command_view = "INTENTINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("INTENTINFO -> INTENT INFO READY");
        return;
    }

    if (command_id == GSHELL_CMD_TASKINFO) {
        gshell_command_name = "TASKINFO";
        gshell_command_result = scheduler_has_broken_tasks() ? "TASK INFO BAD" : "TASK INFO OK";
        gshell_command_view = "TASKINFO";
        gshell_input_status_text = scheduler_has_broken_tasks() ? "TASK BAD" : "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(scheduler_has_broken_tasks() ? "TASKINFO -> TASK ISSUE" : "TASKINFO -> TASK INFO READY");
        return;
    }

    if (command_id == GSHELL_CMD_TASKLIST) {
        gshell_command_name = "TASKLIST";
        gshell_command_result = "TASK LIST OK";
        gshell_command_view = "TASKLIST";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TASKLIST -> TASK TABLE READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCHEDINFO) {
        gshell_command_name = "SCHEDINFO";
        gshell_command_result = "SCHED INFO OK";
        gshell_command_view = "SCHEDINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCHEDINFO -> SCHEDULER READY");
        return;
    }

    if (command_id == GSHELL_CMD_RUNQUEUE) {
        gshell_command_name = "RUNQUEUE";
        gshell_command_result = "RUNQUEUE OK";
        gshell_command_view = "RUNQUEUE";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RUNQUEUE -> RUNNABLE TASKS READY");
        return;
    }

    if (command_id == GSHELL_CMD_SECURITYINFO) {
        gshell_command_name = "SECURITYINFO";
        gshell_command_result = security_doctor_ok() ? "SECURITY INFO OK" : "SECURITY INFO BAD";
        gshell_command_view = "SECURITYINFO";
        gshell_input_status_text = security_doctor_ok() ? "COMMAND OK" : "SECURITY BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(security_doctor_ok() ? "SECURITYINFO -> SECURITY READY" : "SECURITYINFO -> SECURITY ISSUE");
        return;
    }

    if (command_id == GSHELL_CMD_PLATFORMINFO) {
        gshell_command_name = "PLATFORMINFO";
        gshell_command_result = platform_doctor_ok() ? "PLATFORM INFO OK" : "PLATFORM INFO BAD";
        gshell_command_view = "PLATFORMINFO";
        gshell_input_status_text = platform_doctor_ok() ? "COMMAND OK" : "PLATFORM BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(platform_doctor_ok() ? "PLATFORMINFO -> PLATFORM READY" : "PLATFORMINFO -> PLATFORM ISSUE");
        return;
    }

    if (command_id == GSHELL_CMD_IDENTITYINFO) {
        gshell_command_name = "IDENTITYINFO";
        gshell_command_result = identity_doctor_ok() ? "IDENTITY INFO OK" : "IDENTITY INFO BAD";
        gshell_command_view = "IDENTITYINFO";
        gshell_input_status_text = identity_doctor_ok() ? "COMMAND OK" : "IDENTITY BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(identity_doctor_ok() ? "IDENTITYINFO -> IDENTITY READY" : "IDENTITYINFO -> IDENTITY ISSUE");
        return;
    }

    if (command_id == GSHELL_CMD_LANGINFO) {
        gshell_command_name = "LANGINFO";
        gshell_command_result = lang_doctor_ok() ? "LANG INFO OK" : "LANG INFO BAD";
        gshell_command_view = "LANGINFO";
        gshell_input_status_text = lang_doctor_ok() ? "COMMAND OK" : "LANG BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(lang_doctor_ok() ? "LANGINFO -> LANGUAGE READY" : "LANGINFO -> LANGUAGE ISSUE");
        return;
    }

    if (command_id == GSHELL_CMD_BOOTINFO) {
        gshell_command_name = "BOOTINFO";
        gshell_command_result = bootinfo_is_multiboot_valid() ? "BOOT INFO OK" : "BOOT INFO BAD";
        gshell_command_view = "BOOTINFO";
        gshell_input_status_text = bootinfo_is_multiboot_valid() ? "COMMAND OK" : "BOOT BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(bootinfo_is_multiboot_valid() ? "BOOTINFO -> MULTIBOOT READY" : "BOOTINFO -> MULTIBOOT ISSUE");
        return;
    }

    if (command_id == GSHELL_CMD_FBINFO) {
        gshell_command_name = "FBINFO";
        gshell_command_result = framebuffer_is_ready() ? "FB INFO OK" : "FB INFO BAD";
        gshell_command_view = "FBINFO";
        gshell_input_status_text = framebuffer_is_ready() ? "COMMAND OK" : "FB BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(framebuffer_is_ready() ? "FBINFO -> FRAMEBUFFER READY" : "FBINFO -> FRAMEBUFFER ISSUE");
        return;
    }

    if (command_id == GSHELL_CMD_GFXINFO) {
        gshell_command_name = "GFXINFO";
        gshell_command_result = graphics_is_ready() ? "GFX INFO OK" : "GFX INFO BAD";
        gshell_command_view = "GFXINFO";
        gshell_input_status_text = graphics_is_ready() ? "COMMAND OK" : "GFX BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(graphics_is_ready() ? "GFXINFO -> GRAPHICS READY" : "GFXINFO -> GRAPHICS ISSUE");
        return;
    }

    if (command_id == GSHELL_CMD_FONTINFO) {
        gshell_command_name = "FONTINFO";
        gshell_command_result = "FONT INFO OK";
        gshell_command_view = "FONTINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FONTINFO -> FONT METRICS READY");
        return;
    }

    if (command_id == GSHELL_CMD_FSINFO) {
        gshell_command_name = "FSINFO";
        gshell_command_result = "FS INFO OK";
        gshell_command_view = "FSINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FSINFO -> FILESYSTEM METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_DEVICEINFO) {
        gshell_command_name = "DEVICEINFO";
        gshell_command_result = "DEVICE INFO OK";
        gshell_command_view = "DEVICEINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DEVICEINFO -> DEVICE METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_DRIVERINFO) {
        gshell_command_name = "DRIVERINFO";
        gshell_command_result = "DRIVER INFO OK";
        gshell_command_view = "DRIVERINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DRIVERINFO -> DRIVER TABLE READY");
        return;
    }

    if (command_id == GSHELL_CMD_IOINFO) {
        gshell_command_name = "IOINFO";
        gshell_command_result = "IO INFO OK";
        gshell_command_view = "IOINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("IOINFO -> PORT IO READY");
        return;
    }

    if (command_id == GSHELL_CMD_PROCESSINFO) {
        gshell_command_name = "PROCESSINFO";
        gshell_command_result = "PROCESS INFO OK";
        gshell_command_view = "PROCESSINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PROCESSINFO -> PROCESS METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_PROCINFO) {
        gshell_command_name = "PROCINFO";
        gshell_command_result = "PROC INFO OK";
        gshell_command_view = "PROCINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PROCINFO -> PROCESS TABLE READY");
        return;
    }

    if (command_id == GSHELL_CMD_RUNTIMEINFO) {
        gshell_command_name = "RUNTIMEINFO";
        gshell_command_result = "RUNTIME INFO OK";
        gshell_command_view = "RUNTIMEINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RUNTIMEINFO -> USER RUNTIME READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPINFO) {
        gshell_command_name = "APPINFO";
        gshell_command_result = "APP INFO OK";
        gshell_command_view = "APPINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPINFO -> APP LAYER METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_ELFINFO) {
        gshell_command_name = "ELFINFO";
        gshell_command_result = "ELF INFO OK";
        gshell_command_view = "ELFINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ELFINFO -> ELF METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_LOADERINFO) {
        gshell_command_name = "LOADERINFO";
        gshell_command_result = "LOADER INFO OK";
        gshell_command_view = "LOADERINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LOADERINFO -> LOADER METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_BINARYINFO) {
        gshell_command_name = "BINARYINFO";
        gshell_command_result = "BINARY INFO OK";
        gshell_command_view = "BINARYINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BINARYINFO -> BINARY FORMAT READY");
        return;
    }

    if (command_id == GSHELL_CMD_ABIINFO) {
        gshell_command_name = "ABIINFO";
        gshell_command_result = "ABI INFO OK";
        gshell_command_view = "ABIINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ABIINFO -> ABI METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_SANDBOXINFO) {
        gshell_command_name = "SANDBOXINFO";
        gshell_command_result = "SANDBOX INFO OK";
        gshell_command_view = "SANDBOXINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SANDBOXINFO -> SANDBOX METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_POLICYINFO) {
        gshell_command_name = "POLICYINFO";
        gshell_command_result = "POLICY INFO OK";
        gshell_command_view = "POLICYINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLICYINFO -> USER POLICY METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_PERMISSIONINFO) {
        gshell_command_name = "PERMISSIONINFO";
        gshell_command_result = "PERMISSION INFO OK";
        gshell_command_view = "PERMISSIONINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PERMISSIONINFO -> PERMISSION METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_GUARDINFO) {
        gshell_command_name = "GUARDINFO";
        gshell_command_result = "GUARD INFO OK";
        gshell_command_view = "GUARDINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("GUARDINFO -> GUARD METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_CONTROLINFO) {
        gshell_command_name = "CONTROLINFO";
        gshell_command_result = "CONTROL INFO OK";
        gshell_command_view = "CONTROLINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CONTROLINFO -> USER CONTROL METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_OWNERINFO) {
        gshell_command_name = "OWNERINFO";
        gshell_command_result = "OWNER INFO OK";
        gshell_command_view = "OWNERINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("OWNERINFO -> OWNERSHIP METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_DECISIONINFO) {
        gshell_command_name = "DECISIONINFO";
        gshell_command_result = "DECISION INFO OK";
        gshell_command_view = "DECISIONINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DECISIONINFO -> DECISION METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_RULEINFO) {
        gshell_command_name = "RULEINFO";
        gshell_command_result = "RULE INFO OK";
        gshell_command_view = "RULEINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RULEINFO -> USER RULE METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_AUDITINFO) {
        gshell_command_name = "AUDITINFO";
        gshell_command_result = "AUDIT INFO OK";
        gshell_command_view = "AUDITINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("AUDITINFO -> AUDIT METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_TRACEINFO) {
        gshell_command_name = "TRACEINFO";
        gshell_command_result = "TRACE INFO OK";
        gshell_command_view = "TRACEINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TRACEINFO -> TRACE METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_LOGINFO) {
        gshell_command_name = "LOGINFO";
        gshell_command_result = "LOG INFO OK";
        gshell_command_view = "LOGINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LOGINFO -> LOG METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_EVENTINFO) {
        gshell_command_name = "EVENTINFO";
        gshell_command_result = "EVENT INFO OK";
        gshell_command_view = "EVENTINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("EVENTINFO -> EVENT METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_NETWORKINFO) {
        gshell_command_name = "NETWORKINFO";
        gshell_command_result = "NETWORK INFO OK";
        gshell_command_view = "NETWORKINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("NETWORKINFO -> NETWORK METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_NETINFO) {
        gshell_command_name = "NETINFO";
        gshell_command_result = "NET INFO OK";
        gshell_command_view = "NETINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("NETINFO -> NET DEVICE METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_SOCKETINFO) {
        gshell_command_name = "SOCKETINFO";
        gshell_command_result = "SOCKET INFO OK";
        gshell_command_view = "SOCKETINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SOCKETINFO -> SOCKET METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_PROTOCOLINFO) {
        gshell_command_name = "PROTOCOLINFO";
        gshell_command_result = "PROTOCOL INFO OK";
        gshell_command_view = "PROTOCOLINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PROTOCOLINFO -> PROTOCOL METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_SERVICEINFO) {
        gshell_command_name = "SERVICEINFO";
        gshell_command_result = "SERVICE INFO OK";
        gshell_command_view = "SERVICEINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SERVICEINFO -> SERVICE METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_SESSIONINFO) {
        gshell_command_name = "SESSIONINFO";
        gshell_command_result = "SESSION INFO OK";
        gshell_command_view = "SESSIONINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SESSIONINFO -> SESSION METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_CHANNELINFO) {
        gshell_command_name = "CHANNELINFO";
        gshell_command_result = "CHANNEL INFO OK";
        gshell_command_view = "CHANNELINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CHANNELINFO -> CHANNEL METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_BUSINFO) {
        gshell_command_name = "BUSINFO";
        gshell_command_result = "BUS INFO OK";
        gshell_command_view = "BUSINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BUSINFO -> MESSAGE BUS METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_PACKAGEINFO) {
        gshell_command_name = "PACKAGEINFO";
        gshell_command_result = "PACKAGE INFO OK";
        gshell_command_view = "PACKAGEINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PACKAGEINFO -> PACKAGE METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_INSTALLINFO) {
        gshell_command_name = "INSTALLINFO";
        gshell_command_result = "INSTALL INFO OK";
        gshell_command_view = "INSTALLINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("INSTALLINFO -> INSTALL METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_LIFECYCLEINFO) {
        gshell_command_name = "LIFECYCLEINFO";
        gshell_command_result = "LIFECYCLE INFO OK";
        gshell_command_view = "LIFECYCLEINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LIFECYCLEINFO -> LIFECYCLE METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHINFO) {
        gshell_command_name = "LAUNCHINFO";
        gshell_command_result = "LAUNCH INFO OK";
        gshell_command_view = "LAUNCHINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHINFO -> APP LAUNCH METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_OVERVIEWINFO) {
        gshell_command_name = "OVERVIEWINFO";
        gshell_command_result = "OVERVIEW INFO OK";
        gshell_command_view = "OVERVIEWINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("OVERVIEWINFO -> SYSTEM OVERVIEW READY");
        return;
    }

    if (command_id == GSHELL_CMD_MILESTONEINFO) {
        gshell_command_name = "MILESTONEINFO";
        gshell_command_result = "MILESTONE INFO OK";
        gshell_command_view = "MILESTONEINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MILESTONEINFO -> MILESTONE METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_ROADMAPINFO) {
        gshell_command_name = "ROADMAPINFO";
        gshell_command_result = "ROADMAP INFO OK";
        gshell_command_view = "ROADMAPINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ROADMAPINFO -> ROADMAP METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_FINALINFO) {
        gshell_command_name = "FINALINFO";
        gshell_command_result = "FINAL INFO OK";
        gshell_command_view = "FINALINFO";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FINALINFO -> 0.7X BRIDGE CLOSEOUT READY");
        return;
    }

    if (command_id == GSHELL_CMD_CONTROLSTATUS) {
        gshell_command_name = "CONTROLSTATUS";
        gshell_command_result = "CONTROL STATUS OK";
        gshell_command_view = "CONTROLSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CONTROLSTATUS -> USER POLICY READY");
        return;
    }

    if (command_id == GSHELL_CMD_CONTROLSTRICT) {
        security_policy_strict();
        gshell_command_name = "CONTROLSTRICT";
        gshell_command_result = "CONTROL STRICT OK";
        gshell_command_view = "CONTROLSTATUS";
        gshell_input_status_text = "STRICT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CONTROLSTRICT -> STRICT POLICY ENABLED");
        return;
    }

    if (command_id == GSHELL_CMD_CONTROLOPEN) {
        security_policy_open();
        gshell_command_name = "CONTROLOPEN";
        gshell_command_result = "CONTROL OPEN OK";
        gshell_command_view = "CONTROLSTATUS";
        gshell_input_status_text = "OPEN";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CONTROLOPEN -> OPEN POLICY ENABLED");
        return;
    }

    if (command_id == GSHELL_CMD_BLOCKNET) {
        security_policy_block_network();
        gshell_command_name = "BLOCKNET";
        gshell_command_result = "NETWORK BLOCKED";
        gshell_command_view = "CONTROLSTATUS";
        gshell_input_status_text = "NET BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BLOCKNET -> NETWORK INTENT BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_ALLOWNET) {
        security_policy_allow_network();
        gshell_command_name = "ALLOWNET";
        gshell_command_result = "NETWORK ALLOWED";
        gshell_command_view = "CONTROLSTATUS";
        gshell_input_status_text = "NET ALLOW";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ALLOWNET -> NETWORK INTENT ALLOWED");
        return;
    }

    if (command_id == GSHELL_CMD_CHECKNET) {
        int allowed = security_check_intent("network");
        gshell_command_name = "CHECKNET";
        gshell_command_result = allowed ? "NETWORK ALLOWED" : "NETWORK BLOCKED";
        gshell_command_view = "CONTROLSTATUS";
        gshell_input_status_text = allowed ? "NET OK" : "NET BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(allowed ? "CHECKNET -> NETWORK ALLOWED" : "CHECKNET -> NETWORK BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_CAPSTATUS) {
        gshell_command_name = "CAPSTATUS";
        gshell_command_result = "CAP STATUS OK";
        gshell_command_view = "CAPSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CAPSTATUS -> CAPABILITY POLICY READY");
        return;
    }

    if (command_id == GSHELL_CMD_BLOCKFILE) {
        security_policy_block_file();
        gshell_command_name = "BLOCKFILE";
        gshell_command_result = "FILE BLOCKED";
        gshell_command_view = "CAPSTATUS";
        gshell_input_status_text = "FILE BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BLOCKFILE -> FILE CAPABILITY BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_ALLOWFILE) {
        security_policy_allow_file();
        gshell_command_name = "ALLOWFILE";
        gshell_command_result = "FILE ALLOWED";
        gshell_command_view = "CAPSTATUS";
        gshell_input_status_text = "FILE ALLOW";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ALLOWFILE -> FILE CAPABILITY ALLOWED");
        return;
    }

    if (command_id == GSHELL_CMD_BLOCKAI) {
        security_policy_block_ai();
        gshell_command_name = "BLOCKAI";
        gshell_command_result = "AI BLOCKED";
        gshell_command_view = "CAPSTATUS";
        gshell_input_status_text = "AI BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BLOCKAI -> AI CAPABILITY BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_ALLOWAI) {
        security_policy_allow_ai();
        gshell_command_name = "ALLOWAI";
        gshell_command_result = "AI ALLOWED";
        gshell_command_view = "CAPSTATUS";
        gshell_input_status_text = "AI ALLOW";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ALLOWAI -> AI CAPABILITY ALLOWED");
        return;
    }

    if (command_id == GSHELL_CMD_CHECKFILE) {
        int allowed = security_check_capability("file.read", "file");
        gshell_command_name = "CHECKFILE";
        gshell_command_result = allowed ? "FILE ALLOWED" : "FILE BLOCKED";
        gshell_command_view = "CAPSTATUS";
        gshell_input_status_text = allowed ? "FILE OK" : "FILE BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(allowed ? "CHECKFILE -> FILE ALLOWED" : "CHECKFILE -> FILE BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_CHECKAI) {
        int allowed = security_check_capability("ai.assist", "intent");
        gshell_command_name = "CHECKAI";
        gshell_command_result = allowed ? "AI ALLOWED" : "AI BLOCKED";
        gshell_command_view = "CAPSTATUS";
        gshell_input_status_text = allowed ? "AI OK" : "AI BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(allowed ? "CHECKAI -> AI ALLOWED" : "CHECKAI -> AI BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_CHECKGUI) {
        int allowed = security_check_capability("gui.display", "display");
        gshell_command_name = "CHECKGUI";
        gshell_command_result = allowed ? "GUI ALLOWED" : "GUI BLOCKED";
        gshell_command_view = "CAPSTATUS";
        gshell_input_status_text = allowed ? "GUI OK" : "GUI BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(allowed ? "CHECKGUI -> GUI ALLOWED" : "CHECKGUI -> GUI BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_INTENTGATE) {
        gshell_command_name = "INTENTGATE";
        gshell_command_result = "INTENT GATE OK";
        gshell_command_view = "INTENTGATE";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("INTENTGATE -> INTENT POLICY READY");
        return;
    }

    if (command_id == GSHELL_CMD_BLOCKVIDEO) {
        security_policy_block_video_intent();
        gshell_command_name = "BLOCKVIDEO";
        gshell_command_result = "VIDEO INTENT BLOCKED";
        gshell_command_view = "INTENTGATE";
        gshell_input_status_text = "VIDEO BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BLOCKVIDEO -> VIDEO INTENT BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_ALLOWVIDEO) {
        security_policy_allow_video_intent();
        gshell_command_name = "ALLOWVIDEO";
        gshell_command_result = "VIDEO INTENT ALLOWED";
        gshell_command_view = "INTENTGATE";
        gshell_input_status_text = "VIDEO ALLOW";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ALLOWVIDEO -> VIDEO INTENT ALLOWED");
        return;
    }

    if (command_id == GSHELL_CMD_BLOCKAIINTENT) {
        security_policy_block_ai_intent();
        gshell_command_name = "BLOCKAIINTENT";
        gshell_command_result = "AI INTENT BLOCKED";
        gshell_command_view = "INTENTGATE";
        gshell_input_status_text = "AI INT BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BLOCKAIINTENT -> AI INTENT BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_ALLOWAIINTENT) {
        security_policy_allow_ai_intent();
        gshell_command_name = "ALLOWAIINTENT";
        gshell_command_result = "AI INTENT ALLOWED";
        gshell_command_view = "INTENTGATE";
        gshell_input_status_text = "AI INT ALLOW";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ALLOWAIINTENT -> AI INTENT ALLOWED");
        return;
    }

    if (command_id == GSHELL_CMD_CHECKVIDEO) {
        int allowed = security_check_intent("video");
        gshell_command_name = "CHECKVIDEO";
        gshell_command_result = allowed ? "VIDEO INTENT ALLOWED" : "VIDEO INTENT BLOCKED";
        gshell_command_view = "INTENTGATE";
        gshell_input_status_text = allowed ? "VIDEO OK" : "VIDEO BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(allowed ? "CHECKVIDEO -> VIDEO INTENT ALLOWED" : "CHECKVIDEO -> VIDEO INTENT BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_CHECKAIINTENT) {
        int allowed = security_check_intent("ai");
        gshell_command_name = "CHECKAIINTENT";
        gshell_command_result = allowed ? "AI INTENT ALLOWED" : "AI INTENT BLOCKED";
        gshell_command_view = "INTENTGATE";
        gshell_input_status_text = allowed ? "AI INT OK" : "AI INT BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(allowed ? "CHECKAIINTENT -> AI INTENT ALLOWED" : "CHECKAIINTENT -> AI INTENT BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_CHECKNETINTENT) {
        int allowed = security_check_intent("network");
        gshell_command_name = "CHECKNETINTENT";
        gshell_command_result = allowed ? "NET INTENT ALLOWED" : "NET INTENT BLOCKED";
        gshell_command_view = "INTENTGATE";
        gshell_input_status_text = allowed ? "NET INT OK" : "NET INT BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(allowed ? "CHECKNETINTENT -> NET INTENT ALLOWED" : "CHECKNETINTENT -> NET INTENT BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_MODULEGATE) {
        gshell_command_name = "MODULEGATE";
        gshell_command_result = "MODULE GATE OK";
        gshell_command_view = "MODULEGATE";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MODULEGATE -> MODULE POLICY READY");
        return;
    }

    if (command_id == GSHELL_CMD_BLOCKEXTERNAL) {
        security_policy_block_external_modules();
        gshell_command_name = "BLOCKEXTERNAL";
        gshell_command_result = "EXTERNAL MODULES BLOCKED";
        gshell_command_view = "MODULEGATE";
        gshell_input_status_text = "MOD BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BLOCKEXTERNAL -> EXTERNAL MODULES BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_ALLOWEXTERNAL) {
        security_policy_allow_external_modules();
        gshell_command_name = "ALLOWEXTERNAL";
        gshell_command_result = "EXTERNAL MODULES ALLOWED";
        gshell_command_view = "MODULEGATE";
        gshell_input_status_text = "MOD ALLOW";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ALLOWEXTERNAL -> EXTERNAL MODULES ALLOWED");
        return;
    }

    if (command_id == GSHELL_CMD_CHECKGUIMOD) {
        int allowed = security_check_module_load("gui");
        gshell_command_name = "CHECKGUIMOD";
        gshell_command_result = allowed ? "GUI MODULE ALLOWED" : "GUI MODULE BLOCKED";
        gshell_command_view = "MODULEGATE";
        gshell_input_status_text = allowed ? "GUI MOD OK" : "GUI MOD BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(allowed ? "CHECKGUIMOD -> GUI MODULE ALLOWED" : "CHECKGUIMOD -> GUI MODULE BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_CHECKNETMOD) {
        int allowed = security_check_module_load("net");
        gshell_command_name = "CHECKNETMOD";
        gshell_command_result = allowed ? "NET MODULE ALLOWED" : "NET MODULE BLOCKED";
        gshell_command_view = "MODULEGATE";
        gshell_input_status_text = allowed ? "NET MOD OK" : "NET MOD BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(allowed ? "CHECKNETMOD -> NET MODULE ALLOWED" : "CHECKNETMOD -> NET MODULE BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_CHECKAIMOD) {
        int allowed = security_check_module_load("ai");
        gshell_command_name = "CHECKAIMOD";
        gshell_command_result = allowed ? "AI MODULE ALLOWED" : "AI MODULE BLOCKED";
        gshell_command_view = "MODULEGATE";
        gshell_input_status_text = allowed ? "AI MOD OK" : "AI MOD BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(allowed ? "CHECKAIMOD -> AI MODULE ALLOWED" : "CHECKAIMOD -> AI MODULE BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_CHECKCOREMOD) {
        int allowed = security_check_module_load("kernel");
        gshell_command_name = "CHECKCOREMOD";
        gshell_command_result = allowed ? "CORE MODULE ALLOWED" : "CORE MODULE BLOCKED";
        gshell_command_view = "MODULEGATE";
        gshell_input_status_text = allowed ? "CORE OK" : "CORE BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(allowed ? "CHECKCOREMOD -> CORE MODULE ALLOWED" : "CHECKCOREMOD -> CORE MODULE BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_CHECKSHELLMOD) {
        int allowed = security_check_module_load("shell");
        gshell_command_name = "CHECKSHELLMOD";
        gshell_command_result = allowed ? "SHELL MODULE ALLOWED" : "SHELL MODULE BLOCKED";
        gshell_command_view = "MODULEGATE";
        gshell_input_status_text = allowed ? "SHELL OK" : "SHELL BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(allowed ? "CHECKSHELLMOD -> SHELL MODULE ALLOWED" : "CHECKSHELLMOD -> SHELL MODULE BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_SANDBOXSTATUS) {
        gshell_command_name = "SANDBOXSTATUS";
        gshell_command_result = "SANDBOX STATUS OK";
        gshell_command_view = "SANDBOXSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SANDBOXSTATUS -> SANDBOX PROFILE READY");
        return;
    }

    if (command_id == GSHELL_CMD_PROFILEOPEN) {
        security_sandbox_profile_open();
        gshell_command_name = "PROFILEOPEN";
        gshell_command_result = "PROFILE OPEN OK";
        gshell_command_view = "SANDBOXSTATUS";
        gshell_input_status_text = "OPEN";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PROFILEOPEN -> OPEN SANDBOX PROFILE");
        return;
    }

    if (command_id == GSHELL_CMD_PROFILESAFE) {
        security_sandbox_profile_safe();
        gshell_command_name = "PROFILESAFE";
        gshell_command_result = "PROFILE SAFE OK";
        gshell_command_view = "SANDBOXSTATUS";
        gshell_input_status_text = "SAFE";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PROFILESAFE -> SAFE SANDBOX PROFILE");
        return;
    }

    if (command_id == GSHELL_CMD_PROFILELOCKED) {
        security_sandbox_profile_locked();
        gshell_command_name = "PROFILELOCKED";
        gshell_command_result = "PROFILE LOCKED OK";
        gshell_command_view = "SANDBOXSTATUS";
        gshell_input_status_text = "LOCKED";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PROFILELOCKED -> LOCKED SANDBOX PROFILE");
        return;
    }

    if (command_id == GSHELL_CMD_CHECKSANDBOX) {
        int allowed = security_check_sandbox();
        gshell_command_name = "CHECKSANDBOX";
        gshell_command_result = allowed ? "SANDBOX OK" : "SANDBOX BAD";
        gshell_command_view = "SANDBOXSTATUS";
        gshell_input_status_text = allowed ? "SANDBOX OK" : "SANDBOX BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(allowed ? "CHECKSANDBOX -> SANDBOX OK" : "CHECKSANDBOX -> SANDBOX BAD");
        return;
    }

    if (command_id == GSHELL_CMD_SANDBOXRESET) {
        security_sandbox_profile_reset();
        gshell_command_name = "SANDBOXRESET";
        gshell_command_result = "SANDBOX RESET OK";
        gshell_command_view = "SANDBOXSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SANDBOXRESET -> OPEN SANDBOX PROFILE");
        return;
    }

    if (command_id == GSHELL_CMD_DECISIONSTATUS) {
        gshell_command_name = "DECISIONSTATUS";
        gshell_command_result = "DECISION STATUS OK";
        gshell_command_view = "DECISIONSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DECISIONSTATUS -> POLICY DECISION LOG READY");
        return;
    }

    if (command_id == GSHELL_CMD_DECISIONLOG) {
        gshell_command_name = "DECISIONLOG";
        gshell_command_result = "DECISION LOG OK";
        gshell_command_view = "DECISIONSTATUS";
        gshell_input_status_text = "LOG OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DECISIONLOG -> DECISION COUNTERS READY");
        return;
    }

    if (command_id == GSHELL_CMD_AUDITSTATUS) {
        gshell_command_name = "AUDITSTATUS";
        gshell_command_result = "AUDIT STATUS OK";
        gshell_command_view = "DECISIONSTATUS";
        gshell_input_status_text = "AUDIT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("AUDITSTATUS -> AUDIT LOG READY");
        return;
    }

    if (command_id == GSHELL_CMD_AUDITLAST) {
        gshell_command_name = "AUDITLAST";
        gshell_command_result = "AUDIT LAST OK";
        gshell_command_view = "DECISIONSTATUS";
        gshell_input_status_text = "LAST OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(security_audit_last_event());
        return;
    }

    if (command_id == GSHELL_CMD_POLICYTRACE) {
        gshell_command_name = "POLICYTRACE";
        gshell_command_result = "POLICY TRACE OK";
        gshell_command_view = "DECISIONSTATUS";
        gshell_input_status_text = "TRACE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLICYTRACE -> POLICY PATH TRACED");
        return;
    }

    if (command_id == GSHELL_CMD_DECISIONRESET) {
        security_decision_reset_counters();
        gshell_command_name = "DECISIONRESET";
        gshell_command_result = "DECISION RESET OK";
        gshell_command_view = "DECISIONSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DECISIONRESET -> COUNTERS RESET");
        return;
    }

    if (command_id == GSHELL_CMD_RULESTATUS) {
        gshell_command_name = "RULESTATUS";
        gshell_command_result = "RULE STATUS OK";
        gshell_command_view = "RULESTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RULESTATUS -> USER RULE TABLE READY");
        return;
    }

    if (command_id == GSHELL_CMD_RULEALLOW) {
        security_rule_default_allow();
        gshell_command_name = "RULEALLOW";
        gshell_command_result = "RULE DEFAULT ALLOW";
        gshell_command_view = "RULESTATUS";
        gshell_input_status_text = "RULE ALLOW";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RULEALLOW -> DEFAULT ALLOW");
        return;
    }

    if (command_id == GSHELL_CMD_RULEASK) {
        security_rule_default_ask();
        gshell_command_name = "RULEASK";
        gshell_command_result = "RULE DEFAULT ASK";
        gshell_command_view = "RULESTATUS";
        gshell_input_status_text = "RULE ASK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RULEASK -> DEFAULT ASK");
        return;
    }

    if (command_id == GSHELL_CMD_RULEDENY) {
        security_rule_default_deny();
        gshell_command_name = "RULEDENY";
        gshell_command_result = "RULE DEFAULT DENY";
        gshell_command_view = "RULESTATUS";
        gshell_input_status_text = "RULE DENY";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RULEDENY -> DEFAULT DENY");
        return;
    }

    if (command_id == GSHELL_CMD_CHECKRULE) {
        int allowed = security_check_rule("demo.rule");
        gshell_command_name = "CHECKRULE";
        gshell_command_result = allowed ? "RULE ALLOWED" : "RULE BLOCKED";
        gshell_command_view = "RULESTATUS";
        gshell_input_status_text = allowed ? "RULE OK" : "RULE BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(allowed ? "CHECKRULE -> RULE ALLOWED" : "CHECKRULE -> RULE BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_RULERESET) {
        security_rule_reset();
        gshell_command_name = "RULERESET";
        gshell_command_result = "RULE RESET OK";
        gshell_command_view = "RULESTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RULERESET -> RULE TABLE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_CONTROLPANEL) {
        gshell_command_name = "CONTROLPANEL";
        gshell_command_result = "CONTROL PANEL OK";
        gshell_command_view = "CONTROLPANEL";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CONTROLPANEL -> USER CONTROL PANEL READY");
        return;
    }

    if (command_id == GSHELL_CMD_POLICYPANEL) {
        gshell_command_name = "POLICYPANEL";
        gshell_command_result = "POLICY PANEL OK";
        gshell_command_view = "CONTROLPANEL";
        gshell_input_status_text = "POLICY OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLICYPANEL -> POLICY SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_GATEPANEL) {
        gshell_command_name = "GATEPANEL";
        gshell_command_result = "GATE PANEL OK";
        gshell_command_view = "CONTROLPANEL";
        gshell_input_status_text = "GATE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("GATEPANEL -> SECURITY GATES READY");
        return;
    }

    if (command_id == GSHELL_CMD_HEALTHPANEL) {
        gshell_command_name = "HEALTHPANEL";
        gshell_command_result = "HEALTH PANEL OK";
        gshell_command_view = "CONTROLPANEL";
        gshell_input_status_text = "HEALTH OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("HEALTHPANEL -> CONTROL HEALTH READY");
        return;
    }

    if (command_id == GSHELL_CMD_CONTROLRESET) {
        security_sandbox_profile_open();
        security_rule_reset();
        security_decision_reset_counters();
        gshell_command_name = "CONTROLRESET";
        gshell_command_result = "CONTROL RESET OK";
        gshell_command_view = "CONTROLPANEL";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CONTROLRESET -> OPEN PROFILE + RULE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_GATECHECK) {
        int file_ok = security_check_capability("file.read", "file");
        int ai_ok = security_check_capability("ai.assist", "intent");
        int net_ok = security_check_intent("network");
        gshell_command_name = "GATECHECK";
        gshell_command_result = (file_ok && ai_ok && net_ok) ? "GATE CHECK OK" : "GATE CHECK BLOCKED";
        gshell_command_view = "CONTROLPANEL";
        gshell_input_status_text = (file_ok && ai_ok && net_ok) ? "GATES OK" : "GATES BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push((file_ok && ai_ok && net_ok) ? "GATECHECK -> ALL CORE GATES ALLOWED" : "GATECHECK -> SOME CORE GATES BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_CONTROLFINAL) {
        gshell_command_name = "CONTROLFINAL";
        gshell_command_result = "CONTROL FINAL OK";
        gshell_command_view = "CONTROLFINAL";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CONTROLFINAL -> 0.8X CONTROL CLOSEOUT READY");
        return;
    }

    if (command_id == GSHELL_CMD_CONTROLHEALTH) {
        gshell_command_name = "CONTROLHEALTH";
        gshell_command_result = "CONTROL HEALTH OK";
        gshell_command_view = "CONTROLFINAL";
        gshell_input_status_text = "HEALTH OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CONTROLHEALTH -> USER CONTROL HEALTH READY");
        return;
    }

    if (command_id == GSHELL_CMD_POLICYHEALTH) {
        gshell_command_name = "POLICYHEALTH";
        gshell_command_result = "POLICY HEALTH OK";
        gshell_command_view = "CONTROLFINAL";
        gshell_input_status_text = "POLICY OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLICYHEALTH -> POLICY HEALTH READY");
        return;
    }

    if (command_id == GSHELL_CMD_GATESUMMARY) {
        gshell_command_name = "GATESUMMARY";
        gshell_command_result = "GATE SUMMARY OK";
        gshell_command_view = "CONTROLFINAL";
        gshell_input_status_text = "GATES OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("GATESUMMARY -> SECURITY GATES SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_SANDBOXSUMMARY) {
        gshell_command_name = "SANDBOXSUMMARY";
        gshell_command_result = "SANDBOX SUMMARY OK";
        gshell_command_view = "CONTROLFINAL";
        gshell_input_status_text = "SANDBOX OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SANDBOXSUMMARY -> SANDBOX SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_RULESUMMARY) {
        gshell_command_name = "RULESUMMARY";
        gshell_command_result = "RULE SUMMARY OK";
        gshell_command_view = "CONTROLFINAL";
        gshell_input_status_text = "RULE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RULESUMMARY -> USER RULE SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_DECISIONSUMMARY) {
        gshell_command_name = "DECISIONSUMMARY";
        gshell_command_result = "DECISION SUMMARY OK";
        gshell_command_view = "CONTROLFINAL";
        gshell_input_status_text = "DECISION OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DECISIONSUMMARY -> DECISION SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_NEXTSTAGE) {
        gshell_command_name = "NEXTSTAGE";
        gshell_command_result = "NEXT STAGE OK";
        gshell_command_view = "CONTROLFINAL";
        gshell_input_status_text = "NEXT 0.9";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("NEXTSTAGE -> 0.9 APP RUNTIME PREP");
        return;
    }

    gshell_command_unknown++;
    gshell_command_name = "UNKNOWN";
    gshell_command_result = "UNKNOWN";
    gshell_command_view = "ERROR";
    gshell_input_status_text = "UNKNOWN COMMAND";
    gshell_parser_status_text = "UNKNOWN";
    gshell_history_push(gshell_command_normalized, gshell_command_view);
    gshell_result_log_push(gshell_command_normalized, gshell_command_result);
    gshell_terminal_push("UNKNOWN COMMAND -> TYPE HELP");
}

static int gshell_graphics_can_render_now(void) {
    if (!gshell_initialized) {
        return 0;
    }

    if (gshell_broken) {
        return 0;
    }

    if (!graphics_is_ready()) {
        return 0;
    }

    if (!framebuffer_is_ready()) {
        return 0;
    }

    if (framebuffer_get_address() == 0) {
        return 0;
    }

    if (framebuffer_get_bpp() != 32) {
        return 0;
    }

    if (framebuffer_get_width() < 640 || framebuffer_get_height() < 480) {
        return 0;
    }

    return 1;
}

void gshell_input_status(void) {
    platform_print("GShell input:\n");

    platform_print("  buffer: ");
    platform_print(gshell_input_buffer);
    platform_print("\n");

    platform_print("  normalized: ");
    platform_print(gshell_command_normalized);
    platform_print("\n");

    platform_print("  parser: ");
    platform_print(gshell_parser_status_text);
    platform_print("\n");

    platform_print("  command id: ");
    platform_print_uint(gshell_last_command_id);
    platform_print("\n");

    platform_print("  length: ");
    platform_print_uint(gshell_input_len);
    platform_print("\n");

    platform_print("  events: ");
    platform_print_uint(gshell_input_events);
    platform_print("\n");

    platform_print("  enters: ");
    platform_print_uint(gshell_input_enters);
    platform_print("\n");

    platform_print("  backs:  ");
    platform_print_uint(gshell_input_backspaces);
    platform_print("\n");

    platform_print("  status: ");
    platform_print(gshell_input_status_text);
    platform_print("\n");

    platform_print("  command: ");
    platform_print(gshell_command_name);
    platform_print("\n");

    platform_print("  view: ");
    platform_print(gshell_command_view);
    platform_print("\n");

    platform_print("  result text: ");
    platform_print(gshell_command_result);
    platform_print("\n");

    platform_print("  dispatches: ");
    platform_print_uint(gshell_command_dispatches);
    platform_print("\n");

    platform_print("  unknown: ");
    platform_print_uint(gshell_command_unknown);
    platform_print("\n");

    platform_print("  history count: ");
    platform_print_uint(gshell_history_count);
    platform_print("\n");

    platform_print("  history total: ");
    platform_print_uint(gshell_history_total);
    platform_print("\n");

    platform_print("  result log count: ");
    platform_print_uint(gshell_result_log_count);
    platform_print("\n");

    platform_print("  result log total: ");
    platform_print_uint(gshell_result_log_total);
    platform_print("\n");

    platform_print("  terminal count: ");
    platform_print_uint(gshell_terminal_count);
    platform_print("\n");

    platform_print("  terminal total: ");
    platform_print_uint(gshell_terminal_total);
    platform_print("\n");

    platform_print("  render: ");

    if (gshell_graphics_can_render_now()) {
        platform_print("graphics-ready\n");
    } else {
        platform_print("text-safe\n");
    }

    platform_print("  result: ready\n");
}

void gshell_input_char(char c) {
    char normalized_char = c;

    if (!gshell_initialized) {
        return;
    }

    gshell_input_events++;

    /*
     * Enter dispatch.
     */
    if (c == '\n' || c == '\r') {
        gshell_input_enters++;

        gshell_input_buffer[gshell_input_len] = '\0';
        gshell_dispatch_command();

        gshell_input_len = 0;
        gshell_input_buffer[0] = '\0';

        if (gshell_graphics_can_render_now()) {
            gshell_graphics_dashboard();
        }

        return;
    }

    /*
     * Backspace.
     */
    if (c == '\b' || c == 127) {
        gshell_input_backspaces++;

        if (gshell_input_len > 0) {
            gshell_input_len--;
            gshell_input_buffer[gshell_input_len] = '\0';
        }

        gshell_input_status_text = "INPUT EDIT";

        if (gshell_graphics_can_render_now()) {
            gshell_graphics_dashboard();
        }

        return;
    }

    /*
     * Normalize printable command input.
     * Current keyboard path often emits uppercase ASCII.
     * Store lowercase so graphical command parser gets stable text.
     */
    if (normalized_char >= 'A' && normalized_char <= 'Z') {
        normalized_char = (char)(normalized_char + 32);
    }

    /*
     * Allow command characters.
     * Space is intentionally allowed so parser can trim:
     *   "  doctor  "
     */
    if (!(
        (normalized_char >= 'a' && normalized_char <= 'z') ||
        (normalized_char >= '0' && normalized_char <= '9') ||
        normalized_char == ' ' ||
        normalized_char == '-' ||
        normalized_char == '_' ||
        normalized_char == '.'
    )) {
        gshell_input_status_text = "INPUT IGNORED";

        if (gshell_graphics_can_render_now()) {
            gshell_graphics_dashboard();
        }

        return;
    }

    if (gshell_input_len + 1 >= GSHELL_INPUT_BUFFER_SIZE) {
        gshell_input_status_text = "INPUT FULL";

        if (gshell_graphics_can_render_now()) {
            gshell_graphics_dashboard();
        }

        return;
    }

    gshell_input_buffer[gshell_input_len] = normalized_char;
    gshell_input_len++;
    gshell_input_buffer[gshell_input_len] = '\0';

    gshell_input_status_text = "INPUT EDIT";

    if (gshell_graphics_can_render_now()) {
        gshell_graphics_dashboard();
    }
}

static void gshell_uint_to_text(unsigned int value, char* out, unsigned int max_len) {
    char temp[16];
    unsigned int temp_len = 0;
    unsigned int out_index = 0;
    unsigned int digit = 0;

    if (max_len == 0) {
        return;
    }

    if (value == 0) {
        if (max_len > 1) {
            out[0] = '0';
            out[1] = '\0';
        } else {
            out[0] = '\0';
        }

        return;
    }

    while (value > 0 && temp_len < 15) {
        digit = value % 10;
        temp[temp_len] = (char)('0' + digit);
        temp_len++;
        value = value / 10;
    }

    while (temp_len > 0 && out_index + 1 < max_len) {
        temp_len--;
        out[out_index] = temp[temp_len];
        out_index++;
    }

    out[out_index] = '\0';
}

static void gshell_draw_value_text(unsigned int x, unsigned int y, const char* label, const char* value) {
    graphics_text(x, y, label);
    graphics_text(x + 132, y, value);
}

static void gshell_draw_value_uint(unsigned int x, unsigned int y, const char* label, unsigned int value) {
    char value_text[16];

    gshell_uint_to_text(value, value_text, 16);

    graphics_text(x, y, label);
    graphics_text(x + 132, y, value_text);
}

static void gshell_draw_value_ok(unsigned int x, unsigned int y, const char* label, int ok) {
    graphics_text(x, y, label);
    graphics_text(x + 132, y, ok ? "OK" : "BAD");
}

static void gshell_draw_command_view_dash(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    gshell_kernel_bridge_refreshes++;

    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x00004477);
    graphics_rect(x, y + h - 4, w, 4, 0x00004477);
    graphics_rect(x, y, 4, h, 0x00004477);
    graphics_rect(x + w - 4, y, 4, h, 0x00004477);

    graphics_text(x + 24, y + 20, "REAL DASH");

    gshell_draw_value_uint(x + 24, y + 56, "UP", timer_get_seconds());
    gshell_draw_value_uint(x + 24, y + 84, "TICKS", timer_get_ticks());
    gshell_draw_value_uint(x + 24, y + 112, "MODULES", (unsigned int)module_count_loaded());
    gshell_draw_value_uint(x + 24, y + 140, "TASKS", (unsigned int)scheduler_task_count());
    gshell_draw_value_text(x + 24, y + 168, "ACTIVE", scheduler_get_active_task());
    gshell_draw_value_text(x + 24, y + 196, "MODE", scheduler_get_mode());
    gshell_draw_value_uint(x + 24, y + 224, "SYSCALLS", syscall_get_table_count());
    gshell_draw_value_uint(x + 24, y + 252, "BRIDGE", gshell_kernel_bridge_refreshes);

    graphics_rect(x + 24, y + h - 24, w - 48, 10, 0x0000FF66);
}

static void gshell_draw_command_view_health(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    gshell_kernel_bridge_refreshes++;

    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000FF66);
    graphics_rect(x, y + h - 4, w, 4, 0x0000FF66);
    graphics_rect(x, y, 4, h, 0x0000FF66);
    graphics_rect(x + w - 4, y, 4, h, 0x0000FF66);

    graphics_text(x + 24, y + 20, "REAL HEALTH");

    gshell_draw_value_ok(x + 24, y + 56, "DEPS", health_deps_ok());
    gshell_draw_value_ok(x + 24, y + 84, "TASK", health_task_ok());
    gshell_draw_value_ok(x + 24, y + 112, "SECURITY", health_security_ok());
    gshell_draw_value_ok(x + 24, y + 140, "MEMORY", health_memory_ok());
    gshell_draw_value_ok(x + 24, y + 168, "PAGING", health_paging_ok());
    gshell_draw_value_ok(x + 24, y + 196, "SYSCALL", health_syscall_ok());
    gshell_draw_value_ok(x + 24, y + 224, "USER", health_user_ok());
    gshell_draw_value_ok(x + 24, y + 252, "RESULT", health_result_ok());

    graphics_rect(x + 24, y + h - 24, w - 48, 10, health_result_ok() ? 0x0000FF66 : 0x00FFAA00);
}

static void gshell_draw_command_view_doctor(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    gshell_kernel_bridge_refreshes++;

    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x00FFAA00);
    graphics_rect(x, y + h - 4, w, 4, 0x00FFAA00);
    graphics_rect(x, y, 4, h, 0x00FFAA00);
    graphics_rect(x + w - 4, y, 4, h, 0x00FFAA00);

    graphics_text(x + 24, y + 20, "REAL DOCTOR");

    gshell_draw_value_ok(x + 24, y + 56, "MODULE", !module_has_broken_dependencies());
    gshell_draw_value_ok(x + 24, y + 84, "TASK", !scheduler_has_broken_tasks());
    gshell_draw_value_ok(x + 24, y + 112, "SWITCH", health_task_switch_ok());
    gshell_draw_value_ok(x + 24, y + 140, "SYSCALL", health_syscall_ok());
    gshell_draw_value_ok(x + 24, y + 168, "RING3", health_ring3_ok());
    gshell_draw_value_ok(x + 24, y + 196, "MEMORY", health_memory_ok());
    gshell_draw_value_ok(x + 24, y + 224, "PAGING", health_paging_ok());
    gshell_draw_value_ok(x + 24, y + 252, "RESULT", health_result_ok());

    graphics_rect(x + 24, y + h - 24, w - 48, 10, health_result_ok() ? 0x0000FF66 : 0x00FFAA00);
}

static void gshell_draw_command_view_milestone(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "GSHELL MILESTONE");
    graphics_text(x + 24, y + 56, "INPUT READY");
    graphics_text(x + 24, y + 84, "DISPATCH READY");
    graphics_text(x + 24, y + 112, "PARSER READY");
    graphics_text(x + 24, y + 140, "TERMINAL READY");
    graphics_text(x + 24, y + 168, "KERNEL BRIDGE OK");
    graphics_text(x + 24, y + 196, "HISTORY LOG OK");
    graphics_text(x + 24, y + 224, "GRAPHICS READY");
    graphics_text(x + 24, y + 252, "RESULT READY");
}

static void gshell_draw_command_view_status(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "GSHELL STATUS");

    gshell_draw_value_text(x + 24, y + 56, "INPUT", gshell_input_status_text);
    gshell_draw_value_text(x + 24, y + 84, "PARSER", gshell_parser_status_text);
    gshell_draw_value_text(x + 24, y + 112, "COMMAND", gshell_command_name);
    gshell_draw_value_text(x + 24, y + 140, "VIEW", gshell_command_view);

    gshell_draw_value_uint(x + 24, y + 168, "DISPATCH", gshell_command_dispatches);
    gshell_draw_value_uint(x + 24, y + 196, "UNKNOWN", gshell_command_unknown);
    gshell_draw_value_uint(x + 24, y + 224, "HISTORY", gshell_history_total);
    gshell_draw_value_uint(x + 24, y + 252, "TERMINAL", gshell_terminal_total);
}

static void gshell_draw_command_view_version(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    unsigned int label_x = x + 24;
    unsigned int value_x = x + 132;

    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(label_x, y + 20, "VERSION VIEW");

    graphics_text(label_x, y + 56, "NAME");
    graphics_text(value_x, y + 56, "LINGJING");

    graphics_text(label_x, y + 84, "VERSION");
    graphics_text(value_x, y + 84, LINGJING_VERSION);

    graphics_text(label_x, y + 112, "ARCH");
    graphics_text(value_x, y + 112, LINGJING_ARCH);

    graphics_text(label_x, y + 140, "PLATFORM");
    graphics_text(value_x, y + 140, LINGJING_PLATFORM);

    graphics_text(label_x, y + 168, "BOOT");
    graphics_text(value_x, y + 168, "MB1+MB2");

    graphics_text(label_x, y + 196, "STAGE");
    graphics_text(value_x, y + 196, "VALUE UI");

    graphics_text(label_x, y + 224, "INTENT");
    graphics_text(value_x, y + 224, LINGJING_INTENT_LAYER);

    graphics_text(label_x, y + 252, "RESULT");
    graphics_text(value_x, y + 252, "READY");
}

static void gshell_draw_command_view_about(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "ABOUT LINGJING");

    graphics_text(x + 24, y + 56, "MY MACHINE");
    graphics_text(x + 24, y + 84, "MY RULES");
    graphics_text(x + 24, y + 112, "USER CONTROL LAYER");
    graphics_text(x + 24, y + 140, "INTENT DRIVEN CORE");
    graphics_text(x + 24, y + 168, "GRAPHICAL KERNEL UI");
    graphics_text(x + 24, y + 196, "NO BLACK BOX CONTROL");
    graphics_text(x + 24, y + 224, "OWNER FIRST SYSTEM");
    graphics_text(x + 24, y + 252, "RESULT READY");
}

static void gshell_draw_command_view_bridge(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "CMD BRIDGE");

    graphics_text(x + 24, y + 56, "GRAPHICAL");
    graphics_text(x + 156, y + 56, "READY");

    graphics_text(x + 24, y + 84, "PARSER");
    graphics_text(x + 156, y + 84, "READY");

    graphics_text(x + 24, y + 112, "DISPATCH");
    graphics_text(x + 156, y + 112, "READY");

    graphics_text(x + 24, y + 140, "KERNEL READ");
    graphics_text(x + 156, y + 140, "PARTIAL");

    graphics_text(x + 24, y + 168, "TEXT CMDS");
    graphics_text(x + 156, y + 168, "PENDING");

    graphics_text(x + 24, y + 196, "SUPPORTED");
    graphics_text(x + 156, y + 196, "12");

    graphics_text(x + 24, y + 224, "NEXT");
    graphics_text(x + 156, y + 224, "REGISTRY");

    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_registry(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "COMMAND REGISTRY");
    graphics_text(x + 24, y + 52, "dash");
    graphics_text(x + 132, y + 52, "health");
    graphics_text(x + 24, y + 76, "doctor");
    graphics_text(x + 132, y + 76, "status");
    graphics_text(x + 24, y + 100, "version");
    graphics_text(x + 132, y + 100, "sysinfo");
    graphics_text(x + 24, y + 124, "meminfo");
    graphics_text(x + 132, y + 124, "pageinfo");
    graphics_text(x + 24, y + 148, "syscalls");
    graphics_text(x + 132, y + 148, "ring3info");
    graphics_text(x + 24, y + 172, "moduleinfo");
    graphics_text(x + 132, y + 172, "capinfo");
    graphics_text(x + 24, y + 196, "intentinfo");
    graphics_text(x + 132, y + 196, "taskinfo");
    graphics_text(x + 24, y + 224, "controlfinal");
    graphics_text(x + 132, y + 224, "controlhealth");
    graphics_text(x + 24, y + 252, "gatesummary");
    graphics_text(x + 132, y + 252, "nextstage");
}

static void gshell_draw_command_view_textcmds(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "TEXT CMD STATUS");
    graphics_text(x + 24, y + 52, "BASIC/SYSTEM");
    graphics_text(x + 156, y + 52, "PARTIAL");
    graphics_text(x + 24, y + 76, "MEM/PAGE");
    graphics_text(x + 156, y + 76, "PARTIAL");
    graphics_text(x + 24, y + 100, "SYSCALL/USER");
    graphics_text(x + 156, y + 100, "PARTIAL");
    graphics_text(x + 24, y + 124, "MODULE");
    graphics_text(x + 156, y + 124, "PARTIAL");
    graphics_text(x + 24, y + 148, "CAP/INTENT");
    graphics_text(x + 156, y + 148, "PARTIAL");
    graphics_text(x + 24, y + 172, "TASK/SCHED");
    graphics_text(x + 156, y + 172, "PARTIAL");
    graphics_text(x + 24, y + 196, "SEC/PLAT");
    graphics_text(x + 156, y + 196, "PARTIAL");
    graphics_text(x + 24, y + 220, "ID/LANG");
    graphics_text(x + 156, y + 220, "PARTIAL");
    graphics_text(x + 24, y + 244, "SERVICE/PKG");
    graphics_text(x + 156, y + 244, "PARTIAL");
    graphics_text(x + 24, y + 268, "NEXT");
    graphics_text(x + 156, y + 268, "0.8 CONTROL");
}

static void gshell_draw_command_view_sysinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "SYSTEM INFO");

    graphics_text(x + 24, y + 56, "NAME");
    graphics_text(x + 144, y + 56, "LINGJING");

    graphics_text(x + 24, y + 84, "VERSION");
    graphics_text(x + 144, y + 84, LINGJING_VERSION);

    graphics_text(x + 24, y + 112, "ARCH");
    graphics_text(x + 144, y + 112, LINGJING_ARCH);

    graphics_text(x + 24, y + 140, "PLATFORM");
    graphics_text(x + 144, y + 140, "BAREMETAL");

    graphics_text(x + 24, y + 168, "BOOT");
    graphics_text(x + 144, y + 168, "MB2 GFX");

    graphics_text(x + 24, y + 196, "DISPLAY");
    graphics_text(x + 144, y + 196, "800X600");

    graphics_text(x + 24, y + 224, "SHELL");
    graphics_text(x + 144, y + 224, "GRAPHICAL");

    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 144, y + 252, "READY");
}

static void gshell_draw_command_view_meminfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "MEMORY INFO");

    graphics_text(x + 24, y + 56, "ALLOCATOR");
    graphics_text(x + 156, y + 56, "BUMP");

    graphics_text(x + 24, y + 84, "HEAP");
    graphics_text(x + 156, y + 84, "READY");

    graphics_text(x + 24, y + 112, "KERNEL");
    graphics_text(x + 156, y + 112, "MAPPED");

    graphics_text(x + 24, y + 140, "PAGING");
    graphics_text(x + 156, y + 140, "READY");

    graphics_text(x + 24, y + 168, "MODE");
    graphics_text(x + 156, y + 168, "EARLY");

    graphics_text(x + 24, y + 196, "OWNER");
    graphics_text(x + 156, y + 196, "KERNEL");

    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");

    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_pageinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "PAGING INFO");

    graphics_text(x + 24, y + 56, "MODE");
    graphics_text(x + 156, y + 56, "ENABLED");

    graphics_text(x + 24, y + 84, "KERNEL");
    graphics_text(x + 156, y + 84, "MAPPED");

    graphics_text(x + 24, y + 112, "IDENTITY");
    graphics_text(x + 156, y + 112, "READY");

    graphics_text(x + 24, y + 140, "SWITCH");
    graphics_text(x + 156, y + 140, "READY");

    graphics_text(x + 24, y + 168, "USER MAP");
    graphics_text(x + 156, y + 168, "EARLY");

    graphics_text(x + 24, y + 196, "PROTECT");
    graphics_text(x + 156, y + 196, "KERNEL");

    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");

    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_syscalls(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "SYSCALL INFO");

    graphics_text(x + 24, y + 56, "INT");
    graphics_text(x + 156, y + 56, "0X80");

    graphics_text(x + 24, y + 84, "TABLE");
    graphics_text(x + 156, y + 84, "READY");

    graphics_text(x + 24, y + 112, "COUNT");
    graphics_text(x + 156, y + 112, "8");

    graphics_text(x + 24, y + 140, "FRAME");
    graphics_text(x + 156, y + 140, "READY");

    graphics_text(x + 24, y + 168, "ARGS");
    graphics_text(x + 156, y + 168, "READY");

    graphics_text(x + 24, y + 196, "RETURN");
    graphics_text(x + 156, y + 196, "READY");

    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");

    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_ring3info(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "USER RING3 INFO");

    graphics_text(x + 24, y + 56, "USER MODE");
    graphics_text(x + 156, y + 56, "READY");

    graphics_text(x + 24, y + 84, "RING3");
    graphics_text(x + 156, y + 84, "READY");

    graphics_text(x + 24, y + 112, "GDT");
    graphics_text(x + 156, y + 112, "READY");

    graphics_text(x + 24, y + 140, "TSS");
    graphics_text(x + 156, y + 140, "READY");

    graphics_text(x + 24, y + 168, "INT80");
    graphics_text(x + 156, y + 168, "READY");

    graphics_text(x + 24, y + 196, "RETURN");
    graphics_text(x + 156, y + 196, "READY");

    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");

    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_moduleinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "MODULE INFO");
    graphics_text(x + 24, y + 56, "TOTAL");
    graphics_text(x + 156, y + 56, "22");
    graphics_text(x + 24, y + 84, "CORE");
    graphics_text(x + 156, y + 84, "LOCKED");
    graphics_text(x + 24, y + 112, "DEPS");
    graphics_text(x + 156, y + 112, "READY");
    graphics_text(x + 24, y + 140, "LOAD");
    graphics_text(x + 156, y + 140, "EARLY");
    graphics_text(x + 24, y + 168, "UNLOAD");
    graphics_text(x + 156, y + 168, "GUARDED");
    graphics_text(x + 24, y + 196, "TREE");
    graphics_text(x + 156, y + 196, "READY");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_modulecheck(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "MODULE CHECK");
    graphics_text(x + 24, y + 56, "MODULE");
    graphics_text(x + 156, y + 56, "OK");
    graphics_text(x + 24, y + 84, "TASK");
    graphics_text(x + 156, y + 84, "OK");
    graphics_text(x + 24, y + 112, "SYSCALL");
    graphics_text(x + 156, y + 112, "OK");
    graphics_text(x + 24, y + 140, "MEMORY");
    graphics_text(x + 156, y + 140, "OK");
    graphics_text(x + 24, y + 168, "PAGING");
    graphics_text(x + 156, y + 168, "OK");
    graphics_text(x + 24, y + 196, "CAP");
    graphics_text(x + 156, y + 196, "OK");
    graphics_text(x + 24, y + 224, "INTENT");
    graphics_text(x + 156, y + 224, "OK");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "OK");
}

static void gshell_draw_command_view_capinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "CAPABILITY INFO");
    graphics_text(x + 24, y + 56, "REGISTRY");
    graphics_text(x + 156, y + 56, "READY");
    graphics_text(x + 24, y + 84, "GUI");
    graphics_text(x + 156, y + 84, "READY");
    graphics_text(x + 24, y + 112, "NET");
    graphics_text(x + 156, y + 112, "PLANNED");
    graphics_text(x + 24, y + 140, "AI");
    graphics_text(x + 156, y + 140, "PLANNED");
    graphics_text(x + 24, y + 168, "SYSCALL");
    graphics_text(x + 156, y + 168, "READY");
    graphics_text(x + 24, y + 196, "USER");
    graphics_text(x + 156, y + 196, "READY");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_intentinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "INTENT INFO");
    graphics_text(x + 24, y + 56, "LAYER");
    graphics_text(x + 156, y + 56, "READY");
    graphics_text(x + 24, y + 84, "PLAN");
    graphics_text(x + 156, y + 84, "READY");
    graphics_text(x + 24, y + 112, "RUN");
    graphics_text(x + 156, y + 112, "READY");
    graphics_text(x + 24, y + 140, "STOP");
    graphics_text(x + 156, y + 140, "READY");
    graphics_text(x + 24, y + 168, "POLICY");
    graphics_text(x + 156, y + 168, "GUARDED");
    graphics_text(x + 24, y + 196, "CAP REQ");
    graphics_text(x + 156, y + 196, "READY");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_taskinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "TASK INFO");
    gshell_draw_value_uint(x + 24, y + 56, "TASKS", (unsigned int)scheduler_task_count());
    gshell_draw_value_text(x + 24, y + 84, "ACTIVE", scheduler_get_active_task());
    gshell_draw_value_text(x + 24, y + 112, "MODE", scheduler_get_mode());
    gshell_draw_value_uint(x + 24, y + 140, "TICKS", scheduler_get_ticks());
    gshell_draw_value_uint(x + 24, y + 168, "YIELDS", scheduler_get_yields());
    gshell_draw_value_ok(x + 24, y + 196, "BROKEN", !scheduler_has_broken_tasks());
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, scheduler_has_broken_tasks() ? "BAD" : "READY");
}

static void gshell_draw_command_view_tasklist(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "TASK LIST");
    graphics_text(x + 24, y + 56, "0 idle");
    graphics_text(x + 156, y + 56, "running");
    graphics_text(x + 24, y + 84, "1 shell");
    graphics_text(x + 156, y + 84, "ready");
    graphics_text(x + 24, y + 112, "2 intent");
    graphics_text(x + 156, y + 112, "ready");
    graphics_text(x + 24, y + 140, "3 timer");
    graphics_text(x + 156, y + 140, "ready");
    gshell_draw_value_uint(x + 24, y + 180, "COUNT", (unsigned int)scheduler_task_count());
    gshell_draw_value_text(x + 24, y + 208, "ACTIVE", scheduler_get_active_task());
    graphics_text(x + 24, y + 248, "SOURCE");
    graphics_text(x + 156, y + 248, "scheduler");
}

static void gshell_draw_command_view_schedinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "SCHEDULER INFO");
    gshell_draw_value_text(x + 24, y + 56, "MODE", scheduler_get_mode());
    gshell_draw_value_text(x + 24, y + 84, "ACTIVE", scheduler_get_active_task());
    gshell_draw_value_uint(x + 24, y + 112, "TICKS", scheduler_get_ticks());
    gshell_draw_value_uint(x + 24, y + 140, "YIELDS", scheduler_get_yields());
    gshell_draw_value_uint(x + 24, y + 168, "TASKS", (unsigned int)scheduler_task_count());
    gshell_draw_value_ok(x + 24, y + 196, "STATE", !scheduler_has_broken_tasks());
    graphics_text(x + 24, y + 224, "TYPE");
    graphics_text(x + 156, y + 224, "cooperative");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_runqueue(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "RUNQUEUE");
    graphics_text(x + 24, y + 56, "idle");
    graphics_text(x + 156, y + 56, "runnable");
    graphics_text(x + 24, y + 84, "shell");
    graphics_text(x + 156, y + 84, "runnable");
    graphics_text(x + 24, y + 112, "intent");
    graphics_text(x + 156, y + 112, "runnable");
    graphics_text(x + 24, y + 140, "timer");
    graphics_text(x + 156, y + 140, "runnable");
    gshell_draw_value_text(x + 24, y + 180, "ACTIVE", scheduler_get_active_task());
    gshell_draw_value_uint(x + 24, y + 208, "YIELDS", scheduler_get_yields());
    graphics_text(x + 24, y + 248, "POLICY");
    graphics_text(x + 156, y + 248, "round-robin");
}

static void gshell_draw_command_view_securityinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "SECURITY INFO");
    gshell_draw_value_ok(x + 24, y + 56, "DOCTOR", security_doctor_ok());
    gshell_draw_value_ok(x + 24, y + 84, "HEALTH", health_security_ok());
    graphics_text(x + 24, y + 112, "INTENT");
    graphics_text(x + 156, y + 112, "GUARDED");
    graphics_text(x + 24, y + 140, "MODULE LOAD");
    graphics_text(x + 156, y + 140, "GUARDED");
    graphics_text(x + 24, y + 168, "MODULE UNLOAD");
    graphics_text(x + 156, y + 168, "GUARDED");
    graphics_text(x + 24, y + 196, "AUDIT");
    graphics_text(x + 156, y + 196, "READY");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, security_doctor_ok() ? "READY" : "BAD");
}

static void gshell_draw_command_view_platforminfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "PLATFORM INFO");
    gshell_draw_value_text(x + 24, y + 56, "NAME", platform_get_name());
    gshell_draw_value_text(x + 24, y + 84, "DISPLAY", platform_get_display());
    gshell_draw_value_text(x + 24, y + 112, "TIMER", platform_get_timer());
    gshell_draw_value_text(x + 24, y + 140, "INPUT", platform_get_input());
    gshell_draw_value_ok(x + 24, y + 168, "DOCTOR", platform_doctor_ok());
    gshell_draw_value_text(x + 24, y + 196, "BOOT", LINGJING_BOOT);
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, platform_doctor_ok() ? "READY" : "BAD");
}

static void gshell_draw_command_view_identityinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "IDENTITY INFO");
    gshell_draw_value_text(x + 24, y + 56, "STATE", identity_get_state());
    gshell_draw_value_text(x + 24, y + 84, "MODE", identity_get_mode());
    gshell_draw_value_text(x + 24, y + 112, "PUBKEY", identity_get_public_key());
    gshell_draw_value_ok(x + 24, y + 140, "READY", identity_is_ready());
    gshell_draw_value_ok(x + 24, y + 168, "DOCTOR", identity_doctor_ok());
    graphics_text(x + 24, y + 196, "POLICY");
    graphics_text(x + 156, y + 196, "LOCAL");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, identity_doctor_ok() ? "READY" : "BAD");
}

static void gshell_draw_command_view_langinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "LANGUAGE INFO");
    gshell_draw_value_text(x + 24, y + 56, "CURRENT", lang_get_current_name());
    gshell_draw_value_text(x + 24, y + 84, "MSG READY", lang_get(MSG_SYSTEM_READY));
    gshell_draw_value_text(x + 24, y + 112, "MSG DOCTOR", lang_get(MSG_DOCTOR_OK));
    gshell_draw_value_ok(x + 24, y + 140, "DOCTOR", lang_doctor_ok());
    graphics_text(x + 24, y + 168, "ENGLISH");
    graphics_text(x + 156, y + 168, "READY");
    graphics_text(x + 24, y + 196, "CHINESE");
    graphics_text(x + 156, y + 196, "READY");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, lang_doctor_ok() ? "READY" : "BAD");
}

static void gshell_draw_command_view_bootinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "BOOT INFO");
    gshell_draw_value_ok(x + 24, y + 56, "MULTIBOOT", bootinfo_is_multiboot_valid());
    gshell_draw_value_ok(x + 24, y + 84, "FRAMEBUF", bootinfo_has_framebuffer());
    gshell_draw_value_uint(x + 24, y + 112, "MAGIC", bootinfo_get_magic());
    gshell_draw_value_uint(x + 24, y + 140, "INFOADDR", bootinfo_get_info_addr());
    gshell_draw_value_uint(x + 24, y + 168, "FLAGS", bootinfo_get_flags());
    gshell_draw_value_uint(x + 24, y + 196, "REQ W", bootinfo_get_requested_width());
    gshell_draw_value_uint(x + 24, y + 224, "REQ H", bootinfo_get_requested_height());
    gshell_draw_value_uint(x + 24, y + 252, "REQ BPP", bootinfo_get_requested_bpp());
}

static void gshell_draw_command_view_fbinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "FRAMEBUFFER INFO");
    gshell_draw_value_ok(x + 24, y + 56, "READY", framebuffer_is_ready());
    gshell_draw_value_ok(x + 24, y + 84, "BROKEN", !framebuffer_is_broken());
    gshell_draw_value_uint(x + 24, y + 112, "WIDTH", framebuffer_get_width());
    gshell_draw_value_uint(x + 24, y + 140, "HEIGHT", framebuffer_get_height());
    gshell_draw_value_uint(x + 24, y + 168, "BPP", framebuffer_get_bpp());
    gshell_draw_value_uint(x + 24, y + 196, "PITCH", framebuffer_get_pitch());
    gshell_draw_value_uint(x + 24, y + 224, "ADDR", framebuffer_get_address());
    gshell_draw_value_text(x + 24, y + 252, "PROFILE", framebuffer_get_profile());
}

static void gshell_draw_command_view_gfxinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "GRAPHICS INFO");
    gshell_draw_value_ok(x + 24, y + 56, "READY", graphics_is_ready());
    gshell_draw_value_ok(x + 24, y + 84, "BACKEND", graphics_backend_is_ready());
    gshell_draw_value_text(x + 24, y + 112, "MODE", graphics_get_mode());
    gshell_draw_value_text(x + 24, y + 140, "BACKEND", graphics_get_backend());
    gshell_draw_value_text(x + 24, y + 168, "LAST OP", graphics_get_last_op());
    gshell_draw_value_uint(x + 24, y + 196, "DRAWS", graphics_get_draw_count());
    gshell_draw_value_uint(x + 24, y + 224, "LAST W", graphics_get_last_w());
    gshell_draw_value_uint(x + 24, y + 252, "LAST H", graphics_get_last_h());
}

static void gshell_draw_command_view_fontinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "FONT INFO");
    gshell_draw_value_text(x + 24, y + 56, "FONT", graphics_get_font_name());
    gshell_draw_value_uint(x + 24, y + 84, "WIDTH", graphics_get_font_width());
    gshell_draw_value_uint(x + 24, y + 112, "HEIGHT", graphics_get_font_height());
    gshell_draw_value_uint(x + 24, y + 140, "TEXT W", graphics_get_last_text_width());
    gshell_draw_value_uint(x + 24, y + 168, "TEXT H", graphics_get_last_text_height());
    gshell_draw_value_ok(x + 24, y + 196, "TEXT ARM", graphics_text_backend_is_armed());
    graphics_text(x + 24, y + 224, "RENDER");
    graphics_text(x + 156, y + 224, "BITMAP");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_fsinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "FILESYSTEM INFO");
    graphics_text(x + 24, y + 56, "ROOT");
    graphics_text(x + 156, y + 56, "plan");
    graphics_text(x + 24, y + 84, "VFS");
    graphics_text(x + 156, y + 84, "meta");
    graphics_text(x + 24, y + 112, "MOUNT");
    graphics_text(x + 156, y + 112, "not-ready");
    graphics_text(x + 24, y + 140, "FILE OPS");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "RAMFS");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "BRIDGE");
    graphics_text(x + 156, y + 196, "PARTIAL");
    graphics_text(x + 24, y + 224, "POLICY");
    graphics_text(x + 156, y + 224, "MY MACHINE");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_deviceinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "DEVICE INFO");
    graphics_text(x + 24, y + 56, "KEYBOARD");
    graphics_text(x + 156, y + 56, "READY");
    graphics_text(x + 24, y + 84, "TIMER");
    graphics_text(x + 156, y + 84, "READY");
    gshell_draw_value_ok(x + 24, y + 112, "FRAMEBUF", framebuffer_is_ready());
    gshell_draw_value_ok(x + 24, y + 140, "GRAPHICS", graphics_is_ready());
    graphics_text(x + 24, y + 168, "STORAGE");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "NETWORK");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_driverinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "DRIVER INFO");
    graphics_text(x + 24, y + 56, "screen");
    graphics_text(x + 156, y + 56, "built-in");
    graphics_text(x + 24, y + 84, "keyboard");
    graphics_text(x + 156, y + 84, "built-in");
    graphics_text(x + 24, y + 112, "timer");
    graphics_text(x + 156, y + 112, "built-in");
    graphics_text(x + 24, y + 140, "framebuffer");
    graphics_text(x + 156, y + 140, "built-in");
    graphics_text(x + 24, y + 168, "graphics");
    graphics_text(x + 156, y + 168, "built-in");
    graphics_text(x + 24, y + 196, "block");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "net");
    graphics_text(x + 156, y + 224, "plan");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_ioinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "IO INFO");
    graphics_text(x + 24, y + 56, "PORT IO");
    graphics_text(x + 156, y + 56, "READY");
    graphics_text(x + 24, y + 84, "INTERRUPT");
    graphics_text(x + 156, y + 84, "IDT");
    graphics_text(x + 24, y + 112, "INPUT");
    graphics_text(x + 156, y + 112, platform_get_input());
    graphics_text(x + 24, y + 140, "OUTPUT");
    graphics_text(x + 156, y + 140, platform_get_display());
    graphics_text(x + 24, y + 168, "TIMER");
    graphics_text(x + 156, y + 168, platform_get_timer());
    graphics_text(x + 24, y + 196, "DMA");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_processinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "PROCESS INFO");
    graphics_text(x + 24, y + 56, "MODEL");
    graphics_text(x + 156, y + 56, "plan");
    graphics_text(x + 24, y + 84, "ADDR SPACE");
    graphics_text(x + 156, y + 84, "kernel/user");
    graphics_text(x + 24, y + 112, "USER MODE");
    graphics_text(x + 156, y + 112, user_get_mode());
    graphics_text(x + 24, y + 140, "RING3");
    graphics_text(x + 156, y + 140, ring3_get_state());
    gshell_draw_value_uint(x + 24, y + 168, "TASKS", (unsigned int)scheduler_task_count());
    gshell_draw_value_text(x + 24, y + 196, "ACTIVE", scheduler_get_active_task());
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_procinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "PROCESS TABLE");
    graphics_text(x + 24, y + 56, "pid 0");
    graphics_text(x + 156, y + 56, "kernel");
    graphics_text(x + 24, y + 84, "pid 1");
    graphics_text(x + 156, y + 84, "shell");
    graphics_text(x + 24, y + 112, "pid 2");
    graphics_text(x + 156, y + 112, "intent");
    graphics_text(x + 24, y + 140, "pid 3");
    graphics_text(x + 156, y + 140, "runtime");
    gshell_draw_value_uint(x + 24, y + 180, "PROGRAMS", user_get_program_count());
    gshell_draw_value_uint(x + 24, y + 208, "ENTRIES", user_get_entry_count());
    graphics_text(x + 24, y + 248, "SOURCE");
    graphics_text(x + 156, y + 248, "user/task");
}

static void gshell_draw_command_view_runtimeinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "RUNTIME INFO");
    graphics_text(x + 24, y + 56, "USER STATE");
    graphics_text(x + 156, y + 56, user_get_state());
    graphics_text(x + 24, y + 84, "USER MODE");
    graphics_text(x + 156, y + 84, user_get_mode());
    graphics_text(x + 24, y + 112, "RING3 MODE");
    graphics_text(x + 156, y + 112, ring3_get_mode());
    graphics_text(x + 24, y + 140, "SYSCALL");
    graphics_text(x + 156, y + 140, syscall_get_state());
    gshell_draw_value_uint(x + 24, y + 168, "SYSCALLS", syscall_get_table_count());
    graphics_text(x + 24, y + 196, "ABI");
    graphics_text(x + 156, y + 196, "int80");
    graphics_text(x + 24, y + 224, "LOADER");
    graphics_text(x + 156, y + 224, "plan");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_appinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "APP INFO");
    graphics_text(x + 24, y + 56, "APP LAYER");
    graphics_text(x + 156, y + 56, "plan");
    graphics_text(x + 24, y + 84, "BUILTIN APP");
    graphics_text(x + 156, y + 84, "shell");
    graphics_text(x + 24, y + 112, "APP ABI");
    graphics_text(x + 156, y + 112, "plan");
    graphics_text(x + 24, y + 140, "APP LOAD");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "SANDBOX");
    graphics_text(x + 156, y + 168, "cap");
    gshell_draw_value_ok(x + 24, y + 196, "USER", health_user_ok());
    gshell_draw_value_ok(x + 24, y + 224, "RING3", health_ring3_ok());
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_elfinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "ELF INFO");
    graphics_text(x + 24, y + 56, "FORMAT");
    graphics_text(x + 156, y + 56, "ELF32");
    graphics_text(x + 24, y + 84, "CLASS");
    graphics_text(x + 156, y + 84, "32-bit");
    graphics_text(x + 24, y + 112, "ARCH");
    graphics_text(x + 156, y + 112, "i386");
    graphics_text(x + 24, y + 140, "TYPE");
    graphics_text(x + 156, y + 140, "executable");
    graphics_text(x + 24, y + 168, "PARSER");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "VALIDATOR");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_loaderinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "LOADER INFO");
    graphics_text(x + 24, y + 56, "KERNEL LOAD");
    graphics_text(x + 156, y + 56, "grub");
    graphics_text(x + 24, y + 84, "USER LOAD");
    graphics_text(x + 156, y + 84, "plan");
    graphics_text(x + 24, y + 112, "ENTRY CHECK");
    graphics_text(x + 156, y + 112, "plan");
    graphics_text(x + 24, y + 140, "SEGMENTS");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "PAGE MAP");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "CAP CHECK");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_binaryinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "BINARY INFO");
    graphics_text(x + 24, y + 56, "KERNEL");
    graphics_text(x + 156, y + 56, "flat binary");
    graphics_text(x + 24, y + 84, "USER APP");
    graphics_text(x + 156, y + 84, "plan");
    graphics_text(x + 24, y + 112, "SECTION");
    graphics_text(x + 156, y + 112, "plan");
    graphics_text(x + 24, y + 140, "SYMBOL");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "VERIFY");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "SOURCE");
    graphics_text(x + 156, y + 196, "loader");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_abiinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "ABI INFO");
    graphics_text(x + 24, y + 56, "ARCH");
    graphics_text(x + 156, y + 56, "i386");
    graphics_text(x + 24, y + 84, "CALL GATE");
    graphics_text(x + 156, y + 84, "int 0x80");
    graphics_text(x + 24, y + 112, "SYSCALL ABI");
    graphics_text(x + 156, y + 112, "eax/ebx/ecx");
    graphics_text(x + 24, y + 140, "RETURN ABI");
    graphics_text(x + 156, y + 140, "eax");
    graphics_text(x + 24, y + 168, "USER RING");
    graphics_text(x + 156, y + 168, "ring3");
    graphics_text(x + 24, y + 196, "SANDBOX");
    graphics_text(x + 156, y + 196, "cap");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_sandboxinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "SANDBOX INFO");
    graphics_text(x + 24, y + 56, "MODEL");
    graphics_text(x + 156, y + 56, "cap");
    graphics_text(x + 24, y + 84, "USER RING");
    graphics_text(x + 156, y + 84, "ring3");
    graphics_text(x + 24, y + 112, "SYSCALL GATE");
    graphics_text(x + 156, y + 112, "int 0x80");
    graphics_text(x + 24, y + 140, "FILE ACCESS");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "DEV ACCESS");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "NET ACCESS");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_policyinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "POLICY INFO");
    graphics_text(x + 24, y + 56, "OWNER");
    graphics_text(x + 156, y + 56, "user");
    graphics_text(x + 24, y + 84, "DEFAULT");
    graphics_text(x + 156, y + 84, "ask-user");
    graphics_text(x + 24, y + 112, "INTENT RUN");
    graphics_text(x + 156, y + 112, "guarded");
    graphics_text(x + 24, y + 140, "MODULE LOAD");
    graphics_text(x + 156, y + 140, "guarded");
    graphics_text(x + 24, y + 168, "APP START");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "AUDIT");
    graphics_text(x + 156, y + 196, "meta");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_permissioninfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "PERMISSION INFO");
    graphics_text(x + 24, y + 56, "CAPABILITY");
    graphics_text(x + 156, y + 56, "registry");
    graphics_text(x + 24, y + 84, "ALLOW");
    graphics_text(x + 156, y + 84, "meta");
    graphics_text(x + 24, y + 112, "DENY");
    graphics_text(x + 156, y + 112, "meta");
    graphics_text(x + 24, y + 140, "REQUEST");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "SCOPE");
    graphics_text(x + 156, y + 168, "intent/app");
    graphics_text(x + 24, y + 196, "USER DEC");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_guardinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "GUARD INFO");
    graphics_text(x + 24, y + 56, "SECURITY");
    graphics_text(x + 156, y + 56, security_doctor_ok() ? "ok" : "bad");
    graphics_text(x + 24, y + 84, "IDENTITY");
    graphics_text(x + 156, y + 84, identity_doctor_ok() ? "ok" : "bad");
    graphics_text(x + 24, y + 112, "SYSCALL");
    graphics_text(x + 156, y + 112, "guarded");
    graphics_text(x + 24, y + 140, "INTENT");
    graphics_text(x + 156, y + 140, "guarded");
    graphics_text(x + 24, y + 168, "MODULE");
    graphics_text(x + 156, y + 168, "guarded");
    graphics_text(x + 24, y + 196, "APP");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_controlinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "USER CONTROL INFO");
    graphics_text(x + 24, y + 56, "PRINCIPLE");
    graphics_text(x + 156, y + 56, "my rules");
    graphics_text(x + 24, y + 84, "OWNER");
    graphics_text(x + 156, y + 84, "user");
    graphics_text(x + 24, y + 112, "DEFAULT");
    graphics_text(x + 156, y + 112, "ask-user");
    graphics_text(x + 24, y + 140, "INTENT");
    graphics_text(x + 156, y + 140, "guarded");
    graphics_text(x + 24, y + 168, "APP");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "SHELL");
    graphics_text(x + 156, y + 196, "gshell");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_ownerinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "OWNER INFO");
    graphics_text(x + 24, y + 56, "MACHINE");
    graphics_text(x + 156, y + 56, "owned");
    graphics_text(x + 24, y + 84, "IDENTITY");
    graphics_text(x + 156, y + 84, identity_doctor_ok() ? "ready" : "bad");
    graphics_text(x + 24, y + 112, "POLICY");
    graphics_text(x + 156, y + 112, "local");
    graphics_text(x + 24, y + 140, "AUTHORITY");
    graphics_text(x + 156, y + 140, "user");
    graphics_text(x + 24, y + 168, "SYSTEM");
    graphics_text(x + 156, y + 168, "controlled");
    graphics_text(x + 24, y + 196, "OVERRIDE");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_decisioninfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "DECISION INFO");
    graphics_text(x + 24, y + 56, "DECIDER");
    graphics_text(x + 156, y + 56, "user");
    graphics_text(x + 24, y + 84, "ALLOW");
    graphics_text(x + 156, y + 84, "meta");
    graphics_text(x + 24, y + 112, "DENY");
    graphics_text(x + 156, y + 112, "meta");
    graphics_text(x + 24, y + 140, "ASK");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "REMEMBER");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "AUDIT");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_ruleinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "RULE INFO");
    graphics_text(x + 24, y + 56, "RULESET");
    graphics_text(x + 156, y + 56, "user-rules");
    graphics_text(x + 24, y + 84, "SOURCE");
    graphics_text(x + 156, y + 84, "local");
    graphics_text(x + 24, y + 112, "SCOPE");
    graphics_text(x + 156, y + 112, "intent/app");
    graphics_text(x + 24, y + 140, "CAPABILITY");
    graphics_text(x + 156, y + 140, "guarded");
    graphics_text(x + 24, y + 168, "PRIORITY");
    graphics_text(x + 156, y + 168, "user");
    graphics_text(x + 24, y + 196, "ENGINE");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_auditinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "AUDIT INFO");
    graphics_text(x + 24, y + 56, "AUDIT MODE");
    graphics_text(x + 156, y + 56, "meta");
    graphics_text(x + 24, y + 84, "OWNER ACT");
    graphics_text(x + 156, y + 84, "tracked");
    graphics_text(x + 24, y + 112, "INTENT RUN");
    graphics_text(x + 156, y + 112, "tracked");
    graphics_text(x + 24, y + 140, "MOD CHANGE");
    graphics_text(x + 156, y + 140, "tracked");
    graphics_text(x + 24, y + 168, "POLICY CHG");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "PERSIST");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_traceinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "TRACE INFO");
    graphics_text(x + 24, y + 56, "TRACE MODE");
    graphics_text(x + 156, y + 56, "meta");
    graphics_text(x + 24, y + 84, "COMMAND");
    graphics_text(x + 156, y + 84, "tracked");
    graphics_text(x + 24, y + 112, "SYSCALL");
    graphics_text(x + 156, y + 112, "tracked");
    graphics_text(x + 24, y + 140, "TASK");
    graphics_text(x + 156, y + 140, "tracked");
    graphics_text(x + 24, y + 168, "INTENT");
    graphics_text(x + 156, y + 168, "tracked");
    graphics_text(x + 24, y + 196, "EXPORT");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_loginfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "LOG INFO");
    graphics_text(x + 24, y + 56, "TERMINAL LOG");
    graphics_text(x + 156, y + 56, "ready");
    graphics_text(x + 24, y + 84, "RESULT LOG");
    graphics_text(x + 156, y + 84, "ready");
    graphics_text(x + 24, y + 112, "HISTORY");
    graphics_text(x + 156, y + 112, "ready");
    graphics_text(x + 24, y + 140, "KERNEL LOG");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "STORE LOG");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "LEVEL");
    graphics_text(x + 156, y + 196, "info");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_eventinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "EVENT INFO");
    graphics_text(x + 24, y + 56, "INPUT EVENT");
    graphics_text(x + 156, y + 56, "ready");
    graphics_text(x + 24, y + 84, "CMD EVENT");
    graphics_text(x + 156, y + 84, "ready");
    graphics_text(x + 24, y + 112, "TASK EVENT");
    graphics_text(x + 156, y + 112, "meta");
    graphics_text(x + 24, y + 140, "INTENT EVENT");
    graphics_text(x + 156, y + 140, "meta");
    graphics_text(x + 24, y + 168, "SYS EVENT");
    graphics_text(x + 156, y + 168, "meta");
    graphics_text(x + 24, y + 196, "BUS");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_networkinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "NETWORK INFO");
    graphics_text(x + 24, y + 56, "STACK");
    graphics_text(x + 156, y + 56, "plan");
    graphics_text(x + 24, y + 84, "DRIVER");
    graphics_text(x + 156, y + 84, "plan");
    graphics_text(x + 24, y + 112, "IPV4");
    graphics_text(x + 156, y + 112, "plan");
    graphics_text(x + 24, y + 140, "IPV6");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "P2P");
    graphics_text(x + 156, y + 168, "future");
    graphics_text(x + 24, y + 196, "POLICY");
    graphics_text(x + 156, y + 196, "owned");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_netinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "NET DEVICE INFO");
    graphics_text(x + 24, y + 56, "LOOPBACK");
    graphics_text(x + 156, y + 56, "plan");
    graphics_text(x + 24, y + 84, "ETHERNET");
    graphics_text(x + 156, y + 84, "plan");
    graphics_text(x + 24, y + 112, "WIRELESS");
    graphics_text(x + 156, y + 112, "plan");
    graphics_text(x + 24, y + 140, "RX QUEUE");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "TX QUEUE");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "DISCOVERY");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_socketinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "SOCKET INFO");
    graphics_text(x + 24, y + 56, "SOCKET API");
    graphics_text(x + 156, y + 56, "plan");
    graphics_text(x + 24, y + 84, "TCP SOCKET");
    graphics_text(x + 156, y + 84, "plan");
    graphics_text(x + 24, y + 112, "UDP SOCKET");
    graphics_text(x + 156, y + 112, "plan");
    graphics_text(x + 24, y + 140, "PORT TABLE");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "ACCEPT");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "CONNECT");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_protocolinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "PROTOCOL INFO");
    graphics_text(x + 24, y + 56, "ARP");
    graphics_text(x + 156, y + 56, "plan");
    graphics_text(x + 24, y + 84, "IPV4");
    graphics_text(x + 156, y + 84, "plan");
    graphics_text(x + 24, y + 112, "IPV6");
    graphics_text(x + 156, y + 112, "plan");
    graphics_text(x + 24, y + 140, "ICMP");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "TCP");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "UDP");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "DNS");
    graphics_text(x + 156, y + 224, "plan");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_serviceinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);
    graphics_text(x + 24, y + 20, "SERVICE INFO");
    graphics_text(x + 24, y + 56, "SVC MODEL");
    graphics_text(x + 156, y + 56, "plan");
    graphics_text(x + 24, y + 84, "SYS SERVICE");
    graphics_text(x + 156, y + 84, "meta");
    graphics_text(x + 24, y + 112, "USR SERVICE");
    graphics_text(x + 156, y + 112, "plan");
    graphics_text(x + 24, y + 140, "CAPABILITY");
    graphics_text(x + 156, y + 140, "guarded");
    graphics_text(x + 24, y + 168, "SUPERVISOR");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "RESTART");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_sessioninfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);
    graphics_text(x + 24, y + 20, "SESSION INFO");
    graphics_text(x + 24, y + 56, "SESSION");
    graphics_text(x + 156, y + 56, "local");
    graphics_text(x + 24, y + 84, "OWNER");
    graphics_text(x + 156, y + 84, "user");
    graphics_text(x + 24, y + 112, "SHELL");
    graphics_text(x + 156, y + 112, "gshell");
    graphics_text(x + 24, y + 140, "TERMINAL");
    graphics_text(x + 156, y + 140, "ready");
    graphics_text(x + 24, y + 168, "STATE");
    graphics_text(x + 156, y + 168, "active");
    graphics_text(x + 24, y + 196, "AUTH");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_channelinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);
    graphics_text(x + 24, y + 20, "CHANNEL INFO");
    graphics_text(x + 24, y + 56, "CMD CHAN");
    graphics_text(x + 156, y + 56, "ready");
    graphics_text(x + 24, y + 84, "INTENT CH");
    graphics_text(x + 156, y + 84, "meta");
    graphics_text(x + 24, y + 112, "EVENT CH");
    graphics_text(x + 156, y + 112, "meta");
    graphics_text(x + 24, y + 140, "SVC CHAN");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "APP CHAN");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "POLICY");
    graphics_text(x + 156, y + 196, "guarded");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_businfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);
    graphics_text(x + 24, y + 20, "BUS INFO");
    graphics_text(x + 24, y + 56, "MESSAGE BUS");
    graphics_text(x + 156, y + 56, "plan");
    graphics_text(x + 24, y + 84, "EVENT BUS");
    graphics_text(x + 156, y + 84, "plan");
    graphics_text(x + 24, y + 112, "INTENT BUS");
    graphics_text(x + 156, y + 112, "meta");
    graphics_text(x + 24, y + 140, "SERVICE BUS");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "APP BUS");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "SECURITY");
    graphics_text(x + 156, y + 196, "guarded");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_packageinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);
    graphics_text(x + 24, y + 20, "PACKAGE INFO");
    graphics_text(x + 24, y + 56, "PKG MODEL");
    graphics_text(x + 156, y + 56, "plan");
    graphics_text(x + 24, y + 84, "APP PKG");
    graphics_text(x + 156, y + 84, "plan");
    graphics_text(x + 24, y + 112, "MANIFEST");
    graphics_text(x + 156, y + 112, "plan");
    graphics_text(x + 24, y + 140, "SIGNATURE");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "CAPABILITY");
    graphics_text(x + 156, y + 168, "required");
    graphics_text(x + 24, y + 196, "SOURCE");
    graphics_text(x + 156, y + 196, "local");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_installinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);
    graphics_text(x + 24, y + 20, "INSTALL INFO");
    graphics_text(x + 24, y + 56, "INSTALLER");
    graphics_text(x + 156, y + 56, "plan");
    graphics_text(x + 24, y + 84, "VERIFY");
    graphics_text(x + 156, y + 84, "plan");
    graphics_text(x + 24, y + 112, "COPY");
    graphics_text(x + 156, y + 112, "plan");
    graphics_text(x + 24, y + 140, "REG APP");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "PERMISSION");
    graphics_text(x + 156, y + 168, "ask-user");
    graphics_text(x + 24, y + 196, "ROLLBACK");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_lifecycleinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);
    graphics_text(x + 24, y + 20, "LIFECYCLE INFO");
    graphics_text(x + 24, y + 56, "INSTALL");
    graphics_text(x + 156, y + 56, "plan");
    graphics_text(x + 24, y + 84, "START");
    graphics_text(x + 156, y + 84, "plan");
    graphics_text(x + 24, y + 112, "PAUSE");
    graphics_text(x + 156, y + 112, "plan");
    graphics_text(x + 24, y + 140, "STOP");
    graphics_text(x + 156, y + 140, "plan");
    graphics_text(x + 24, y + 168, "UPDATE");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "REMOVE");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_launchinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);
    graphics_text(x + 24, y + 20, "LAUNCH INFO");
    graphics_text(x + 24, y + 56, "LAUNCHER");
    graphics_text(x + 156, y + 56, "plan");
    graphics_text(x + 24, y + 84, "ENTRY");
    graphics_text(x + 156, y + 84, "plan");
    graphics_text(x + 24, y + 112, "ELF LOADER");
    graphics_text(x + 156, y + 112, "plan");
    graphics_text(x + 24, y + 140, "SANDBOX");
    graphics_text(x + 156, y + 140, "cap");
    graphics_text(x + 24, y + 168, "DECISION");
    graphics_text(x + 156, y + 168, "ask-user");
    graphics_text(x + 24, y + 196, "SESSION");
    graphics_text(x + 156, y + 196, "local");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "PARTIAL");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_overviewinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);
    graphics_text(x + 24, y + 20, "SYSTEM OVERVIEW");
    graphics_text(x + 24, y + 56, "BOOT");
    graphics_text(x + 156, y + 56, "ready");
    graphics_text(x + 24, y + 84, "GSHELL");
    graphics_text(x + 156, y + 84, "ready");
    graphics_text(x + 24, y + 112, "CMD BRIDGE");
    graphics_text(x + 156, y + 112, "active");
    graphics_text(x + 24, y + 140, "USER CONTROL");
    graphics_text(x + 156, y + 140, "meta");
    graphics_text(x + 24, y + 168, "APP RUNTIME");
    graphics_text(x + 156, y + 168, "plan");
    graphics_text(x + 24, y + 196, "NETWORK");
    graphics_text(x + 156, y + 196, "plan");
    graphics_text(x + 24, y + 224, "0.7X");
    graphics_text(x + 156, y + 224, "closeout");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_milestoneinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);
    graphics_text(x + 24, y + 20, "MILESTONE INFO");
    graphics_text(x + 24, y + 56, "0.5X");
    graphics_text(x + 156, y + 56, "gshell ui");
    graphics_text(x + 24, y + 84, "0.6X");
    graphics_text(x + 156, y + 84, "cmd bridge");
    graphics_text(x + 24, y + 112, "0.7X");
    graphics_text(x + 156, y + 112, "control map");
    graphics_text(x + 24, y + 140, "0.8X");
    graphics_text(x + 156, y + 140, "real policy");
    graphics_text(x + 24, y + 168, "0.9X");
    graphics_text(x + 156, y + 168, "loader/fs");
    graphics_text(x + 24, y + 196, "1.0");
    graphics_text(x + 156, y + 196, "os proto");
    graphics_text(x + 24, y + 224, "BRIDGE");
    graphics_text(x + 156, y + 224, "READY");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_roadmapinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);
    graphics_text(x + 24, y + 20, "ROADMAP INFO");
    graphics_text(x + 24, y + 56, "NEXT STAGE");
    graphics_text(x + 156, y + 56, "0.8 control");
    graphics_text(x + 24, y + 84, "REAL POLICY");
    graphics_text(x + 156, y + 84, "next");
    graphics_text(x + 24, y + 112, "REAL SANDBOX");
    graphics_text(x + 156, y + 112, "next");
    graphics_text(x + 24, y + 140, "APP LOAD");
    graphics_text(x + 156, y + 140, "later");
    graphics_text(x + 24, y + 168, "FS");
    graphics_text(x + 156, y + 168, "later");
    graphics_text(x + 24, y + 196, "NETWORK");
    graphics_text(x + 156, y + 196, "later");
    graphics_text(x + 24, y + 224, "DIRECTION");
    graphics_text(x + 156, y + 224, "owned");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "READY");
}

static void gshell_draw_command_view_finalinfo(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);
    graphics_text(x + 24, y + 20, "0.7X FINAL INFO");
    graphics_text(x + 24, y + 56, "GSHELL");
    graphics_text(x + 156, y + 56, "console");
    graphics_text(x + 24, y + 84, "COMMANDS");
    graphics_text(x + 156, y + 84, "expanded");
    graphics_text(x + 24, y + 112, "KERNEL INFO");
    graphics_text(x + 156, y + 112, "bridged");
    graphics_text(x + 24, y + 140, "USER CONTROL");
    graphics_text(x + 156, y + 140, "mapped");
    graphics_text(x + 24, y + 168, "SYSTEM MAP");
    graphics_text(x + 156, y + 168, "ready");
    graphics_text(x + 24, y + 196, "NEXT");
    graphics_text(x + 156, y + 196, "0.8 control");
    graphics_text(x + 24, y + 224, "MOTTO");
    graphics_text(x + 156, y + 224, "my rules");
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "0.7x done");
}

static void gshell_draw_command_view_controlstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "REAL USER CONTROL");
    graphics_text(x + 24, y + 56, "CONTROL");
    graphics_text(x + 156, y + 56, security_user_control_enabled() ? "enabled" : "disabled");
    graphics_text(x + 24, y + 84, "MODE");
    graphics_text(x + 156, y + 84, security_policy_mode());
    graphics_text(x + 24, y + 112, "NETWORK");
    graphics_text(x + 156, y + 112, security_network_policy());
    graphics_text(x + 24, y + 140, "EXT MODULE");
    graphics_text(x + 156, y + 140, security_external_module_policy());
    gshell_draw_value_uint(x + 24, y + 168, "DECISIONS", security_policy_decision_count());
    gshell_draw_value_uint(x + 24, y + 196, "ALLOWED", security_policy_allowed_count());
    gshell_draw_value_uint(x + 24, y + 224, "BLOCKED", security_policy_blocked_count());
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "POLICY ACTIVE");
}

static void gshell_draw_command_view_capstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "CAPABILITY POLICY");
    graphics_text(x + 24, y + 56, "NETWORK");
    graphics_text(x + 156, y + 56, security_network_policy());
    graphics_text(x + 24, y + 84, "FILE");
    graphics_text(x + 156, y + 84, security_file_policy());
    graphics_text(x + 24, y + 112, "AI");
    graphics_text(x + 156, y + 112, security_ai_policy());
    graphics_text(x + 24, y + 140, "GUI");
    graphics_text(x + 156, y + 140, "allowed");
    gshell_draw_value_uint(x + 24, y + 168, "DECISIONS", security_policy_decision_count());
    gshell_draw_value_uint(x + 24, y + 196, "ALLOWED", security_policy_allowed_count());
    gshell_draw_value_uint(x + 24, y + 224, "BLOCKED", security_policy_blocked_count());
    graphics_text(x + 24, y + 252, "GATE");
    graphics_text(x + 156, y + 252, "capability_is_ready");
}

static void gshell_draw_command_view_intentgate(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "INTENT POLICY GATE");
    graphics_text(x + 24, y + 56, "VIDEO INT");
    graphics_text(x + 156, y + 56, security_video_intent_policy());
    graphics_text(x + 24, y + 84, "AI INT");
    graphics_text(x + 156, y + 84, security_ai_intent_policy());
    graphics_text(x + 24, y + 112, "NET INT");
    graphics_text(x + 156, y + 112, security_network_intent_policy());
    graphics_text(x + 24, y + 140, "RUN GATE");
    graphics_text(x + 156, y + 140, "security");
    graphics_text(x + 24, y + 168, "SWITCH GATE");
    graphics_text(x + 156, y + 168, "security");
    gshell_draw_value_uint(x + 24, y + 196, "DECISIONS", security_policy_decision_count());
    gshell_draw_value_uint(x + 24, y + 224, "BLOCKED", security_policy_blocked_count());
    graphics_text(x + 24, y + 252, "HOOK");
    graphics_text(x + 156, y + 252, "intent_run");
}

static void gshell_draw_command_view_modulegate(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "MODULE POLICY GATE");
    graphics_text(x + 24, y + 56, "EXT MODULE");
    graphics_text(x + 156, y + 56, security_external_module_policy());
    graphics_text(x + 24, y + 84, "GUI MOD");
    graphics_text(x + 156, y + 84, security_policy_external_modules_blocked() ? "blocked" : "allowed");
    graphics_text(x + 24, y + 112, "NET MOD");
    graphics_text(x + 156, y + 112, security_policy_external_modules_blocked() ? "blocked" : "allowed");
    graphics_text(x + 24, y + 140, "AI MOD");
    graphics_text(x + 156, y + 140, security_policy_external_modules_blocked() ? "blocked" : "allowed");
    graphics_text(x + 24, y + 168, "CORE MOD");
    graphics_text(x + 156, y + 168, "protected");
    gshell_draw_value_uint(x + 24, y + 196, "DECISIONS", security_policy_decision_count());
    gshell_draw_value_uint(x + 24, y + 224, "BLOCKED", security_policy_blocked_count());
    graphics_text(x + 24, y + 252, "HOOK");
    graphics_text(x + 156, y + 252, "module_load");
}

static void gshell_draw_command_view_sandboxstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "SANDBOX PROFILE");
    graphics_text(x + 24, y + 56, "STATE");
    graphics_text(x + 156, y + 56, security_sandbox_state());
    graphics_text(x + 24, y + 84, "PROFILE");
    graphics_text(x + 156, y + 84, security_sandbox_profile());
    graphics_text(x + 24, y + 112, "NETWORK");
    graphics_text(x + 156, y + 112, security_network_policy());
    graphics_text(x + 24, y + 140, "FILE");
    graphics_text(x + 156, y + 140, security_file_policy());
    graphics_text(x + 24, y + 168, "AI");
    graphics_text(x + 156, y + 168, security_ai_policy());
    graphics_text(x + 24, y + 196, "EXT MOD");
    graphics_text(x + 156, y + 196, security_external_module_policy());
    gshell_draw_value_uint(x + 24, y + 224, "SWITCHES", security_sandbox_switch_count());
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "PROFILE ACTIVE");
}

static void gshell_draw_command_view_decisionstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "POLICY DECISION LOG");
    graphics_text(x + 24, y + 56, "HEALTH");
    graphics_text(x + 156, y + 56, security_decision_health());
    graphics_text(x + 24, y + 84, "SUMMARY");
    graphics_text(x + 156, y + 84, security_decision_summary());
    gshell_draw_value_uint(x + 24, y + 112, "DECISIONS", security_policy_decision_count());
    gshell_draw_value_uint(x + 24, y + 140, "ALLOWED", security_policy_allowed_count());
    gshell_draw_value_uint(x + 24, y + 168, "BLOCKED", security_policy_blocked_count());
    gshell_draw_value_uint(x + 24, y + 196, "AUDIT LOGS", security_audit_log_count());
    graphics_text(x + 24, y + 224, "LAST");
    graphics_text(x + 156, y + 224, security_audit_last_event());
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "TRACE READY");
}

static void gshell_draw_command_view_rulestatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "USER RULE TABLE");
    graphics_text(x + 24, y + 56, "DEFAULT");
    graphics_text(x + 156, y + 56, security_rule_default_policy());
    gshell_draw_value_uint(x + 24, y + 84, "RULE CHECKS", security_rule_check_count());
    graphics_text(x + 24, y + 112, "LAST RULE");
    graphics_text(x + 156, y + 112, security_rule_last_target());
    graphics_text(x + 24, y + 140, "LAST RESULT");
    graphics_text(x + 156, y + 140, security_rule_last_result());
    gshell_draw_value_uint(x + 24, y + 168, "DECISIONS", security_policy_decision_count());
    gshell_draw_value_uint(x + 24, y + 196, "ALLOWED", security_policy_allowed_count());
    gshell_draw_value_uint(x + 24, y + 224, "BLOCKED", security_policy_blocked_count());
    graphics_text(x + 24, y + 252, "RESULT");
    graphics_text(x + 156, y + 252, "RULE TABLE READY");
}

static void gshell_draw_command_view_controlpanel(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "USER CONTROL PANEL");
    graphics_text(x + 24, y + 52, "MODE");
    graphics_text(x + 156, y + 52, security_policy_mode());
    graphics_text(x + 24, y + 76, "SANDBOX");
    graphics_text(x + 156, y + 76, security_sandbox_profile());
    graphics_text(x + 24, y + 100, "RULE");
    graphics_text(x + 156, y + 100, security_rule_default_policy());
    graphics_text(x + 24, y + 124, "NETWORK");
    graphics_text(x + 156, y + 124, security_network_policy());
    graphics_text(x + 24, y + 148, "FILE");
    graphics_text(x + 156, y + 148, security_file_policy());
    graphics_text(x + 24, y + 172, "AI");
    graphics_text(x + 156, y + 172, security_ai_policy());
    graphics_text(x + 24, y + 196, "EXT MOD");
    graphics_text(x + 156, y + 196, security_external_module_policy());
    gshell_draw_value_uint(x + 24, y + 220, "DECISIONS", security_policy_decision_count());
    gshell_draw_value_uint(x + 24, y + 244, "BLOCKED", security_policy_blocked_count());
    graphics_text(x + 24, y + 268, "HEALTH");
    graphics_text(x + 156, y + 268, security_decision_health());
}

static void gshell_draw_command_view_controlfinal(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "0.8X CONTROL CLOSEOUT");
    graphics_text(x + 24, y + 52, "CONTROL");
    graphics_text(x + 156, y + 52, security_user_control_enabled() ? "enabled" : "disabled");
    graphics_text(x + 24, y + 76, "MODE");
    graphics_text(x + 156, y + 76, security_policy_mode());
    graphics_text(x + 24, y + 100, "SANDBOX");
    graphics_text(x + 156, y + 100, security_sandbox_profile());
    graphics_text(x + 24, y + 124, "RULE");
    graphics_text(x + 156, y + 124, security_rule_default_policy());
    graphics_text(x + 24, y + 148, "NETWORK");
    graphics_text(x + 156, y + 148, security_network_policy());
    graphics_text(x + 24, y + 172, "FILE");
    graphics_text(x + 156, y + 172, security_file_policy());
    graphics_text(x + 24, y + 196, "AI");
    graphics_text(x + 156, y + 196, security_ai_policy());
    graphics_text(x + 24, y + 220, "EXT MOD");
    graphics_text(x + 156, y + 220, security_external_module_policy());
    graphics_text(x + 24, y + 244, "DECISION");
    graphics_text(x + 156, y + 244, security_decision_summary());
    graphics_text(x + 24, y + 268, "NEXT");
    graphics_text(x + 156, y + 268, "0.9 runtime");
}

static void gshell_draw_command_view_clear(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x00004477);
    graphics_rect(x, y + h - 4, w, 4, 0x00004477);
    graphics_rect(x, y, 4, h, 0x00004477);
    graphics_rect(x + w - 4, y, 4, h, 0x00004477);

    graphics_text(x + 24, y + 20, "CLEAR VIEW");
    graphics_text(x + 24, y + 72, "SCREEN CLEAN");
    graphics_text(x + 24, y + 112, "READY FOR NEXT COMMAND");
}

static void gshell_draw_command_view_error(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x00FFAA00);
    graphics_rect(x, y + h - 4, w, 4, 0x00FFAA00);
    graphics_rect(x, y, 4, h, 0x00FFAA00);
    graphics_rect(x + w - 4, y, 4, h, 0x00FFAA00);

    graphics_text(x + 24, y + 20, "ERROR VIEW");
    graphics_text(x + 24, y + 72, "UNKNOWN COMMAND");
    graphics_text(x + 24, y + 112, "TYPE HELP");
}

static void gshell_draw_command_view_history(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "HISTORY VIEW");

    if (gshell_history_count == 0) {
        graphics_text(x + 24, y + 72, "NO HISTORY");
        return;
    }

    if (gshell_history_count > 0) {
        graphics_text(x + 24, y + 64, gshell_history_commands[0]);
        graphics_text(x + 132, y + 64, gshell_history_views[0]);
    }

    if (gshell_history_count > 1) {
        graphics_text(x + 24, y + 96, gshell_history_commands[1]);
        graphics_text(x + 132, y + 96, gshell_history_views[1]);
    }

    if (gshell_history_count > 2) {
        graphics_text(x + 24, y + 128, gshell_history_commands[2]);
        graphics_text(x + 132, y + 128, gshell_history_views[2]);
    }

    if (gshell_history_count > 3) {
        graphics_text(x + 24, y + 160, gshell_history_commands[3]);
        graphics_text(x + 132, y + 160, gshell_history_views[3]);
    }

    gshell_draw_value_uint(x + 24, y + 220, "COUNT", gshell_history_count);
    gshell_draw_value_uint(x + 24, y + 248, "TOTAL", gshell_history_total);
}

static void gshell_draw_command_view_histclear(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x00004477);
    graphics_rect(x, y + h - 4, w, 4, 0x00004477);
    graphics_rect(x, y, 4, h, 0x00004477);
    graphics_rect(x + w - 4, y, 4, h, 0x00004477);

    graphics_text(x + 24, y + 20, "HISTORY CLEARED");
    graphics_text(x + 24, y + 72, "COUNT 0");
    graphics_text(x + 24, y + 112, "READY FOR NEW COMMANDS");
}

static void gshell_draw_result_log_line(unsigned int x, unsigned int y, unsigned int index) {
    if (index >= gshell_result_log_count) {
        return;
    }

    graphics_text(x, y, gshell_result_log_commands[index]);
    graphics_text(x + 108, y, gshell_result_log_results[index]);
}

static void gshell_draw_command_view_log(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "RESULT LOG");

    if (gshell_result_log_count == 0) {
        graphics_text(x + 24, y + 72, "NO LOG");
        return;
    }

    if (gshell_result_log_count > 0) {
        gshell_draw_result_log_line(x + 24, y + 64, 0);
    }

    if (gshell_result_log_count > 1) {
        gshell_draw_result_log_line(x + 24, y + 96, 1);
    }

    if (gshell_result_log_count > 2) {
        gshell_draw_result_log_line(x + 24, y + 128, 2);
    }

    if (gshell_result_log_count > 3) {
        gshell_draw_result_log_line(x + 24, y + 160, 3);
    }

    gshell_draw_value_uint(x + 24, y + 220, "COUNT", gshell_result_log_count);
    gshell_draw_value_uint(x + 24, y + 248, "TOTAL", gshell_result_log_total);
}

static void gshell_draw_command_view_logclear(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x00004477);
    graphics_rect(x, y + h - 4, w, 4, 0x00004477);
    graphics_rect(x, y, 4, h, 0x00004477);
    graphics_rect(x + w - 4, y, 4, h, 0x00004477);

    graphics_text(x + 24, y + 20, "LOG CLEARED");
    graphics_text(x + 24, y + 72, "COUNT 1");
    graphics_text(x + 24, y + 112, "LOGCLEAR RECORDED");
}

static void gshell_draw_command_view_idle(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x00004477);
    graphics_rect(x, y + h - 4, w, 4, 0x00004477);
    graphics_rect(x, y, 4, h, 0x00004477);
    graphics_rect(x + w - 4, y, 4, h, 0x00004477);

    graphics_text(x + 24, y + 20, "COMMAND VIEW");
    graphics_text(x + 24, y + 72, "TYPE HELP");
    graphics_text(x + 24, y + 112, "LJ SHELL READY");
}

static void gshell_draw_history_panel(unsigned int x, unsigned int y) {
    graphics_text(x, y, "RECENT");

    if (gshell_history_count == 0) {
        graphics_text(x, y + 24, "NONE");
        return;
    }

    graphics_text(x, y + 24, "HAS HISTORY");
}

static void gshell_draw_command_view_help(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "HELP VIEW");
    graphics_text(x + 24, y + 52, "dash");
    graphics_text(x + 132, y + 52, "health");
    graphics_text(x + 24, y + 76, "doctor");
    graphics_text(x + 132, y + 76, "status");
    graphics_text(x + 24, y + 100, "version");
    graphics_text(x + 132, y + 100, "sysinfo");
    graphics_text(x + 24, y + 124, "meminfo");
    graphics_text(x + 132, y + 124, "pageinfo");
    graphics_text(x + 24, y + 148, "syscalls");
    graphics_text(x + 132, y + 148, "ring3info");
    graphics_text(x + 24, y + 172, "moduleinfo");
    graphics_text(x + 132, y + 172, "modulecheck");
    graphics_text(x + 24, y + 196, "capinfo");
    graphics_text(x + 132, y + 196, "intentinfo");
    graphics_text(x + 24, y + 224, "serviceinfo");
    graphics_text(x + 132, y + 224, "packageinfo");
    graphics_text(x + 24, y + 252, "overviewinfo");
    graphics_text(x + 132, y + 252, "finalinfo");
}

static void gshell_draw_command_view(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    if (gshell_text_equal(gshell_command_view, "DASH")) {
        gshell_draw_command_view_dash(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "HEALTH")) {
        gshell_draw_command_view_health(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "DOCTOR")) {
        gshell_draw_command_view_doctor(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "HELP")) {
        gshell_draw_command_view_help(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "MILESTONE")) {
        gshell_draw_command_view_milestone(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "STATUS")) {
        gshell_draw_command_view_status(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "VERSION")) {
        gshell_draw_command_view_version(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "ABOUT")) {
        gshell_draw_command_view_about(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "BRIDGE")) {
        gshell_draw_command_view_bridge(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "REGISTRY")) {
        gshell_draw_command_view_registry(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "TEXTCMDS")) {
        gshell_draw_command_view_textcmds(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "SYSINFO")) {
        gshell_draw_command_view_sysinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "MEMINFO")) {
        gshell_draw_command_view_meminfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "PAGEINFO")) {
        gshell_draw_command_view_pageinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "SYSCALLS")) {
        gshell_draw_command_view_syscalls(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "RING3INFO")) {
        gshell_draw_command_view_ring3info(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "MODULEINFO")) {
        gshell_draw_command_view_moduleinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "MODULECHECK")) {
        gshell_draw_command_view_modulecheck(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "CAPINFO")) {
        gshell_draw_command_view_capinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "INTENTINFO")) {
        gshell_draw_command_view_intentinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "TASKINFO")) {
        gshell_draw_command_view_taskinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "TASKLIST")) {
        gshell_draw_command_view_tasklist(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "SCHEDINFO")) {
        gshell_draw_command_view_schedinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "RUNQUEUE")) {
        gshell_draw_command_view_runqueue(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "SECURITYINFO")) {
        gshell_draw_command_view_securityinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "PLATFORMINFO")) {
        gshell_draw_command_view_platforminfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "IDENTITYINFO")) {
        gshell_draw_command_view_identityinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "LANGINFO")) {
        gshell_draw_command_view_langinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "BOOTINFO")) {
        gshell_draw_command_view_bootinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "FBINFO")) {
        gshell_draw_command_view_fbinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "GFXINFO")) {
        gshell_draw_command_view_gfxinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "FONTINFO")) {
        gshell_draw_command_view_fontinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "FSINFO")) {
        gshell_draw_command_view_fsinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "DEVICEINFO")) {
        gshell_draw_command_view_deviceinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "DRIVERINFO")) {
        gshell_draw_command_view_driverinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "IOINFO")) {
        gshell_draw_command_view_ioinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "PROCESSINFO")) {
        gshell_draw_command_view_processinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "PROCINFO")) {
        gshell_draw_command_view_procinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "RUNTIMEINFO")) {
        gshell_draw_command_view_runtimeinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "APPINFO")) {
        gshell_draw_command_view_appinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "ELFINFO")) {
        gshell_draw_command_view_elfinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "LOADERINFO")) {
        gshell_draw_command_view_loaderinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "BINARYINFO")) {
        gshell_draw_command_view_binaryinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "ABIINFO")) {
        gshell_draw_command_view_abiinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "SANDBOXINFO")) {
        gshell_draw_command_view_sandboxinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "POLICYINFO")) {
        gshell_draw_command_view_policyinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "PERMISSIONINFO")) {
        gshell_draw_command_view_permissioninfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "GUARDINFO")) {
        gshell_draw_command_view_guardinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "CONTROLINFO")) {
        gshell_draw_command_view_controlinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "OWNERINFO")) {
        gshell_draw_command_view_ownerinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "DECISIONINFO")) {
        gshell_draw_command_view_decisioninfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "RULEINFO")) {
        gshell_draw_command_view_ruleinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "AUDITINFO")) {
        gshell_draw_command_view_auditinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "TRACEINFO")) {
        gshell_draw_command_view_traceinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "LOGINFO")) {
        gshell_draw_command_view_loginfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "EVENTINFO")) {
        gshell_draw_command_view_eventinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "NETWORKINFO")) {
        gshell_draw_command_view_networkinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "NETINFO")) {
        gshell_draw_command_view_netinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "SOCKETINFO")) {
        gshell_draw_command_view_socketinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "PROTOCOLINFO")) {
        gshell_draw_command_view_protocolinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "SERVICEINFO")) {
        gshell_draw_command_view_serviceinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "SESSIONINFO")) {
        gshell_draw_command_view_sessioninfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "CHANNELINFO")) {
        gshell_draw_command_view_channelinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "BUSINFO")) {
        gshell_draw_command_view_businfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "PACKAGEINFO")) {
        gshell_draw_command_view_packageinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "INSTALLINFO")) {
        gshell_draw_command_view_installinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "LIFECYCLEINFO")) {
        gshell_draw_command_view_lifecycleinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "LAUNCHINFO")) {
        gshell_draw_command_view_launchinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "OVERVIEWINFO")) {
        gshell_draw_command_view_overviewinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "MILESTONEINFO")) {
        gshell_draw_command_view_milestoneinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "ROADMAPINFO")) {
        gshell_draw_command_view_roadmapinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "FINALINFO")) {
        gshell_draw_command_view_finalinfo(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "CONTROLSTATUS")) {
        gshell_draw_command_view_controlstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "CAPSTATUS")) {
        gshell_draw_command_view_capstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "INTENTGATE")) {
        gshell_draw_command_view_intentgate(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "MODULEGATE")) {
        gshell_draw_command_view_modulegate(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "SANDBOXSTATUS")) {
        gshell_draw_command_view_sandboxstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "DECISIONSTATUS")) {
        gshell_draw_command_view_decisionstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "RULESTATUS")) {
        gshell_draw_command_view_rulestatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "CONTROLPANEL")) {
        gshell_draw_command_view_controlpanel(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "CONTROLFINAL")) {
        gshell_draw_command_view_controlfinal(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "CLEAR")) {
        gshell_draw_command_view_clear(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "HISTORY")) {
        gshell_draw_command_view_history(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "HISTCLEAR")) {
        gshell_draw_command_view_histclear(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "LOG")) {
        gshell_draw_command_view_log(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "LOGCLEAR")) {
        gshell_draw_command_view_logclear(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "ERROR")) {
        gshell_draw_command_view_error(x, y, w, h);
        return;
    }

    gshell_draw_command_view_idle(x, y, w, h);
}

static void gshell_build_visible_input(void) {
    unsigned int i = 0;
    unsigned int out = 0;
    unsigned int start = 0;

    for (i = 0; i < GSHELL_INPUT_VISIBLE_SIZE; i++) {
        gshell_input_visible_text[i] = '\0';
    }

    if (gshell_input_len == 0) {
        return;
    }

    if (gshell_input_len < GSHELL_INPUT_VISIBLE_SIZE) {
        i = 0;

        while (i < gshell_input_len && i + 1 < GSHELL_INPUT_VISIBLE_SIZE) {
            gshell_input_visible_text[i] = gshell_input_buffer[i];
            i++;
        }

        gshell_input_visible_text[i] = '\0';
        return;
    }

    gshell_input_visible_text[0] = '.';
    gshell_input_visible_text[1] = '.';
    gshell_input_visible_text[2] = '.';

    out = 3;
    start = gshell_input_len - (GSHELL_INPUT_VISIBLE_SIZE - 4);

    while (start < gshell_input_len && out + 1 < GSHELL_INPUT_VISIBLE_SIZE) {
        gshell_input_visible_text[out] = gshell_input_buffer[start];
        out++;
        start++;
    }

    gshell_input_visible_text[out] = '\0';
}

static unsigned int gshell_visible_input_len(void) {
    unsigned int i = 0;

    while (gshell_input_visible_text[i] != '\0') {
        i++;
    }

    return i;
}

static void gshell_draw_input_cursor(unsigned int x, unsigned int y) {
    unsigned int tick = timer_get_ticks();
    unsigned int blink = (tick / 18U) % 2U;

    if (blink == 0 && gshell_input_len == 0) {
        graphics_rect(x, y + 14, 12, 2, 0x00004477);
        return;
    }

    graphics_rect(x, y - 2, 3, 18, 0x00FFFFFF);
    graphics_rect(x + 5, y + 14, 10, 2, 0x0000AAFF);
}

static void gshell_draw_input_zone(unsigned int width, unsigned int height) {
    unsigned int input_x = 106;
    unsigned int input_y = height - 92;
    unsigned int status_y = height - 68;
    unsigned int cursor_x = 0;
    unsigned int visible_len = 0;

    gshell_build_visible_input();
    visible_len = gshell_visible_input_len();

    graphics_rect(36, height - 170, width - 72, 122, 0x00000000);
    graphics_rect(36, height - 170, width - 72, 4, 0x00004477);
    graphics_rect(36, height - 52, width - 72, 4, 0x00004477);
    graphics_rect(36, height - 170, 4, 122, 0x00004477);
    graphics_rect(width - 40, height - 170, 4, 122, 0x00004477);

    graphics_text(58, height - 156, "GRAPHICAL TERMINAL");

    if (gshell_terminal_count == 0) {
        graphics_text(58, height - 134, "READY");
    }

    if (gshell_terminal_count > 0) {
        graphics_text(58, height - 134, gshell_terminal_lines[0]);
    }

    if (gshell_terminal_count > 1) {
        graphics_text(58, height - 116, gshell_terminal_lines[1]);
    }

    graphics_text(58, input_y, "LJ>");

    if (gshell_input_len == 0) {
        graphics_text(input_x, input_y, "TYPE HELP");
        cursor_x = input_x;
    } else {
        graphics_text(input_x, input_y, gshell_input_visible_text);
        cursor_x = input_x + (visible_len * 12);
    }

    if (cursor_x > width - 64) {
        cursor_x = width - 64;
    }

    gshell_draw_input_cursor(cursor_x, input_y);

    graphics_text(58, status_y, "STATUS");

    if (gshell_text_equal(gshell_input_status_text, "UNKNOWN COMMAND")) {
        graphics_text(154, status_y, "UNKNOWN");
    } else if (gshell_text_equal(gshell_input_status_text, "ENTER ACCEPTED")) {
        graphics_text(154, status_y, "ENTER");
    } else {
        graphics_text(154, status_y, gshell_input_status_text);
    }

    graphics_text(286, status_y, "PARSER");
    graphics_text(382, status_y, gshell_parser_status_text);

    graphics_text(width - 210, status_y, "VIEW");
    graphics_text(width - 150, status_y, gshell_command_view);
}

void gshell_graphics_dashboard(void) {
    unsigned int width = framebuffer_get_width();
    unsigned int height = framebuffer_get_height();
    unsigned int center_x = 0;
    unsigned int center_y = 0;
    unsigned int command_x = 318;
    unsigned int command_y = 116;
    unsigned int command_w = 250;
    unsigned int command_h = 300;

    gshell_render_begin("graphics-dashboard");

    if (!gshell_render_guard()) {
        return;
    }

    if (!framebuffer_is_ready()) {
        gshell_blocked_count++;
        platform_print("  result: blocked\n");
        platform_print("  reason: framebuffer not ready\n");
        return;
    }

    if (framebuffer_get_address() == 0) {
        gshell_blocked_count++;
        platform_print("  result: blocked\n");
        platform_print("  reason: framebuffer address missing\n");
        return;
    }

    if (framebuffer_get_bpp() != 32) {
        gshell_blocked_count++;
        platform_print("  result: blocked\n");
        platform_print("  reason: only 32bpp graphics framebuffer is enabled now\n");
        return;
    }

    if (width < 640 || height < 480) {
        gshell_blocked_count++;
        platform_print("  result: blocked\n");
        platform_print("  reason: framebuffer too small\n");
        return;
    }

    center_x = width / 2;
    center_y = height / 2;

    gshell_mode = "real-framebuffer-dashboard";
    gshell_output_mode = "graphics-self";
    gshell_output_plan = "graphics-self";
    gshell_output_reason = "graphical shell rendered into framebuffer";
    gshell_output_changes++;

    graphics_real_arm();
    graphics_text_arm();

    graphics_clear();

    graphics_rect(18, 18, width - 36, 4, 0x0000AAFF);
    graphics_rect(18, height - 22, width - 36, 4, 0x0000AAFF);
    graphics_rect(18, 18, 4, height - 36, 0x0000AAFF);
    graphics_rect(width - 22, 18, 4, height - 36, 0x0000AAFF);

    graphics_rect(36, 36, width - 72, 54, 0x00112233);
    graphics_rect(36, 36, width - 72, 4, 0x0000AAFF);
    graphics_rect(36, 86, width - 72, 4, 0x00004477);

    graphics_text(54, 54, "LINGJING OS");
    graphics_text(250, 54, LINGJING_VERSION);
    graphics_text(390, 54, "0.8X CLOSEOUT");

    graphics_rect(36, 116, 254, 300, 0x00112233);
    graphics_rect(36, 116, 254, 4, 0x0000AAFF);
    graphics_rect(36, 412, 254, 4, 0x0000AAFF);
    graphics_rect(36, 116, 4, 300, 0x0000AAFF);
    graphics_rect(286, 116, 4, 300, 0x0000AAFF);

    graphics_text(58, 136, "SYSTEM");
    graphics_rect(58, 166, 190, 10, 0x0000FF66);
    graphics_text(58, 188, "PLATFORM BAREMETAL");
    graphics_text(58, 216, "BOOT MB2 GFX");
    graphics_text(58, 244, "FB 800X600X32");
    graphics_text(58, 272, "GRAPHICS READY");
    graphics_text(58, 300, "TEXT READY");
    graphics_text(58, 328, "MCI READY");
    graphics_text(58, 356, "GSHELL ONLINE");

    gshell_draw_command_view(command_x, command_y, command_w, command_h);

    graphics_rect(width - 230, 116, 194, 300, 0x00112233);
    graphics_rect(width - 230, 116, 194, 4, 0x00FFAA00);
    graphics_rect(width - 230, 412, 194, 4, 0x00FFAA00);
    graphics_rect(width - 230, 116, 4, 300, 0x00FFAA00);
    graphics_rect(width - 40, 116, 4, 300, 0x00FFAA00);

    graphics_text(width - 208, 136, "COMMANDS");
    graphics_text(width - 208, 164, "CONTROLFINAL");
    graphics_text(width - 208, 188, "CONTROLHEALTH");
    graphics_text(width - 208, 212, "POLICYHEALTH");
    graphics_text(width - 208, 236, "GATESUMMARY");
    graphics_text(width - 208, 260, "SANDBOXSUMMARY");
    graphics_text(width - 208, 284, "RULESUMMARY");
    graphics_text(width - 208, 308, "DECISIONSUMMARY");
    graphics_text(width - 208, 332, "NEXTSTAGE");

    gshell_draw_history_panel(width - 208, 368);

    gshell_draw_input_zone(width, height);

    graphics_pixel(center_x, center_y, 0x00FFFFFF);
    graphics_pixel(center_x + 1, center_y, 0x00FFFFFF);
    graphics_pixel(center_x - 1, center_y, 0x00FFFFFF);
    graphics_pixel(center_x, center_y + 1, 0x00FFFFFF);
    graphics_pixel(center_x, center_y - 1, 0x00FFFFFF);

    platform_print("  output: graphics-self\n");
    platform_print("  command zone: 0.8x-control-closeout\n");
    platform_print("  result: real-written\n");
}

void gshell_break(void) {
    gshell_broken = 1;

    platform_print("Graphical shell break:\n");
    platform_print("  result: broken\n");
}

void gshell_fix(void) {
    gshell_broken = 0;

    if (!gshell_initialized) {
        gshell_init();
    }

    platform_print("Graphical shell fix:\n");
    platform_print("  result: fixed\n");
}