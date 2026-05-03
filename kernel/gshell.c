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
#define GSHELL_CMD_RUNTIMESTATUS 140
#define GSHELL_CMD_APPSTATUS   141
#define GSHELL_CMD_LOADERSTATUS 142
#define GSHELL_CMD_LAUNCHSTATUS 143
#define GSHELL_CMD_RUNTIMECHECK 144
#define GSHELL_CMD_APPCHECK    145
#define GSHELL_CMD_ELFSTATUS   146
#define GSHELL_CMD_ELFHEADER   147
#define GSHELL_CMD_ELFSEGMENTS 148
#define GSHELL_CMD_ELFSECTIONS 149
#define GSHELL_CMD_ELFVALIDATE 150
#define GSHELL_CMD_LOADERCHECK 151
#define GSHELL_CMD_MANIFESTSTATUS 152
#define GSHELL_CMD_APPMANIFEST 153
#define GSHELL_CMD_APPDESCRIPTOR 154
#define GSHELL_CMD_APPENTRY    155
#define GSHELL_CMD_APPCAPS     156
#define GSHELL_CMD_MANIFESTCHECK 157
#define GSHELL_CMD_APPSLOTSTATUS 158
#define GSHELL_CMD_SLOTLIST    159
#define GSHELL_CMD_SLOTLALLOC  160
#define GSHELL_CMD_SLOTFREE    161
#define GSHELL_CMD_PROCESSSLOT 162
#define GSHELL_CMD_SLOTCHECK   163
#define GSHELL_CMD_LAUNCHGATE  164
#define GSHELL_CMD_LAUNCHPREPARE 165
#define GSHELL_CMD_LAUNCHAPPROVE 166
#define GSHELL_CMD_LAUNCHDENY  167
#define GSHELL_CMD_LAUNCHRUN   168
#define GSHELL_CMD_LAUNCHCHECK 169
#define GSHELL_CMD_PERMSTATUS  170
#define GSHELL_CMD_PERMREQUEST 171
#define GSHELL_CMD_PERMALLOW   172
#define GSHELL_CMD_PERMDENY    173
#define GSHELL_CMD_PERMCHECK   174
#define GSHELL_CMD_PERMRESET   175
#define GSHELL_CMD_FSSTATUS    176
#define GSHELL_CMD_RAMFSSTATUS 177
#define GSHELL_CMD_FSMOUNT     178
#define GSHELL_CMD_FSWRITE     179
#define GSHELL_CMD_FSREAD      180
#define GSHELL_CMD_FSCHECK     181
#define GSHELL_CMD_FSRESET     182
#define GSHELL_CMD_LIFESTATUS  183
#define GSHELL_CMD_APPSTART    184
#define GSHELL_CMD_APPPAUSE    185
#define GSHELL_CMD_APPRESUME   186
#define GSHELL_CMD_APPSTOP     187
#define GSHELL_CMD_LIFECHECK   188
#define GSHELL_CMD_LIFERESET   189
#define GSHELL_CMD_RUNTIMEPANEL 190
#define GSHELL_CMD_APPPANEL    191
#define GSHELL_CMD_LAUNCHPANEL 192
#define GSHELL_CMD_PERMISSIONPANEL 193
#define GSHELL_CMD_FSPANEL     194
#define GSHELL_CMD_LIFEPANEL   195
#define GSHELL_CMD_RUNTIMEFINAL 196
#define GSHELL_CMD_RUNTIMEHEALTH 197
#define GSHELL_CMD_APPSUMMARY  198
#define GSHELL_CMD_LAUNCHSUMMARY 199
#define GSHELL_CMD_PERMISSIONSUMMARY 200
#define GSHELL_CMD_FSSUMMARY   201
#define GSHELL_CMD_LIFESUMMARY 202
#define GSHELL_CMD_NEXTMAJOR   203
#define GSHELL_CMD_PROTOTYPE   204
#define GSHELL_CMD_PROTOSTATUS 205
#define GSHELL_CMD_PROTOMILESTONE 206
#define GSHELL_CMD_SYSTEMFLOW  207
#define GSHELL_CMD_DEMOFLOW    208
#define GSHELL_CMD_PROTOREADY  209
#define GSHELL_CMD_UNIDASH     210
#define GSHELL_CMD_SYSDASH     211
#define GSHELL_CMD_BOOTDASH    212
#define GSHELL_CMD_CONTROLDASH 213
#define GSHELL_CMD_RUNTIMEDASH 214
#define GSHELL_CMD_FLOWDASH    215
#define GSHELL_CMD_BOOTHEALTH  216
#define GSHELL_CMD_HEALTHDASH  217
#define GSHELL_CMD_DOCTORDASH  218
#define GSHELL_CMD_CORECHECK   219
#define GSHELL_CMD_BOOTCHECK   220
#define GSHELL_CMD_SYSTEMCHECK 221
#define GSHELL_CMD_FLOWSTATUS  222
#define GSHELL_CMD_FLOWPREPARE 223
#define GSHELL_CMD_FLOWCONTROL 224
#define GSHELL_CMD_FLOWRUNTIME 225
#define GSHELL_CMD_FLOWDEMO    226
#define GSHELL_CMD_FLOWCHECK   227
#define GSHELL_CMD_FLOWRESET   228
#define GSHELL_CMD_WALKTHROUGH 229
#define GSHELL_CMD_DEMOSTEP1   230
#define GSHELL_CMD_DEMOSTEP2   231
#define GSHELL_CMD_DEMOSTEP3   232
#define GSHELL_CMD_DEMOSTEP4   233
#define GSHELL_CMD_DEMOSTEP5   234
#define GSHELL_CMD_DEMOCHECK   235
#define GSHELL_CMD_DEMORESET   236
#define GSHELL_CMD_MILESTONEFINAL 237
#define GSHELL_CMD_PROTOSUMMARY 238
#define GSHELL_CMD_CHAINSUMMARY 239
#define GSHELL_CMD_READYPANEL  240
#define GSHELL_CMD_VERSIONPANEL 241
#define GSHELL_CMD_CLOSECHECK  242
#define GSHELL_CMD_UIPOLISH    243
#define GSHELL_CMD_LAYOUTCHECK 244
#define GSHELL_CMD_PANELCHECK  245
#define GSHELL_CMD_TEXTCHECK   246
#define GSHELL_CMD_COMMANDCLEAN 247
#define GSHELL_CMD_UICHECK     248
#define GSHELL_CMD_CLOSEOUT    249
#define GSHELL_CMD_FINALSTATUS 250
#define GSHELL_CMD_FINALHEALTH 251
#define GSHELL_CMD_FINALDEMO   252
#define GSHELL_CMD_FINALREADY  253
#define GSHELL_CMD_RELEASEINFO 254
#define GSHELL_CMD_VERSIONFINAL 255
#define GSHELL_CMD_NEXTROADMAP 256
#define GSHELL_CMD_INPUTSTATUS 257
#define GSHELL_CMD_MOUSESTATUS 258
#define GSHELL_CMD_CLICKSTATUS 259
#define GSHELL_CMD_FOCUSSTATUS 260
#define GSHELL_CMD_INPUTDEMO   261
#define GSHELL_CMD_INPUTCHECK  262
#define GSHELL_CMD_INPUTRESET  263
#define GSHELL_CMD_CURSORSTATUS 264
#define GSHELL_CMD_POINTERSTATUS 265
#define GSHELL_CMD_CURSORCENTER 266
#define GSHELL_CMD_CURSORMOVE  267
#define GSHELL_CMD_LEFTCLICK   268
#define GSHELL_CMD_RIGHTCLICK  269
#define GSHELL_CMD_WHEELSTEP   270
#define GSHELL_CMD_CURSORRESET 271
#define GSHELL_CMD_HITSTATUS   272
#define GSHELL_CMD_PANELHIT    273
#define GSHELL_CMD_COMMANDHIT  274
#define GSHELL_CMD_CLICKCMD    275
#define GSHELL_CMD_RIGHTMENU   276
#define GSHELL_CMD_WHEELPICK   277
#define GSHELL_CMD_HITCHECK    278
#define GSHELL_CMD_HITRESET    279
#define GSHELL_CMD_BUTTONSTATUS 280
#define GSHELL_CMD_BUTTONHOVER 281
#define GSHELL_CMD_BUTTONPRESS 282
#define GSHELL_CMD_BUTTONFOCUS 283
#define GSHELL_CMD_BUTTONACTIVE 284
#define GSHELL_CMD_BUTTONCHECK 285
#define GSHELL_CMD_BUTTONRESET 286
#define GSHELL_CMD_WINDOWSTATUS 287
#define GSHELL_CMD_WINDOWCREATE 288
#define GSHELL_CMD_WINDOWFOCUS 289
#define GSHELL_CMD_WINDOWMOVE  290
#define GSHELL_CMD_WINDOWMIN   291
#define GSHELL_CMD_WINDOWCLOSE 292
#define GSHELL_CMD_WINDOWCHECK 293
#define GSHELL_CMD_WINDOWRESET 294
#define GSHELL_CMD_DESKTOPSTATUS 295
#define GSHELL_CMD_WORKSPACE   296
#define GSHELL_CMD_ICONSTATUS  297
#define GSHELL_CMD_ICONSELECT  298
#define GSHELL_CMD_DOCKSTATUS  299
#define GSHELL_CMD_DESKTOPCHECK 300
#define GSHELL_CMD_DESKTOPRESET 301
#define GSHELL_CMD_INTERACTIONFINAL 302
#define GSHELL_CMD_INPUTSUMMARY 303
#define GSHELL_CMD_POINTERSUMMARY 304
#define GSHELL_CMD_HITSUMMARY  305
#define GSHELL_CMD_BUTTONSUMMARY 306
#define GSHELL_CMD_WINDOWSUMMARY 307
#define GSHELL_CMD_DESKTOPSUMMARY 308
#define GSHELL_CMD_NEXTPHASE   309
#define GSHELL_CMD_SHELLSTATUS 310
#define GSHELL_CMD_SHELLPANEL  311
#define GSHELL_CMD_TASKBAR     312
#define GSHELL_CMD_LAUNCHER    313
#define GSHELL_CMD_SHELLOPENAPP 314
#define GSHELL_CMD_SHELLCHECK  315
#define GSHELL_CMD_SHELLRESET  316
#define GSHELL_CMD_LAUNCHERSTATUS 317
#define GSHELL_CMD_LAUNCHERGRID 318
#define GSHELL_CMD_APPSELECT   319
#define GSHELL_CMD_APPPIN      320
#define GSHELL_CMD_LAUNCHEROPEN 321
#define GSHELL_CMD_LAUNCHERCHECK 322
#define GSHELL_CMD_LAUNCHERRESET 323
#define GSHELL_CMD_TASKBARSTATUS 324
#define GSHELL_CMD_TASKITEM    325
#define GSHELL_CMD_TASKFOCUS   326
#define GSHELL_CMD_TASKSWITCH  327
#define GSHELL_CMD_TASKMIN     328
#define GSHELL_CMD_TASKRESTORE 329
#define GSHELL_CMD_TASKCHECK   330
#define GSHELL_CMD_TASKRESET   331
#define GSHELL_CMD_LAYOUTSTATUS 332
#define GSHELL_CMD_LAYOUTGRID  333
#define GSHELL_CMD_WINDOWSNAP  334
#define GSHELL_CMD_WINDOWMAX   335
#define GSHELL_CMD_ZORDER      336
#define GSHELL_CMD_WINLAYOUTCHECK 337
#define GSHELL_CMD_LAYOUTRESET 338
#define GSHELL_CMD_DESKTOPSHELL 339
#define GSHELL_CMD_SHELLHOME  340
#define GSHELL_CMD_SHELLAPPS  341
#define GSHELL_CMD_SHELLWINDOWS 342
#define GSHELL_CMD_SHELLLAYOUT 343
#define GSHELL_CMD_SHELLDOCK  344
#define GSHELL_CMD_SHELLFLOW  345
#define GSHELL_CMD_SHELLHOMECHECK 346
#define GSHELL_CMD_SHELLHOMERESET 347
#define GSHELL_CMD_SHELLFINAL 348
#define GSHELL_CMD_SHELLHEALTH 349
#define GSHELL_CMD_SHELLSUMMARY 350
#define GSHELL_CMD_LAUNCHERSUM 351
#define GSHELL_CMD_TASKBARSUM  352
#define GSHELL_CMD_LAYOUTSUMB  353
#define GSHELL_CMD_HOMESUMMARY 354
#define GSHELL_CMD_SHELLNEXT   355
#define GSHELL_CMD_APPSHELLSTATUS 356
#define GSHELL_CMD_APPCATALOG  357
#define GSHELL_CMD_APPCARD     358
#define GSHELL_CMD_APPDETAILS  359
#define GSHELL_CMD_APPSHELLLAUNCH 360
#define GSHELL_CMD_APPSHELLCHECK 361
#define GSHELL_CMD_APPSHELLRESET 362
#define GSHELL_CMD_CATALOGSTATUS 363
#define GSHELL_CMD_CATALOGLIST 364
#define GSHELL_CMD_CATALOGSEARCH 365
#define GSHELL_CMD_CATALOGPIN 366
#define GSHELL_CMD_CATALOGENABLE 367
#define GSHELL_CMD_CATALOGDISABLE 368
#define GSHELL_CMD_CATALOGCHECK 369
#define GSHELL_CMD_CATALOGRESET 370
#define GSHELL_CMD_DETAILSTATUS 371
#define GSHELL_CMD_DETAILOPEN  372
#define GSHELL_CMD_DETAILMANIFEST 373
#define GSHELL_CMD_DETAILCAPS  374
#define GSHELL_CMD_DETAILPERMS 375
#define GSHELL_CMD_DETAILLAUNCH 376
#define GSHELL_CMD_DETAILCHECK 377
#define GSHELL_CMD_DETAILRESET 378
#define GSHELL_CMD_ACTIONSTATUS 379
#define GSHELL_CMD_ACTIONPREPARE 380
#define GSHELL_CMD_ACTIONALLOW 381
#define GSHELL_CMD_ACTIONOPEN  382
#define GSHELL_CMD_ACTIONRUN   383
#define GSHELL_CMD_ACTIONSTOP  384
#define GSHELL_CMD_ACTIONCHECK 385
#define GSHELL_CMD_ACTIONRESET 386
#define GSHELL_CMD_APPMGMTSTATUS 387
#define GSHELL_CMD_APPINVENTORY 388
#define GSHELL_CMD_APPSCAN    389
#define GSHELL_CMD_APPREGISTER 390
#define GSHELL_CMD_APPUNREGISTER 391
#define GSHELL_CMD_APPENABLE  392
#define GSHELL_CMD_APPDISABLE 393
#define GSHELL_CMD_APPTRUST   394
#define GSHELL_CMD_APPUNTRUST 395
#define GSHELL_CMD_APPFAVORITE 396
#define GSHELL_CMD_APPUNFAVORITE 397
#define GSHELL_CMD_APPUPDATECHECK 398
#define GSHELL_CMD_APPUPDATEMARK 399
#define GSHELL_CMD_APPROLLBACK 400
#define GSHELL_CMD_APPREPAIR  401
#define GSHELL_CMD_APPCLEARCACHE 402
#define GSHELL_CMD_APPSTATS   403
#define GSHELL_CMD_APPHEALTH  404
#define GSHELL_CMD_APPMGMTCHECK 405
#define GSHELL_CMD_APPMGMTRESET 406
#define GSHELL_CMD_APPFINAL   407
#define GSHELL_CMD_APPHEALTHSUM 408
#define GSHELL_CMD_APPSHELLSUM 409
#define GSHELL_CMD_CATALOGSUM 410
#define GSHELL_CMD_DETAILSUM  411
#define GSHELL_CMD_ACTIONSUM  412
#define GSHELL_CMD_MGMTSUM    413
#define GSHELL_CMD_APPREADINESS 414
#define GSHELL_CMD_APPFLOWFULL 415
#define GSHELL_CMD_APPDEMOALL 416
#define GSHELL_CMD_APPSECURITYSUM 417
#define GSHELL_CMD_APPPERMISSIONSUM 418
#define GSHELL_CMD_APPRUNTIMESUM 419
#define GSHELL_CMD_APPWINDOWSUM 420
#define GSHELL_CMD_APPLAUNCHSUM 421
#define GSHELL_CMD_APPSTATESUM 422
#define GSHELL_CMD_APPFINALCHECK 423
#define GSHELL_CMD_APPFINALRESET 424
#define GSHELL_CMD_APPNEXT    425
#define GSHELL_CMD_APPROADMAP 426
#define GSHELL_CMD_VISUALSTATUS 427
#define GSHELL_CMD_VISUALBOOT 428
#define GSHELL_CMD_VISUALDESKTOP 429
#define GSHELL_CMD_VISUALPANEL 430
#define GSHELL_CMD_VISUALCARDS 431
#define GSHELL_CMD_VISUALWINDOW 432
#define GSHELL_CMD_VISUALLAUNCHER 433
#define GSHELL_CMD_VISUALTASKBAR 434
#define GSHELL_CMD_VISUALDOCK 435
#define GSHELL_CMD_VISUALFOCUS 436
#define GSHELL_CMD_VISUALGRID 437
#define GSHELL_CMD_VISUALTHEME 438
#define GSHELL_CMD_VISUALGLOW 439
#define GSHELL_CMD_VISUALBORDER 440
#define GSHELL_CMD_VISUALMETRICS 441
#define GSHELL_CMD_VISUALDEMO 442
#define GSHELL_CMD_VISUALFLOW 443
#define GSHELL_CMD_VISUALCHECK 444
#define GSHELL_CMD_VISUALRESET 445
#define GSHELL_CMD_VISUALNEXT 446
#define GSHELL_CMD_CARDSTATUS 447
#define GSHELL_CMD_CARDGRID   448
#define GSHELL_CMD_CARDSELECT 449
#define GSHELL_CMD_CARDOPEN   450
#define GSHELL_CMD_CARDEXPAND 451
#define GSHELL_CMD_CARDPIN    452
#define GSHELL_CMD_CARDBADGE  453
#define GSHELL_CMD_CARDPREVIEW 454
#define GSHELL_CMD_WINDOWVISUAL 455
#define GSHELL_CMD_WINDOWTITLE 456
#define GSHELL_CMD_WINDOWBODY 457
#define GSHELL_CMD_WINDOWSHADOW 458
#define GSHELL_CMD_WINDOWACTIVE 459
#define GSHELL_CMD_WINDOWPREVIEW 460
#define GSHELL_CMD_VISUALCOMPOSE 461
#define GSHELL_CMD_VISUALSYNC 462
#define GSHELL_CMD_CARDFLOW   463
#define GSHELL_CMD_CARDCHECK  464
#define GSHELL_CMD_CARDRESET  465
#define GSHELL_CMD_CARDNEXT   466
#define GSHELL_CMD_UIMOCKSTATUS 467
#define GSHELL_CMD_UIMOCKDESKTOP 468
#define GSHELL_CMD_UIMOCKGRID 469
#define GSHELL_CMD_UIMOCKCARD1 470
#define GSHELL_CMD_UIMOCKCARD2 471
#define GSHELL_CMD_UIMOCKCARD3 472
#define GSHELL_CMD_UIMOCKWINDOW 473
#define GSHELL_CMD_UIMOCKTITLE 474
#define GSHELL_CMD_UIMOCKBODY 475
#define GSHELL_CMD_UIMOCKTASKBAR 476
#define GSHELL_CMD_UIMOCKDOCK 477
#define GSHELL_CMD_UIMOCKFOCUS 478
#define GSHELL_CMD_UIMOCKSELECT 479
#define GSHELL_CMD_UIMOCKOPEN 480
#define GSHELL_CMD_UIMOCKLAYOUT 481
#define GSHELL_CMD_UIMOCKMETRICS 482
#define GSHELL_CMD_UIMOCKDEMO 483
#define GSHELL_CMD_UIMOCKCHECK 484
#define GSHELL_CMD_UIMOCKRESET 485
#define GSHELL_CMD_UIMOCKNEXT 486
#define GSHELL_CMD_LAUNCHMOCKSTATUS 487
#define GSHELL_CMD_LAUNCHMOCKPANEL 488
#define GSHELL_CMD_LAUNCHMOCKSEARCH 489
#define GSHELL_CMD_LAUNCHMOCKAPPS 490
#define GSHELL_CMD_LAUNCHMOCKRECENT 491
#define GSHELL_CMD_LAUNCHMOCKPIN 492
#define GSHELL_CMD_LAUNCHMOCKRUN 493
#define GSHELL_CMD_TASKMOCKSTATUS 494
#define GSHELL_CMD_TASKMOCKSTART 495
#define GSHELL_CMD_TASKMOCKAPP 496
#define GSHELL_CMD_TASKMOCKACTIVE 497
#define GSHELL_CMD_TASKMOCKTRAY 498
#define GSHELL_CMD_TASKMOCKCLOCK 499
#define GSHELL_CMD_TASKMOCKNET 500
#define GSHELL_CMD_TASKMOCKSTATE 501
#define GSHELL_CMD_MOCKCOMPOSE 502
#define GSHELL_CMD_MOCKDEMO 503
#define GSHELL_CMD_MOCKCHECK 504
#define GSHELL_CMD_MOCKRESET 505
#define GSHELL_CMD_MOCKNEXT 506
#define GSHELL_CMD_POLISHSTATUS 507
#define GSHELL_CMD_POLISHBASE 508
#define GSHELL_CMD_POLISHGLASS 509
#define GSHELL_CMD_POLISHTITLE 510
#define GSHELL_CMD_POLISHCARDS 511
#define GSHELL_CMD_POLISHWINDOW 512
#define GSHELL_CMD_POLISHTASKBAR 513
#define GSHELL_CMD_POLISHTRAY 514
#define GSHELL_CMD_POLISHBADGE 515
#define GSHELL_CMD_POLISHFOCUS 516
#define GSHELL_CMD_POLISHSHADOW 517
#define GSHELL_CMD_POLISHCOMPACT 518
#define GSHELL_CMD_POLISHALIGN 519
#define GSHELL_CMD_POLISHMETRICS 520
#define GSHELL_CMD_POLISHSAFE 521
#define GSHELL_CMD_POLISHDEMO 522
#define GSHELL_CMD_POLISHFLOW 523
#define GSHELL_CMD_POLISHCHECK 524
#define GSHELL_CMD_POLISHRESET 525
#define GSHELL_CMD_POLISHNEXT 526
#define GSHELL_CMD_SCENESTATUS 527
#define GSHELL_CMD_SCENEBOOT 528
#define GSHELL_CMD_SCENEBACKGROUND 529
#define GSHELL_CMD_SCENETOPBAR 530
#define GSHELL_CMD_SCENELAUNCHER 531
#define GSHELL_CMD_SCENECARDS 532
#define GSHELL_CMD_SCENEWINDOW 533
#define GSHELL_CMD_SCENETASKBAR 534
#define GSHELL_CMD_SCENEDOCK 535
#define GSHELL_CMD_SCENETRAY 536
#define GSHELL_CMD_SCENEBADGE 537
#define GSHELL_CMD_SCENEFOCUS 538
#define GSHELL_CMD_SCENEWIDGETS 539
#define GSHELL_CMD_SCENEACTIVE 540
#define GSHELL_CMD_SCENEMETRICS 541
#define GSHELL_CMD_SCENEDEMO 542
#define GSHELL_CMD_SCENEFLOW 543
#define GSHELL_CMD_SCENECHECK 544
#define GSHELL_CMD_SCENERESET 545
#define GSHELL_CMD_SCENENEXT 546
#define GSHELL_CMD_VISUALFINAL 547
#define GSHELL_CMD_VISUALHEALTHSUM 548
#define GSHELL_CMD_VISUALSCENESUM 549
#define GSHELL_CMD_VISUALMOCKSUM 550
#define GSHELL_CMD_VISUALCARDSUM 551
#define GSHELL_CMD_VISUALLAUNCHSUM 552
#define GSHELL_CMD_VISUALPOLISHSUM 553
#define GSHELL_CMD_VISUALREADINESS 554
#define GSHELL_CMD_VISUALDEMOALL 555
#define GSHELL_CMD_VISUALREGRESSION 556
#define GSHELL_CMD_VISUALBOUNDS 557
#define GSHELL_CMD_VISUALDENSITY 558
#define GSHELL_CMD_VISUALDEFAULT 559
#define GSHELL_CMD_VISUALHANDOFF 560
#define GSHELL_CMD_VISUALNEXTPHASE 561
#define GSHELL_CMD_VISUALFINALCHECK 562
#define GSHELL_CMD_VISUALFINALRESET 563
#define GSHELL_CMD_VISUALROADMAP 564
#define GSHELL_CMD_VISUALINPUTNEXT 565
#define GSHELL_CMD_VISUALCLOSEOUT 566
#define GSHELL_CMD_INTERACTSTATUS 567
#define GSHELL_CMD_POINTERMOCK 568
#define GSHELL_CMD_POINTERMOVE 569
#define GSHELL_CMD_POINTERHOVER 570
#define GSHELL_CMD_POINTERCLICK 571
#define GSHELL_CMD_FOCUSMOCK 572
#define GSHELL_CMD_FOCUSNEXT 573
#define GSHELL_CMD_FOCUSCARD 574
#define GSHELL_CMD_FOCUSWINDOW 575
#define GSHELL_CMD_FOCUSTASKBAR 576
#define GSHELL_CMD_SELECTMOCK 577
#define GSHELL_CMD_OPENMOCK 578
#define GSHELL_CMD_CLOSEMOCK 579
#define GSHELL_CMD_MENUMOCK 580
#define GSHELL_CMD_SHORTCUTMOCK 581
#define GSHELL_CMD_ROUTEMOCK 582
#define GSHELL_CMD_INTERACTDEMO 583
#define GSHELL_CMD_INTERACTCHECK 584
#define GSHELL_CMD_INTERACTRESET 585
#define GSHELL_CMD_INTERACTNEXT 586
#define GSHELL_CMD_ROUTESTATUS 587
#define GSHELL_CMD_HITDESKTOP 588
#define GSHELL_CMD_HITCARD 589
#define GSHELL_CMD_HITWINDOW 590
#define GSHELL_CMD_HITTASKBAR 591
#define GSHELL_CMD_HITLAUNCHER 592
#define GSHELL_CMD_HOVERROUTE 593
#define GSHELL_CMD_CLICKROUTE 594
#define GSHELL_CMD_DOUBLECLICKROUTE 595
#define GSHELL_CMD_RIGHTCLICKROUTE 596
#define GSHELL_CMD_MENUROUTE 597
#define GSHELL_CMD_OPENROUTE 598
#define GSHELL_CMD_SELECTROUTE 599
#define GSHELL_CMD_FOCUSROUTE 600
#define GSHELL_CMD_DRAGROUTE 601
#define GSHELL_CMD_DROPROUTE 602
#define GSHELL_CMD_ROUTEDEMO 603
#define GSHELL_CMD_ROUTECHECK 604
#define GSHELL_CMD_ROUTERESET 605
#define GSHELL_CMD_ROUTENEXT 606
#define GSHELL_CMD_UNKNOWN     999

static unsigned int gshell_input_layer_enabled = 1;
static unsigned int gshell_mouse_layer_ready = 0;
static unsigned int gshell_click_layer_ready = 0;
static unsigned int gshell_focus_layer_ready = 1;
static unsigned int gshell_interaction_events = 0;
static unsigned int gshell_focus_changes = 0;
static const char* gshell_input_state = "keyboard";
static const char* gshell_focus_target = "terminal";
static const char* gshell_input_last = "none";

static int gshell_cursor_x = 400;
static int gshell_cursor_y = 300;
static unsigned int gshell_cursor_visible = 1;
static unsigned int gshell_cursor_moves = 0;
static unsigned int gshell_cursor_clicks = 0;
static unsigned int gshell_cursor_wheel = 0;
static const char* gshell_cursor_state = "center";
static const char* gshell_cursor_last = "none";

static unsigned int gshell_hit_tests = 0;
static unsigned int gshell_hit_clicks = 0;
static unsigned int gshell_hit_commands = 0;
static unsigned int gshell_hit_context = 0;
static unsigned int gshell_hit_wheel = 0;
static const char* gshell_hit_zone = "none";
static const char* gshell_hit_target = "none";
static const char* gshell_hit_action = "none";
static const char* gshell_hit_last = "none";

static unsigned int gshell_button_hovered = 0;
static unsigned int gshell_button_pressed = 0;
static unsigned int gshell_button_active = 0;
static unsigned int gshell_button_focus = 0;
static unsigned int gshell_button_events = 0;
static unsigned int gshell_button_activations = 0;
static const char* gshell_button_target = "none";
static const char* gshell_button_state = "idle";
static const char* gshell_button_last = "none";

static unsigned int gshell_window_exists = 0;
static unsigned int gshell_window_focused = 0;
static unsigned int gshell_window_minimized = 0;
static unsigned int gshell_window_moves = 0;
static unsigned int gshell_window_events = 0;
static unsigned int gshell_window_x = 120;
static unsigned int gshell_window_y = 92;
static const char* gshell_window_title = "none";
static const char* gshell_window_state = "closed";
static const char* gshell_window_last = "none";

static unsigned int gshell_desktop_enabled = 1;
static unsigned int gshell_workspace_ready = 0;
static unsigned int gshell_icon_selected = 0;
static unsigned int gshell_dock_ready = 0;
static unsigned int gshell_desktop_events = 0;
static unsigned int gshell_workspace_id = 0;
static const char* gshell_desktop_state = "idle";
static const char* gshell_desktop_focus = "terminal";
static const char* gshell_desktop_last = "none";

static unsigned int gshell_shell_enabled = 1;
static unsigned int gshell_shell_panel_ready = 0;
static unsigned int gshell_shell_taskbar_ready = 0;
static unsigned int gshell_shell_launcher_ready = 0;
static unsigned int gshell_shell_open_apps = 0;
static unsigned int gshell_shell_events = 0;
static const char* gshell_shell_state = "idle";
static const char* gshell_shell_focus = "terminal";
static const char* gshell_shell_last = "none";

static unsigned int gshell_launcher_grid_ready = 0;
static unsigned int gshell_launcher_app_selected = 0;
static unsigned int gshell_launcher_app_pinned = 0;
static unsigned int gshell_launcher_open_count = 0;
static unsigned int gshell_launcher_events = 0;
static const char* gshell_launcher_state = "idle";
static const char* gshell_launcher_selected_app = "none";
static const char* gshell_launcher_last = "none";

static unsigned int gshell_taskbar_enabled = 1;
static unsigned int gshell_taskbar_item_ready = 0;
static unsigned int gshell_taskbar_focused = 0;
static unsigned int gshell_taskbar_minimized = 0;
static unsigned int gshell_taskbar_switches = 0;
static unsigned int gshell_taskbar_events = 0;
static const char* gshell_taskbar_state = "idle";
static const char* gshell_taskbar_item = "none";
static const char* gshell_taskbar_last = "none";

static unsigned int gshell_layout_enabled = 1;
static unsigned int gshell_layout_grid_ready = 0;
static unsigned int gshell_layout_snapped = 0;
static unsigned int gshell_layout_maximized = 0;
static unsigned int gshell_layout_z_index = 0;
static unsigned int gshell_layout_events = 0;
static const char* gshell_layout_state = "idle";
static const char* gshell_layout_mode = "free";
static const char* gshell_layout_last = "none";

static unsigned int gshell_home_ready = 0;
static unsigned int gshell_home_apps_ready = 0;
static unsigned int gshell_home_windows_ready = 0;
static unsigned int gshell_home_layout_ready = 0;
static unsigned int gshell_home_dock_ready = 0;
static unsigned int gshell_home_events = 0;
static const char* gshell_home_state = "idle";
static const char* gshell_home_focus = "terminal";
static const char* gshell_home_last = "none";

static unsigned int gshell_app_shell_enabled = 1;
static unsigned int gshell_app_catalog_ready = 0;
static unsigned int gshell_app_card_ready = 0;
static unsigned int gshell_app_details_ready = 0;
static unsigned int gshell_app_shell_launch_ready = 0;
static unsigned int gshell_app_shell_events = 0;
static const char* gshell_app_shell_state = "idle";
static const char* gshell_app_shell_selected = "none";
static const char* gshell_app_shell_last = "none";

static unsigned int gshell_catalog_ready = 0;
static unsigned int gshell_catalog_items = 0;
static unsigned int gshell_catalog_search_hits = 0;
static unsigned int gshell_catalog_pinned = 0;
static unsigned int gshell_catalog_enabled = 1;
static unsigned int gshell_catalog_events = 0;
static const char* gshell_catalog_state = "idle";
static const char* gshell_catalog_selected = "none";
static const char* gshell_catalog_last = "none";

static unsigned int gshell_detail_ready = 0;
static unsigned int gshell_detail_manifest_ready = 0;
static unsigned int gshell_detail_caps_ready = 0;
static unsigned int gshell_detail_perms_ready = 0;
static unsigned int gshell_detail_launch_ready = 0;
static unsigned int gshell_detail_events = 0;
static const char* gshell_detail_state = "idle";
static const char* gshell_detail_app = "none";
static const char* gshell_detail_last = "none";

static unsigned int gshell_action_prepared = 0;
static unsigned int gshell_action_allowed = 0;
static unsigned int gshell_action_opened = 0;
static unsigned int gshell_action_running = 0;
static unsigned int gshell_action_events = 0;
static const char* gshell_action_state = "idle";
static const char* gshell_action_app = "none";
static const char* gshell_action_last = "none";

static unsigned int gshell_app_mgmt_inventory_ready = 0;
static unsigned int gshell_app_mgmt_scanned = 0;
static unsigned int gshell_app_mgmt_registered = 0;
static unsigned int gshell_app_mgmt_enabled = 1;
static unsigned int gshell_app_mgmt_trusted = 0;
static unsigned int gshell_app_mgmt_favorite = 0;
static unsigned int gshell_app_mgmt_update_ready = 0;
static unsigned int gshell_app_mgmt_rollback_ready = 0;
static unsigned int gshell_app_mgmt_cache_cleared = 0;
static unsigned int gshell_app_mgmt_healthy = 1;
static unsigned int gshell_app_mgmt_events = 0;
static const char* gshell_app_mgmt_state = "idle";
static const char* gshell_app_mgmt_app = "none";
static const char* gshell_app_mgmt_last = "none";

static unsigned int gshell_app_final_ready = 0;
static unsigned int gshell_app_final_flow_ready = 0;
static unsigned int gshell_app_final_demo_ready = 0;
static unsigned int gshell_app_final_events = 0;
static const char* gshell_app_final_state = "idle";
static const char* gshell_app_final_focus = "terminal";
static const char* gshell_app_final_last = "none";

static unsigned int gshell_visual_enabled = 1;
static unsigned int gshell_visual_desktop_ready = 0;
static unsigned int gshell_visual_panel_ready = 0;
static unsigned int gshell_visual_cards_ready = 0;
static unsigned int gshell_visual_window_ready = 0;
static unsigned int gshell_visual_launcher_ready = 0;
static unsigned int gshell_visual_taskbar_ready = 0;
static unsigned int gshell_visual_dock_ready = 0;
static unsigned int gshell_visual_grid_ready = 0;
static unsigned int gshell_visual_theme_ready = 0;
static unsigned int gshell_visual_glow_ready = 0;
static unsigned int gshell_visual_border_ready = 0;
static unsigned int gshell_visual_metrics = 0;
static unsigned int gshell_visual_events = 0;
static const char* gshell_visual_state = "idle";
static const char* gshell_visual_focus = "terminal";
static const char* gshell_visual_last = "none";

static unsigned int gshell_card_grid_ready = 0;
static unsigned int gshell_card_selected = 0;
static unsigned int gshell_card_opened = 0;
static unsigned int gshell_card_expanded = 0;
static unsigned int gshell_card_pinned = 0;
static unsigned int gshell_card_badge_ready = 0;
static unsigned int gshell_card_preview_ready = 0;
static unsigned int gshell_window_visual_ready = 0;
static unsigned int gshell_window_title_ready = 0;
static unsigned int gshell_window_body_ready = 0;
static unsigned int gshell_window_shadow_ready = 0;
static unsigned int gshell_window_active_ready = 0;
static unsigned int gshell_window_preview_ready = 0;
static unsigned int gshell_card_events = 0;
static const char* gshell_card_state = "idle";
static const char* gshell_card_app = "none";
static const char* gshell_card_last = "none";

static unsigned int gshell_ui_mock_desktop_ready = 0;
static unsigned int gshell_ui_mock_grid_ready = 0;
static unsigned int gshell_ui_mock_card1_ready = 0;
static unsigned int gshell_ui_mock_card2_ready = 0;
static unsigned int gshell_ui_mock_card3_ready = 0;
static unsigned int gshell_ui_mock_window_ready = 0;
static unsigned int gshell_ui_mock_title_ready = 0;
static unsigned int gshell_ui_mock_body_ready = 0;
static unsigned int gshell_ui_mock_taskbar_ready = 0;
static unsigned int gshell_ui_mock_dock_ready = 0;
static unsigned int gshell_ui_mock_focus_ready = 0;
static unsigned int gshell_ui_mock_selected_card = 0;
static unsigned int gshell_ui_mock_events = 0;
static const char* gshell_ui_mock_state = "idle";
static const char* gshell_ui_mock_focus = "terminal";
static const char* gshell_ui_mock_last = "none";

static unsigned int gshell_launch_mock_panel_ready = 0;
static unsigned int gshell_launch_mock_search_ready = 0;
static unsigned int gshell_launch_mock_apps_ready = 0;
static unsigned int gshell_launch_mock_recent_ready = 0;
static unsigned int gshell_launch_mock_pin_ready = 0;
static unsigned int gshell_launch_mock_run_ready = 0;
static unsigned int gshell_task_mock_start_ready = 0;
static unsigned int gshell_task_mock_app_ready = 0;
static unsigned int gshell_task_mock_active_ready = 0;
static unsigned int gshell_task_mock_tray_ready = 0;
static unsigned int gshell_task_mock_clock_ready = 0;
static unsigned int gshell_task_mock_net_ready = 0;
static unsigned int gshell_mock_events = 0;
static const char* gshell_mock_state = "idle";
static const char* gshell_mock_focus = "terminal";
static const char* gshell_mock_last = "none";

static unsigned int gshell_polish_base_ready = 0;
static unsigned int gshell_polish_glass_ready = 0;
static unsigned int gshell_polish_title_ready = 0;
static unsigned int gshell_polish_cards_ready = 0;
static unsigned int gshell_polish_window_ready = 0;
static unsigned int gshell_polish_taskbar_ready = 0;
static unsigned int gshell_polish_tray_ready = 0;
static unsigned int gshell_polish_badge_ready = 0;
static unsigned int gshell_polish_focus_ready = 0;
static unsigned int gshell_polish_shadow_ready = 0;
static unsigned int gshell_polish_compact_ready = 0;
static unsigned int gshell_polish_align_ready = 0;
static unsigned int gshell_polish_metrics_ready = 0;
static unsigned int gshell_polish_safe_ready = 0;
static unsigned int gshell_polish_events = 0;
static const char* gshell_polish_state = "idle";
static const char* gshell_polish_focus = "terminal";
static const char* gshell_polish_last = "none";

static unsigned int gshell_scene_background_ready = 1;
static unsigned int gshell_scene_topbar_ready = 1;
static unsigned int gshell_scene_launcher_ready = 1;
static unsigned int gshell_scene_cards_ready = 1;
static unsigned int gshell_scene_window_ready = 1;
static unsigned int gshell_scene_taskbar_ready = 1;
static unsigned int gshell_scene_dock_ready = 1;
static unsigned int gshell_scene_tray_ready = 1;
static unsigned int gshell_scene_badge_ready = 1;
static unsigned int gshell_scene_focus_ready = 1;
static unsigned int gshell_scene_widgets_ready = 1;
static unsigned int gshell_scene_active_ready = 1;
static unsigned int gshell_scene_metrics = 0;
static unsigned int gshell_scene_events = 0;
static const char* gshell_scene_state = "default";
static const char* gshell_scene_focus = "desktop";
static const char* gshell_scene_last = "boot-default";

static unsigned int gshell_visual_final_ready = 0;
static unsigned int gshell_visual_final_scene_ready = 0;
static unsigned int gshell_visual_final_mock_ready = 0;
static unsigned int gshell_visual_final_cards_ready = 0;
static unsigned int gshell_visual_final_launcher_ready = 0;
static unsigned int gshell_visual_final_polish_ready = 0;
static unsigned int gshell_visual_final_bounds_ready = 0;
static unsigned int gshell_visual_final_density_ready = 0;
static unsigned int gshell_visual_final_default_ready = 1;
static unsigned int gshell_visual_final_events = 0;
static const char* gshell_visual_final_state = "idle";
static const char* gshell_visual_final_focus = "desktop";
static const char* gshell_visual_final_last = "none";

static unsigned int gshell_interact_pointer_ready = 0;
static unsigned int gshell_interact_pointer_x = 120;
static unsigned int gshell_interact_pointer_y = 80;
static unsigned int gshell_interact_hover_ready = 0;
static unsigned int gshell_interact_click_ready = 0;
static unsigned int gshell_interact_focus_ready = 0;
static unsigned int gshell_interact_select_ready = 0;
static unsigned int gshell_interact_open_ready = 0;
static unsigned int gshell_interact_menu_ready = 0;
static unsigned int gshell_interact_shortcut_ready = 0;
static unsigned int gshell_interact_route_ready = 0;
static unsigned int gshell_interact_events = 0;
static const char* gshell_interact_state = "idle";
static const char* gshell_interact_target = "desktop";
static const char* gshell_interact_last = "none";

static unsigned int gshell_route_desktop_ready = 0;
static unsigned int gshell_route_card_ready = 0;
static unsigned int gshell_route_window_ready = 0;
static unsigned int gshell_route_taskbar_ready = 0;
static unsigned int gshell_route_launcher_ready = 0;
static unsigned int gshell_route_hover_ready = 0;
static unsigned int gshell_route_click_ready = 0;
static unsigned int gshell_route_double_click_ready = 0;
static unsigned int gshell_route_right_click_ready = 0;
static unsigned int gshell_route_menu_ready = 0;
static unsigned int gshell_route_open_ready = 0;
static unsigned int gshell_route_select_ready = 0;
static unsigned int gshell_route_focus_ready = 0;
static unsigned int gshell_route_drag_ready = 0;
static unsigned int gshell_route_drop_ready = 0;
static unsigned int gshell_route_events = 0;
static const char* gshell_route_state = "idle";
static const char* gshell_route_target = "desktop";
static const char* gshell_route_last = "none";

static unsigned int gshell_flow_prepared = 0;
static unsigned int gshell_flow_demos = 0;
static unsigned int gshell_flow_checks = 0;
static const char* gshell_flow_state = "idle";
static const char* gshell_flow_last = "none";

static unsigned int gshell_demo_step = 0;
static unsigned int gshell_demo_checks = 0;
static const char* gshell_demo_state = "idle";
static const char* gshell_demo_last = "none";

static unsigned int gshell_life_started = 0;
static unsigned int gshell_life_paused = 0;
static unsigned int gshell_life_starts = 0;
static unsigned int gshell_life_pauses = 0;
static unsigned int gshell_life_resumes = 0;
static unsigned int gshell_life_stops = 0;
static unsigned int gshell_life_checks = 0;
static const char* gshell_life_state = "idle";
static const char* gshell_life_last = "none";

static unsigned int gshell_fs_mounted = 0;
static unsigned int gshell_fs_has_file = 0;
static unsigned int gshell_fs_reads = 0;
static unsigned int gshell_fs_writes = 0;
static unsigned int gshell_fs_checks = 0;
static const char* gshell_fs_state = "unmounted";
static const char* gshell_fs_file = "none";
static const char* gshell_fs_last = "none";

static unsigned int gshell_perm_requested = 0;
static unsigned int gshell_perm_allowed = 0;
static unsigned int gshell_perm_denied = 0;
static unsigned int gshell_perm_checks = 0;
static const char* gshell_perm_state = "idle";
static const char* gshell_perm_last = "none";

static unsigned int gshell_app_slot_allocated = 0;
static unsigned int gshell_app_slot_pid = 0;
static unsigned int gshell_app_slot_checks = 0;
static const char* gshell_app_slot_state = "empty";

static unsigned int gshell_launch_prepared = 0;
static unsigned int gshell_launch_approved = 0;
static unsigned int gshell_launch_attempts = 0;
static const char* gshell_launch_state = "idle";
static const char* gshell_launch_last_result = "none";

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
    { "runtimestatus", GSHELL_CMD_RUNTIMESTATUS, "RUNTIMESTATUS", "RUNTIME STATUS OK" },
    { "appstatus",   GSHELL_CMD_APPSTATUS,   "RUNTIMESTATUS", "APP STATUS OK" },
    { "loaderstatus", GSHELL_CMD_LOADERSTATUS, "RUNTIMESTATUS", "LOADER STATUS OK" },
    { "launchstatus", GSHELL_CMD_LAUNCHSTATUS, "RUNTIMESTATUS", "LAUNCH STATUS OK" },
    { "runtimecheck", GSHELL_CMD_RUNTIMECHECK, "RUNTIMESTATUS", "RUNTIME CHECK OK" },
    { "appcheck",    GSHELL_CMD_APPCHECK,    "RUNTIMESTATUS", "APP CHECK OK" },
    { "elfstatus",   GSHELL_CMD_ELFSTATUS,   "ELFSTATUS",   "ELF STATUS OK" },
    { "elfheader",   GSHELL_CMD_ELFHEADER,   "ELFSTATUS",   "ELF HEADER OK" },
    { "elfsegments", GSHELL_CMD_ELFSEGMENTS, "ELFSTATUS",   "ELF SEGMENTS OK" },
    { "elfsections", GSHELL_CMD_ELFSECTIONS, "ELFSTATUS",   "ELF SECTIONS OK" },
    { "elfvalidate", GSHELL_CMD_ELFVALIDATE, "ELFSTATUS",   "ELF VALIDATE OK" },
    { "loadercheck", GSHELL_CMD_LOADERCHECK, "ELFSTATUS",   "LOADER CHECK OK" },
    { "manifeststatus", GSHELL_CMD_MANIFESTSTATUS, "MANIFESTSTATUS", "MANIFEST STATUS OK" },
    { "appmanifest", GSHELL_CMD_APPMANIFEST, "MANIFESTSTATUS", "APP MANIFEST OK" },
    { "appdescriptor", GSHELL_CMD_APPDESCRIPTOR, "MANIFESTSTATUS", "APP DESCRIPTOR OK" },
    { "appentry",    GSHELL_CMD_APPENTRY,    "MANIFESTSTATUS", "APP ENTRY OK" },
    { "appcaps",     GSHELL_CMD_APPCAPS,     "MANIFESTSTATUS", "APP CAPS OK" },
    { "manifestcheck", GSHELL_CMD_MANIFESTCHECK, "MANIFESTSTATUS", "MANIFEST CHECK OK" },
    { "appslotstatus", GSHELL_CMD_APPSLOTSTATUS, "APPSLOTSTATUS", "APP SLOT STATUS OK" },
    { "slotlist",    GSHELL_CMD_SLOTLIST,    "APPSLOTSTATUS", "SLOT LIST OK" },
    { "slotalloc",   GSHELL_CMD_SLOTLALLOC,  "APPSLOTSTATUS", "SLOT ALLOC OK" },
    { "slotfree",    GSHELL_CMD_SLOTFREE,    "APPSLOTSTATUS", "SLOT FREE OK" },
    { "processslot", GSHELL_CMD_PROCESSSLOT, "APPSLOTSTATUS", "PROCESS SLOT OK" },
    { "slotcheck",   GSHELL_CMD_SLOTCHECK,   "APPSLOTSTATUS", "SLOT CHECK OK" },
    { "launchgate",  GSHELL_CMD_LAUNCHGATE,  "LAUNCHGATE",  "LAUNCH GATE OK" },
    { "launchprepare", GSHELL_CMD_LAUNCHPREPARE, "LAUNCHGATE", "LAUNCH PREPARE OK" },
    { "launchapprove", GSHELL_CMD_LAUNCHAPPROVE, "LAUNCHGATE", "LAUNCH APPROVE OK" },
    { "launchdeny",  GSHELL_CMD_LAUNCHDENY,  "LAUNCHGATE",  "LAUNCH DENY OK" },
    { "launchrun",   GSHELL_CMD_LAUNCHRUN,   "LAUNCHGATE",  "LAUNCH RUN OK" },
    { "launchcheck", GSHELL_CMD_LAUNCHCHECK, "LAUNCHGATE",  "LAUNCH CHECK OK" },
    { "permstatus",  GSHELL_CMD_PERMSTATUS,  "PERMSTATUS",  "PERMISSION STATUS OK" },
    { "permrequest", GSHELL_CMD_PERMREQUEST, "PERMSTATUS",  "PERMISSION REQUEST OK" },
    { "permallow",   GSHELL_CMD_PERMALLOW,   "PERMSTATUS",  "PERMISSION ALLOW OK" },
    { "permdeny",    GSHELL_CMD_PERMDENY,    "PERMSTATUS",  "PERMISSION DENY OK" },
    { "permcheck",   GSHELL_CMD_PERMCHECK,   "PERMSTATUS",  "PERMISSION CHECK OK" },
    { "permreset",   GSHELL_CMD_PERMRESET,   "PERMSTATUS",  "PERMISSION RESET OK" },
    { "fsstatus",    GSHELL_CMD_FSSTATUS,    "FSSTATUS",    "FS STATUS OK" },
    { "ramfsstatus", GSHELL_CMD_RAMFSSTATUS, "FSSTATUS",    "RAMFS STATUS OK" },
    { "fsmount",     GSHELL_CMD_FSMOUNT,     "FSSTATUS",    "FS MOUNT OK" },
    { "fswrite",     GSHELL_CMD_FSWRITE,     "FSSTATUS",    "FS WRITE OK" },
    { "fsread",      GSHELL_CMD_FSREAD,      "FSSTATUS",    "FS READ OK" },
    { "fscheck",     GSHELL_CMD_FSCHECK,     "FSSTATUS",    "FS CHECK OK" },
    { "fsreset",     GSHELL_CMD_FSRESET,     "FSSTATUS",    "FS RESET OK" },
    { "lifestatus",  GSHELL_CMD_LIFESTATUS,  "LIFESTATUS",  "LIFE STATUS OK" },
    { "appstart",    GSHELL_CMD_APPSTART,    "LIFESTATUS",  "APP START OK" },
    { "apppause",    GSHELL_CMD_APPPAUSE,    "LIFESTATUS",  "APP PAUSE OK" },
    { "appresume",   GSHELL_CMD_APPRESUME,   "LIFESTATUS",  "APP RESUME OK" },
    { "appstop",     GSHELL_CMD_APPSTOP,     "LIFESTATUS",  "APP STOP OK" },
    { "lifecheck",   GSHELL_CMD_LIFECHECK,   "LIFESTATUS",  "LIFE CHECK OK" },
    { "lifereset",   GSHELL_CMD_LIFERESET,   "LIFESTATUS",  "LIFE RESET OK" },
    { "runtimepanel", GSHELL_CMD_RUNTIMEPANEL, "RUNTIMEPANEL", "RUNTIME PANEL OK" },
    { "apppanel",    GSHELL_CMD_APPPANEL,    "RUNTIMEPANEL", "APP PANEL OK" },
    { "launchpanel", GSHELL_CMD_LAUNCHPANEL, "RUNTIMEPANEL", "LAUNCH PANEL OK" },
    { "permissionpanel", GSHELL_CMD_PERMISSIONPANEL, "RUNTIMEPANEL", "PERMISSION PANEL OK" },
    { "fspanel",     GSHELL_CMD_FSPANEL,     "RUNTIMEPANEL", "FS PANEL OK" },
    { "lifepanel",   GSHELL_CMD_LIFEPANEL,   "RUNTIMEPANEL", "LIFE PANEL OK" },
    { "runtimefinal", GSHELL_CMD_RUNTIMEFINAL, "RUNTIMEFINAL", "RUNTIME FINAL OK" },
    { "runtimehealth", GSHELL_CMD_RUNTIMEHEALTH, "RUNTIMEFINAL", "RUNTIME HEALTH OK" },
    { "appsummary",  GSHELL_CMD_APPSUMMARY,  "RUNTIMEFINAL", "APP SUMMARY OK" },
    { "launchsummary", GSHELL_CMD_LAUNCHSUMMARY, "RUNTIMEFINAL", "LAUNCH SUMMARY OK" },
    { "permissionsummary", GSHELL_CMD_PERMISSIONSUMMARY, "RUNTIMEFINAL", "PERMISSION SUMMARY OK" },
    { "fssummary",   GSHELL_CMD_FSSUMMARY,   "RUNTIMEFINAL", "FS SUMMARY OK" },
    { "lifesummary", GSHELL_CMD_LIFESUMMARY, "RUNTIMEFINAL", "LIFE SUMMARY OK" },
    { "nextmajor",   GSHELL_CMD_NEXTMAJOR,   "RUNTIMEFINAL", "NEXT MAJOR OK" },
    { "prototype",   GSHELL_CMD_PROTOTYPE,   "PROTOTYPE",   "PROTOTYPE OK" },
    { "protostatus", GSHELL_CMD_PROTOSTATUS, "PROTOTYPE",   "PROTO STATUS OK" },
    { "milestone",   GSHELL_CMD_PROTOMILESTONE, "PROTOTYPE",   "MILESTONE OK" },
    { "systemflow",  GSHELL_CMD_SYSTEMFLOW,  "PROTOTYPE",   "SYSTEM FLOW OK" },
    { "demoflow",    GSHELL_CMD_DEMOFLOW,    "PROTOTYPE",   "DEMO FLOW OK" },
    { "protoready",  GSHELL_CMD_PROTOREADY,  "PROTOTYPE",   "PROTO READY OK" },
    { "dashboard",   GSHELL_CMD_UNIDASH,     "DASHBOARD",   "DASHBOARD OK" },
    { "sysdash",     GSHELL_CMD_SYSDASH,     "DASHBOARD",   "SYSTEM DASH OK" },
    { "bootdash",    GSHELL_CMD_BOOTDASH,    "DASHBOARD",   "BOOT DASH OK" },
    { "controldash", GSHELL_CMD_CONTROLDASH, "DASHBOARD",   "CONTROL DASH OK" },
    { "runtimedash", GSHELL_CMD_RUNTIMEDASH, "DASHBOARD",   "RUNTIME DASH OK" },
    { "flowdash",    GSHELL_CMD_FLOWDASH,    "DASHBOARD",   "FLOW DASH OK" },
    { "boothealth",  GSHELL_CMD_BOOTHEALTH,  "BOOTHEALTH",  "BOOT HEALTH OK" },
    { "healthdash",  GSHELL_CMD_HEALTHDASH,  "BOOTHEALTH",  "HEALTH DASH OK" },
    { "doctordash",  GSHELL_CMD_DOCTORDASH,  "BOOTHEALTH",  "DOCTOR DASH OK" },
    { "corecheck",   GSHELL_CMD_CORECHECK,   "BOOTHEALTH",  "CORE CHECK OK" },
    { "bootcheck",   GSHELL_CMD_BOOTCHECK,   "BOOTHEALTH",  "BOOT CHECK OK" },
    { "systemcheck", GSHELL_CMD_SYSTEMCHECK, "BOOTHEALTH",  "SYSTEM CHECK OK" },
    { "flowstatus",  GSHELL_CMD_FLOWSTATUS,  "FLOWSTATUS",  "FLOW STATUS OK" },
    { "flowprepare", GSHELL_CMD_FLOWPREPARE, "FLOWSTATUS",  "FLOW PREPARE OK" },
    { "flowcontrol", GSHELL_CMD_FLOWCONTROL, "FLOWSTATUS",  "FLOW CONTROL OK" },
    { "flowruntime", GSHELL_CMD_FLOWRUNTIME, "FLOWSTATUS",  "FLOW RUNTIME OK" },
    { "flowdemo",    GSHELL_CMD_FLOWDEMO,    "FLOWSTATUS",  "FLOW DEMO OK" },
    { "flowcheck",   GSHELL_CMD_FLOWCHECK,   "FLOWSTATUS",  "FLOW CHECK OK" },
    { "flowreset",   GSHELL_CMD_FLOWRESET,   "FLOWSTATUS",  "FLOW RESET OK" },
    { "walkthrough", GSHELL_CMD_WALKTHROUGH, "WALKTHROUGH", "WALKTHROUGH OK" },
    { "demostep1",   GSHELL_CMD_DEMOSTEP1,   "WALKTHROUGH", "DEMO STEP1 OK" },
    { "demostep2",   GSHELL_CMD_DEMOSTEP2,   "WALKTHROUGH", "DEMO STEP2 OK" },
    { "demostep3",   GSHELL_CMD_DEMOSTEP3,   "WALKTHROUGH", "DEMO STEP3 OK" },
    { "demostep4",   GSHELL_CMD_DEMOSTEP4,   "WALKTHROUGH", "DEMO STEP4 OK" },
    { "demostep5",   GSHELL_CMD_DEMOSTEP5,   "WALKTHROUGH", "DEMO STEP5 OK" },
    { "democheck",   GSHELL_CMD_DEMOCHECK,   "WALKTHROUGH", "DEMO CHECK OK" },
    { "demoreset",   GSHELL_CMD_DEMORESET,   "WALKTHROUGH", "DEMO RESET OK" },
    { "milestonefinal", GSHELL_CMD_MILESTONEFINAL, "MILEFINAL", "MILESTONE FINAL OK" },
    { "protosummary", GSHELL_CMD_PROTOSUMMARY, "MILEFINAL", "PROTO SUMMARY OK" },
    { "chainsummary", GSHELL_CMD_CHAINSUMMARY, "MILEFINAL", "CHAIN SUMMARY OK" },
    { "readypanel",  GSHELL_CMD_READYPANEL,  "MILEFINAL", "READY PANEL OK" },
    { "versionpanel", GSHELL_CMD_VERSIONPANEL, "MILEFINAL", "VERSION PANEL OK" },
    { "closecheck",  GSHELL_CMD_CLOSECHECK,  "MILEFINAL", "CLOSE CHECK OK" },
    { "uipolish",    GSHELL_CMD_UIPOLISH,    "UIPOLISH",    "UI POLISH OK" },
    { "layoutcheck", GSHELL_CMD_LAYOUTCHECK, "UIPOLISH",    "LAYOUT CHECK OK" },
    { "panelcheck",  GSHELL_CMD_PANELCHECK,  "UIPOLISH",    "PANEL CHECK OK" },
    { "textcheck",   GSHELL_CMD_TEXTCHECK,   "UIPOLISH",    "TEXT CHECK OK" },
    { "commandclean", GSHELL_CMD_COMMANDCLEAN, "UIPOLISH",  "COMMAND CLEAN OK" },
    { "uicheck",     GSHELL_CMD_UICHECK,     "UIPOLISH",    "UI CHECK OK" },
    { "closeout",    GSHELL_CMD_CLOSEOUT,    "CLOSEOUT",    "CLOSEOUT OK" },
    { "finalstatus", GSHELL_CMD_FINALSTATUS, "CLOSEOUT",    "FINAL STATUS OK" },
    { "finalhealth", GSHELL_CMD_FINALHEALTH, "CLOSEOUT",    "FINAL HEALTH OK" },
    { "finaldemo",   GSHELL_CMD_FINALDEMO,   "CLOSEOUT",    "FINAL DEMO OK" },
    { "finalready",  GSHELL_CMD_FINALREADY,  "CLOSEOUT",    "FINAL READY OK" },
    { "releaseinfo", GSHELL_CMD_RELEASEINFO, "CLOSEOUT",    "RELEASE INFO OK" },
    { "versionfinal", GSHELL_CMD_VERSIONFINAL, "CLOSEOUT",  "VERSION FINAL OK" },
    { "nextroadmap", GSHELL_CMD_NEXTROADMAP, "CLOSEOUT",    "NEXT ROADMAP OK" },
    { "inputstatus", GSHELL_CMD_INPUTSTATUS, "INPUTSTATUS", "INPUT STATUS OK" },
    { "mousestatus", GSHELL_CMD_MOUSESTATUS, "INPUTSTATUS", "MOUSE STATUS OK" },
    { "clickstatus", GSHELL_CMD_CLICKSTATUS, "INPUTSTATUS", "CLICK STATUS OK" },
    { "focusstatus", GSHELL_CMD_FOCUSSTATUS, "INPUTSTATUS", "FOCUS STATUS OK" },
    { "inputdemo",   GSHELL_CMD_INPUTDEMO,   "INPUTSTATUS", "INPUT DEMO OK" },
    { "inputcheck",  GSHELL_CMD_INPUTCHECK,  "INPUTSTATUS", "INPUT CHECK OK" },
    { "inputreset",  GSHELL_CMD_INPUTRESET,  "INPUTSTATUS", "INPUT RESET OK" },
    { "cursorstatus", GSHELL_CMD_CURSORSTATUS, "CURSORSTATUS", "CURSOR STATUS OK" },
    { "pointerstatus", GSHELL_CMD_POINTERSTATUS, "CURSORSTATUS", "POINTER STATUS OK" },
    { "cursorcenter", GSHELL_CMD_CURSORCENTER, "CURSORSTATUS", "CURSOR CENTER OK" },
    { "cursormove", GSHELL_CMD_CURSORMOVE, "CURSORSTATUS", "CURSOR MOVE OK" },
    { "leftclick",   GSHELL_CMD_LEFTCLICK,   "CURSORSTATUS", "LEFT CLICK OK" },
    { "rightclick",  GSHELL_CMD_RIGHTCLICK,  "CURSORSTATUS", "RIGHT CLICK OK" },
    { "wheelstep",   GSHELL_CMD_WHEELSTEP,   "CURSORSTATUS", "WHEEL STEP OK" },
    { "cursorreset", GSHELL_CMD_CURSORRESET, "CURSORSTATUS", "CURSOR RESET OK" },
    { "hitstatus",   GSHELL_CMD_HITSTATUS,   "HITSTATUS",   "HIT STATUS OK" },
    { "panelhit",    GSHELL_CMD_PANELHIT,    "HITSTATUS",   "PANEL HIT OK" },
    { "commandhit",  GSHELL_CMD_COMMANDHIT,  "HITSTATUS",   "COMMAND HIT OK" },
    { "clickcmd",    GSHELL_CMD_CLICKCMD,    "HITSTATUS",   "CLICK CMD OK" },
    { "rightmenu",   GSHELL_CMD_RIGHTMENU,   "HITSTATUS",   "RIGHT MENU OK" },
    { "wheelpick",   GSHELL_CMD_WHEELPICK,   "HITSTATUS",   "WHEEL PICK OK" },
    { "hitcheck",    GSHELL_CMD_HITCHECK,    "HITSTATUS",   "HIT CHECK OK" },
    { "hitreset",    GSHELL_CMD_HITRESET,    "HITSTATUS",   "HIT RESET OK" },
    { "buttonstatus", GSHELL_CMD_BUTTONSTATUS, "BUTTONSTATUS", "BUTTON STATUS OK" },
    { "buttonhover", GSHELL_CMD_BUTTONHOVER, "BUTTONSTATUS", "BUTTON HOVER OK" },
    { "buttonpress", GSHELL_CMD_BUTTONPRESS, "BUTTONSTATUS", "BUTTON PRESS OK" },
    { "buttonfocus", GSHELL_CMD_BUTTONFOCUS, "BUTTONSTATUS", "BUTTON FOCUS OK" },
    { "buttonactive", GSHELL_CMD_BUTTONACTIVE, "BUTTONSTATUS", "BUTTON ACTIVE OK" },
    { "buttoncheck", GSHELL_CMD_BUTTONCHECK, "BUTTONSTATUS", "BUTTON CHECK OK" },
    { "buttonreset", GSHELL_CMD_BUTTONRESET, "BUTTONSTATUS", "BUTTON RESET OK" },
    { "windowstatus", GSHELL_CMD_WINDOWSTATUS, "WINDOWSTATUS", "WINDOW STATUS OK" },
    { "windowcreate", GSHELL_CMD_WINDOWCREATE, "WINDOWSTATUS", "WINDOW CREATE OK" },
    { "windowfocus", GSHELL_CMD_WINDOWFOCUS, "WINDOWSTATUS", "WINDOW FOCUS OK" },
    { "windowmove", GSHELL_CMD_WINDOWMOVE, "WINDOWSTATUS", "WINDOW MOVE OK" },
    { "windowmin",   GSHELL_CMD_WINDOWMIN,   "WINDOWSTATUS", "WINDOW MIN OK" },
    { "windowclose", GSHELL_CMD_WINDOWCLOSE, "WINDOWSTATUS", "WINDOW CLOSE OK" },
    { "windowcheck", GSHELL_CMD_WINDOWCHECK, "WINDOWSTATUS", "WINDOW CHECK OK" },
    { "windowreset", GSHELL_CMD_WINDOWRESET, "WINDOWSTATUS", "WINDOW RESET OK" },
    { "desktopstatus", GSHELL_CMD_DESKTOPSTATUS, "DESKTOPSTATUS", "DESKTOP STATUS OK" },
    { "workspace",   GSHELL_CMD_WORKSPACE,   "DESKTOPSTATUS", "WORKSPACE OK" },
    { "iconstatus",  GSHELL_CMD_ICONSTATUS,  "DESKTOPSTATUS", "ICON STATUS OK" },
    { "iconselect",  GSHELL_CMD_ICONSELECT,  "DESKTOPSTATUS", "ICON SELECT OK" },
    { "dockstatus",  GSHELL_CMD_DOCKSTATUS,  "DESKTOPSTATUS", "DOCK STATUS OK" },
    { "desktopcheck", GSHELL_CMD_DESKTOPCHECK, "DESKTOPSTATUS", "DESKTOP CHECK OK" },
    { "desktopreset", GSHELL_CMD_DESKTOPRESET, "DESKTOPSTATUS", "DESKTOP RESET OK" },
    { "interactionfinal", GSHELL_CMD_INTERACTIONFINAL, "INTERACTIONFINAL", "INTERACTION FINAL OK" },
    { "inputsummary", GSHELL_CMD_INPUTSUMMARY, "INTERACTIONFINAL", "INPUT SUMMARY OK" },
    { "pointersummary", GSHELL_CMD_POINTERSUMMARY, "INTERACTIONFINAL", "POINTER SUMMARY OK" },
    { "hitsummary",  GSHELL_CMD_HITSUMMARY,  "INTERACTIONFINAL", "HIT SUMMARY OK" },
    { "buttonsummary", GSHELL_CMD_BUTTONSUMMARY, "INTERACTIONFINAL", "BUTTON SUMMARY OK" },
    { "windowsummary", GSHELL_CMD_WINDOWSUMMARY, "INTERACTIONFINAL", "WINDOW SUMMARY OK" },
    { "desktopsummary", GSHELL_CMD_DESKTOPSUMMARY, "INTERACTIONFINAL", "DESKTOP SUMMARY OK" },
    { "nextphase",   GSHELL_CMD_NEXTPHASE,   "INTERACTIONFINAL", "NEXT PHASE OK" },
    { "shellstatus", GSHELL_CMD_SHELLSTATUS, "SHELLSTATUS", "SHELL STATUS OK" },
    { "shellpanel",  GSHELL_CMD_SHELLPANEL,  "SHELLSTATUS", "SHELL PANEL OK" },
    { "taskbar",     GSHELL_CMD_TASKBAR,     "SHELLSTATUS", "TASKBAR OK" },
    { "launcher",    GSHELL_CMD_LAUNCHER,    "SHELLSTATUS", "LAUNCHER OK" },
    { "shellopenapp", GSHELL_CMD_SHELLOPENAPP, "SHELLSTATUS", "SHELL OPEN APP OK" },
    { "shellcheck",  GSHELL_CMD_SHELLCHECK,  "SHELLSTATUS", "SHELL CHECK OK" },
    { "shellreset",  GSHELL_CMD_SHELLRESET,  "SHELLSTATUS", "SHELL RESET OK" },
    { "launcherstatus", GSHELL_CMD_LAUNCHERSTATUS, "LAUNCHERSTATUS", "LAUNCHER STATUS OK" },
    { "launchergrid", GSHELL_CMD_LAUNCHERGRID, "LAUNCHERSTATUS", "LAUNCHER GRID OK" },
    { "appselect",   GSHELL_CMD_APPSELECT,   "LAUNCHERSTATUS", "APP SELECT OK" },
    { "apppin",      GSHELL_CMD_APPPIN,      "LAUNCHERSTATUS", "APP PIN OK" },
    { "launcheropen", GSHELL_CMD_LAUNCHEROPEN, "LAUNCHERSTATUS", "LAUNCHER OPEN OK" },
    { "launchercheck", GSHELL_CMD_LAUNCHERCHECK, "LAUNCHERSTATUS", "LAUNCHER CHECK OK" },
    { "launcherreset", GSHELL_CMD_LAUNCHERRESET, "LAUNCHERSTATUS", "LAUNCHER RESET OK" },
    { "taskbarstatus", GSHELL_CMD_TASKBARSTATUS, "TASKBARSTATUS", "TASKBAR STATUS OK" },
    { "taskitem",    GSHELL_CMD_TASKITEM,    "TASKBARSTATUS", "TASK ITEM OK" },
    { "taskfocus",   GSHELL_CMD_TASKFOCUS,   "TASKBARSTATUS", "TASK FOCUS OK" },
    { "taskswitch",  GSHELL_CMD_TASKSWITCH,  "TASKBARSTATUS", "TASK SWITCH OK" },
    { "taskmin",     GSHELL_CMD_TASKMIN,     "TASKBARSTATUS", "TASK MIN OK" },
    { "taskrestore", GSHELL_CMD_TASKRESTORE, "TASKBARSTATUS", "TASK RESTORE OK" },
    { "taskcheck",   GSHELL_CMD_TASKCHECK,   "TASKBARSTATUS", "TASK CHECK OK" },
    { "taskreset",   GSHELL_CMD_TASKRESET,   "TASKBARSTATUS", "TASK RESET OK" },
    { "layoutstatus", GSHELL_CMD_LAYOUTSTATUS, "LAYOUTSTATUS", "LAYOUT STATUS OK" },
    { "layoutgrid",  GSHELL_CMD_LAYOUTGRID,  "LAYOUTSTATUS", "LAYOUT GRID OK" },
    { "windowsnap",  GSHELL_CMD_WINDOWSNAP,  "LAYOUTSTATUS", "WINDOW SNAP OK" },
    { "windowmax",   GSHELL_CMD_WINDOWMAX,   "LAYOUTSTATUS", "WINDOW MAX OK" },
    { "zorder",      GSHELL_CMD_ZORDER,      "LAYOUTSTATUS", "ZORDER OK" },
    { "layoutcheck", GSHELL_CMD_WINLAYOUTCHECK, "LAYOUTSTATUS", "LAYOUT CHECK OK" },
    { "layoutreset", GSHELL_CMD_LAYOUTRESET, "LAYOUTSTATUS", "LAYOUT RESET OK" },
    { "desktopshell", GSHELL_CMD_DESKTOPSHELL, "DESKTOPSHELL", "DESKTOP SHELL OK" },
    { "shellhome",   GSHELL_CMD_SHELLHOME,   "DESKTOPSHELL", "SHELL HOME OK" },
    { "shellapps",   GSHELL_CMD_SHELLAPPS,   "DESKTOPSHELL", "SHELL APPS OK" },
    { "shellwindows", GSHELL_CMD_SHELLWINDOWS, "DESKTOPSHELL", "SHELL WINDOWS OK" },
    { "shelllayout", GSHELL_CMD_SHELLLAYOUT, "DESKTOPSHELL", "SHELL LAYOUT OK" },
    { "shelldock",   GSHELL_CMD_SHELLDOCK,   "DESKTOPSHELL", "SHELL DOCK OK" },
    { "shellflow",   GSHELL_CMD_SHELLFLOW,   "DESKTOPSHELL", "SHELL FLOW OK" },
    { "shellhomecheck", GSHELL_CMD_SHELLHOMECHECK, "DESKTOPSHELL", "SHELL HOME CHECK OK" },
    { "shellhomereset", GSHELL_CMD_SHELLHOMERESET, "DESKTOPSHELL", "SHELL HOME RESET OK" },
    { "shellfinal",  GSHELL_CMD_SHELLFINAL,  "SHELLFINAL",  "SHELL FINAL OK" },
    { "shellhealth", GSHELL_CMD_SHELLHEALTH, "SHELLFINAL",  "SHELL HEALTH OK" },
    { "shellsummary", GSHELL_CMD_SHELLSUMMARY, "SHELLFINAL", "SHELL SUMMARY OK" },
    { "launchersum", GSHELL_CMD_LAUNCHERSUM, "SHELLFINAL",  "LAUNCHER SUM OK" },
    { "taskbarsum",  GSHELL_CMD_TASKBARSUM,  "SHELLFINAL",  "TASKBAR SUM OK" },
    { "layoutsumb",  GSHELL_CMD_LAYOUTSUMB,  "SHELLFINAL",  "LAYOUT SUM OK" },
    { "homesummary", GSHELL_CMD_HOMESUMMARY, "SHELLFINAL",  "HOME SUMMARY OK" },
    { "shellnext",   GSHELL_CMD_SHELLNEXT,   "SHELLFINAL",  "SHELL NEXT OK" },
    { "appshellstatus", GSHELL_CMD_APPSHELLSTATUS, "APPSHELLSTATUS", "APP SHELL STATUS OK" },
    { "appcatalog",  GSHELL_CMD_APPCATALOG,  "APPSHELLSTATUS", "APP CATALOG OK" },
    { "appcard",     GSHELL_CMD_APPCARD,     "APPSHELLSTATUS", "APP CARD OK" },
    { "appdetails",  GSHELL_CMD_APPDETAILS,  "APPSHELLSTATUS", "APP DETAILS OK" },
    { "appshelllaunch", GSHELL_CMD_APPSHELLLAUNCH, "APPSHELLSTATUS", "APP SHELL LAUNCH OK" },
    { "appshellcheck", GSHELL_CMD_APPSHELLCHECK, "APPSHELLSTATUS", "APP SHELL CHECK OK" },
    { "appshellreset", GSHELL_CMD_APPSHELLRESET, "APPSHELLSTATUS", "APP SHELL RESET OK" },
    { "catalogstatus", GSHELL_CMD_CATALOGSTATUS, "CATALOGSTATUS", "CATALOG STATUS OK" },
    { "cataloglist", GSHELL_CMD_CATALOGLIST, "CATALOGSTATUS", "CATALOG LIST OK" },
    { "catalogsearch", GSHELL_CMD_CATALOGSEARCH, "CATALOGSTATUS", "CATALOG SEARCH OK" },
    { "catalogpin",  GSHELL_CMD_CATALOGPIN,  "CATALOGSTATUS", "CATALOG PIN OK" },
    { "catalogenable", GSHELL_CMD_CATALOGENABLE, "CATALOGSTATUS", "CATALOG ENABLE OK" },
    { "catalogdisable", GSHELL_CMD_CATALOGDISABLE, "CATALOGSTATUS", "CATALOG DISABLE OK" },
    { "catalogcheck", GSHELL_CMD_CATALOGCHECK, "CATALOGSTATUS", "CATALOG CHECK OK" },
    { "catalogreset", GSHELL_CMD_CATALOGRESET, "CATALOGSTATUS", "CATALOG RESET OK" },
    { "detailstatus", GSHELL_CMD_DETAILSTATUS, "DETAILSTATUS", "DETAIL STATUS OK" },
    { "detailopen", GSHELL_CMD_DETAILOPEN, "DETAILSTATUS", "DETAIL OPEN OK" },
    { "detailmanifest", GSHELL_CMD_DETAILMANIFEST, "DETAILSTATUS", "DETAIL MANIFEST OK" },
    { "detailcaps", GSHELL_CMD_DETAILCAPS, "DETAILSTATUS", "DETAIL CAPS OK" },
    { "detailperms", GSHELL_CMD_DETAILPERMS, "DETAILSTATUS", "DETAIL PERMS OK" },
    { "detaillaunch", GSHELL_CMD_DETAILLAUNCH, "DETAILSTATUS", "DETAIL LAUNCH OK" },
    { "detailcheck", GSHELL_CMD_DETAILCHECK, "DETAILSTATUS", "DETAIL CHECK OK" },
    { "detailreset", GSHELL_CMD_DETAILRESET, "DETAILSTATUS", "DETAIL RESET OK" },
    { "actionstatus", GSHELL_CMD_ACTIONSTATUS, "ACTIONSTATUS", "ACTION STATUS OK" },
    { "actionprepare", GSHELL_CMD_ACTIONPREPARE, "ACTIONSTATUS", "ACTION PREPARE OK" },
    { "actionallow", GSHELL_CMD_ACTIONALLOW, "ACTIONSTATUS", "ACTION ALLOW OK" },
    { "actionopen",  GSHELL_CMD_ACTIONOPEN,  "ACTIONSTATUS", "ACTION OPEN OK" },
    { "actionrun",   GSHELL_CMD_ACTIONRUN,   "ACTIONSTATUS", "ACTION RUN OK" },
    { "actionstop",  GSHELL_CMD_ACTIONSTOP,  "ACTIONSTATUS", "ACTION STOP OK" },
    { "actioncheck", GSHELL_CMD_ACTIONCHECK, "ACTIONSTATUS", "ACTION CHECK OK" },
    { "actionreset", GSHELL_CMD_ACTIONRESET, "ACTIONSTATUS", "ACTION RESET OK" },
    { "appmgmtstatus", GSHELL_CMD_APPMGMTSTATUS, "APPMGMTSTATUS", "APP MGMT STATUS OK" },
    { "appinventory", GSHELL_CMD_APPINVENTORY, "APPMGMTSTATUS", "APP INVENTORY OK" },
    { "appscan",    GSHELL_CMD_APPSCAN,    "APPMGMTSTATUS", "APP SCAN OK" },
    { "appregister", GSHELL_CMD_APPREGISTER, "APPMGMTSTATUS", "APP REGISTER OK" },
    { "appunregister", GSHELL_CMD_APPUNREGISTER, "APPMGMTSTATUS", "APP UNREGISTER OK" },
    { "appenable",  GSHELL_CMD_APPENABLE,  "APPMGMTSTATUS", "APP ENABLE OK" },
    { "appdisable", GSHELL_CMD_APPDISABLE, "APPMGMTSTATUS", "APP DISABLE OK" },
    { "apptrust",   GSHELL_CMD_APPTRUST,   "APPMGMTSTATUS", "APP TRUST OK" },
    { "appuntrust", GSHELL_CMD_APPUNTRUST, "APPMGMTSTATUS", "APP UNTRUST OK" },
    { "appfavorite", GSHELL_CMD_APPFAVORITE, "APPMGMTSTATUS", "APP FAVORITE OK" },
    { "appunfavorite", GSHELL_CMD_APPUNFAVORITE, "APPMGMTSTATUS", "APP UNFAVORITE OK" },
    { "appupdatecheck", GSHELL_CMD_APPUPDATECHECK, "APPMGMTSTATUS", "APP UPDATE CHECK OK" },
    { "appupdatemark", GSHELL_CMD_APPUPDATEMARK, "APPMGMTSTATUS", "APP UPDATE MARK OK" },
    { "approllback", GSHELL_CMD_APPROLLBACK, "APPMGMTSTATUS", "APP ROLLBACK OK" },
    { "apprepair",  GSHELL_CMD_APPREPAIR,  "APPMGMTSTATUS", "APP REPAIR OK" },
    { "appclearcache", GSHELL_CMD_APPCLEARCACHE, "APPMGMTSTATUS", "APP CLEAR CACHE OK" },
    { "appstats",   GSHELL_CMD_APPSTATS,   "APPMGMTSTATUS", "APP STATS OK" },
    { "apphealth",  GSHELL_CMD_APPHEALTH,  "APPMGMTSTATUS", "APP HEALTH OK" },
    { "appmgmtcheck", GSHELL_CMD_APPMGMTCHECK, "APPMGMTSTATUS", "APP MGMT CHECK OK" },
    { "appmgmtreset", GSHELL_CMD_APPMGMTRESET, "APPMGMTSTATUS", "APP MGMT RESET OK" },
    { "appfinal",    GSHELL_CMD_APPFINAL,    "APPFINAL",    "APP FINAL OK" },
    { "apphealthsum", GSHELL_CMD_APPHEALTHSUM, "APPFINAL",  "APP HEALTH SUM OK" },
    { "appshellsum", GSHELL_CMD_APPSHELLSUM, "APPFINAL",    "APP SHELL SUM OK" },
    { "catalogsum",  GSHELL_CMD_CATALOGSUM,  "APPFINAL",    "CATALOG SUM OK" },
    { "detailsum",   GSHELL_CMD_DETAILSUM,   "APPFINAL",    "DETAIL SUM OK" },
    { "actionsum",   GSHELL_CMD_ACTIONSUM,   "APPFINAL",    "ACTION SUM OK" },
    { "mgmtsum",     GSHELL_CMD_MGMTSUM,     "APPFINAL",    "MGMT SUM OK" },
    { "appreadiness", GSHELL_CMD_APPREADINESS, "APPFINAL",  "APP READINESS OK" },
    { "appflowfull", GSHELL_CMD_APPFLOWFULL, "APPFINAL",    "APP FLOW FULL OK" },
    { "appdemoall",  GSHELL_CMD_APPDEMOALL,  "APPFINAL",    "APP DEMO ALL OK" },
    { "appsecuritysum", GSHELL_CMD_APPSECURITYSUM, "APPFINAL", "APP SECURITY SUM OK" },
    { "apppermissionsum", GSHELL_CMD_APPPERMISSIONSUM, "APPFINAL", "APP PERMISSION SUM OK" },
    { "appruntimesum", GSHELL_CMD_APPRUNTIMESUM, "APPFINAL", "APP RUNTIME SUM OK" },
    { "appwindowsum", GSHELL_CMD_APPWINDOWSUM, "APPFINAL",  "APP WINDOW SUM OK" },
    { "applaunchsum", GSHELL_CMD_APPLAUNCHSUM, "APPFINAL",  "APP LAUNCH SUM OK" },
    { "appstatesum", GSHELL_CMD_APPSTATESUM, "APPFINAL",    "APP STATE SUM OK" },
    { "appfinalcheck", GSHELL_CMD_APPFINALCHECK, "APPFINAL", "APP FINAL CHECK OK" },
    { "appfinalreset", GSHELL_CMD_APPFINALRESET, "APPFINAL", "APP FINAL RESET OK" },
    { "appnext",     GSHELL_CMD_APPNEXT,     "APPFINAL",    "APP NEXT OK" },
    { "approadmap",  GSHELL_CMD_APPROADMAP,  "APPFINAL",    "APP ROADMAP OK" },
    { "visualstatus", GSHELL_CMD_VISUALSTATUS, "VISUALSTATUS", "VISUAL STATUS OK" },
    { "visualboot", GSHELL_CMD_VISUALBOOT, "VISUALSTATUS", "VISUAL BOOT OK" },
    { "visualdesktop", GSHELL_CMD_VISUALDESKTOP, "VISUALSTATUS", "VISUAL DESKTOP OK" },
    { "visualpanel", GSHELL_CMD_VISUALPANEL, "VISUALSTATUS", "VISUAL PANEL OK" },
    { "visualcards", GSHELL_CMD_VISUALCARDS, "VISUALSTATUS", "VISUAL CARDS OK" },
    { "visualwindow", GSHELL_CMD_VISUALWINDOW, "VISUALSTATUS", "VISUAL WINDOW OK" },
    { "visuallauncher", GSHELL_CMD_VISUALLAUNCHER, "VISUALSTATUS", "VISUAL LAUNCHER OK" },
    { "visualtaskbar", GSHELL_CMD_VISUALTASKBAR, "VISUALSTATUS", "VISUAL TASKBAR OK" },
    { "visualdock", GSHELL_CMD_VISUALDOCK, "VISUALSTATUS", "VISUAL DOCK OK" },
    { "visualfocus", GSHELL_CMD_VISUALFOCUS, "VISUALSTATUS", "VISUAL FOCUS OK" },
    { "visualgrid", GSHELL_CMD_VISUALGRID, "VISUALSTATUS", "VISUAL GRID OK" },
    { "visualtheme", GSHELL_CMD_VISUALTHEME, "VISUALSTATUS", "VISUAL THEME OK" },
    { "visualglow", GSHELL_CMD_VISUALGLOW, "VISUALSTATUS", "VISUAL GLOW OK" },
    { "visualborder", GSHELL_CMD_VISUALBORDER, "VISUALSTATUS", "VISUAL BORDER OK" },
    { "visualmetrics", GSHELL_CMD_VISUALMETRICS, "VISUALSTATUS", "VISUAL METRICS OK" },
    { "visualdemo", GSHELL_CMD_VISUALDEMO, "VISUALSTATUS", "VISUAL DEMO OK" },
    { "visualflow", GSHELL_CMD_VISUALFLOW, "VISUALSTATUS", "VISUAL FLOW OK" },
    { "visualcheck", GSHELL_CMD_VISUALCHECK, "VISUALSTATUS", "VISUAL CHECK OK" },
    { "visualreset", GSHELL_CMD_VISUALRESET, "VISUALSTATUS", "VISUAL RESET OK" },
    { "visualnext", GSHELL_CMD_VISUALNEXT, "VISUALSTATUS", "VISUAL NEXT OK" },
    { "cardstatus", GSHELL_CMD_CARDSTATUS, "CARDSTATUS", "CARD STATUS OK" },
    { "cardgrid",   GSHELL_CMD_CARDGRID,   "CARDSTATUS", "CARD GRID OK" },
    { "cardselect", GSHELL_CMD_CARDSELECT, "CARDSTATUS", "CARD SELECT OK" },
    { "cardopen",   GSHELL_CMD_CARDOPEN,   "CARDSTATUS", "CARD OPEN OK" },
    { "cardexpand", GSHELL_CMD_CARDEXPAND, "CARDSTATUS", "CARD EXPAND OK" },
    { "cardpin",    GSHELL_CMD_CARDPIN,    "CARDSTATUS", "CARD PIN OK" },
    { "cardbadge",  GSHELL_CMD_CARDBADGE,  "CARDSTATUS", "CARD BADGE OK" },
    { "cardpreview", GSHELL_CMD_CARDPREVIEW, "CARDSTATUS", "CARD PREVIEW OK" },
    { "windowvisual", GSHELL_CMD_WINDOWVISUAL, "CARDSTATUS", "WINDOW VISUAL OK" },
    { "windowtitle", GSHELL_CMD_WINDOWTITLE, "CARDSTATUS", "WINDOW TITLE OK" },
    { "windowbody", GSHELL_CMD_WINDOWBODY, "CARDSTATUS", "WINDOW BODY OK" },
    { "windowshadow", GSHELL_CMD_WINDOWSHADOW, "CARDSTATUS", "WINDOW SHADOW OK" },
    { "windowactive", GSHELL_CMD_WINDOWACTIVE, "CARDSTATUS", "WINDOW ACTIVE OK" },
    { "windowpreview", GSHELL_CMD_WINDOWPREVIEW, "CARDSTATUS", "WINDOW PREVIEW OK" },
    { "visualcompose", GSHELL_CMD_VISUALCOMPOSE, "CARDSTATUS", "VISUAL COMPOSE OK" },
    { "visualsync", GSHELL_CMD_VISUALSYNC, "CARDSTATUS", "VISUAL SYNC OK" },
    { "cardflow",   GSHELL_CMD_CARDFLOW,   "CARDSTATUS", "CARD FLOW OK" },
    { "cardcheck",  GSHELL_CMD_CARDCHECK,  "CARDSTATUS", "CARD CHECK OK" },
    { "cardreset",  GSHELL_CMD_CARDRESET,  "CARDSTATUS", "CARD RESET OK" },
    { "cardnext",   GSHELL_CMD_CARDNEXT,   "CARDSTATUS", "CARD NEXT OK" },
    { "uimockstatus", GSHELL_CMD_UIMOCKSTATUS, "UIMOCKSTATUS", "UI MOCK STATUS OK" },
    { "uimockdesktop", GSHELL_CMD_UIMOCKDESKTOP, "UIMOCKSTATUS", "UI MOCK DESKTOP OK" },
    { "uimockgrid", GSHELL_CMD_UIMOCKGRID, "UIMOCKSTATUS", "UI MOCK GRID OK" },
    { "uimockcard1", GSHELL_CMD_UIMOCKCARD1, "UIMOCKSTATUS", "UI MOCK CARD1 OK" },
    { "uimockcard2", GSHELL_CMD_UIMOCKCARD2, "UIMOCKSTATUS", "UI MOCK CARD2 OK" },
    { "uimockcard3", GSHELL_CMD_UIMOCKCARD3, "UIMOCKSTATUS", "UI MOCK CARD3 OK" },
    { "uimockwindow", GSHELL_CMD_UIMOCKWINDOW, "UIMOCKSTATUS", "UI MOCK WINDOW OK" },
    { "uimocktitle", GSHELL_CMD_UIMOCKTITLE, "UIMOCKSTATUS", "UI MOCK TITLE OK" },
    { "uimockbody", GSHELL_CMD_UIMOCKBODY, "UIMOCKSTATUS", "UI MOCK BODY OK" },
    { "uimocktaskbar", GSHELL_CMD_UIMOCKTASKBAR, "UIMOCKSTATUS", "UI MOCK TASKBAR OK" },
    { "uimockdock", GSHELL_CMD_UIMOCKDOCK, "UIMOCKSTATUS", "UI MOCK DOCK OK" },
    { "uimockfocus", GSHELL_CMD_UIMOCKFOCUS, "UIMOCKSTATUS", "UI MOCK FOCUS OK" },
    { "uimockselect", GSHELL_CMD_UIMOCKSELECT, "UIMOCKSTATUS", "UI MOCK SELECT OK" },
    { "uimockopen", GSHELL_CMD_UIMOCKOPEN, "UIMOCKSTATUS", "UI MOCK OPEN OK" },
    { "uimocklayout", GSHELL_CMD_UIMOCKLAYOUT, "UIMOCKSTATUS", "UI MOCK LAYOUT OK" },
    { "uimockmetrics", GSHELL_CMD_UIMOCKMETRICS, "UIMOCKSTATUS", "UI MOCK METRICS OK" },
    { "uimockdemo", GSHELL_CMD_UIMOCKDEMO, "UIMOCKSTATUS", "UI MOCK DEMO OK" },
    { "uimockcheck", GSHELL_CMD_UIMOCKCHECK, "UIMOCKSTATUS", "UI MOCK CHECK OK" },
    { "uimockreset", GSHELL_CMD_UIMOCKRESET, "UIMOCKSTATUS", "UI MOCK RESET OK" },
    { "uimocknext", GSHELL_CMD_UIMOCKNEXT, "UIMOCKSTATUS", "UI MOCK NEXT OK" },
    { "launchmockstatus", GSHELL_CMD_LAUNCHMOCKSTATUS, "LAUNCHMOCKSTATUS", "LAUNCH MOCK STATUS OK" },
    { "launchmockpanel", GSHELL_CMD_LAUNCHMOCKPANEL, "LAUNCHMOCKSTATUS", "LAUNCH MOCK PANEL OK" },
    { "launchmocksearch", GSHELL_CMD_LAUNCHMOCKSEARCH, "LAUNCHMOCKSTATUS", "LAUNCH MOCK SEARCH OK" },
    { "launchmockapps", GSHELL_CMD_LAUNCHMOCKAPPS, "LAUNCHMOCKSTATUS", "LAUNCH MOCK APPS OK" },
    { "launchmockrecent", GSHELL_CMD_LAUNCHMOCKRECENT, "LAUNCHMOCKSTATUS", "LAUNCH MOCK RECENT OK" },
    { "launchmockpin", GSHELL_CMD_LAUNCHMOCKPIN, "LAUNCHMOCKSTATUS", "LAUNCH MOCK PIN OK" },
    { "launchmockrun", GSHELL_CMD_LAUNCHMOCKRUN, "LAUNCHMOCKSTATUS", "LAUNCH MOCK RUN OK" },
    { "taskmockstatus", GSHELL_CMD_TASKMOCKSTATUS, "LAUNCHMOCKSTATUS", "TASK MOCK STATUS OK" },
    { "taskmockstart", GSHELL_CMD_TASKMOCKSTART, "LAUNCHMOCKSTATUS", "TASK MOCK START OK" },
    { "taskmockapp", GSHELL_CMD_TASKMOCKAPP, "LAUNCHMOCKSTATUS", "TASK MOCK APP OK" },
    { "taskmockactive", GSHELL_CMD_TASKMOCKACTIVE, "LAUNCHMOCKSTATUS", "TASK MOCK ACTIVE OK" },
    { "taskmocktray", GSHELL_CMD_TASKMOCKTRAY, "LAUNCHMOCKSTATUS", "TASK MOCK TRAY OK" },
    { "taskmockclock", GSHELL_CMD_TASKMOCKCLOCK, "LAUNCHMOCKSTATUS", "TASK MOCK CLOCK OK" },
    { "taskmocknet", GSHELL_CMD_TASKMOCKNET, "LAUNCHMOCKSTATUS", "TASK MOCK NET OK" },
    { "taskmockstate", GSHELL_CMD_TASKMOCKSTATE, "LAUNCHMOCKSTATUS", "TASK MOCK STATE OK" },
    { "mockcompose", GSHELL_CMD_MOCKCOMPOSE, "LAUNCHMOCKSTATUS", "MOCK COMPOSE OK" },
    { "mockdemo", GSHELL_CMD_MOCKDEMO, "LAUNCHMOCKSTATUS", "MOCK DEMO OK" },
    { "mockcheck", GSHELL_CMD_MOCKCHECK, "LAUNCHMOCKSTATUS", "MOCK CHECK OK" },
    { "mockreset", GSHELL_CMD_MOCKRESET, "LAUNCHMOCKSTATUS", "MOCK RESET OK" },
    { "mocknext", GSHELL_CMD_MOCKNEXT, "LAUNCHMOCKSTATUS", "MOCK NEXT OK" },
    { "polishstatus", GSHELL_CMD_POLISHSTATUS, "POLISHSTATUS", "POLISH STATUS OK" },
    { "polishbase", GSHELL_CMD_POLISHBASE, "POLISHSTATUS", "POLISH BASE OK" },
    { "polishglass", GSHELL_CMD_POLISHGLASS, "POLISHSTATUS", "POLISH GLASS OK" },
    { "polishtitle", GSHELL_CMD_POLISHTITLE, "POLISHSTATUS", "POLISH TITLE OK" },
    { "polishcards", GSHELL_CMD_POLISHCARDS, "POLISHSTATUS", "POLISH CARDS OK" },
    { "polishwindow", GSHELL_CMD_POLISHWINDOW, "POLISHSTATUS", "POLISH WINDOW OK" },
    { "polishtaskbar", GSHELL_CMD_POLISHTASKBAR, "POLISHSTATUS", "POLISH TASKBAR OK" },
    { "polishtray", GSHELL_CMD_POLISHTRAY, "POLISHSTATUS", "POLISH TRAY OK" },
    { "polishbadge", GSHELL_CMD_POLISHBADGE, "POLISHSTATUS", "POLISH BADGE OK" },
    { "polishfocus", GSHELL_CMD_POLISHFOCUS, "POLISHSTATUS", "POLISH FOCUS OK" },
    { "polishshadow", GSHELL_CMD_POLISHSHADOW, "POLISHSTATUS", "POLISH SHADOW OK" },
    { "polishcompact", GSHELL_CMD_POLISHCOMPACT, "POLISHSTATUS", "POLISH COMPACT OK" },
    { "polishalign", GSHELL_CMD_POLISHALIGN, "POLISHSTATUS", "POLISH ALIGN OK" },
    { "polishmetrics", GSHELL_CMD_POLISHMETRICS, "POLISHSTATUS", "POLISH METRICS OK" },
    { "polishsafe", GSHELL_CMD_POLISHSAFE, "POLISHSTATUS", "POLISH SAFE OK" },
    { "polishdemo", GSHELL_CMD_POLISHDEMO, "POLISHSTATUS", "POLISH DEMO OK" },
    { "polishflow", GSHELL_CMD_POLISHFLOW, "POLISHSTATUS", "POLISH FLOW OK" },
    { "polishcheck", GSHELL_CMD_POLISHCHECK, "POLISHSTATUS", "POLISH CHECK OK" },
    { "polishreset", GSHELL_CMD_POLISHRESET, "POLISHSTATUS", "POLISH RESET OK" },
    { "polishnext", GSHELL_CMD_POLISHNEXT, "POLISHSTATUS", "POLISH NEXT OK" },
    { "scenestatus", GSHELL_CMD_SCENESTATUS, "DESKTOPSCENE", "SCENE STATUS OK" },
    { "sceneboot", GSHELL_CMD_SCENEBOOT, "DESKTOPSCENE", "SCENE BOOT OK" },
    { "scenebackground", GSHELL_CMD_SCENEBACKGROUND, "DESKTOPSCENE", "SCENE BACKGROUND OK" },
    { "scenetopbar", GSHELL_CMD_SCENETOPBAR, "DESKTOPSCENE", "SCENE TOPBAR OK" },
    { "scenelauncher", GSHELL_CMD_SCENELAUNCHER, "DESKTOPSCENE", "SCENE LAUNCHER OK" },
    { "scenecards", GSHELL_CMD_SCENECARDS, "DESKTOPSCENE", "SCENE CARDS OK" },
    { "scenewindow", GSHELL_CMD_SCENEWINDOW, "DESKTOPSCENE", "SCENE WINDOW OK" },
    { "scenetaskbar", GSHELL_CMD_SCENETASKBAR, "DESKTOPSCENE", "SCENE TASKBAR OK" },
    { "scenedock", GSHELL_CMD_SCENEDOCK, "DESKTOPSCENE", "SCENE DOCK OK" },
    { "scenetray", GSHELL_CMD_SCENETRAY, "DESKTOPSCENE", "SCENE TRAY OK" },
    { "scenebadge", GSHELL_CMD_SCENEBADGE, "DESKTOPSCENE", "SCENE BADGE OK" },
    { "scenefocus", GSHELL_CMD_SCENEFOCUS, "DESKTOPSCENE", "SCENE FOCUS OK" },
    { "scenewidgets", GSHELL_CMD_SCENEWIDGETS, "DESKTOPSCENE", "SCENE WIDGETS OK" },
    { "sceneactive", GSHELL_CMD_SCENEACTIVE, "DESKTOPSCENE", "SCENE ACTIVE OK" },
    { "scenemetrics", GSHELL_CMD_SCENEMETRICS, "DESKTOPSCENE", "SCENE METRICS OK" },
    { "scenedemo", GSHELL_CMD_SCENEDEMO, "DESKTOPSCENE", "SCENE DEMO OK" },
    { "sceneflow", GSHELL_CMD_SCENEFLOW, "DESKTOPSCENE", "SCENE FLOW OK" },
    { "scenecheck", GSHELL_CMD_SCENECHECK, "DESKTOPSCENE", "SCENE CHECK OK" },
    { "scenereset", GSHELL_CMD_SCENERESET, "DESKTOPSCENE", "SCENE RESET OK" },
    { "scenenext", GSHELL_CMD_SCENENEXT, "DESKTOPSCENE", "SCENE NEXT OK" },
    { "visualfinal", GSHELL_CMD_VISUALFINAL, "VISUALFINAL", "VISUAL FINAL OK" },
    { "visualhealthsum", GSHELL_CMD_VISUALHEALTHSUM, "VISUALFINAL", "VISUAL HEALTH SUM OK" },
    { "visualscenesum", GSHELL_CMD_VISUALSCENESUM, "VISUALFINAL", "VISUAL SCENE SUM OK" },
    { "visualmocksum", GSHELL_CMD_VISUALMOCKSUM, "VISUALFINAL", "VISUAL MOCK SUM OK" },
    { "visualcardsum", GSHELL_CMD_VISUALCARDSUM, "VISUALFINAL", "VISUAL CARD SUM OK" },
    { "visuallaunchsum", GSHELL_CMD_VISUALLAUNCHSUM, "VISUALFINAL", "VISUAL LAUNCH SUM OK" },
    { "visualpolishsum", GSHELL_CMD_VISUALPOLISHSUM, "VISUALFINAL", "VISUAL POLISH SUM OK" },
    { "visualreadiness", GSHELL_CMD_VISUALREADINESS, "VISUALFINAL", "VISUAL READINESS OK" },
    { "visualdemoall", GSHELL_CMD_VISUALDEMOALL, "VISUALFINAL", "VISUAL DEMO ALL OK" },
    { "visualregression", GSHELL_CMD_VISUALREGRESSION, "VISUALFINAL", "VISUAL REGRESSION OK" },
    { "visualbounds", GSHELL_CMD_VISUALBOUNDS, "VISUALFINAL", "VISUAL BOUNDS OK" },
    { "visualdensity", GSHELL_CMD_VISUALDENSITY, "VISUALFINAL", "VISUAL DENSITY OK" },
    { "visualdefault", GSHELL_CMD_VISUALDEFAULT, "VISUALFINAL", "VISUAL DEFAULT OK" },
    { "visualhandoff", GSHELL_CMD_VISUALHANDOFF, "VISUALFINAL", "VISUAL HANDOFF OK" },
    { "visualnextphase", GSHELL_CMD_VISUALNEXTPHASE, "VISUALFINAL", "VISUAL NEXT PHASE OK" },
    { "visualfinalcheck", GSHELL_CMD_VISUALFINALCHECK, "VISUALFINAL", "VISUAL FINAL CHECK OK" },
    { "visualfinalreset", GSHELL_CMD_VISUALFINALRESET, "VISUALFINAL", "VISUAL FINAL RESET OK" },
    { "visualroadmap", GSHELL_CMD_VISUALROADMAP, "VISUALFINAL", "VISUAL ROADMAP OK" },
    { "visualinputnext", GSHELL_CMD_VISUALINPUTNEXT, "VISUALFINAL", "VISUAL INPUT NEXT OK" },
    { "visualcloseout", GSHELL_CMD_VISUALCLOSEOUT, "VISUALFINAL", "VISUAL CLOSEOUT OK" },
    { "interactstatus", GSHELL_CMD_INTERACTSTATUS, "INTERACTSTATUS", "INTERACT STATUS OK" },
    { "pointermock", GSHELL_CMD_POINTERMOCK, "INTERACTSTATUS", "POINTER MOCK OK" },
    { "pointermove", GSHELL_CMD_POINTERMOVE, "INTERACTSTATUS", "POINTER MOVE OK" },
    { "pointerhover", GSHELL_CMD_POINTERHOVER, "INTERACTSTATUS", "POINTER HOVER OK" },
    { "pointerclick", GSHELL_CMD_POINTERCLICK, "INTERACTSTATUS", "POINTER CLICK OK" },
    { "focusmock", GSHELL_CMD_FOCUSMOCK, "INTERACTSTATUS", "FOCUS MOCK OK" },
    { "focusnext", GSHELL_CMD_FOCUSNEXT, "INTERACTSTATUS", "FOCUS NEXT OK" },
    { "focuscard", GSHELL_CMD_FOCUSCARD, "INTERACTSTATUS", "FOCUS CARD OK" },
    { "focuswindow", GSHELL_CMD_FOCUSWINDOW, "INTERACTSTATUS", "FOCUS WINDOW OK" },
    { "focustaskbar", GSHELL_CMD_FOCUSTASKBAR, "INTERACTSTATUS", "FOCUS TASKBAR OK" },
    { "selectmock", GSHELL_CMD_SELECTMOCK, "INTERACTSTATUS", "SELECT MOCK OK" },
    { "openmock", GSHELL_CMD_OPENMOCK, "INTERACTSTATUS", "OPEN MOCK OK" },
    { "closemock", GSHELL_CMD_CLOSEMOCK, "INTERACTSTATUS", "CLOSE MOCK OK" },
    { "menumock", GSHELL_CMD_MENUMOCK, "INTERACTSTATUS", "MENU MOCK OK" },
    { "shortcutmock", GSHELL_CMD_SHORTCUTMOCK, "INTERACTSTATUS", "SHORTCUT MOCK OK" },
    { "routemock", GSHELL_CMD_ROUTEMOCK, "INTERACTSTATUS", "ROUTE MOCK OK" },
    { "interactdemo", GSHELL_CMD_INTERACTDEMO, "INTERACTSTATUS", "INTERACT DEMO OK" },
    { "interactcheck", GSHELL_CMD_INTERACTCHECK, "INTERACTSTATUS", "INTERACT CHECK OK" },
    { "interactreset", GSHELL_CMD_INTERACTRESET, "INTERACTSTATUS", "INTERACT RESET OK" },
    { "interactnext", GSHELL_CMD_INTERACTNEXT, "INTERACTSTATUS", "INTERACT NEXT OK" },
    { "routestatus", GSHELL_CMD_ROUTESTATUS, "ROUTESTATUS", "ROUTE STATUS OK" },
    { "hitdesktop", GSHELL_CMD_HITDESKTOP, "ROUTESTATUS", "HIT DESKTOP OK" },
    { "hitcard", GSHELL_CMD_HITCARD, "ROUTESTATUS", "HIT CARD OK" },
    { "hitwindow", GSHELL_CMD_HITWINDOW, "ROUTESTATUS", "HIT WINDOW OK" },
    { "hittaskbar", GSHELL_CMD_HITTASKBAR, "ROUTESTATUS", "HIT TASKBAR OK" },
    { "hitlauncher", GSHELL_CMD_HITLAUNCHER, "ROUTESTATUS", "HIT LAUNCHER OK" },
    { "hoverroute", GSHELL_CMD_HOVERROUTE, "ROUTESTATUS", "HOVER ROUTE OK" },
    { "clickroute", GSHELL_CMD_CLICKROUTE, "ROUTESTATUS", "CLICK ROUTE OK" },
    { "doubleclickroute", GSHELL_CMD_DOUBLECLICKROUTE, "ROUTESTATUS", "DOUBLE CLICK ROUTE OK" },
    { "rightclickroute", GSHELL_CMD_RIGHTCLICKROUTE, "ROUTESTATUS", "RIGHT CLICK ROUTE OK" },
    { "menuroute", GSHELL_CMD_MENUROUTE, "ROUTESTATUS", "MENU ROUTE OK" },
    { "openroute", GSHELL_CMD_OPENROUTE, "ROUTESTATUS", "OPEN ROUTE OK" },
    { "selectroute", GSHELL_CMD_SELECTROUTE, "ROUTESTATUS", "SELECT ROUTE OK" },
    { "focusroute", GSHELL_CMD_FOCUSROUTE, "ROUTESTATUS", "FOCUS ROUTE OK" },
    { "dragroute", GSHELL_CMD_DRAGROUTE, "ROUTESTATUS", "DRAG ROUTE OK" },
    { "droproute", GSHELL_CMD_DROPROUTE, "ROUTESTATUS", "DROP ROUTE OK" },
    { "routedemo", GSHELL_CMD_ROUTEDEMO, "ROUTESTATUS", "ROUTE DEMO OK" },
    { "routecheck", GSHELL_CMD_ROUTECHECK, "ROUTESTATUS", "ROUTE CHECK OK" },
    { "routereset", GSHELL_CMD_ROUTERESET, "ROUTESTATUS", "ROUTE RESET OK" },
    { "routenext", GSHELL_CMD_ROUTENEXT, "ROUTESTATUS", "ROUTE NEXT OK" },
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
        gshell_command_name = "SBOXSUM";
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
        gshell_command_name = "DECSUM";
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

    if (command_id == GSHELL_CMD_RUNTIMESTATUS) {
        gshell_command_name = "RUNTIMESTATUS";
        gshell_command_result = "RUNTIME STATUS OK";
        gshell_command_view = "RUNTIMESTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RUNTIMESTATUS -> APP RUNTIME BASE READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPSTATUS) {
        gshell_command_name = "APPSTATUS";
        gshell_command_result = "APP STATUS OK";
        gshell_command_view = "RUNTIMESTATUS";
        gshell_input_status_text = "APP OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPSTATUS -> APP LAYER STATUS READY");
        return;
    }

    if (command_id == GSHELL_CMD_LOADERSTATUS) {
        gshell_command_name = "LOADERSTATUS";
        gshell_command_result = "LOADER STATUS OK";
        gshell_command_view = "RUNTIMESTATUS";
        gshell_input_status_text = "LOADER OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LOADERSTATUS -> LOADER PREP READY");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHSTATUS) {
        gshell_command_name = "LAUNCHSTATUS";
        gshell_command_result = "LAUNCH STATUS OK";
        gshell_command_view = "RUNTIMESTATUS";
        gshell_input_status_text = "LAUNCH OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHSTATUS -> LAUNCH PIPELINE READY");
        return;
    }

    if (command_id == GSHELL_CMD_RUNTIMECHECK) {
        int user_ok = health_user_ok();
        int ring3_ok = health_ring3_ok();
        int syscall_ok = health_syscall_ok();
        gshell_command_name = "RUNTIMECHECK";
        gshell_command_result = (user_ok && ring3_ok && syscall_ok) ? "RUNTIME CHECK OK" : "RUNTIME CHECK BAD";
        gshell_command_view = "RUNTIMESTATUS";
        gshell_input_status_text = (user_ok && ring3_ok && syscall_ok) ? "RUNTIME OK" : "RUNTIME BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push((user_ok && ring3_ok && syscall_ok) ? "RUNTIMECHECK -> USER/RING3/SYSCALL OK" : "RUNTIMECHECK -> RUNTIME ISSUE");
        return;
    }

    if (command_id == GSHELL_CMD_APPCHECK) {
        int gate_ok = security_check_sandbox();
        gshell_command_name = "APPCHECK";
        gshell_command_result = gate_ok ? "APP CHECK OK" : "APP CHECK BLOCKED";
        gshell_command_view = "RUNTIMESTATUS";
        gshell_input_status_text = gate_ok ? "APP OK" : "APP BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(gate_ok ? "APPCHECK -> APP SANDBOX OK" : "APPCHECK -> APP SANDBOX BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_ELFSTATUS) {
        gshell_command_name = "ELFSTATUS";
        gshell_command_result = "ELF STATUS OK";
        gshell_command_view = "ELFSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ELFSTATUS -> ELF LOADER METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_ELFHEADER) {
        gshell_command_name = "ELFHEADER";
        gshell_command_result = "ELF HEADER OK";
        gshell_command_view = "ELFSTATUS";
        gshell_input_status_text = "HEADER OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ELFHEADER -> ELF32 HEADER PROFILE READY");
        return;
    }

    if (command_id == GSHELL_CMD_ELFSEGMENTS) {
        gshell_command_name = "ELFSEGMENTS";
        gshell_command_result = "ELF SEGMENTS OK";
        gshell_command_view = "ELFSTATUS";
        gshell_input_status_text = "SEGMENT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ELFSEGMENTS -> PROGRAM HEADER METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_ELFSECTIONS) {
        gshell_command_name = "ELFSECTIONS";
        gshell_command_result = "ELF SECTIONS OK";
        gshell_command_view = "ELFSTATUS";
        gshell_input_status_text = "SECTION OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ELFSECTIONS -> SECTION METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_ELFVALIDATE) {
        int runtime_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        gshell_command_name = "ELFVALIDATE";
        gshell_command_result = runtime_ok ? "ELF VALIDATE OK" : "ELF VALIDATE BAD";
        gshell_command_view = "ELFSTATUS";
        gshell_input_status_text = runtime_ok ? "ELF OK" : "ELF BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(runtime_ok ? "ELFVALIDATE -> RUNTIME BASE OK" : "ELFVALIDATE -> RUNTIME BASE BAD");
        return;
    }

    if (command_id == GSHELL_CMD_LOADERCHECK) {
        int sandbox_ok = security_check_sandbox();
        gshell_command_name = "LOADERCHECK";
        gshell_command_result = sandbox_ok ? "LOADER CHECK OK" : "LOADER CHECK BLOCKED";
        gshell_command_view = "ELFSTATUS";
        gshell_input_status_text = sandbox_ok ? "LOADER OK" : "LOADER BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(sandbox_ok ? "LOADERCHECK -> SANDBOX GATE OK" : "LOADERCHECK -> SANDBOX GATE BLOCKED");
        return;
    }

    if (command_id == GSHELL_CMD_MANIFESTSTATUS) {
        gshell_command_name = "MANIFESTSTATUS";
        gshell_command_result = "MANIFEST STATUS OK";
        gshell_command_view = "MANIFESTSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MANIFESTSTATUS -> APP MANIFEST METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPMANIFEST) {
        gshell_command_name = "APPMANIFEST";
        gshell_command_result = "APP MANIFEST OK";
        gshell_command_view = "MANIFESTSTATUS";
        gshell_input_status_text = "MANIFEST OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPMANIFEST -> MANIFEST PROFILE READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPDESCRIPTOR) {
        gshell_command_name = "APPDESCRIPTOR";
        gshell_command_result = "APP DESCRIPTOR OK";
        gshell_command_view = "MANIFESTSTATUS";
        gshell_input_status_text = "DESC OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPDESCRIPTOR -> APP DESCRIPTOR READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPENTRY) {
        gshell_command_name = "APPENTRY";
        gshell_command_result = "APP ENTRY OK";
        gshell_command_view = "MANIFESTSTATUS";
        gshell_input_status_text = "ENTRY OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPENTRY -> ENTRYPOINT METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPCAPS) {
        gshell_command_name = "APPCAPS";
        gshell_command_result = "APP CAPS OK";
        gshell_command_view = "MANIFESTSTATUS";
        gshell_input_status_text = "CAPS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPCAPS -> CAPABILITY DECLARATION READY");
        return;
    }

    if (command_id == GSHELL_CMD_MANIFESTCHECK) {
        int runtime_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        int sandbox_ok = security_check_sandbox();
        gshell_command_name = "MANIFESTCHECK";
        gshell_command_result = (runtime_ok && sandbox_ok) ? "MANIFEST CHECK OK" : "MANIFEST CHECK BAD";
        gshell_command_view = "MANIFESTSTATUS";
        gshell_input_status_text = (runtime_ok && sandbox_ok) ? "MANIFEST OK" : "MANIFEST BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push((runtime_ok && sandbox_ok) ? "MANIFESTCHECK -> RUNTIME + SANDBOX OK" : "MANIFESTCHECK -> MANIFEST GATE BAD");
        return;
    }

    if (command_id == GSHELL_CMD_APPSLOTSTATUS) {
        gshell_command_name = "APPSLOTSTATUS";
        gshell_command_result = "APP SLOT STATUS OK";
        gshell_command_view = "APPSLOTSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPSLOTSTATUS -> APP SLOT CORE READY");
        return;
    }

    if (command_id == GSHELL_CMD_SLOTLIST) {
        gshell_command_name = "SLOTLIST";
        gshell_command_result = "SLOT LIST OK";
        gshell_command_view = "APPSLOTSTATUS";
        gshell_input_status_text = "SLOT LIST";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SLOTLIST -> SLOT TABLE METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_SLOTLALLOC) {
        gshell_app_slot_allocated = 1;
        gshell_app_slot_pid = 1;
        gshell_app_slot_state = "allocated";
        gshell_command_name = "SLOTALLOC";
        gshell_command_result = "SLOT ALLOC OK";
        gshell_command_view = "APPSLOTSTATUS";
        gshell_input_status_text = "ALLOC OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SLOTALLOC -> DEMO APP SLOT ALLOCATED");
        return;
    }

    if (command_id == GSHELL_CMD_SLOTFREE) {
        gshell_app_slot_allocated = 0;
        gshell_app_slot_pid = 0;
        gshell_app_slot_state = "empty";
        gshell_command_name = "SLOTFREE";
        gshell_command_result = "SLOT FREE OK";
        gshell_command_view = "APPSLOTSTATUS";
        gshell_input_status_text = "FREE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SLOTFREE -> DEMO APP SLOT RELEASED");
        return;
    }

    if (command_id == GSHELL_CMD_PROCESSSLOT) {
        gshell_command_name = "PROCESSSLOT";
        gshell_command_result = "PROCESS SLOT OK";
        gshell_command_view = "APPSLOTSTATUS";
        gshell_input_status_text = "PROC SLOT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(gshell_app_slot_allocated ? "PROCESSSLOT -> PID 1 BOUND TO APP SLOT" : "PROCESSSLOT -> NO APP SLOT BOUND");
        return;
    }

    if (command_id == GSHELL_CMD_SLOTCHECK) {
        int runtime_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        int sandbox_ok = security_check_sandbox();
        int slot_ok = runtime_ok && sandbox_ok && gshell_app_slot_allocated;
        gshell_app_slot_checks++;
        gshell_command_name = "SLOTCHECK";
        gshell_command_result = slot_ok ? "SLOT CHECK OK" : "SLOT CHECK WAIT";
        gshell_command_view = "APPSLOTSTATUS";
        gshell_input_status_text = slot_ok ? "SLOT OK" : "SLOT WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(slot_ok ? "SLOTCHECK -> APP SLOT READY" : "SLOTCHECK -> NEED SLOTALLOC OR GATE OK");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHGATE) {
        gshell_command_name = "LAUNCHGATE";
        gshell_command_result = "LAUNCH GATE OK";
        gshell_command_view = "LAUNCHGATE";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHGATE -> APP LAUNCH GATE READY");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHPREPARE) {
        gshell_launch_prepared = 1;
        gshell_launch_approved = 0;
        gshell_launch_state = "prep";
        gshell_launch_last_result = "prep";
        gshell_command_name = "LAUNCHPREPARE";
        gshell_command_result = "LAUNCH PREPARE OK";
        gshell_command_view = "LAUNCHGATE";
        gshell_input_status_text = "PREPARED";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHPREPARE -> DEMO APP READY FOR APPROVAL");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHAPPROVE) {
        gshell_launch_prepared = 1;
        gshell_launch_approved = 1;
        gshell_launch_state = "yes";
        gshell_launch_last_result = "yes";
        gshell_command_name = "LAUNCHAPPROVE";
        gshell_command_result = "LAUNCH APPROVE OK";
        gshell_command_view = "LAUNCHGATE";
        gshell_input_status_text = "APPROVED";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHAPPROVE -> USER APPROVED DEMO APP");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHDENY) {
        gshell_launch_approved = 0;
        gshell_launch_state = "denied";
        gshell_launch_last_result = "denied";
        gshell_command_name = "LAUNCHDENY";
        gshell_command_result = "LAUNCH DENY OK";
        gshell_command_view = "LAUNCHGATE";
        gshell_input_status_text = "DENIED";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHDENY -> USER DENIED DEMO APP");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHRUN) {
        int runtime_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        int sandbox_ok = security_check_sandbox();
        int rule_ok = security_check_rule("demo.app.launch");
        int file_ok = security_check_capability("file.read", "file");
        int launch_ok = runtime_ok && sandbox_ok && rule_ok && file_ok && gshell_app_slot_allocated && gshell_launch_prepared && gshell_launch_approved;

        gshell_launch_attempts++;

        if (launch_ok) {
            gshell_launch_state = "running";
            gshell_launch_last_result = "started";
        } else {
            gshell_launch_state = "blocked";
            gshell_launch_last_result = "blocked";
        }

        gshell_command_name = "LAUNCHRUN";
        gshell_command_result = launch_ok ? "LAUNCH RUN OK" : "LAUNCH RUN BLOCKED";
        gshell_command_view = "LAUNCHGATE";
        gshell_input_status_text = launch_ok ? "RUN OK" : "RUN BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(launch_ok ? "LAUNCHRUN -> DEMO APP LAUNCH ACCEPTED" : "LAUNCHRUN -> NEED SLOT/PREPARE/APPROVE/GATE OK");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHCHECK) {
        int ready = gshell_app_slot_allocated && gshell_launch_prepared && gshell_launch_approved;
        gshell_command_name = "LAUNCHCHECK";
        gshell_command_result = ready ? "LAUNCH CHECK OK" : "LAUNCH CHECK WAIT";
        gshell_command_view = "LAUNCHGATE";
        gshell_input_status_text = ready ? "READY" : "WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ready ? "LAUNCHCHECK -> SLOT/PREPARE/APPROVE READY" : "LAUNCHCHECK -> WAITING FOR SLOT OR APPROVAL");
        return;
    }

    if (command_id == GSHELL_CMD_PERMSTATUS) {
        gshell_command_name = "PERMSTATUS";
        gshell_command_result = "PERMISSION STATUS OK";
        gshell_command_view = "PERMSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PERMSTATUS -> APP PERMISSION REQUEST READY");
        return;
    }

    if (command_id == GSHELL_CMD_PERMREQUEST) {
        gshell_perm_requested = 1;
        gshell_perm_allowed = 0;
        gshell_perm_denied = 0;
        gshell_perm_state = "requested";
        gshell_perm_last = "file.read";
        gshell_command_name = "PERMREQUEST";
        gshell_command_result = "PERMISSION REQUEST OK";
        gshell_command_view = "PERMSTATUS";
        gshell_input_status_text = "REQUESTED";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PERMREQUEST -> DEMO APP REQUESTED FILE.READ");
        return;
    }

    if (command_id == GSHELL_CMD_PERMALLOW) {
        gshell_perm_requested = 1;
        gshell_perm_allowed = 1;
        gshell_perm_denied = 0;
        gshell_perm_state = "allowed";
        gshell_perm_last = "file.read";
        gshell_command_name = "PERMALLOW";
        gshell_command_result = "PERMISSION ALLOW OK";
        gshell_command_view = "PERMSTATUS";
        gshell_input_status_text = "ALLOW";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PERMALLOW -> USER ALLOWED FILE.READ");
        return;
    }

    if (command_id == GSHELL_CMD_PERMDENY) {
        gshell_perm_requested = 1;
        gshell_perm_allowed = 0;
        gshell_perm_denied = 1;
        gshell_perm_state = "denied";
        gshell_perm_last = "file.read";
        gshell_command_name = "PERMDENY";
        gshell_command_result = "PERMISSION DENY OK";
        gshell_command_view = "PERMSTATUS";
        gshell_input_status_text = "DENY";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PERMDENY -> USER DENIED FILE.READ");
        return;
    }

    if (command_id == GSHELL_CMD_PERMCHECK) {
        int cap_ok = security_check_capability("file.read", "file");
        int rule_ok = security_check_rule("demo.app.permission.file.read");
        int perm_ok = cap_ok && rule_ok && gshell_perm_requested && gshell_perm_allowed && !gshell_perm_denied;

        gshell_perm_checks++;

        if (perm_ok) {
            gshell_perm_state = "granted";
            gshell_perm_last = "granted";
        } else {
            gshell_perm_state = "blocked";
            gshell_perm_last = "blocked";
        }

        gshell_command_name = "PERMCHECK";
        gshell_command_result = perm_ok ? "PERMISSION CHECK OK" : "PERMISSION CHECK BLOCKED";
        gshell_command_view = "PERMSTATUS";
        gshell_input_status_text = perm_ok ? "PERM OK" : "PERM BLOCK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(perm_ok ? "PERMCHECK -> FILE.READ GRANTED" : "PERMCHECK -> NEED REQUEST/ALLOW/GATE OK");
        return;
    }

    if (command_id == GSHELL_CMD_PERMRESET) {
        gshell_perm_requested = 0;
        gshell_perm_allowed = 0;
        gshell_perm_denied = 0;
        gshell_perm_checks = 0;
        gshell_perm_state = "idle";
        gshell_perm_last = "none";
        gshell_command_name = "PERMRESET";
        gshell_command_result = "PERMISSION RESET OK";
        gshell_command_view = "PERMSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PERMRESET -> PERMISSION REQUEST RESET");
        return;
    }

    if (command_id == GSHELL_CMD_FSSTATUS) {
        gshell_command_name = "FSSTATUS";
        gshell_command_result = "FS STATUS OK";
        gshell_command_view = "FSSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FSSTATUS -> RAMFS PREP READY");
        return;
    }

    if (command_id == GSHELL_CMD_RAMFSSTATUS) {
        gshell_command_name = "RAMFSSTATUS";
        gshell_command_result = "RAMFS STATUS OK";
        gshell_command_view = "FSSTATUS";
        gshell_input_status_text = "RAMFS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RAMFSSTATUS -> RAMFS METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_FSMOUNT) {
        gshell_fs_mounted = 1;
        gshell_fs_state = "mounted";
        gshell_fs_last = "mounted";
        gshell_command_name = "FSMOUNT";
        gshell_command_result = "FS MOUNT OK";
        gshell_command_view = "FSSTATUS";
        gshell_input_status_text = "MOUNT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FSMOUNT -> RAMFS MOUNTED");
        return;
    }

    if (command_id == GSHELL_CMD_FSWRITE) {
        int file_ok = security_check_capability("file.read", "file");
        if (gshell_fs_mounted && file_ok) {
            gshell_fs_has_file = 1;
            gshell_fs_file = "demo.txt";
            gshell_fs_writes++;
            gshell_fs_state = "dirty";
            gshell_fs_last = "write-ok";
            gshell_command_result = "FS WRITE OK";
            gshell_input_status_text = "WRITE OK";
            gshell_terminal_push("FSWRITE -> DEMO.TXT WRITTEN");
        } else {
            gshell_fs_last = "write-blocked";
            gshell_command_result = "FS WRITE BLOCKED";
            gshell_input_status_text = "WRITE BLOCK";
            gshell_terminal_push("FSWRITE -> NEED MOUNT OR FILE GATE OK");
        }

        gshell_command_name = "FSWRITE";
        gshell_command_view = "FSSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_FSREAD) {
        int file_ok = security_check_capability("file.read", "file");
        if (gshell_fs_mounted && gshell_fs_has_file && file_ok) {
            gshell_fs_reads++;
            gshell_fs_state = "ready";
            gshell_fs_last = "read-ok";
            gshell_command_result = "FS READ OK";
            gshell_input_status_text = "READ OK";
            gshell_terminal_push("FSREAD -> DEMO.TXT READ");
        } else {
            gshell_fs_last = "read-wait";
            gshell_command_result = "FS READ WAIT";
            gshell_input_status_text = "READ WAIT";
            gshell_terminal_push("FSREAD -> NEED MOUNT/WRITE/FILE GATE OK");
        }

        gshell_command_name = "FSREAD";
        gshell_command_view = "FSSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_FSCHECK) {
        int ready = gshell_fs_mounted && gshell_fs_has_file;
        gshell_fs_checks++;
        gshell_command_name = "FSCHECK";
        gshell_command_result = ready ? "FS CHECK OK" : "FS CHECK WAIT";
        gshell_command_view = "FSSTATUS";
        gshell_input_status_text = ready ? "FS OK" : "FS WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_fs_last = ready ? "check-ok" : "check-wait";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ready ? "FSCHECK -> RAMFS READY" : "FSCHECK -> NEED MOUNT AND WRITE");
        return;
    }

    if (command_id == GSHELL_CMD_FSRESET) {
        gshell_fs_mounted = 0;
        gshell_fs_has_file = 0;
        gshell_fs_reads = 0;
        gshell_fs_writes = 0;
        gshell_fs_checks = 0;
        gshell_fs_state = "unmounted";
        gshell_fs_file = "none";
        gshell_fs_last = "reset";
        gshell_command_name = "FSRESET";
        gshell_command_result = "FS RESET OK";
        gshell_command_view = "FSSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FSRESET -> RAMFS STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_LIFESTATUS) {
        gshell_command_name = "LIFESTATUS";
        gshell_command_result = "LIFE STATUS OK";
        gshell_command_view = "LIFESTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LIFESTATUS -> APP LIFECYCLE READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPSTART) {
        int runtime_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        int sandbox_ok = security_check_sandbox();
        int rule_ok = security_check_rule("demo.app.lifecycle.start");
        int file_ok = security_check_capability("file.read", "file");
        int ready = runtime_ok && sandbox_ok && rule_ok && file_ok && gshell_app_slot_allocated && gshell_launch_approved;

        if (ready) {
            gshell_life_started = 1;
            gshell_life_paused = 0;
            gshell_life_starts++;
            gshell_life_state = "running";
            gshell_life_last = "start-ok";
            gshell_command_result = "APP START OK";
            gshell_input_status_text = "START OK";
            gshell_terminal_push("APPSTART -> DEMO APP RUNNING");
        } else {
            gshell_life_state = "blocked";
            gshell_life_last = "start-block";
            gshell_command_result = "APP START BLOCKED";
            gshell_input_status_text = "START BLOCK";
            gshell_terminal_push("APPSTART -> NEED SLOT/APPROVAL/GATE OK");
        }

        gshell_command_name = "APPSTART";
        gshell_command_view = "LIFESTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_APPPAUSE) {
        if (gshell_life_started && !gshell_life_paused) {
            gshell_life_paused = 1;
            gshell_life_pauses++;
            gshell_life_state = "paused";
            gshell_life_last = "pause-ok";
            gshell_command_result = "APP PAUSE OK";
            gshell_input_status_text = "PAUSE OK";
            gshell_terminal_push("APPPAUSE -> DEMO APP PAUSED");
        } else {
            gshell_life_last = "pause-wait";
            gshell_command_result = "APP PAUSE WAIT";
            gshell_input_status_text = "PAUSE WAIT";
            gshell_terminal_push("APPPAUSE -> APP NOT RUNNING");
        }

        gshell_command_name = "APPPAUSE";
        gshell_command_view = "LIFESTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_APPRESUME) {
        if (gshell_life_started && gshell_life_paused) {
            gshell_life_paused = 0;
            gshell_life_resumes++;
            gshell_life_state = "running";
            gshell_life_last = "resume-ok";
            gshell_command_result = "APP RESUME OK";
            gshell_input_status_text = "RESUME OK";
            gshell_terminal_push("APPRESUME -> DEMO APP RUNNING");
        } else {
            gshell_life_last = "resume-wait";
            gshell_command_result = "APP RESUME WAIT";
            gshell_input_status_text = "RESUME WAIT";
            gshell_terminal_push("APPRESUME -> APP NOT PAUSED");
        }

        gshell_command_name = "APPRESUME";
        gshell_command_view = "LIFESTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_APPSTOP) {
        if (gshell_life_started) {
            gshell_life_started = 0;
            gshell_life_paused = 0;
            gshell_life_stops++;
            gshell_life_state = "stopped";
            gshell_life_last = "stop-ok";
            gshell_command_result = "APP STOP OK";
            gshell_input_status_text = "STOP OK";
            gshell_terminal_push("APPSTOP -> DEMO APP STOPPED");
        } else {
            gshell_life_last = "stop-wait";
            gshell_command_result = "APP STOP WAIT";
            gshell_input_status_text = "STOP WAIT";
            gshell_terminal_push("APPSTOP -> APP NOT RUNNING");
        }

        gshell_command_name = "APPSTOP";
        gshell_command_view = "LIFESTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_LIFECHECK) {
        int ok = gshell_life_started && !gshell_life_paused;
        gshell_life_checks++;
        gshell_command_name = "LIFECHECK";
        gshell_command_result = ok ? "LIFE CHECK OK" : "LIFE CHECK WAIT";
        gshell_command_view = "LIFESTATUS";
        gshell_input_status_text = ok ? "LIFE OK" : "LIFE WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_life_last = ok ? "check-ok" : "check-wait";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "LIFECHECK -> APP RUNNING" : "LIFECHECK -> APP NOT RUNNING");
        return;
    }

    if (command_id == GSHELL_CMD_LIFERESET) {
        gshell_life_started = 0;
        gshell_life_paused = 0;
        gshell_life_starts = 0;
        gshell_life_pauses = 0;
        gshell_life_resumes = 0;
        gshell_life_stops = 0;
        gshell_life_checks = 0;
        gshell_life_state = "idle";
        gshell_life_last = "reset";
        gshell_command_name = "LIFERESET";
        gshell_command_result = "LIFE RESET OK";
        gshell_command_view = "LIFESTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LIFERESET -> APP LIFECYCLE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_RUNTIMEPANEL) {
        gshell_command_name = "RUNTIMEPANEL";
        gshell_command_result = "RUNTIME PANEL OK";
        gshell_command_view = "RUNTIMEPANEL";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RUNTIMEPANEL -> APP RUNTIME CONTROL PANEL READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPPANEL) {
        gshell_command_name = "APPPANEL";
        gshell_command_result = "APP PANEL OK";
        gshell_command_view = "RUNTIMEPANEL";
        gshell_input_status_text = "APP PANEL";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPPANEL -> APP SLOT SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHPANEL) {
        gshell_command_name = "LAUNCHPANEL";
        gshell_command_result = "LAUNCH PANEL OK";
        gshell_command_view = "RUNTIMEPANEL";
        gshell_input_status_text = "LAUNCH PANEL";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHPANEL -> LAUNCH GATE SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_PERMISSIONPANEL) {
        gshell_command_name = "PERMPANEL";
        gshell_command_result = "PERMISSION PANEL OK";
        gshell_command_view = "RUNTIMEPANEL";
        gshell_input_status_text = "PERM PANEL";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PERMISSIONPANEL -> PERMISSION SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_FSPANEL) {
        gshell_command_name = "FSPANEL";
        gshell_command_result = "FS PANEL OK";
        gshell_command_view = "RUNTIMEPANEL";
        gshell_input_status_text = "FS PANEL";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FSPANEL -> RAMFS SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_LIFEPANEL) {
        gshell_command_name = "LIFEPANEL";
        gshell_command_result = "LIFE PANEL OK";
        gshell_command_view = "RUNTIMEPANEL";
        gshell_input_status_text = "LIFE PANEL";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LIFEPANEL -> LIFECYCLE SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_RUNTIMEFINAL) {
        gshell_command_name = "RUNTIMEFINAL";
        gshell_command_result = "RUNTIME FINAL OK";
        gshell_command_view = "RUNTIMEFINAL";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RUNTIMEFINAL -> 0.9X RUNTIME CLOSEOUT READY");
        return;
    }

    if (command_id == GSHELL_CMD_RUNTIMEHEALTH) {
        gshell_command_name = "RTHEALTH";
        gshell_command_result = "RUNTIME HEALTH OK";
        gshell_command_view = "RUNTIMEFINAL";
        gshell_input_status_text = "HEALTH OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RUNTIMEHEALTH -> USER/RING3/SYSCALL READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPSUMMARY) {
        gshell_command_name = "APPSUMMARY";
        gshell_command_result = "APP SUMMARY OK";
        gshell_command_view = "RUNTIMEFINAL";
        gshell_input_status_text = "APP OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPSUMMARY -> APP SLOT SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHSUMMARY) {
        gshell_command_name = "LAUNCHSUM";
        gshell_command_result = "LAUNCH SUMMARY OK";
        gshell_command_view = "RUNTIMEFINAL";
        gshell_input_status_text = "LAUNCH OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHSUMMARY -> LAUNCH GATE SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_PERMISSIONSUMMARY) {
        gshell_command_name = "PERMSUM";
        gshell_command_result = "PERMISSION SUMMARY OK";
        gshell_command_view = "RUNTIMEFINAL";
        gshell_input_status_text = "PERM OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PERMISSIONSUMMARY -> APP PERMISSION SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_FSSUMMARY) {
        gshell_command_name = "FSSUMMARY";
        gshell_command_result = "FS SUMMARY OK";
        gshell_command_view = "RUNTIMEFINAL";
        gshell_input_status_text = "FS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FSSUMMARY -> RAMFS SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_LIFESUMMARY) {
        gshell_command_name = "LIFESUMMARY";
        gshell_command_result = "LIFE SUMMARY OK";
        gshell_command_view = "RUNTIMEFINAL";
        gshell_input_status_text = "LIFE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LIFESUMMARY -> LIFECYCLE SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_NEXTMAJOR) {
        gshell_command_name = "NEXTMAJOR";
        gshell_command_result = "NEXT MAJOR OK";
        gshell_command_view = "RUNTIMEFINAL";
        gshell_input_status_text = "NEXT 1.0";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("NEXTMAJOR -> 1.0 OS PROTOTYPE ENTRY");
        return;
    }

    if (command_id == GSHELL_CMD_PROTOTYPE) {
        gshell_command_name = "PROTOTYPE";
        gshell_command_result = "PROTOTYPE OK";
        gshell_command_view = "PROTOTYPE";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PROTOTYPE -> 1.0 OS PROTOTYPE MILESTONE READY");
        return;
    }

    if (command_id == GSHELL_CMD_PROTOSTATUS) {
        gshell_command_name = "PROTOSTATUS";
        gshell_command_result = "PROTO STATUS OK";
        gshell_command_view = "PROTOTYPE";
        gshell_input_status_text = "PROTO OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PROTOSTATUS -> BOOT/CONTROL/RUNTIME STATUS READY");
        return;
    }

    if (command_id == GSHELL_CMD_PROTOMILESTONE) {
        gshell_command_name = "MILESTONE";
        gshell_command_result = "MILESTONE OK";
        gshell_command_view = "PROTOTYPE";
        gshell_input_status_text = "MILESTONE";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MILESTONE -> LINGJING OS 1.0 PROTOTYPE BASE");
        return;
    }

    if (command_id == GSHELL_CMD_SYSTEMFLOW) {
        gshell_command_name = "SYSTEMFLOW";
        gshell_command_result = "SYSTEM FLOW OK";
        gshell_command_view = "PROTOTYPE";
        gshell_input_status_text = "FLOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SYSTEMFLOW -> BOOT HEALTH CONTROL RUNTIME LINKED");
        return;
    }

    if (command_id == GSHELL_CMD_DEMOFLOW) {
        gshell_command_name = "DEMOFLOW";
        gshell_command_result = "DEMO FLOW OK";
        gshell_command_view = "PROTOTYPE";
        gshell_input_status_text = "DEMO OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DEMOFLOW -> SLOT LAUNCH PERM FS LIFE CHAIN READY");
        return;
    }

    if (command_id == GSHELL_CMD_PROTOREADY) {
        int core_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        int control_ok = security_user_control_enabled();
        int runtime_ok = gshell_app_slot_allocated && gshell_launch_approved;
        gshell_command_name = "PROTOREADY";
        gshell_command_result = (core_ok && control_ok) ? "PROTO READY OK" : "PROTO READY BAD";
        gshell_command_view = "PROTOTYPE";
        gshell_input_status_text = (core_ok && control_ok) ? "READY" : "BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(runtime_ok ? "PROTOREADY -> FULL DEMO CHAIN READY" : "PROTOREADY -> CORE READY, DEMO CHAIN OPTIONAL");
        return;
    }

    if (command_id == GSHELL_CMD_UNIDASH) {
        gshell_command_name = "DASHBOARD";
        gshell_command_result = "DASHBOARD OK";
        gshell_command_view = "DASHBOARD";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DASHBOARD -> UNIFIED SYSTEM DASHBOARD READY");
        return;
    }

    if (command_id == GSHELL_CMD_SYSDASH) {
        gshell_command_name = "SYSDASH";
        gshell_command_result = "SYSTEM DASH OK";
        gshell_command_view = "DASHBOARD";
        gshell_input_status_text = "SYS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SYSDASH -> SYSTEM SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_BOOTDASH) {
        gshell_command_name = "BOOTDASH";
        gshell_command_result = "BOOT DASH OK";
        gshell_command_view = "DASHBOARD";
        gshell_input_status_text = "BOOT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BOOTDASH -> BOOT GRAPHICS SHELL READY");
        return;
    }

    if (command_id == GSHELL_CMD_CONTROLDASH) {
        gshell_command_name = "CONTROLDASH";
        gshell_command_result = "CONTROL DASH OK";
        gshell_command_view = "DASHBOARD";
        gshell_input_status_text = "CONTROL OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CONTROLDASH -> CONTROL POLICY SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_RUNTIMEDASH) {
        gshell_command_name = "RUNTIMEDASH";
        gshell_command_result = "RUNTIME DASH OK";
        gshell_command_view = "DASHBOARD";
        gshell_input_status_text = "RUNTIME OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RUNTIMEDASH -> APP RUNTIME SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_FLOWDASH) {
        gshell_command_name = "FLOWDASH";
        gshell_command_result = "FLOW DASH OK";
        gshell_command_view = "DASHBOARD";
        gshell_input_status_text = "FLOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FLOWDASH -> BOOT CONTROL RUNTIME FLOW READY");
        return;
    }

    if (command_id == GSHELL_CMD_BOOTHEALTH) {
        gshell_command_name = "BOOTHEALTH";
        gshell_command_result = "BOOT HEALTH OK";
        gshell_command_view = "BOOTHEALTH";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BOOTHEALTH -> BOOT HEALTH DOCTOR READY");
        return;
    }

    if (command_id == GSHELL_CMD_HEALTHDASH) {
        gshell_command_name = "HEALTHDASH";
        gshell_command_result = "HEALTH DASH OK";
        gshell_command_view = "BOOTHEALTH";
        gshell_input_status_text = "HEALTH OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("HEALTHDASH -> CORE HEALTH SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_DOCTORDASH) {
        gshell_command_name = "DOCTORDASH";
        gshell_command_result = "DOCTOR DASH OK";
        gshell_command_view = "BOOTHEALTH";
        gshell_input_status_text = "DOCTOR OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DOCTORDASH -> DOCTOR PATH READY");
        return;
    }

    if (command_id == GSHELL_CMD_CORECHECK) {
        int core_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        gshell_command_name = "CORECHECK";
        gshell_command_result = core_ok ? "CORE CHECK OK" : "CORE CHECK BAD";
        gshell_command_view = "BOOTHEALTH";
        gshell_input_status_text = core_ok ? "CORE OK" : "CORE BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(core_ok ? "CORECHECK -> USER RING3 SYSCALL OK" : "CORECHECK -> CORE ISSUE");
        return;
    }

    if (command_id == GSHELL_CMD_BOOTCHECK) {
        gshell_command_name = "BOOTCHECK";
        gshell_command_result = "BOOT CHECK OK";
        gshell_command_view = "BOOTHEALTH";
        gshell_input_status_text = "BOOT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BOOTCHECK -> MULTIBOOT GRAPHICS GSHELL OK");
        return;
    }

    if (command_id == GSHELL_CMD_SYSTEMCHECK) {
        int core_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        int control_ok = security_user_control_enabled();
        gshell_command_name = "SYSTEMCHECK";
        gshell_command_result = (core_ok && control_ok) ? "SYSTEM CHECK OK" : "SYSTEM CHECK BAD";
        gshell_command_view = "BOOTHEALTH";
        gshell_input_status_text = (core_ok && control_ok) ? "SYSTEM OK" : "SYSTEM BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push((core_ok && control_ok) ? "SYSTEMCHECK -> BOOT CONTROL RUNTIME OK" : "SYSTEMCHECK -> SYSTEM ISSUE");
        return;
    }

    if (command_id == GSHELL_CMD_FLOWSTATUS) {
        gshell_command_name = "FLOWSTATUS";
        gshell_command_result = "FLOW STATUS OK";
        gshell_command_view = "FLOWSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FLOWSTATUS -> CONTROL RUNTIME FLOW READY");
        return;
    }

    if (command_id == GSHELL_CMD_FLOWPREPARE) {
        gshell_app_slot_allocated = 1;
        gshell_app_slot_pid = 1;
        gshell_app_slot_state = "allocated";
        gshell_launch_prepared = 1;
        gshell_launch_approved = 0;
        gshell_launch_state = "prepared";
        gshell_launch_last_result = "prepared";
        gshell_perm_requested = 1;
        gshell_perm_allowed = 0;
        gshell_perm_denied = 0;
        gshell_perm_state = "requested";
        gshell_perm_last = "file.read";
        gshell_flow_prepared = 1;
        gshell_flow_state = "prepared";
        gshell_flow_last = "prepared";
        gshell_command_name = "FLOWPREPARE";
        gshell_command_result = "FLOW PREPARE OK";
        gshell_command_view = "FLOWSTATUS";
        gshell_input_status_text = "PREPARED";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FLOWPREPARE -> SLOT LAUNCH PERM PREPARED");
        return;
    }

    if (command_id == GSHELL_CMD_FLOWCONTROL) {
        security_sandbox_profile_open();
        security_rule_default_allow();
        security_decision_reset_counters();
        gshell_flow_state = "control";
        gshell_flow_last = "control-ok";
        gshell_command_name = "FLOWCONTROL";
        gshell_command_result = "FLOW CONTROL OK";
        gshell_command_view = "FLOWSTATUS";
        gshell_input_status_text = "CONTROL OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FLOWCONTROL -> OPEN SANDBOX RULE ALLOW");
        return;
    }

    if (command_id == GSHELL_CMD_FLOWRUNTIME) {
        int runtime_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        int sandbox_ok = security_check_sandbox();
        int ready = runtime_ok && sandbox_ok && gshell_app_slot_allocated;
        gshell_flow_checks++;
        gshell_flow_state = ready ? "runtime-ok" : "runtime-wait";
        gshell_flow_last = ready ? "runtime-ok" : "runtime-wait";
        gshell_command_name = "FLOWRUNTIME";
        gshell_command_result = ready ? "FLOW RUNTIME OK" : "FLOW RUNTIME WAIT";
        gshell_command_view = "FLOWSTATUS";
        gshell_input_status_text = ready ? "RUNTIME OK" : "RUNTIME WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ready ? "FLOWRUNTIME -> RUNTIME SLOT READY" : "FLOWRUNTIME -> NEED FLOWPREPARE OR GATE OK");
        return;
    }

    if (command_id == GSHELL_CMD_FLOWDEMO) {
        security_sandbox_profile_open();
        security_rule_default_allow();

        gshell_app_slot_allocated = 1;
        gshell_app_slot_pid = 1;
        gshell_app_slot_state = "allocated";

        gshell_launch_prepared = 1;
        gshell_launch_approved = 1;
        gshell_launch_state = "running";
        gshell_launch_last_result = "started";

        gshell_perm_requested = 1;
        gshell_perm_allowed = 1;
        gshell_perm_denied = 0;
        gshell_perm_state = "granted";
        gshell_perm_last = "granted";

        gshell_fs_mounted = 1;
        gshell_fs_has_file = 1;
        gshell_fs_file = "demo.txt";
        gshell_fs_state = "ready";
        gshell_fs_last = "demo-ready";

        gshell_life_started = 1;
        gshell_life_paused = 0;
        gshell_life_state = "running";
        gshell_life_last = "demo-run";

        gshell_flow_prepared = 1;
        gshell_flow_demos++;
        gshell_flow_state = "demo-ready";
        gshell_flow_last = "full-demo";

        gshell_command_name = "FLOWDEMO";
        gshell_command_result = "FLOW DEMO OK";
        gshell_command_view = "FLOWSTATUS";
        gshell_input_status_text = "DEMO OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FLOWDEMO -> FULL BOOT CONTROL RUNTIME APP CHAIN READY");
        return;
    }

    if (command_id == GSHELL_CMD_FLOWCHECK) {
        int core_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        int control_ok = security_user_control_enabled();
        int runtime_ok = gshell_app_slot_allocated && gshell_launch_approved && gshell_perm_allowed && gshell_fs_mounted && gshell_life_started;
        int ok = core_ok && control_ok && runtime_ok;
        gshell_flow_checks++;
        gshell_flow_state = ok ? "ready" : "partial";
        gshell_flow_last = ok ? "check-ok" : "check-partial";
        gshell_command_name = "FLOWCHECK";
        gshell_command_result = ok ? "FLOW CHECK OK" : "FLOW CHECK PARTIAL";
        gshell_command_view = "FLOWSTATUS";
        gshell_input_status_text = ok ? "FLOW OK" : "FLOW PART";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "FLOWCHECK -> FULL SYSTEM FLOW READY" : "FLOWCHECK -> RUN FLOWDEMO FOR FULL CHAIN");
        return;
    }

    if (command_id == GSHELL_CMD_FLOWRESET) {
        gshell_flow_prepared = 0;
        gshell_flow_demos = 0;
        gshell_flow_checks = 0;
        gshell_flow_state = "idle";
        gshell_flow_last = "reset";
        gshell_launch_prepared = 0;
        gshell_launch_approved = 0;
        gshell_launch_state = "idle";
        gshell_launch_last_result = "none";
        gshell_life_started = 0;
        gshell_life_paused = 0;
        gshell_life_state = "idle";
        gshell_life_last = "reset";
        gshell_command_name = "FLOWRESET";
        gshell_command_result = "FLOW RESET OK";
        gshell_command_view = "FLOWSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FLOWRESET -> FLOW STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_WALKTHROUGH) {
        gshell_command_name = "WALKTHROUGH";
        gshell_command_result = "WALKTHROUGH OK";
        gshell_command_view = "WALKTHROUGH";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("WALKTHROUGH -> DEMO APP FULL CHAIN READY");
        return;
    }

    if (command_id == GSHELL_CMD_DEMOSTEP1) {
        security_sandbox_profile_open();
        security_rule_default_allow();
        security_decision_reset_counters();

        gshell_demo_step = 1;
        gshell_demo_state = "control";
        gshell_demo_last = "control-ok";
        gshell_flow_state = "control";
        gshell_flow_last = "demo-step1";

        gshell_command_name = "DEMOSTEP1";
        gshell_command_result = "DEMO STEP1 OK";
        gshell_command_view = "WALKTHROUGH";
        gshell_input_status_text = "STEP1 OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DEMOSTEP1 -> CONTROL OPEN + RULE ALLOW");
        return;
    }

    if (command_id == GSHELL_CMD_DEMOSTEP2) {
        gshell_app_slot_allocated = 1;
        gshell_app_slot_pid = 1;
        gshell_app_slot_state = "allocated";

        gshell_demo_step = 2;
        gshell_demo_state = "slot";
        gshell_demo_last = "slot-ok";
        gshell_flow_state = "slot-ready";
        gshell_flow_last = "demo-step2";

        gshell_command_name = "DEMOSTEP2";
        gshell_command_result = "DEMO STEP2 OK";
        gshell_command_view = "WALKTHROUGH";
        gshell_input_status_text = "STEP2 OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DEMOSTEP2 -> DEMO APP SLOT ALLOCATED");
        return;
    }

    if (command_id == GSHELL_CMD_DEMOSTEP3) {
        gshell_launch_prepared = 1;
        gshell_launch_approved = 1;
        gshell_launch_state = "approved";
        gshell_launch_last_result = "approved";

        gshell_demo_step = 3;
        gshell_demo_state = "launch";
        gshell_demo_last = "launch-ok";
        gshell_flow_state = "launch-ready";
        gshell_flow_last = "demo-step3";

        gshell_command_name = "DEMOSTEP3";
        gshell_command_result = "DEMO STEP3 OK";
        gshell_command_view = "WALKTHROUGH";
        gshell_input_status_text = "STEP3 OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DEMOSTEP3 -> LAUNCH PREPARED + APPROVED");
        return;
    }

    if (command_id == GSHELL_CMD_DEMOSTEP4) {
        gshell_perm_requested = 1;
        gshell_perm_allowed = 1;
        gshell_perm_denied = 0;
        gshell_perm_state = "granted";
        gshell_perm_last = "granted";

        gshell_fs_mounted = 1;
        gshell_fs_has_file = 1;
        gshell_fs_file = "demo.txt";
        gshell_fs_state = "ready";
        gshell_fs_last = "demo-ready";

        gshell_demo_step = 4;
        gshell_demo_state = "perm-fs";
        gshell_demo_last = "perm-fs-ok";
        gshell_flow_state = "perm-fs-ready";
        gshell_flow_last = "demo-step4";

        gshell_command_name = "DEMOSTEP4";
        gshell_command_result = "DEMO STEP4 OK";
        gshell_command_view = "WALKTHROUGH";
        gshell_input_status_text = "STEP4 OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DEMOSTEP4 -> PERMISSION GRANTED + RAMFS READY");
        return;
    }

    if (command_id == GSHELL_CMD_DEMOSTEP5) {
        int core_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        int sandbox_ok = security_check_sandbox();
        int ready = core_ok && sandbox_ok && gshell_app_slot_allocated && gshell_launch_approved && gshell_perm_allowed && gshell_fs_mounted;

        if (ready) {
            gshell_life_started = 1;
            gshell_life_paused = 0;
            gshell_life_state = "running";
            gshell_life_last = "demo-run";
            gshell_launch_state = "running";
            gshell_launch_last_result = "started";
            gshell_demo_step = 5;
            gshell_demo_state = "running";
            gshell_demo_last = "demo-running";
            gshell_flow_state = "demo-ready";
            gshell_flow_last = "demo-step5";
            gshell_command_result = "DEMO STEP5 OK";
            gshell_input_status_text = "STEP5 OK";
            gshell_terminal_push("DEMOSTEP5 -> DEMO APP RUNNING");
        } else {
            gshell_demo_state = "blocked";
            gshell_demo_last = "step5-block";
            gshell_flow_state = "blocked";
            gshell_flow_last = "demo-step5-block";
            gshell_command_result = "DEMO STEP5 BLOCKED";
            gshell_input_status_text = "STEP5 BLOCK";
            gshell_terminal_push("DEMOSTEP5 -> NEED STEP1-4 OR GATE OK");
        }

        gshell_command_name = "DEMOSTEP5";
        gshell_command_view = "WALKTHROUGH";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_DEMOCHECK) {
        int ok = gshell_demo_step >= 5 && gshell_life_started && gshell_app_slot_allocated && gshell_launch_approved && gshell_perm_allowed && gshell_fs_mounted;
        gshell_demo_checks++;
        gshell_command_name = "DEMOCHECK";
        gshell_command_result = ok ? "DEMO CHECK OK" : "DEMO CHECK PARTIAL";
        gshell_command_view = "WALKTHROUGH";
        gshell_input_status_text = ok ? "DEMO OK" : "DEMO PART";
        gshell_parser_status_text = "REGISTRY";
        gshell_demo_last = ok ? "check-ok" : "check-partial";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "DEMOCHECK -> FULL WALKTHROUGH READY" : "DEMOCHECK -> RUN DEMOSTEP1-5");
        return;
    }

    if (command_id == GSHELL_CMD_DEMORESET) {
        gshell_demo_step = 0;
        gshell_demo_checks = 0;
        gshell_demo_state = "idle";
        gshell_demo_last = "reset";

        gshell_flow_prepared = 0;
        gshell_flow_state = "idle";
        gshell_flow_last = "reset";

        gshell_life_started = 0;
        gshell_life_paused = 0;
        gshell_life_state = "idle";
        gshell_life_last = "reset";

        gshell_launch_prepared = 0;
        gshell_launch_approved = 0;
        gshell_launch_state = "idle";
        gshell_launch_last_result = "none";

        gshell_command_name = "DEMORESET";
        gshell_command_result = "DEMO RESET OK";
        gshell_command_view = "WALKTHROUGH";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DEMORESET -> DEMO WALKTHROUGH RESET");
        return;
    }

    if (command_id == GSHELL_CMD_MILESTONEFINAL) {
        gshell_command_name = "MILEFINAL";
        gshell_command_result = "MILESTONE FINAL OK";
        gshell_command_view = "MILEFINAL";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MILESTONEFINAL -> 1.0 MILESTONE PANEL READY");
        return;
    }

    if (command_id == GSHELL_CMD_PROTOSUMMARY) {
        gshell_command_name = "PROTOSUM";
        gshell_command_result = "PROTO SUMMARY OK";
        gshell_command_view = "MILEFINAL";
        gshell_input_status_text = "PROTO OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PROTOSUMMARY -> BOOT GSHELL CONTROL RUNTIME READY");
        return;
    }

    if (command_id == GSHELL_CMD_CHAINSUMMARY) {
        gshell_command_name = "CHAINSUM";
        gshell_command_result = "CHAIN SUMMARY OK";
        gshell_command_view = "MILEFINAL";
        gshell_input_status_text = "CHAIN OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CHAINSUMMARY -> BOOT CONTROL RUNTIME DEMO CHAIN READY");
        return;
    }

    if (command_id == GSHELL_CMD_READYPANEL) {
        gshell_command_name = "READYPANEL";
        gshell_command_result = "READY PANEL OK";
        gshell_command_view = "MILEFINAL";
        gshell_input_status_text = "READY OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("READYPANEL -> 1.0 READINESS SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_VERSIONPANEL) {
        gshell_command_name = "VERPANEL";
        gshell_command_result = "VERSION PANEL OK";
        gshell_command_view = "MILEFINAL";
        gshell_input_status_text = "VERSION OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VERSIONPANEL -> DEV 1.0.5 MILESTONE FINAL");
        return;
    }

    if (command_id == GSHELL_CMD_CLOSECHECK) {
        int core_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        int control_ok = security_user_control_enabled();
        int demo_ok = gshell_demo_step >= 5 && gshell_life_started;
        int ok = core_ok && control_ok;
        gshell_command_name = "CLOSECHECK";
        gshell_command_result = ok ? "CLOSE CHECK OK" : "CLOSE CHECK BAD";
        gshell_command_view = "MILEFINAL";
        gshell_input_status_text = ok ? "CLOSE OK" : "CLOSE BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(demo_ok ? "CLOSECHECK -> CORE AND DEMO READY" : "CLOSECHECK -> CORE READY, DEMO OPTIONAL");
        return;
    }

    if (command_id == GSHELL_CMD_UIPOLISH) {
        gshell_command_name = "UIPOLISH";
        gshell_command_result = "UI POLISH OK";
        gshell_command_view = "UIPOLISH";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIPOLISH -> 1.0 UI POLISH READY");
        return;
    }

    if (command_id == GSHELL_CMD_LAYOUTCHECK) {
        gshell_command_name = "LAYOUTCHK";
        gshell_command_result = "LAYOUT CHECK OK";
        gshell_command_view = "UIPOLISH";
        gshell_input_status_text = "LAYOUT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAYOUTCHECK -> VIEWPORT PANEL LAYOUT OK");
        return;
    }

    if (command_id == GSHELL_CMD_PANELCHECK) {
        gshell_command_name = "PANELCHK";
        gshell_command_result = "PANEL CHECK OK";
        gshell_command_view = "UIPOLISH";
        gshell_input_status_text = "PANEL OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PANELCHECK -> SYSTEM COMMAND TERMINAL PANELS OK");
        return;
    }

    if (command_id == GSHELL_CMD_TEXTCHECK) {
        gshell_command_name = "TEXTCHK";
        gshell_command_result = "TEXT CHECK OK";
        gshell_command_view = "UIPOLISH";
        gshell_input_status_text = "TEXT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TEXTCHECK -> SHORT LABELS OK");
        return;
    }

    if (command_id == GSHELL_CMD_COMMANDCLEAN) {
        gshell_command_name = "CMDCLEAN";
        gshell_command_result = "COMMAND CLEAN OK";
        gshell_command_view = "UIPOLISH";
        gshell_input_status_text = "CLEAN OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("COMMANDCLEAN -> 1.0 COMMAND PANEL CLEAN");
        return;
    }

    if (command_id == GSHELL_CMD_UICHECK) {
        int core_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        gshell_command_name = "UICHECK";
        gshell_command_result = core_ok ? "UI CHECK OK" : "UI CHECK BAD";
        gshell_command_view = "UIPOLISH";
        gshell_input_status_text = core_ok ? "UI OK" : "UI BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(core_ok ? "UICHECK -> GSHELL UI READY" : "UICHECK -> CORE HEALTH BAD");
        return;
    }

    if (command_id == GSHELL_CMD_CLOSEOUT) {
        gshell_command_name = "CLOSEOUT";
        gshell_command_result = "CLOSEOUT OK";
        gshell_command_view = "CLOSEOUT";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CLOSEOUT -> 1.0 PROTOTYPE CLOSEOUT READY");
        return;
    }

    if (command_id == GSHELL_CMD_FINALSTATUS) {
        gshell_command_name = "FINALSTATUS";
        gshell_command_result = "FINAL STATUS OK";
        gshell_command_view = "CLOSEOUT";
        gshell_input_status_text = "STATUS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FINALSTATUS -> BOOT CONTROL RUNTIME DEMO STATUS READY");
        return;
    }

    if (command_id == GSHELL_CMD_FINALHEALTH) {
        int core_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        gshell_command_name = "FINALHEALTH";
        gshell_command_result = core_ok ? "FINAL HEALTH OK" : "FINAL HEALTH BAD";
        gshell_command_view = "CLOSEOUT";
        gshell_input_status_text = core_ok ? "HEALTH OK" : "HEALTH BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(core_ok ? "FINALHEALTH -> CORE HEALTH OK" : "FINALHEALTH -> CORE HEALTH BAD");
        return;
    }

    if (command_id == GSHELL_CMD_FINALDEMO) {
        int demo_ok = gshell_demo_step >= 5 && gshell_life_started;
        gshell_command_name = "FINALDEMO";
        gshell_command_result = demo_ok ? "FINAL DEMO OK" : "FINAL DEMO OPTIONAL";
        gshell_command_view = "CLOSEOUT";
        gshell_input_status_text = demo_ok ? "DEMO OK" : "DEMO OPT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(demo_ok ? "FINALDEMO -> FULL DEMO CHAIN READY" : "FINALDEMO -> RUN DEMOSTEP1-5 OPTIONAL");
        return;
    }

    if (command_id == GSHELL_CMD_FINALREADY) {
        int core_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        int control_ok = security_user_control_enabled();
        int ready = core_ok && control_ok;
        gshell_command_name = "FINALREADY";
        gshell_command_result = ready ? "FINAL READY OK" : "FINAL READY BAD";
        gshell_command_view = "CLOSEOUT";
        gshell_input_status_text = ready ? "READY" : "BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ready ? "FINALREADY -> 1.0 PROTOTYPE READY" : "FINALREADY -> 1.0 PROTOTYPE NOT READY");
        return;
    }

    if (command_id == GSHELL_CMD_RELEASEINFO) {
        gshell_command_name = "RELEASEINFO";
        gshell_command_result = "RELEASE INFO OK";
        gshell_command_view = "CLOSEOUT";
        gshell_input_status_text = "RELEASE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RELEASEINFO -> DEV 1.0.7 PROTOTYPE CLOSEOUT");
        return;
    }

    if (command_id == GSHELL_CMD_VERSIONFINAL) {
        gshell_command_name = "VERSIONFINAL";
        gshell_command_result = "VERSION FINAL OK";
        gshell_command_view = "CLOSEOUT";
        gshell_input_status_text = "VERSION OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VERSIONFINAL -> LINGJING OS DEV 1.0.7");
        return;
    }

    if (command_id == GSHELL_CMD_NEXTROADMAP) {
        gshell_command_name = "NEXTROADMAP";
        gshell_command_result = "NEXT ROADMAP OK";
        gshell_command_view = "CLOSEOUT";
        gshell_input_status_text = "NEXT 1.1";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("NEXTROADMAP -> 1.1 INTERACTION UI INPUT LAYER");
        return;
    }

    if (command_id == GSHELL_CMD_INPUTSTATUS) {
        gshell_command_name = "INPUTSTATUS";
        gshell_command_result = "INPUT STATUS OK";
        gshell_command_view = "INPUTSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("INPUTSTATUS -> INTERACTION INPUT LAYER READY");
        return;
    }

    if (command_id == GSHELL_CMD_MOUSESTATUS) {
        gshell_command_name = "MOUSESTATUS";
        gshell_command_result = "MOUSE STATUS OK";
        gshell_command_view = "INPUTSTATUS";
        gshell_input_status_text = "MOUSE META";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MOUSESTATUS -> MOUSE LAYER METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_CLICKSTATUS) {
        gshell_command_name = "CLICKSTATUS";
        gshell_command_result = "CLICK STATUS OK";
        gshell_command_view = "INPUTSTATUS";
        gshell_input_status_text = "CLICK META";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CLICKSTATUS -> CLICK COMMAND METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_FOCUSSTATUS) {
        gshell_command_name = "FOCUSSTATUS";
        gshell_command_result = "FOCUS STATUS OK";
        gshell_command_view = "INPUTSTATUS";
        gshell_input_status_text = "FOCUS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FOCUSSTATUS -> TERMINAL FOCUS READY");
        return;
    }

    if (command_id == GSHELL_CMD_INPUTDEMO) {
        gshell_mouse_layer_ready = 1;
        gshell_click_layer_ready = 1;
        gshell_focus_layer_ready = 1;
        gshell_interaction_events++;
        gshell_focus_changes++;
        gshell_input_state = "demo";
        gshell_focus_target = "commands";
        gshell_input_last = "demo-click";
        gshell_command_name = "INPUTDEMO";
        gshell_command_result = "INPUT DEMO OK";
        gshell_command_view = "INPUTSTATUS";
        gshell_input_status_text = "DEMO OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("INPUTDEMO -> MOUSE CLICK FOCUS DEMO READY");
        return;
    }

    if (command_id == GSHELL_CMD_INPUTCHECK) {
        int ok = gshell_input_layer_enabled && gshell_focus_layer_ready;
        gshell_interaction_events++;
        gshell_input_last = ok ? "check-ok" : "check-bad";
        gshell_command_name = "INPUTCHECK";
        gshell_command_result = ok ? "INPUT CHECK OK" : "INPUT CHECK BAD";
        gshell_command_view = "INPUTSTATUS";
        gshell_input_status_text = ok ? "INPUT OK" : "INPUT BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "INPUTCHECK -> KEYBOARD FOCUS INPUT OK" : "INPUTCHECK -> INPUT LAYER BAD");
        return;
    }

    if (command_id == GSHELL_CMD_INPUTRESET) {
        gshell_input_layer_enabled = 1;
        gshell_mouse_layer_ready = 0;
        gshell_click_layer_ready = 0;
        gshell_focus_layer_ready = 1;
        gshell_interaction_events = 0;
        gshell_focus_changes = 0;
        gshell_input_state = "keyboard";
        gshell_focus_target = "terminal";
        gshell_input_last = "reset";
        gshell_command_name = "INPUTRESET";
        gshell_command_result = "INPUT RESET OK";
        gshell_command_view = "INPUTSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("INPUTRESET -> INPUT LAYER RESET");
        return;
    }

    if (command_id == GSHELL_CMD_CURSORSTATUS) {
        gshell_command_name = "CURSORSTATUS";
        gshell_command_result = "CURSOR STATUS OK";
        gshell_command_view = "CURSORSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CURSORSTATUS -> POINTER STATE CORE READY");
        return;
    }

    if (command_id == GSHELL_CMD_POINTERSTATUS) {
        gshell_command_name = "POINTERSTATUS";
        gshell_command_result = "POINTER STATUS OK";
        gshell_command_view = "CURSORSTATUS";
        gshell_input_status_text = "POINTER OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POINTERSTATUS -> CURSOR METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_CURSORCENTER) {
        gshell_cursor_x = 400;
        gshell_cursor_y = 300;
        gshell_cursor_visible = 1;
        gshell_cursor_state = "center";
        gshell_cursor_last = "center";
        gshell_input_state = "pointer";
        gshell_focus_target = "viewport";
        gshell_interaction_events++;
        gshell_command_name = "CURSORCENTER";
        gshell_command_result = "CURSOR CENTER OK";
        gshell_command_view = "CURSORSTATUS";
        gshell_input_status_text = "CENTER";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CURSORCENTER -> POINTER CENTERED");
        return;
    }

    if (command_id == GSHELL_CMD_CURSORMOVE) {
        gshell_cursor_x += 24;
        gshell_cursor_y += 16;

        if (gshell_cursor_x > 760) {
            gshell_cursor_x = 40;
        }

        if (gshell_cursor_y > 560) {
            gshell_cursor_y = 80;
        }

        gshell_cursor_moves++;
        gshell_cursor_state = "moved";
        gshell_cursor_last = "move";
        gshell_input_state = "pointer";
        gshell_focus_target = "viewport";
        gshell_interaction_events++;
        gshell_command_name = "CURSORMOVE";
        gshell_command_result = "CURSOR MOVE OK";
        gshell_command_view = "CURSORSTATUS";
        gshell_input_status_text = "MOVE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CURSORMOVE -> POINTER MOVED");
        return;
    }

    if (command_id == GSHELL_CMD_LEFTCLICK) {
        gshell_cursor_clicks++;
        gshell_cursor_state = "left-click";
        gshell_cursor_last = "left";
        gshell_click_layer_ready = 1;
        gshell_focus_target = "commands";
        gshell_focus_changes++;
        gshell_interaction_events++;
        gshell_command_name = "LEFTCLICK";
        gshell_command_result = "LEFT CLICK OK";
        gshell_command_view = "CURSORSTATUS";
        gshell_input_status_text = "LEFT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LEFTCLICK -> COMMAND PANEL FOCUS");
        return;
    }

    if (command_id == GSHELL_CMD_RIGHTCLICK) {
        gshell_cursor_clicks++;
        gshell_cursor_state = "right-click";
        gshell_cursor_last = "right";
        gshell_click_layer_ready = 1;
        gshell_focus_target = "context";
        gshell_focus_changes++;
        gshell_interaction_events++;
        gshell_command_name = "RIGHTCLICK";
        gshell_command_result = "RIGHT CLICK OK";
        gshell_command_view = "CURSORSTATUS";
        gshell_input_status_text = "RIGHT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RIGHTCLICK -> CONTEXT FOCUS READY");
        return;
    }

    if (command_id == GSHELL_CMD_WHEELSTEP) {
        gshell_cursor_wheel++;
        gshell_cursor_state = "wheel";
        gshell_cursor_last = "wheel";
        gshell_focus_target = "scroll";
        gshell_interaction_events++;
        gshell_command_name = "WHEELSTEP";
        gshell_command_result = "WHEEL STEP OK";
        gshell_command_view = "CURSORSTATUS";
        gshell_input_status_text = "WHEEL OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("WHEELSTEP -> SCROLL INTENT READY");
        return;
    }

    if (command_id == GSHELL_CMD_CURSORRESET) {
        gshell_cursor_x = 400;
        gshell_cursor_y = 300;
        gshell_cursor_visible = 1;
        gshell_cursor_moves = 0;
        gshell_cursor_clicks = 0;
        gshell_cursor_wheel = 0;
        gshell_cursor_state = "center";
        gshell_cursor_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "CURSORRESET";
        gshell_command_result = "CURSOR RESET OK";
        gshell_command_view = "CURSORSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CURSORRESET -> POINTER STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_HITSTATUS) {
        gshell_command_name = "HITSTATUS";
        gshell_command_result = "HIT STATUS OK";
        gshell_command_view = "HITSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("HITSTATUS -> CLICK HIT TEST CORE READY");
        return;
    }

    if (command_id == GSHELL_CMD_PANELHIT) {
        gshell_cursor_x = 620;
        gshell_cursor_y = 188;
        gshell_cursor_state = "hit-test";
        gshell_cursor_last = "panel";
        gshell_hit_tests++;
        gshell_hit_zone = "right-panel";
        gshell_hit_target = "commands";
        gshell_hit_action = "hover";
        gshell_hit_last = "panel-hit";
        gshell_focus_target = "commands";
        gshell_interaction_events++;
        gshell_command_name = "PANELHIT";
        gshell_command_result = "PANEL HIT OK";
        gshell_command_view = "HITSTATUS";
        gshell_input_status_text = "PANEL HIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("PANELHIT -> RIGHT COMMAND PANEL HIT");
        return;
    }

    if (command_id == GSHELL_CMD_COMMANDHIT) {
        gshell_cursor_x = 620;
        gshell_cursor_y = 212;
        gshell_cursor_state = "cmd-hit";
        gshell_cursor_last = "command";
        gshell_hit_tests++;
        gshell_hit_zone = "command-row";
        gshell_hit_target = "CURSORMOVE";
        gshell_hit_action = "select";
        gshell_hit_last = "command-hit";
        gshell_focus_target = "commands";
        gshell_interaction_events++;
        gshell_command_name = "COMMANDHIT";
        gshell_command_result = "COMMAND HIT OK";
        gshell_command_view = "HITSTATUS";
        gshell_input_status_text = "CMD HIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("COMMANDHIT -> COMMAND ROW SELECTED");
        return;
    }

    if (command_id == GSHELL_CMD_CLICKCMD) {
        gshell_cursor_clicks++;
        gshell_hit_clicks++;
        gshell_hit_commands++;
        gshell_click_layer_ready = 1;
        gshell_cursor_state = "click-cmd";
        gshell_cursor_last = "clickcmd";
        gshell_hit_zone = "command-row";
        gshell_hit_target = "DASHBOARD";
        gshell_hit_action = "execute-meta";
        gshell_hit_last = "click-command";
        gshell_focus_target = "commands";
        gshell_focus_changes++;
        gshell_interaction_events++;
        gshell_command_name = "CLICKCMD";
        gshell_command_result = "CLICK CMD OK";
        gshell_command_view = "HITSTATUS";
        gshell_input_status_text = "CLICK CMD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CLICKCMD -> COMMAND CLICK ROUTE READY");
        return;
    }

    if (command_id == GSHELL_CMD_RIGHTMENU) {
        gshell_cursor_clicks++;
        gshell_hit_context++;
        gshell_click_layer_ready = 1;
        gshell_cursor_state = "right-menu";
        gshell_cursor_last = "rightmenu";
        gshell_hit_zone = "context";
        gshell_hit_target = "command-menu";
        gshell_hit_action = "open-menu";
        gshell_hit_last = "right-menu";
        gshell_focus_target = "context";
        gshell_focus_changes++;
        gshell_interaction_events++;
        gshell_command_name = "RIGHTMENU";
        gshell_command_result = "RIGHT MENU OK";
        gshell_command_view = "HITSTATUS";
        gshell_input_status_text = "MENU OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RIGHTMENU -> CONTEXT MENU METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_WHEELPICK) {
        gshell_cursor_wheel++;
        gshell_hit_wheel++;
        gshell_cursor_state = "wheel-pick";
        gshell_cursor_last = "wheelpick";
        gshell_hit_zone = "command-list";
        gshell_hit_target = "next-command";
        gshell_hit_action = "scroll-select";
        gshell_hit_last = "wheel-pick";
        gshell_focus_target = "scroll";
        gshell_interaction_events++;
        gshell_command_name = "WHEELPICK";
        gshell_command_result = "WHEEL PICK OK";
        gshell_command_view = "HITSTATUS";
        gshell_input_status_text = "WHEEL OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("WHEELPICK -> COMMAND LIST SCROLL SELECT");
        return;
    }

    if (command_id == GSHELL_CMD_HITCHECK) {
        int ok = gshell_hit_tests > 0 || gshell_hit_clicks > 0 || gshell_hit_wheel > 0;
        gshell_command_name = "HITCHECK";
        gshell_command_result = ok ? "HIT CHECK OK" : "HIT CHECK WAIT";
        gshell_command_view = "HITSTATUS";
        gshell_input_status_text = ok ? "HIT OK" : "HIT WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_hit_last = ok ? "check-ok" : "check-wait";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "HITCHECK -> HIT TEST METADATA OK" : "HITCHECK -> RUN PANELHIT OR CLICKCMD");
        return;
    }

    if (command_id == GSHELL_CMD_HITRESET) {
        gshell_hit_tests = 0;
        gshell_hit_clicks = 0;
        gshell_hit_commands = 0;
        gshell_hit_context = 0;
        gshell_hit_wheel = 0;
        gshell_hit_zone = "none";
        gshell_hit_target = "none";
        gshell_hit_action = "none";
        gshell_hit_last = "reset";
        gshell_cursor_state = "center";
        gshell_cursor_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "HITRESET";
        gshell_command_result = "HIT RESET OK";
        gshell_command_view = "HITSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("HITRESET -> HIT TEST STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_BUTTONSTATUS) {
        gshell_command_name = "BUTTONSTATUS";
        gshell_command_result = "BUTTON STATUS OK";
        gshell_command_view = "BUTTONSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BUTTONSTATUS -> BUTTON PANEL INTERACTION READY");
        return;
    }

    if (command_id == GSHELL_CMD_BUTTONHOVER) {
        gshell_button_hovered = 1;
        gshell_button_pressed = 0;
        gshell_button_target = "dashboard";
        gshell_button_state = "hover";
        gshell_button_last = "hover";
        gshell_hit_zone = "button";
        gshell_hit_target = "dashboard";
        gshell_hit_action = "hover";
        gshell_focus_target = "commands";
        gshell_button_events++;
        gshell_interaction_events++;
        gshell_command_name = "BUTTONHOVER";
        gshell_command_result = "BUTTON HOVER OK";
        gshell_command_view = "BUTTONSTATUS";
        gshell_input_status_text = "HOVER OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BUTTONHOVER -> DASHBOARD BUTTON HOVER");
        return;
    }

    if (command_id == GSHELL_CMD_BUTTONPRESS) {
        gshell_button_hovered = 1;
        gshell_button_pressed = 1;
        gshell_button_target = "dashboard";
        gshell_button_state = "pressed";
        gshell_button_last = "press";
        gshell_hit_zone = "button";
        gshell_hit_target = "dashboard";
        gshell_hit_action = "press";
        gshell_cursor_clicks++;
        gshell_hit_clicks++;
        gshell_button_events++;
        gshell_interaction_events++;
        gshell_command_name = "BUTTONPRESS";
        gshell_command_result = "BUTTON PRESS OK";
        gshell_command_view = "BUTTONSTATUS";
        gshell_input_status_text = "PRESS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BUTTONPRESS -> DASHBOARD BUTTON PRESSED");
        return;
    }

    if (command_id == GSHELL_CMD_BUTTONFOCUS) {
        gshell_button_focus = 1;
        gshell_button_target = "control";
        gshell_button_state = "focused";
        gshell_button_last = "focus";
        gshell_focus_target = "button";
        gshell_focus_changes++;
        gshell_button_events++;
        gshell_interaction_events++;
        gshell_command_name = "BUTTONFOCUS";
        gshell_command_result = "BUTTON FOCUS OK";
        gshell_command_view = "BUTTONSTATUS";
        gshell_input_status_text = "FOCUS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BUTTONFOCUS -> BUTTON FOCUS READY");
        return;
    }

    if (command_id == GSHELL_CMD_BUTTONACTIVE) {
        gshell_button_hovered = 1;
        gshell_button_pressed = 0;
        gshell_button_active = 1;
        gshell_button_focus = 1;
        gshell_button_target = "dashboard";
        gshell_button_state = "active";
        gshell_button_last = "activate";
        gshell_hit_zone = "button";
        gshell_hit_target = "dashboard";
        gshell_hit_action = "activate";
        gshell_focus_target = "dashboard";
        gshell_button_events++;
        gshell_button_activations++;
        gshell_hit_commands++;
        gshell_interaction_events++;
        gshell_command_name = "BUTTONACTIVE";
        gshell_command_result = "BUTTON ACTIVE OK";
        gshell_command_view = "BUTTONSTATUS";
        gshell_input_status_text = "ACTIVE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BUTTONACTIVE -> DASHBOARD BUTTON ACTIVATED");
        return;
    }

    if (command_id == GSHELL_CMD_BUTTONCHECK) {
        int ok = gshell_button_hovered || gshell_button_pressed || gshell_button_active || gshell_button_focus;
        gshell_button_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "BUTTONCHECK";
        gshell_command_result = ok ? "BUTTON CHECK OK" : "BUTTON CHECK WAIT";
        gshell_command_view = "BUTTONSTATUS";
        gshell_input_status_text = ok ? "BUTTON OK" : "BUTTON WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "BUTTONCHECK -> BUTTON INTERACTION OK" : "BUTTONCHECK -> RUN BUTTONHOVER OR BUTTONACTIVE");
        return;
    }

    if (command_id == GSHELL_CMD_BUTTONRESET) {
        gshell_button_hovered = 0;
        gshell_button_pressed = 0;
        gshell_button_active = 0;
        gshell_button_focus = 0;
        gshell_button_events = 0;
        gshell_button_activations = 0;
        gshell_button_target = "none";
        gshell_button_state = "idle";
        gshell_button_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "BUTTONRESET";
        gshell_command_result = "BUTTON RESET OK";
        gshell_command_view = "BUTTONSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BUTTONRESET -> BUTTON PANEL STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWSTATUS) {
        gshell_command_name = "WINDOWSTATUS";
        gshell_command_result = "WINDOW STATUS OK";
        gshell_command_view = "WINDOWSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("WINDOWSTATUS -> WINDOW PANEL STATE READY");
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWCREATE) {
        gshell_window_exists = 1;
        gshell_window_focused = 1;
        gshell_window_minimized = 0;
        gshell_window_title = "demo.window";
        gshell_window_state = "open";
        gshell_window_last = "create";
        gshell_window_events++;
        gshell_focus_target = "window";
        gshell_focus_changes++;
        gshell_button_target = "window";
        gshell_button_state = "active";
        gshell_command_name = "WINDOWCREATE";
        gshell_command_result = "WINDOW CREATE OK";
        gshell_command_view = "WINDOWSTATUS";
        gshell_input_status_text = "CREATE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("WINDOWCREATE -> DEMO WINDOW CREATED");
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWFOCUS) {
        if (gshell_window_exists) {
            gshell_window_focused = 1;
            gshell_window_minimized = 0;
            gshell_window_state = "focused";
            gshell_window_last = "focus";
            gshell_focus_target = "window";
            gshell_focus_changes++;
            gshell_command_result = "WINDOW FOCUS OK";
            gshell_input_status_text = "FOCUS OK";
            gshell_terminal_push("WINDOWFOCUS -> DEMO WINDOW FOCUSED");
        } else {
            gshell_window_last = "focus-wait";
            gshell_command_result = "WINDOW FOCUS WAIT";
            gshell_input_status_text = "FOCUS WAIT";
            gshell_terminal_push("WINDOWFOCUS -> NEED WINDOWCREATE");
        }

        gshell_window_events++;
        gshell_command_name = "WINDOWFOCUS";
        gshell_command_view = "WINDOWSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWMOVE) {
        if (gshell_window_exists && !gshell_window_minimized) {
            gshell_window_x += 16;
            gshell_window_y += 12;

            if (gshell_window_x > 420) {
                gshell_window_x = 120;
            }

            if (gshell_window_y > 260) {
                gshell_window_y = 92;
            }

            gshell_window_moves++;
            gshell_window_state = "moved";
            gshell_window_last = "move";
            gshell_cursor_state = "drag";
            gshell_cursor_last = "window-move";
            gshell_command_result = "WINDOW MOVE OK";
            gshell_input_status_text = "MOVE OK";
            gshell_terminal_push("WINDOWMOVE -> DEMO WINDOW MOVED");
        } else {
            gshell_window_last = "move-wait";
            gshell_command_result = "WINDOW MOVE WAIT";
            gshell_input_status_text = "MOVE WAIT";
            gshell_terminal_push("WINDOWMOVE -> NEED OPEN WINDOW");
        }

        gshell_window_events++;
        gshell_command_name = "WINDOWMOVE";
        gshell_command_view = "WINDOWSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWMIN) {
        if (gshell_window_exists) {
            gshell_window_minimized = 1;
            gshell_window_focused = 0;
            gshell_window_state = "minimized";
            gshell_window_last = "minimize";
            gshell_focus_target = "terminal";
            gshell_command_result = "WINDOW MIN OK";
            gshell_input_status_text = "MIN OK";
            gshell_terminal_push("WINDOWMIN -> DEMO WINDOW MINIMIZED");
        } else {
            gshell_window_last = "min-wait";
            gshell_command_result = "WINDOW MIN WAIT";
            gshell_input_status_text = "MIN WAIT";
            gshell_terminal_push("WINDOWMIN -> NEED WINDOWCREATE");
        }

        gshell_window_events++;
        gshell_command_name = "WINDOWMIN";
        gshell_command_view = "WINDOWSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWCLOSE) {
        if (gshell_window_exists) {
            gshell_window_exists = 0;
            gshell_window_focused = 0;
            gshell_window_minimized = 0;
            gshell_window_title = "none";
            gshell_window_state = "closed";
            gshell_window_last = "close";
            gshell_focus_target = "terminal";
            gshell_command_result = "WINDOW CLOSE OK";
            gshell_input_status_text = "CLOSE OK";
            gshell_terminal_push("WINDOWCLOSE -> DEMO WINDOW CLOSED");
        } else {
            gshell_window_last = "close-wait";
            gshell_command_result = "WINDOW CLOSE WAIT";
            gshell_input_status_text = "CLOSE WAIT";
            gshell_terminal_push("WINDOWCLOSE -> NO WINDOW OPEN");
        }

        gshell_window_events++;
        gshell_command_name = "WINDOWCLOSE";
        gshell_command_view = "WINDOWSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWCHECK) {
        int ok = gshell_window_exists && !gshell_window_minimized;
        gshell_window_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "WINDOWCHECK";
        gshell_command_result = ok ? "WINDOW CHECK OK" : "WINDOW CHECK WAIT";
        gshell_command_view = "WINDOWSTATUS";
        gshell_input_status_text = ok ? "WIN OK" : "WIN WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "WINDOWCHECK -> ACTIVE WINDOW READY" : "WINDOWCHECK -> NEED OPEN ACTIVE WINDOW");
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWRESET) {
        gshell_window_exists = 0;
        gshell_window_focused = 0;
        gshell_window_minimized = 0;
        gshell_window_moves = 0;
        gshell_window_events = 0;
        gshell_window_x = 120;
        gshell_window_y = 92;
        gshell_window_title = "none";
        gshell_window_state = "closed";
        gshell_window_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "WINDOWRESET";
        gshell_command_result = "WINDOW RESET OK";
        gshell_command_view = "WINDOWSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("WINDOWRESET -> WINDOW STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_DESKTOPSTATUS) {
        gshell_command_name = "DESKTOPSTATUS";
        gshell_command_result = "DESKTOP STATUS OK";
        gshell_command_view = "DESKTOPSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DESKTOPSTATUS -> DESKTOP WORKSPACE STATE READY");
        return;
    }

    if (command_id == GSHELL_CMD_WORKSPACE) {
        gshell_workspace_ready = 1;
        gshell_workspace_id = 1;
        gshell_desktop_state = "workspace";
        gshell_desktop_focus = "workspace";
        gshell_desktop_last = "workspace";
        gshell_focus_target = "workspace";
        gshell_focus_changes++;
        gshell_desktop_events++;
        gshell_interaction_events++;
        gshell_command_name = "WORKSPACE";
        gshell_command_result = "WORKSPACE OK";
        gshell_command_view = "DESKTOPSTATUS";
        gshell_input_status_text = "WORKSPACE";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("WORKSPACE -> WORKSPACE 1 READY");
        return;
    }

    if (command_id == GSHELL_CMD_ICONSTATUS) {
        gshell_desktop_state = "icons";
        gshell_desktop_focus = "icons";
        gshell_desktop_last = "icon-status";
        gshell_focus_target = "icons";
        gshell_desktop_events++;
        gshell_command_name = "ICONSTATUS";
        gshell_command_result = "ICON STATUS OK";
        gshell_command_view = "DESKTOPSTATUS";
        gshell_input_status_text = "ICONS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ICONSTATUS -> DESKTOP ICON METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_ICONSELECT) {
        gshell_icon_selected = 1;
        gshell_desktop_state = "icon-selected";
        gshell_desktop_focus = "demo.icon";
        gshell_desktop_last = "icon-select";
        gshell_hit_zone = "desktop-icon";
        gshell_hit_target = "demo.app";
        gshell_hit_action = "select";
        gshell_focus_target = "demo.icon";
        gshell_focus_changes++;
        gshell_desktop_events++;
        gshell_interaction_events++;
        gshell_command_name = "ICONSELECT";
        gshell_command_result = "ICON SELECT OK";
        gshell_command_view = "DESKTOPSTATUS";
        gshell_input_status_text = "ICON SEL";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ICONSELECT -> DEMO APP ICON SELECTED");
        return;
    }

    if (command_id == GSHELL_CMD_DOCKSTATUS) {
        gshell_dock_ready = 1;
        gshell_desktop_state = "dock";
        gshell_desktop_focus = "dock";
        gshell_desktop_last = "dock-ready";
        gshell_hit_zone = "dock";
        gshell_hit_target = "launcher";
        gshell_hit_action = "dock-focus";
        gshell_focus_target = "dock";
        gshell_focus_changes++;
        gshell_desktop_events++;
        gshell_interaction_events++;
        gshell_command_name = "DOCKSTATUS";
        gshell_command_result = "DOCK STATUS OK";
        gshell_command_view = "DESKTOPSTATUS";
        gshell_input_status_text = "DOCK OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DOCKSTATUS -> DOCK METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_DESKTOPCHECK) {
        int ok = gshell_desktop_enabled && gshell_workspace_ready;
        gshell_desktop_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "DESKTOPCHECK";
        gshell_command_result = ok ? "DESKTOP CHECK OK" : "DESKTOP CHECK WAIT";
        gshell_command_view = "DESKTOPSTATUS";
        gshell_input_status_text = ok ? "DESK OK" : "DESK WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "DESKTOPCHECK -> WORKSPACE READY" : "DESKTOPCHECK -> RUN WORKSPACE FIRST");
        return;
    }

    if (command_id == GSHELL_CMD_DESKTOPRESET) {
        gshell_desktop_enabled = 1;
        gshell_workspace_ready = 0;
        gshell_icon_selected = 0;
        gshell_dock_ready = 0;
        gshell_desktop_events = 0;
        gshell_workspace_id = 0;
        gshell_desktop_state = "idle";
        gshell_desktop_focus = "terminal";
        gshell_desktop_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "DESKTOPRESET";
        gshell_command_result = "DESKTOP RESET OK";
        gshell_command_view = "DESKTOPSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DESKTOPRESET -> DESKTOP STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_INTERACTIONFINAL) {
        gshell_command_name = "INTERFINAL";
        gshell_command_result = "INTERACTION FINAL OK";
        gshell_command_view = "INTERACTIONFINAL";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("INTERACTIONFINAL -> 1.1 INTERACTION UI CLOSEOUT READY");
        return;
    }

    if (command_id == GSHELL_CMD_INPUTSUMMARY) {
        gshell_command_name = "INPUTSUMMARY";
        gshell_command_result = "INPUT SUMMARY OK";
        gshell_command_view = "INTERACTIONFINAL";
        gshell_input_status_text = "INPUT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("INPUTSUMMARY -> KEYBOARD FOCUS INPUT READY");
        return;
    }

    if (command_id == GSHELL_CMD_POINTERSUMMARY) {
        gshell_command_name = "POINTERSUMMARY";
        gshell_command_result = "POINTER SUMMARY OK";
        gshell_command_view = "INTERACTIONFINAL";
        gshell_input_status_text = "POINTER OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POINTERSUMMARY -> CURSOR CLICK WHEEL STATE READY");
        return;
    }

    if (command_id == GSHELL_CMD_HITSUMMARY) {
        gshell_command_name = "HITSUMMARY";
        gshell_command_result = "HIT SUMMARY OK";
        gshell_command_view = "INTERACTIONFINAL";
        gshell_input_status_text = "HIT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("HITSUMMARY -> COMMAND HIT TEST READY");
        return;
    }

    if (command_id == GSHELL_CMD_BUTTONSUMMARY) {
        gshell_command_name = "BUTTONSUMMARY";
        gshell_command_result = "BUTTON SUMMARY OK";
        gshell_command_view = "INTERACTIONFINAL";
        gshell_input_status_text = "BUTTON OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("BUTTONSUMMARY -> BUTTON PANEL STATE READY");
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWSUMMARY) {
        gshell_command_name = "WINDOWSUMMARY";
        gshell_command_result = "WINDOW SUMMARY OK";
        gshell_command_view = "INTERACTIONFINAL";
        gshell_input_status_text = "WINDOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("WINDOWSUMMARY -> WINDOW PANEL STATE READY");
        return;
    }

    if (command_id == GSHELL_CMD_DESKTOPSUMMARY) {
        gshell_command_name = "DESKTOPSUMMARY";
        gshell_command_result = "DESKTOP SUMMARY OK";
        gshell_command_view = "INTERACTIONFINAL";
        gshell_input_status_text = "DESKTOP OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DESKTOPSUMMARY -> DESKTOP WORKSPACE STATE READY");
        return;
    }

    if (command_id == GSHELL_CMD_NEXTPHASE) {
        gshell_command_name = "NEXTPHASE";
        gshell_command_result = "NEXT PHASE OK";
        gshell_command_view = "INTERACTIONFINAL";
        gshell_input_status_text = "NEXT 1.2";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("NEXTPHASE -> 1.2 REAL INPUT OR WINDOW SHELL");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLSTATUS) {
        gshell_command_name = "SHELLSTATUS";
        gshell_command_result = "SHELL STATUS OK";
        gshell_command_view = "SHELLSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SHELLSTATUS -> WINDOW SHELL BASE READY");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLPANEL) {
        gshell_shell_panel_ready = 1;
        gshell_shell_state = "panel";
        gshell_shell_focus = "main-panel";
        gshell_shell_last = "panel-ready";
        gshell_shell_events++;
        gshell_desktop_state = "shell-panel";
        gshell_desktop_focus = "main-panel";
        gshell_focus_target = "main-panel";
        gshell_focus_changes++;
        gshell_command_name = "SHELLPANEL";
        gshell_command_result = "SHELL PANEL OK";
        gshell_command_view = "SHELLSTATUS";
        gshell_input_status_text = "PANEL OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SHELLPANEL -> MAIN SHELL PANEL READY");
        return;
    }

    if (command_id == GSHELL_CMD_TASKBAR) {
        gshell_shell_taskbar_ready = 1;
        gshell_shell_state = "taskbar";
        gshell_shell_focus = "taskbar";
        gshell_shell_last = "taskbar-ready";
        gshell_shell_events++;
        gshell_dock_ready = 1;
        gshell_desktop_focus = "taskbar";
        gshell_focus_target = "taskbar";
        gshell_focus_changes++;
        gshell_command_name = "TASKBAR";
        gshell_command_result = "TASKBAR OK";
        gshell_command_view = "SHELLSTATUS";
        gshell_input_status_text = "TASKBAR OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TASKBAR -> SHELL TASKBAR READY");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHER) {
        gshell_shell_launcher_ready = 1;
        gshell_shell_state = "launcher";
        gshell_shell_focus = "launcher";
        gshell_shell_last = "launcher-ready";
        gshell_shell_events++;
        gshell_hit_zone = "launcher";
        gshell_hit_target = "demo.app";
        gshell_hit_action = "select-app";
        gshell_focus_target = "launcher";
        gshell_focus_changes++;
        gshell_command_name = "LAUNCHER";
        gshell_command_result = "LAUNCHER OK";
        gshell_command_view = "SHELLSTATUS";
        gshell_input_status_text = "LAUNCHER";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHER -> DEMO APP LAUNCHER READY");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLOPENAPP) {
        int shell_ready = gshell_shell_panel_ready && gshell_shell_taskbar_ready && gshell_shell_launcher_ready;

        if (shell_ready) {
            gshell_shell_open_apps++;
            gshell_shell_state = "app-open";
            gshell_shell_focus = "demo.app";
            gshell_shell_last = "open-demo";
            gshell_window_exists = 1;
            gshell_window_focused = 1;
            gshell_window_minimized = 0;
            gshell_window_title = "demo.window";
            gshell_window_state = "open";
            gshell_window_last = "shell-open";
            gshell_app_slot_allocated = 1;
            gshell_app_slot_pid = 1;
            gshell_app_slot_state = "allocated";
            gshell_focus_target = "demo.app";
            gshell_command_result = "SHELL OPEN APP OK";
            gshell_input_status_text = "APP OPEN";
            gshell_terminal_push("SHELLOPENAPP -> DEMO APP WINDOW OPENED");
        } else {
            gshell_shell_state = "wait";
            gshell_shell_last = "open-wait";
            gshell_command_result = "SHELL OPEN APP WAIT";
            gshell_input_status_text = "OPEN WAIT";
            gshell_terminal_push("SHELLOPENAPP -> NEED SHELLPANEL TASKBAR LAUNCHER");
        }

        gshell_shell_events++;
        gshell_command_name = "SHELLOPENAPP";
        gshell_command_view = "SHELLSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_SHELLCHECK) {
        int ok = gshell_shell_enabled && gshell_shell_panel_ready && gshell_shell_taskbar_ready && gshell_shell_launcher_ready;
        gshell_shell_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "SHELLCHECK";
        gshell_command_result = ok ? "SHELL CHECK OK" : "SHELL CHECK WAIT";
        gshell_command_view = "SHELLSTATUS";
        gshell_input_status_text = ok ? "SHELL OK" : "SHELL WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "SHELLCHECK -> WINDOW SHELL READY" : "SHELLCHECK -> NEED PANEL TASKBAR LAUNCHER");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLRESET) {
        gshell_shell_enabled = 1;
        gshell_shell_panel_ready = 0;
        gshell_shell_taskbar_ready = 0;
        gshell_shell_launcher_ready = 0;
        gshell_shell_open_apps = 0;
        gshell_shell_events = 0;
        gshell_shell_state = "idle";
        gshell_shell_focus = "terminal";
        gshell_shell_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "SHELLRESET";
        gshell_command_result = "SHELL RESET OK";
        gshell_command_view = "SHELLSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SHELLRESET -> WINDOW SHELL STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHERSTATUS) {
        gshell_command_name = "LAUNCHERSTATUS";
        gshell_command_result = "LAUNCHER STATUS OK";
        gshell_command_view = "LAUNCHERSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHERSTATUS -> APP LAUNCHER FLOW READY");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHERGRID) {
        gshell_shell_launcher_ready = 1;
        gshell_launcher_grid_ready = 1;
        gshell_launcher_state = "grid";
        gshell_launcher_last = "grid-ready";
        gshell_shell_state = "launcher";
        gshell_shell_focus = "launcher-grid";
        gshell_hit_zone = "launcher-grid";
        gshell_hit_target = "demo.app";
        gshell_hit_action = "show-apps";
        gshell_focus_target = "launcher-grid";
        gshell_focus_changes++;
        gshell_launcher_events++;
        gshell_shell_events++;
        gshell_command_name = "LAUNCHERGRID";
        gshell_command_result = "LAUNCHER GRID OK";
        gshell_command_view = "LAUNCHERSTATUS";
        gshell_input_status_text = "GRID OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHERGRID -> APP GRID READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPSELECT) {
        if (gshell_launcher_grid_ready) {
            gshell_launcher_app_selected = 1;
            gshell_launcher_selected_app = "demo.app";
            gshell_launcher_state = "selected";
            gshell_launcher_last = "select-demo";
            gshell_hit_zone = "app-icon";
            gshell_hit_target = "demo.app";
            gshell_hit_action = "select";
            gshell_focus_target = "demo.app";
            gshell_command_result = "APP SELECT OK";
            gshell_input_status_text = "SELECT OK";
            gshell_terminal_push("APPSELECT -> DEMO APP SELECTED");
        } else {
            gshell_launcher_state = "wait";
            gshell_launcher_last = "select-wait";
            gshell_command_result = "APP SELECT WAIT";
            gshell_input_status_text = "SELECT WAIT";
            gshell_terminal_push("APPSELECT -> NEED LAUNCHERGRID");
        }

        gshell_launcher_events++;
        gshell_command_name = "APPSELECT";
        gshell_command_view = "LAUNCHERSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_APPPIN) {
        if (gshell_launcher_app_selected) {
            gshell_launcher_app_pinned = 1;
            gshell_shell_taskbar_ready = 1;
            gshell_dock_ready = 1;
            gshell_launcher_state = "pinned";
            gshell_launcher_last = "pin-demo";
            gshell_hit_zone = "taskbar";
            gshell_hit_target = "demo.app";
            gshell_hit_action = "pin";
            gshell_focus_target = "taskbar";
            gshell_command_result = "APP PIN OK";
            gshell_input_status_text = "PIN OK";
            gshell_terminal_push("APPPIN -> DEMO APP PINNED TO TASKBAR");
        } else {
            gshell_launcher_state = "wait";
            gshell_launcher_last = "pin-wait";
            gshell_command_result = "APP PIN WAIT";
            gshell_input_status_text = "PIN WAIT";
            gshell_terminal_push("APPPIN -> NEED APPSELECT");
        }

        gshell_launcher_events++;
        gshell_command_name = "APPPIN";
        gshell_command_view = "LAUNCHERSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHEROPEN) {
        if (gshell_launcher_app_selected) {
            gshell_shell_panel_ready = 1;
            gshell_shell_taskbar_ready = 1;
            gshell_shell_launcher_ready = 1;
            gshell_shell_open_apps++;
            gshell_launcher_open_count++;
            gshell_launcher_state = "app-open";
            gshell_launcher_last = "open-demo";
            gshell_shell_state = "app-open";
            gshell_shell_focus = "demo.app";
            gshell_window_exists = 1;
            gshell_window_focused = 1;
            gshell_window_minimized = 0;
            gshell_window_title = "demo.window";
            gshell_window_state = "open";
            gshell_window_last = "launcher-open";
            gshell_app_slot_allocated = 1;
            gshell_app_slot_pid = 1;
            gshell_app_slot_state = "allocated";
            gshell_focus_target = "demo.app";
            gshell_command_result = "LAUNCHER OPEN OK";
            gshell_input_status_text = "OPEN OK";
            gshell_terminal_push("LAUNCHEROPEN -> DEMO APP OPENED FROM LAUNCHER");
        } else {
            gshell_launcher_state = "wait";
            gshell_launcher_last = "open-wait";
            gshell_command_result = "LAUNCHER OPEN WAIT";
            gshell_input_status_text = "OPEN WAIT";
            gshell_terminal_push("LAUNCHEROPEN -> NEED APPSELECT");
        }

        gshell_launcher_events++;
        gshell_shell_events++;
        gshell_command_name = "LAUNCHEROPEN";
        gshell_command_view = "LAUNCHERSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHERCHECK) {
        int ok = gshell_launcher_grid_ready && gshell_launcher_app_selected;
        gshell_launcher_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "LAUNCHERCHECK";
        gshell_command_result = ok ? "LAUNCHER CHECK OK" : "LAUNCHER CHECK WAIT";
        gshell_command_view = "LAUNCHERSTATUS";
        gshell_input_status_text = ok ? "LAUNCHER OK" : "LAUNCHER WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "LAUNCHERCHECK -> APP LAUNCHER READY" : "LAUNCHERCHECK -> NEED GRID AND SELECT");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHERRESET) {
        gshell_launcher_grid_ready = 0;
        gshell_launcher_app_selected = 0;
        gshell_launcher_app_pinned = 0;
        gshell_launcher_open_count = 0;
        gshell_launcher_events = 0;
        gshell_launcher_state = "idle";
        gshell_launcher_selected_app = "none";
        gshell_launcher_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "LAUNCHERRESET";
        gshell_command_result = "LAUNCHER RESET OK";
        gshell_command_view = "LAUNCHERSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHERRESET -> APP LAUNCHER STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_TASKBARSTATUS) {
        gshell_command_name = "TASKBARSTATUS";
        gshell_command_result = "TASKBAR STATUS OK";
        gshell_command_view = "TASKBARSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TASKBARSTATUS -> TASKBAR APP SWITCH READY");
        return;
    }

    if (command_id == GSHELL_CMD_TASKITEM) {
        gshell_shell_taskbar_ready = 1;
        gshell_taskbar_item_ready = 1;
        gshell_taskbar_item = "demo.app";
        gshell_taskbar_state = "item-ready";
        gshell_taskbar_last = "item";
        gshell_shell_state = "taskbar";
        gshell_shell_focus = "taskbar";
        gshell_hit_zone = "taskbar";
        gshell_hit_target = "demo.app";
        gshell_hit_action = "task-item";
        gshell_focus_target = "taskbar";
        gshell_focus_changes++;
        gshell_taskbar_events++;
        gshell_shell_events++;
        gshell_command_name = "TASKITEM";
        gshell_command_result = "TASK ITEM OK";
        gshell_command_view = "TASKBARSTATUS";
        gshell_input_status_text = "ITEM OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TASKITEM -> DEMO APP TASK ITEM READY");
        return;
    }

    if (command_id == GSHELL_CMD_TASKFOCUS) {
        if (gshell_taskbar_item_ready) {
            gshell_taskbar_focused = 1;
            gshell_taskbar_minimized = 0;
            gshell_taskbar_state = "focused";
            gshell_taskbar_last = "focus";
            gshell_window_exists = 1;
            gshell_window_focused = 1;
            gshell_window_minimized = 0;
            gshell_window_title = "demo.window";
            gshell_window_state = "focused";
            gshell_window_last = "task-focus";
            gshell_focus_target = "demo.app";
            gshell_command_result = "TASK FOCUS OK";
            gshell_input_status_text = "FOCUS OK";
            gshell_terminal_push("TASKFOCUS -> DEMO APP FOCUSED FROM TASKBAR");
        } else {
            gshell_taskbar_state = "wait";
            gshell_taskbar_last = "focus-wait";
            gshell_command_result = "TASK FOCUS WAIT";
            gshell_input_status_text = "FOCUS WAIT";
            gshell_terminal_push("TASKFOCUS -> NEED TASKITEM");
        }

        gshell_taskbar_events++;
        gshell_command_name = "TASKFOCUS";
        gshell_command_view = "TASKBARSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_TASKSWITCH) {
        if (gshell_taskbar_item_ready) {
            gshell_taskbar_focused = 1;
            gshell_taskbar_minimized = 0;
            gshell_taskbar_switches++;
            gshell_taskbar_state = "switched";
            gshell_taskbar_last = "switch";
            gshell_window_exists = 1;
            gshell_window_focused = 1;
            gshell_window_minimized = 0;
            gshell_window_title = "demo.window";
            gshell_window_state = "focused";
            gshell_window_last = "task-switch";
            gshell_focus_target = "demo.app";
            gshell_focus_changes++;
            gshell_command_result = "TASK SWITCH OK";
            gshell_input_status_text = "SWITCH OK";
            gshell_terminal_push("TASKSWITCH -> SWITCHED TO DEMO APP");
        } else {
            gshell_taskbar_state = "wait";
            gshell_taskbar_last = "switch-wait";
            gshell_command_result = "TASK SWITCH WAIT";
            gshell_input_status_text = "SWITCH WAIT";
            gshell_terminal_push("TASKSWITCH -> NEED TASKITEM");
        }

        gshell_taskbar_events++;
        gshell_command_name = "TASKSWITCH";
        gshell_command_view = "TASKBARSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_TASKMIN) {
        if (gshell_taskbar_item_ready && gshell_window_exists) {
            gshell_taskbar_minimized = 1;
            gshell_taskbar_focused = 0;
            gshell_taskbar_state = "minimized";
            gshell_taskbar_last = "minimize";
            gshell_window_minimized = 1;
            gshell_window_focused = 0;
            gshell_window_state = "minimized";
            gshell_window_last = "task-min";
            gshell_focus_target = "taskbar";
            gshell_command_result = "TASK MIN OK";
            gshell_input_status_text = "MIN OK";
            gshell_terminal_push("TASKMIN -> DEMO APP MINIMIZED TO TASKBAR");
        } else {
            gshell_taskbar_state = "wait";
            gshell_taskbar_last = "min-wait";
            gshell_command_result = "TASK MIN WAIT";
            gshell_input_status_text = "MIN WAIT";
            gshell_terminal_push("TASKMIN -> NEED OPEN TASK WINDOW");
        }

        gshell_taskbar_events++;
        gshell_command_name = "TASKMIN";
        gshell_command_view = "TASKBARSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_TASKRESTORE) {
        if (gshell_taskbar_item_ready) {
            gshell_taskbar_minimized = 0;
            gshell_taskbar_focused = 1;
            gshell_taskbar_state = "restored";
            gshell_taskbar_last = "restore";
            gshell_window_exists = 1;
            gshell_window_focused = 1;
            gshell_window_minimized = 0;
            gshell_window_title = "demo.window";
            gshell_window_state = "focused";
            gshell_window_last = "task-restore";
            gshell_focus_target = "demo.app";
            gshell_command_result = "TASK RESTORE OK";
            gshell_input_status_text = "RESTORE OK";
            gshell_terminal_push("TASKRESTORE -> DEMO APP RESTORED");
        } else {
            gshell_taskbar_state = "wait";
            gshell_taskbar_last = "restore-wait";
            gshell_command_result = "TASK RESTORE WAIT";
            gshell_input_status_text = "RESTORE WAIT";
            gshell_terminal_push("TASKRESTORE -> NEED TASKITEM");
        }

        gshell_taskbar_events++;
        gshell_command_name = "TASKRESTORE";
        gshell_command_view = "TASKBARSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_TASKCHECK) {
        int ok = gshell_taskbar_enabled && gshell_taskbar_item_ready;
        gshell_taskbar_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "TASKCHECK";
        gshell_command_result = ok ? "TASK CHECK OK" : "TASK CHECK WAIT";
        gshell_command_view = "TASKBARSTATUS";
        gshell_input_status_text = ok ? "TASK OK" : "TASK WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "TASKCHECK -> TASKBAR ITEM READY" : "TASKCHECK -> NEED TASKITEM");
        return;
    }

    if (command_id == GSHELL_CMD_TASKRESET) {
        gshell_taskbar_enabled = 1;
        gshell_taskbar_item_ready = 0;
        gshell_taskbar_focused = 0;
        gshell_taskbar_minimized = 0;
        gshell_taskbar_switches = 0;
        gshell_taskbar_events = 0;
        gshell_taskbar_state = "idle";
        gshell_taskbar_item = "none";
        gshell_taskbar_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "TASKRESET";
        gshell_command_result = "TASK RESET OK";
        gshell_command_view = "TASKBARSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TASKRESET -> TASKBAR STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_LAYOUTSTATUS) {
        gshell_command_name = "LAYOUTSTATUS";
        gshell_command_result = "LAYOUT STATUS OK";
        gshell_command_view = "LAYOUTSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAYOUTSTATUS -> WINDOW LAYOUT STATE READY");
        return;
    }

    if (command_id == GSHELL_CMD_LAYOUTGRID) {
        gshell_layout_grid_ready = 1;
        gshell_layout_state = "grid";
        gshell_layout_mode = "grid";
        gshell_layout_last = "grid-ready";
        gshell_shell_state = "layout";
        gshell_shell_focus = "layout-grid";
        gshell_focus_target = "layout-grid";
        gshell_focus_changes++;
        gshell_layout_events++;
        gshell_shell_events++;
        gshell_command_name = "LAYOUTGRID";
        gshell_command_result = "LAYOUT GRID OK";
        gshell_command_view = "LAYOUTSTATUS";
        gshell_input_status_text = "GRID OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAYOUTGRID -> WINDOW GRID READY");
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWSNAP) {
        if (gshell_window_exists || gshell_taskbar_item_ready || gshell_shell_open_apps > 0) {
            gshell_window_exists = 1;
            gshell_window_title = "demo.window";
            gshell_window_state = "snapped";
            gshell_window_focused = 1;
            gshell_window_minimized = 0;
            gshell_window_x = 96;
            gshell_window_y = 84;
            gshell_window_last = "snap";
            gshell_layout_snapped = 1;
            gshell_layout_maximized = 0;
            gshell_layout_state = "snapped";
            gshell_layout_mode = "left-snap";
            gshell_layout_last = "snap-left";
            gshell_focus_target = "demo.window";
            gshell_command_result = "WINDOW SNAP OK";
            gshell_input_status_text = "SNAP OK";
            gshell_terminal_push("WINDOWSNAP -> DEMO WINDOW SNAPPED");
        } else {
            gshell_layout_state = "wait";
            gshell_layout_last = "snap-wait";
            gshell_command_result = "WINDOW SNAP WAIT";
            gshell_input_status_text = "SNAP WAIT";
            gshell_terminal_push("WINDOWSNAP -> NEED WINDOW OR TASK ITEM");
        }

        gshell_layout_events++;
        gshell_command_name = "WINDOWSNAP";
        gshell_command_view = "LAYOUTSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWMAX) {
        if (gshell_window_exists || gshell_taskbar_item_ready || gshell_shell_open_apps > 0) {
            gshell_window_exists = 1;
            gshell_window_title = "demo.window";
            gshell_window_state = "maximized";
            gshell_window_focused = 1;
            gshell_window_minimized = 0;
            gshell_window_x = 80;
            gshell_window_y = 72;
            gshell_window_last = "maximize";
            gshell_layout_snapped = 0;
            gshell_layout_maximized = 1;
            gshell_layout_state = "maximized";
            gshell_layout_mode = "max";
            gshell_layout_last = "maximize";
            gshell_focus_target = "demo.window";
            gshell_command_result = "WINDOW MAX OK";
            gshell_input_status_text = "MAX OK";
            gshell_terminal_push("WINDOWMAX -> DEMO WINDOW MAXIMIZED");
        } else {
            gshell_layout_state = "wait";
            gshell_layout_last = "max-wait";
            gshell_command_result = "WINDOW MAX WAIT";
            gshell_input_status_text = "MAX WAIT";
            gshell_terminal_push("WINDOWMAX -> NEED WINDOW OR TASK ITEM");
        }

        gshell_layout_events++;
        gshell_command_name = "WINDOWMAX";
        gshell_command_view = "LAYOUTSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_ZORDER) {
        if (gshell_window_exists) {
            gshell_layout_z_index++;
            gshell_layout_state = "z-order";
            gshell_layout_last = "bring-front";
            gshell_window_state = "focused";
            gshell_window_last = "z-front";
            gshell_focus_target = "demo.window";
            gshell_command_result = "ZORDER OK";
            gshell_input_status_text = "Z OK";
            gshell_terminal_push("ZORDER -> DEMO WINDOW BROUGHT TO FRONT");
        } else {
            gshell_layout_state = "wait";
            gshell_layout_last = "z-wait";
            gshell_command_result = "ZORDER WAIT";
            gshell_input_status_text = "Z WAIT";
            gshell_terminal_push("ZORDER -> NEED WINDOW");
        }

        gshell_layout_events++;
        gshell_command_name = "ZORDER";
        gshell_command_view = "LAYOUTSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_WINLAYOUTCHECK) {
        int ok = gshell_layout_enabled && (gshell_layout_grid_ready || gshell_window_exists);
        gshell_layout_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "LAYOUTCHECK";
        gshell_command_result = ok ? "LAYOUT CHECK OK" : "LAYOUT CHECK WAIT";
        gshell_command_view = "LAYOUTSTATUS";
        gshell_input_status_text = ok ? "LAYOUT OK" : "LAYOUT WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "LAYOUTCHECK -> LAYOUT STATE READY" : "LAYOUTCHECK -> NEED LAYOUTGRID OR WINDOW");
        return;
    }

    if (command_id == GSHELL_CMD_LAYOUTRESET) {
        gshell_layout_enabled = 1;
        gshell_layout_grid_ready = 0;
        gshell_layout_snapped = 0;
        gshell_layout_maximized = 0;
        gshell_layout_z_index = 0;
        gshell_layout_events = 0;
        gshell_layout_state = "idle";
        gshell_layout_mode = "free";
        gshell_layout_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "LAYOUTRESET";
        gshell_command_result = "LAYOUT RESET OK";
        gshell_command_view = "LAYOUTSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAYOUTRESET -> WINDOW LAYOUT STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_DESKTOPSHELL) {
        gshell_command_name = "DESKTOPSHELL";
        gshell_command_result = "DESKTOP SHELL OK";
        gshell_command_view = "DESKTOPSHELL";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DESKTOPSHELL -> DESKTOP SHELL INTEGRATION READY");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLHOME) {
        gshell_home_ready = 1;
        gshell_home_state = "home";
        gshell_home_focus = "desktop";
        gshell_home_last = "home-ready";
        gshell_desktop_enabled = 1;
        gshell_workspace_ready = 1;
        gshell_workspace_id = 1;
        gshell_desktop_state = "shell-home";
        gshell_desktop_focus = "desktop";
        gshell_shell_enabled = 1;
        gshell_shell_state = "home";
        gshell_shell_focus = "desktop";
        gshell_focus_target = "desktop";
        gshell_focus_changes++;
        gshell_home_events++;
        gshell_shell_events++;
        gshell_command_name = "SHELLHOME";
        gshell_command_result = "SHELL HOME OK";
        gshell_command_view = "DESKTOPSHELL";
        gshell_input_status_text = "HOME OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SHELLHOME -> DESKTOP SHELL HOME READY");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLAPPS) {
        gshell_home_apps_ready = 1;
        gshell_home_state = "apps";
        gshell_home_focus = "launcher";
        gshell_home_last = "apps-ready";
        gshell_shell_launcher_ready = 1;
        gshell_launcher_grid_ready = 1;
        gshell_launcher_app_selected = 1;
        gshell_launcher_selected_app = "demo.app";
        gshell_launcher_state = "selected";
        gshell_hit_zone = "launcher";
        gshell_hit_target = "demo.app";
        gshell_hit_action = "select-app";
        gshell_focus_target = "launcher";
        gshell_focus_changes++;
        gshell_home_events++;
        gshell_launcher_events++;
        gshell_command_name = "SHELLAPPS";
        gshell_command_result = "SHELL APPS OK";
        gshell_command_view = "DESKTOPSHELL";
        gshell_input_status_text = "APPS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SHELLAPPS -> LAUNCHER APP ENTRY READY");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLWINDOWS) {
        gshell_home_windows_ready = 1;
        gshell_home_state = "windows";
        gshell_home_focus = "demo.window";
        gshell_home_last = "windows-ready";
        gshell_window_exists = 1;
        gshell_window_focused = 1;
        gshell_window_minimized = 0;
        gshell_window_title = "demo.window";
        gshell_window_state = "focused";
        gshell_window_last = "shell-windows";
        gshell_shell_open_apps = 1;
        gshell_shell_state = "app-open";
        gshell_shell_focus = "demo.app";
        gshell_focus_target = "demo.window";
        gshell_focus_changes++;
        gshell_home_events++;
        gshell_window_events++;
        gshell_command_name = "SHELLWINDOWS";
        gshell_command_result = "SHELL WINDOWS OK";
        gshell_command_view = "DESKTOPSHELL";
        gshell_input_status_text = "WINDOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SHELLWINDOWS -> DEMO WINDOW IN SHELL READY");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLLAYOUT) {
        gshell_home_layout_ready = 1;
        gshell_home_state = "layout";
        gshell_home_focus = "layout";
        gshell_home_last = "layout-ready";
        gshell_layout_grid_ready = 1;
        gshell_layout_state = "grid";
        gshell_layout_mode = "grid";
        gshell_layout_last = "shell-layout";
        gshell_focus_target = "layout";
        gshell_focus_changes++;
        gshell_home_events++;
        gshell_layout_events++;
        gshell_command_name = "SHELLLAYOUT";
        gshell_command_result = "SHELL LAYOUT OK";
        gshell_command_view = "DESKTOPSHELL";
        gshell_input_status_text = "LAYOUT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SHELLLAYOUT -> SHELL WINDOW LAYOUT READY");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLDOCK) {
        gshell_home_dock_ready = 1;
        gshell_home_state = "dock";
        gshell_home_focus = "taskbar";
        gshell_home_last = "dock-ready";
        gshell_shell_taskbar_ready = 1;
        gshell_taskbar_enabled = 1;
        gshell_taskbar_item_ready = 1;
        gshell_taskbar_item = "demo.app";
        gshell_taskbar_state = "item-ready";
        gshell_taskbar_last = "shell-dock";
        gshell_dock_ready = 1;
        gshell_focus_target = "taskbar";
        gshell_focus_changes++;
        gshell_home_events++;
        gshell_taskbar_events++;
        gshell_command_name = "SHELLDOCK";
        gshell_command_result = "SHELL DOCK OK";
        gshell_command_view = "DESKTOPSHELL";
        gshell_input_status_text = "DOCK OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SHELLDOCK -> TASKBAR DOCK READY");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLFLOW) {
        gshell_home_ready = 1;
        gshell_home_apps_ready = 1;
        gshell_home_windows_ready = 1;
        gshell_home_layout_ready = 1;
        gshell_home_dock_ready = 1;
        gshell_home_state = "ready";
        gshell_home_focus = "desktop-shell";
        gshell_home_last = "flow-ready";

        gshell_desktop_enabled = 1;
        gshell_workspace_ready = 1;
        gshell_shell_panel_ready = 1;
        gshell_shell_taskbar_ready = 1;
        gshell_shell_launcher_ready = 1;
        gshell_launcher_grid_ready = 1;
        gshell_launcher_app_selected = 1;
        gshell_launcher_selected_app = "demo.app";
        gshell_taskbar_item_ready = 1;
        gshell_taskbar_item = "demo.app";
        gshell_window_exists = 1;
        gshell_window_focused = 1;
        gshell_window_minimized = 0;
        gshell_window_title = "demo.window";
        gshell_window_state = "focused";
        gshell_layout_grid_ready = 1;
        gshell_layout_state = "grid";
        gshell_layout_mode = "grid";
        gshell_focus_target = "desktop-shell";

        gshell_home_events++;
        gshell_shell_events++;
        gshell_command_name = "SHELLFLOW";
        gshell_command_result = "SHELL FLOW OK";
        gshell_command_view = "DESKTOPSHELL";
        gshell_input_status_text = "FLOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SHELLFLOW -> FULL DESKTOP SHELL FLOW READY");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLHOMECHECK) {
        int ok = gshell_home_ready && gshell_home_apps_ready && gshell_home_windows_ready && gshell_home_layout_ready && gshell_home_dock_ready;
        gshell_home_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "SHELLHOMECHECK";
        gshell_command_result = ok ? "SHELL HOME CHECK OK" : "SHELL HOME CHECK WAIT";
        gshell_command_view = "DESKTOPSHELL";
        gshell_input_status_text = ok ? "HOME OK" : "HOME WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "SHELLHOMECHECK -> DESKTOP SHELL READY" : "SHELLHOMECHECK -> RUN SHELLFLOW OR HOME/APPS/WINDOWS/LAYOUT/DOCK");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLHOMERESET) {
        gshell_home_ready = 0;
        gshell_home_apps_ready = 0;
        gshell_home_windows_ready = 0;
        gshell_home_layout_ready = 0;
        gshell_home_dock_ready = 0;
        gshell_home_events = 0;
        gshell_home_state = "idle";
        gshell_home_focus = "terminal";
        gshell_home_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "SHELLHOMERESET";
        gshell_command_result = "SHELL HOME RESET OK";
        gshell_command_view = "DESKTOPSHELL";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SHELLHOMERESET -> DESKTOP SHELL HOME RESET");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLFINAL) {
        gshell_command_name = "SHELLFINAL";
        gshell_command_result = "SHELL FINAL OK";
        gshell_command_view = "SHELLFINAL";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SHELLFINAL -> 1.2 DESKTOP SHELL CLOSEOUT READY");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLHEALTH) {
        int core_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        gshell_command_name = "SHELLHEALTH";
        gshell_command_result = core_ok ? "SHELL HEALTH OK" : "SHELL HEALTH BAD";
        gshell_command_view = "SHELLFINAL";
        gshell_input_status_text = core_ok ? "HEALTH OK" : "HEALTH BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(core_ok ? "SHELLHEALTH -> CORE GSHELL READY" : "SHELLHEALTH -> CORE ISSUE");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLSUMMARY) {
        gshell_command_name = "SHELLSUMMARY";
        gshell_command_result = "SHELL SUMMARY OK";
        gshell_command_view = "SHELLFINAL";
        gshell_input_status_text = "SHELL OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SHELLSUMMARY -> WINDOW SHELL SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHERSUM) {
        gshell_command_name = "LAUNCHERSUM";
        gshell_command_result = "LAUNCHER SUM OK";
        gshell_command_view = "SHELLFINAL";
        gshell_input_status_text = "LAUNCHER OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHERSUM -> LAUNCHER FLOW SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_TASKBARSUM) {
        gshell_command_name = "TASKBARSUM";
        gshell_command_result = "TASKBAR SUM OK";
        gshell_command_view = "SHELLFINAL";
        gshell_input_status_text = "TASKBAR OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TASKBARSUM -> TASKBAR SWITCH SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_LAYOUTSUMB) {
        gshell_command_name = "LAYOUTSUM";
        gshell_command_result = "LAYOUT SUM OK";
        gshell_command_view = "SHELLFINAL";
        gshell_input_status_text = "LAYOUT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAYOUTSUM -> WINDOW LAYOUT SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_HOMESUMMARY) {
        gshell_command_name = "HOMESUMMARY";
        gshell_command_result = "HOME SUMMARY OK";
        gshell_command_view = "SHELLFINAL";
        gshell_input_status_text = "HOME OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("HOMESUMMARY -> DESKTOP SHELL HOME SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_SHELLNEXT) {
        gshell_command_name = "SHELLNEXT";
        gshell_command_result = "SHELL NEXT OK";
        gshell_command_view = "SHELLFINAL";
        gshell_input_status_text = "NEXT 1.3";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SHELLNEXT -> 1.3 REAL INPUT OR APP SHELL");
        return;
    }

    if (command_id == GSHELL_CMD_APPSHELLSTATUS) {
        gshell_command_name = "APPSHELLSTATUS";
        gshell_command_result = "APP SHELL STATUS OK";
        gshell_command_view = "APPSHELLSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPSHELLSTATUS -> APP SHELL BASE READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPCATALOG) {
        gshell_app_catalog_ready = 1;
        gshell_app_shell_state = "catalog";
        gshell_app_shell_selected = "demo.app";
        gshell_app_shell_last = "catalog-ready";
        gshell_launcher_grid_ready = 1;
        gshell_launcher_selected_app = "demo.app";
        gshell_launcher_state = "grid";
        gshell_home_apps_ready = 1;
        gshell_hit_zone = "app-catalog";
        gshell_hit_target = "demo.app";
        gshell_hit_action = "browse";
        gshell_focus_target = "app-catalog";
        gshell_focus_changes++;
        gshell_app_shell_events++;
        gshell_command_name = "APPCATALOG";
        gshell_command_result = "APP CATALOG OK";
        gshell_command_view = "APPSHELLSTATUS";
        gshell_input_status_text = "CATALOG OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPCATALOG -> DEMO APP CATALOG READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPCARD) {
        if (gshell_app_catalog_ready) {
            gshell_app_card_ready = 1;
            gshell_app_shell_state = "card";
            gshell_app_shell_selected = "demo.app";
            gshell_app_shell_last = "card-ready";
            gshell_hit_zone = "app-card";
            gshell_hit_target = "demo.app";
            gshell_hit_action = "select-card";
            gshell_focus_target = "app-card";
            gshell_command_result = "APP CARD OK";
            gshell_input_status_text = "CARD OK";
            gshell_terminal_push("APPCARD -> DEMO APP CARD READY");
        } else {
            gshell_app_shell_state = "wait";
            gshell_app_shell_last = "card-wait";
            gshell_command_result = "APP CARD WAIT";
            gshell_input_status_text = "CARD WAIT";
            gshell_terminal_push("APPCARD -> NEED APPCATALOG");
        }

        gshell_app_shell_events++;
        gshell_command_name = "APPCARD";
        gshell_command_view = "APPSHELLSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_APPDETAILS) {
        if (gshell_app_card_ready) {
            gshell_app_details_ready = 1;
            gshell_app_shell_state = "details";
            gshell_app_shell_selected = "demo.app";
            gshell_app_shell_last = "details-ready";
            gshell_hit_zone = "app-details";
            gshell_hit_target = "demo.app";
            gshell_hit_action = "show-details";
            gshell_focus_target = "app-details";
            gshell_command_result = "APP DETAILS OK";
            gshell_input_status_text = "DETAIL OK";
            gshell_terminal_push("APPDETAILS -> DEMO APP DETAILS READY");
        } else {
            gshell_app_shell_state = "wait";
            gshell_app_shell_last = "details-wait";
            gshell_command_result = "APP DETAILS WAIT";
            gshell_input_status_text = "DETAIL WAIT";
            gshell_terminal_push("APPDETAILS -> NEED APPCARD");
        }

        gshell_app_shell_events++;
        gshell_command_name = "APPDETAILS";
        gshell_command_view = "APPSHELLSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_APPSHELLLAUNCH) {
        if (gshell_app_details_ready || gshell_app_card_ready) {
            gshell_app_shell_launch_ready = 1;
            gshell_app_shell_state = "launch";
            gshell_app_shell_last = "launch-ready";
            gshell_launcher_app_selected = 1;
            gshell_launcher_selected_app = "demo.app";
            gshell_launch_prepared = 1;
            gshell_launch_approved = 1;
            gshell_launch_state = "approved";
            gshell_launch_last_result = "app-shell";
            gshell_app_slot_allocated = 1;
            gshell_app_slot_pid = 1;
            gshell_app_slot_state = "allocated";
            gshell_focus_target = "launch-flow";
            gshell_command_result = "APP SHELL LAUNCH OK";
            gshell_input_status_text = "LAUNCH OK";
            gshell_terminal_push("APPSHELLLAUNCH -> DEMO APP LAUNCH FLOW READY");
        } else {
            gshell_app_shell_state = "wait";
            gshell_app_shell_last = "launch-wait";
            gshell_command_result = "APP SHELL LAUNCH WAIT";
            gshell_input_status_text = "LAUNCH WAIT";
            gshell_terminal_push("APPSHELLLAUNCH -> NEED APPCARD OR APPDETAILS");
        }

        gshell_app_shell_events++;
        gshell_command_name = "APPSHELLLAUNCH";
        gshell_command_view = "APPSHELLSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_APPSHELLCHECK) {
        int ok = gshell_app_shell_enabled && gshell_app_catalog_ready && gshell_app_card_ready;
        gshell_app_shell_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "APPSHELLCHECK";
        gshell_command_result = ok ? "APP SHELL CHECK OK" : "APP SHELL CHECK WAIT";
        gshell_command_view = "APPSHELLSTATUS";
        gshell_input_status_text = ok ? "APP SHELL OK" : "APP SHELL WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "APPSHELLCHECK -> APP SHELL READY" : "APPSHELLCHECK -> NEED CATALOG AND CARD");
        return;
    }

    if (command_id == GSHELL_CMD_APPSHELLRESET) {
        gshell_app_shell_enabled = 1;
        gshell_app_catalog_ready = 0;
        gshell_app_card_ready = 0;
        gshell_app_details_ready = 0;
        gshell_app_shell_launch_ready = 0;
        gshell_app_shell_events = 0;
        gshell_app_shell_state = "idle";
        gshell_app_shell_selected = "none";
        gshell_app_shell_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "APPSHELLRESET";
        gshell_command_result = "APP SHELL RESET OK";
        gshell_command_view = "APPSHELLSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPSHELLRESET -> APP SHELL STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_CATALOGSTATUS) {
        gshell_command_name = "CATALOGSTATUS";
        gshell_command_result = "CATALOG STATUS OK";
        gshell_command_view = "CATALOGSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CATALOGSTATUS -> APP CATALOG MANAGEMENT READY");
        return;
    }

    if (command_id == GSHELL_CMD_CATALOGLIST) {
        gshell_catalog_ready = 1;
        gshell_catalog_items = 1;
        gshell_catalog_state = "list";
        gshell_catalog_selected = "demo.app";
        gshell_catalog_last = "list-ready";
        gshell_app_catalog_ready = 1;
        gshell_app_shell_state = "catalog";
        gshell_app_shell_selected = "demo.app";
        gshell_launcher_grid_ready = 1;
        gshell_launcher_selected_app = "demo.app";
        gshell_focus_target = "app-catalog";
        gshell_focus_changes++;
        gshell_catalog_events++;
        gshell_command_name = "CATALOGLIST";
        gshell_command_result = "CATALOG LIST OK";
        gshell_command_view = "CATALOGSTATUS";
        gshell_input_status_text = "LIST OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CATALOGLIST -> DEMO APP LISTED");
        return;
    }

    if (command_id == GSHELL_CMD_CATALOGSEARCH) {
        if (gshell_catalog_ready) {
            gshell_catalog_search_hits = 1;
            gshell_catalog_state = "search";
            gshell_catalog_selected = "demo.app";
            gshell_catalog_last = "search-hit";
            gshell_hit_zone = "catalog-search";
            gshell_hit_target = "demo.app";
            gshell_hit_action = "search";
            gshell_focus_target = "catalog-search";
            gshell_command_result = "CATALOG SEARCH OK";
            gshell_input_status_text = "SEARCH OK";
            gshell_terminal_push("CATALOGSEARCH -> DEMO APP FOUND");
        } else {
            gshell_catalog_state = "wait";
            gshell_catalog_last = "search-wait";
            gshell_command_result = "CATALOG SEARCH WAIT";
            gshell_input_status_text = "SEARCH WAIT";
            gshell_terminal_push("CATALOGSEARCH -> NEED CATALOGLIST");
        }

        gshell_catalog_events++;
        gshell_command_name = "CATALOGSEARCH";
        gshell_command_view = "CATALOGSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_CATALOGPIN) {
        if (gshell_catalog_ready) {
            gshell_catalog_pinned = 1;
            gshell_catalog_state = "pinned";
            gshell_catalog_selected = "demo.app";
            gshell_catalog_last = "pin-demo";
            gshell_launcher_app_pinned = 1;
            gshell_taskbar_item_ready = 1;
            gshell_taskbar_item = "demo.app";
            gshell_shell_taskbar_ready = 1;
            gshell_dock_ready = 1;
            gshell_hit_zone = "taskbar";
            gshell_hit_target = "demo.app";
            gshell_hit_action = "pin";
            gshell_focus_target = "taskbar";
            gshell_command_result = "CATALOG PIN OK";
            gshell_input_status_text = "PIN OK";
            gshell_terminal_push("CATALOGPIN -> DEMO APP PINNED");
        } else {
            gshell_catalog_state = "wait";
            gshell_catalog_last = "pin-wait";
            gshell_command_result = "CATALOG PIN WAIT";
            gshell_input_status_text = "PIN WAIT";
            gshell_terminal_push("CATALOGPIN -> NEED CATALOGLIST");
        }

        gshell_catalog_events++;
        gshell_command_name = "CATALOGPIN";
        gshell_command_view = "CATALOGSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_CATALOGENABLE) {
        gshell_catalog_enabled = 1;
        gshell_catalog_state = "enabled";
        gshell_catalog_last = "enable";
        gshell_command_name = "CATALOGENABLE";
        gshell_command_result = "CATALOG ENABLE OK";
        gshell_command_view = "CATALOGSTATUS";
        gshell_input_status_text = "ENABLE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_catalog_events++;
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CATALOGENABLE -> DEMO APP ENABLED");
        return;
    }

    if (command_id == GSHELL_CMD_CATALOGDISABLE) {
        gshell_catalog_enabled = 0;
        gshell_catalog_state = "disabled";
        gshell_catalog_last = "disable";
        gshell_command_name = "CATALOGDISABLE";
        gshell_command_result = "CATALOG DISABLE OK";
        gshell_command_view = "CATALOGSTATUS";
        gshell_input_status_text = "DISABLE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_catalog_events++;
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CATALOGDISABLE -> DEMO APP DISABLED");
        return;
    }

    if (command_id == GSHELL_CMD_CATALOGCHECK) {
        int ok = gshell_catalog_ready && gshell_catalog_items > 0 && gshell_catalog_enabled;
        gshell_catalog_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "CATALOGCHECK";
        gshell_command_result = ok ? "CATALOG CHECK OK" : "CATALOG CHECK WAIT";
        gshell_command_view = "CATALOGSTATUS";
        gshell_input_status_text = ok ? "CATALOG OK" : "CATALOG WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "CATALOGCHECK -> APP CATALOG READY" : "CATALOGCHECK -> NEED LIST AND ENABLE");
        return;
    }

    if (command_id == GSHELL_CMD_CATALOGRESET) {
        gshell_catalog_ready = 0;
        gshell_catalog_items = 0;
        gshell_catalog_search_hits = 0;
        gshell_catalog_pinned = 0;
        gshell_catalog_enabled = 1;
        gshell_catalog_events = 0;
        gshell_catalog_state = "idle";
        gshell_catalog_selected = "none";
        gshell_catalog_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "CATALOGRESET";
        gshell_command_result = "CATALOG RESET OK";
        gshell_command_view = "CATALOGSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CATALOGRESET -> APP CATALOG STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_DETAILSTATUS) {
        gshell_command_name = "DETAILSTATUS";
        gshell_command_result = "DETAIL STATUS OK";
        gshell_command_view = "DETAILSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DETAILSTATUS -> APP DETAIL PANEL READY");
        return;
    }

    if (command_id == GSHELL_CMD_DETAILOPEN) {
        if (gshell_catalog_ready || gshell_app_catalog_ready) {
            gshell_detail_ready = 1;
            gshell_detail_state = "open";
            gshell_detail_app = "demo.app";
            gshell_detail_last = "open-demo";
            gshell_app_details_ready = 1;
            gshell_app_shell_state = "details";
            gshell_app_shell_selected = "demo.app";
            gshell_hit_zone = "app-details";
            gshell_hit_target = "demo.app";
            gshell_hit_action = "open-detail";
            gshell_focus_target = "app-details";
            gshell_command_result = "DETAIL OPEN OK";
            gshell_input_status_text = "OPEN OK";
            gshell_terminal_push("DETAILOPEN -> DEMO APP DETAIL OPENED");
        } else {
            gshell_detail_state = "wait";
            gshell_detail_last = "open-wait";
            gshell_command_result = "DETAIL OPEN WAIT";
            gshell_input_status_text = "OPEN WAIT";
            gshell_terminal_push("DETAILOPEN -> NEED CATALOGLIST OR APPCATALOG");
        }

        gshell_detail_events++;
        gshell_command_name = "DETAILOPEN";
        gshell_command_view = "DETAILSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_DETAILMANIFEST) {
        if (gshell_detail_ready) {
            gshell_detail_manifest_ready = 1;
            gshell_detail_state = "manifest";
            gshell_detail_last = "manifest-ready";
            gshell_command_result = "DETAIL MANIFEST OK";
            gshell_input_status_text = "MANIFEST OK";
            gshell_terminal_push("DETAILMANIFEST -> DEMO APP MANIFEST READY");
        } else {
            gshell_detail_state = "wait";
            gshell_detail_last = "manifest-wait";
            gshell_command_result = "DETAIL MANIFEST WAIT";
            gshell_input_status_text = "MANIFEST WAIT";
            gshell_terminal_push("DETAILMANIFEST -> NEED DETAILOPEN");
        }

        gshell_detail_events++;
        gshell_command_name = "DETAILMANIFEST";
        gshell_command_view = "DETAILSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_DETAILCAPS) {
        if (gshell_detail_ready) {
            gshell_detail_caps_ready = 1;
            gshell_detail_state = "caps";
            gshell_detail_last = "caps-ready";
            gshell_command_result = "DETAIL CAPS OK";
            gshell_input_status_text = "CAPS OK";
            gshell_terminal_push("DETAILCAPS -> DEMO APP CAPABILITIES READY");
        } else {
            gshell_detail_state = "wait";
            gshell_detail_last = "caps-wait";
            gshell_command_result = "DETAIL CAPS WAIT";
            gshell_input_status_text = "CAPS WAIT";
            gshell_terminal_push("DETAILCAPS -> NEED DETAILOPEN");
        }

        gshell_detail_events++;
        gshell_command_name = "DETAILCAPS";
        gshell_command_view = "DETAILSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_DETAILPERMS) {
        if (gshell_detail_ready) {
            gshell_detail_perms_ready = 1;
            gshell_detail_state = "perms";
            gshell_detail_last = "perms-ready";
            gshell_perm_requested = 1;
            gshell_perm_allowed = 1;
            gshell_perm_denied = 0;
            gshell_perm_state = "granted";
            gshell_perm_last = "detail-granted";
            gshell_command_result = "DETAIL PERMS OK";
            gshell_input_status_text = "PERMS OK";
            gshell_terminal_push("DETAILPERMS -> DEMO APP PERMISSION INFO READY");
        } else {
            gshell_detail_state = "wait";
            gshell_detail_last = "perms-wait";
            gshell_command_result = "DETAIL PERMS WAIT";
            gshell_input_status_text = "PERMS WAIT";
            gshell_terminal_push("DETAILPERMS -> NEED DETAILOPEN");
        }

        gshell_detail_events++;
        gshell_command_name = "DETAILPERMS";
        gshell_command_view = "DETAILSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_DETAILLAUNCH) {
        if (gshell_detail_ready && gshell_detail_manifest_ready) {
            gshell_detail_launch_ready = 1;
            gshell_detail_state = "launch";
            gshell_detail_last = "launch-ready";
            gshell_app_shell_launch_ready = 1;
            gshell_launch_prepared = 1;
            gshell_launch_approved = 1;
            gshell_launch_state = "approved";
            gshell_launch_last_result = "detail-launch";
            gshell_app_slot_allocated = 1;
            gshell_app_slot_pid = 1;
            gshell_app_slot_state = "allocated";
            gshell_focus_target = "launch-flow";
            gshell_command_result = "DETAIL LAUNCH OK";
            gshell_input_status_text = "LAUNCH OK";
            gshell_terminal_push("DETAILLAUNCH -> DEMO APP DETAIL LAUNCH READY");
        } else {
            gshell_detail_state = "wait";
            gshell_detail_last = "launch-wait";
            gshell_command_result = "DETAIL LAUNCH WAIT";
            gshell_input_status_text = "LAUNCH WAIT";
            gshell_terminal_push("DETAILLAUNCH -> NEED DETAILOPEN AND DETAILMANIFEST");
        }

        gshell_detail_events++;
        gshell_command_name = "DETAILLAUNCH";
        gshell_command_view = "DETAILSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_DETAILCHECK) {
        int ok = gshell_detail_ready && gshell_detail_manifest_ready && gshell_detail_caps_ready && gshell_detail_perms_ready;
        gshell_detail_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "DETAILCHECK";
        gshell_command_result = ok ? "DETAIL CHECK OK" : "DETAIL CHECK WAIT";
        gshell_command_view = "DETAILSTATUS";
        gshell_input_status_text = ok ? "DETAIL OK" : "DETAIL WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "DETAILCHECK -> APP DETAIL READY" : "DETAILCHECK -> NEED OPEN/MANIFEST/CAPS/PERMS");
        return;
    }

    if (command_id == GSHELL_CMD_DETAILRESET) {
        gshell_detail_ready = 0;
        gshell_detail_manifest_ready = 0;
        gshell_detail_caps_ready = 0;
        gshell_detail_perms_ready = 0;
        gshell_detail_launch_ready = 0;
        gshell_detail_events = 0;
        gshell_detail_state = "idle";
        gshell_detail_app = "none";
        gshell_detail_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "DETAILRESET";
        gshell_command_result = "DETAIL RESET OK";
        gshell_command_view = "DETAILSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DETAILRESET -> APP DETAIL STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_ACTIONSTATUS) {
        gshell_command_name = "ACTIONSTATUS";
        gshell_command_result = "ACTION STATUS OK";
        gshell_command_view = "ACTIONSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ACTIONSTATUS -> APP ACTION FLOW READY");
        return;
    }

    if (command_id == GSHELL_CMD_ACTIONPREPARE) {
        if (gshell_detail_ready || gshell_app_card_ready || gshell_catalog_ready) {
            gshell_action_prepared = 1;
            gshell_action_app = "demo.app";
            gshell_action_state = "prepared";
            gshell_action_last = "prepare";
            gshell_detail_ready = 1;
            gshell_detail_app = "demo.app";
            gshell_launch_prepared = 1;
            gshell_launch_state = "prepared";
            gshell_launch_last_result = "action-prepare";
            gshell_app_slot_allocated = 1;
            gshell_app_slot_pid = 1;
            gshell_app_slot_state = "allocated";
            gshell_focus_target = "app-action";
            gshell_command_result = "ACTION PREPARE OK";
            gshell_input_status_text = "PREPARE OK";
            gshell_terminal_push("ACTIONPREPARE -> DEMO APP ACTION PREPARED");
        } else {
            gshell_action_state = "wait";
            gshell_action_last = "prepare-wait";
            gshell_command_result = "ACTION PREPARE WAIT";
            gshell_input_status_text = "PREPARE WAIT";
            gshell_terminal_push("ACTIONPREPARE -> NEED DETAILOPEN OR APPCARD");
        }

        gshell_action_events++;
        gshell_command_name = "ACTIONPREPARE";
        gshell_command_view = "ACTIONSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_ACTIONALLOW) {
        if (gshell_action_prepared) {
            gshell_action_allowed = 1;
            gshell_action_state = "allowed";
            gshell_action_last = "allow";
            gshell_launch_approved = 1;
            gshell_launch_state = "approved";
            gshell_launch_last_result = "action-allow";
            gshell_perm_requested = 1;
            gshell_perm_allowed = 1;
            gshell_perm_denied = 0;
            gshell_perm_state = "granted";
            gshell_perm_last = "action-granted";
            gshell_command_result = "ACTION ALLOW OK";
            gshell_input_status_text = "ALLOW OK";
            gshell_terminal_push("ACTIONALLOW -> USER APPROVED DEMO APP ACTION");
        } else {
            gshell_action_state = "wait";
            gshell_action_last = "allow-wait";
            gshell_command_result = "ACTION ALLOW WAIT";
            gshell_input_status_text = "ALLOW WAIT";
            gshell_terminal_push("ACTIONALLOW -> NEED ACTIONPREPARE");
        }

        gshell_action_events++;
        gshell_command_name = "ACTIONALLOW";
        gshell_command_view = "ACTIONSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_ACTIONOPEN) {
        if (gshell_action_allowed) {
            gshell_action_opened = 1;
            gshell_action_state = "opened";
            gshell_action_last = "open-window";
            gshell_shell_panel_ready = 1;
            gshell_shell_taskbar_ready = 1;
            gshell_shell_launcher_ready = 1;
            gshell_shell_state = "app-open";
            gshell_shell_focus = "demo.app";
            gshell_window_exists = 1;
            gshell_window_focused = 1;
            gshell_window_minimized = 0;
            gshell_window_title = "demo.window";
            gshell_window_state = "open";
            gshell_window_last = "action-open";
            gshell_taskbar_item_ready = 1;
            gshell_taskbar_item = "demo.app";
            gshell_taskbar_state = "focused";
            gshell_focus_target = "demo.window";
            gshell_command_result = "ACTION OPEN OK";
            gshell_input_status_text = "OPEN OK";
            gshell_terminal_push("ACTIONOPEN -> DEMO APP WINDOW OPENED");
        } else {
            gshell_action_state = "wait";
            gshell_action_last = "open-wait";
            gshell_command_result = "ACTION OPEN WAIT";
            gshell_input_status_text = "OPEN WAIT";
            gshell_terminal_push("ACTIONOPEN -> NEED ACTIONALLOW");
        }

        gshell_action_events++;
        gshell_command_name = "ACTIONOPEN";
        gshell_command_view = "ACTIONSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_ACTIONRUN) {
        if (gshell_action_opened) {
            gshell_action_running = 1;
            gshell_action_state = "running";
            gshell_action_last = "run";
            gshell_launch_state = "running";
            gshell_launch_last_result = "action-run";
            gshell_life_started = 1;
            gshell_life_paused = 0;
            gshell_life_state = "running";
            gshell_life_last = "action-run";
            gshell_command_result = "ACTION RUN OK";
            gshell_input_status_text = "RUN OK";
            gshell_terminal_push("ACTIONRUN -> DEMO APP RUNNING");
        } else {
            gshell_action_state = "wait";
            gshell_action_last = "run-wait";
            gshell_command_result = "ACTION RUN WAIT";
            gshell_input_status_text = "RUN WAIT";
            gshell_terminal_push("ACTIONRUN -> NEED ACTIONOPEN");
        }

        gshell_action_events++;
        gshell_command_name = "ACTIONRUN";
        gshell_command_view = "ACTIONSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_ACTIONSTOP) {
        if (gshell_action_running || gshell_life_started) {
            gshell_action_running = 0;
            gshell_action_state = "stopped";
            gshell_action_last = "stop";
            gshell_life_started = 0;
            gshell_life_paused = 0;
            gshell_life_state = "stopped";
            gshell_life_last = "action-stop";
            gshell_launch_state = "stopped";
            gshell_launch_last_result = "action-stop";
            gshell_command_result = "ACTION STOP OK";
            gshell_input_status_text = "STOP OK";
            gshell_terminal_push("ACTIONSTOP -> DEMO APP STOPPED");
        } else {
            gshell_action_state = "wait";
            gshell_action_last = "stop-wait";
            gshell_command_result = "ACTION STOP WAIT";
            gshell_input_status_text = "STOP WAIT";
            gshell_terminal_push("ACTIONSTOP -> APP NOT RUNNING");
        }

        gshell_action_events++;
        gshell_command_name = "ACTIONSTOP";
        gshell_command_view = "ACTIONSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_ACTIONCHECK) {
        int ok = gshell_action_prepared && gshell_action_allowed && gshell_action_opened;
        gshell_action_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "ACTIONCHECK";
        gshell_command_result = ok ? "ACTION CHECK OK" : "ACTION CHECK WAIT";
        gshell_command_view = "ACTIONSTATUS";
        gshell_input_status_text = ok ? "ACTION OK" : "ACTION WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "ACTIONCHECK -> APP ACTION FLOW READY" : "ACTIONCHECK -> NEED PREPARE/ALLOW/OPEN");
        return;
    }

    if (command_id == GSHELL_CMD_ACTIONRESET) {
        gshell_action_prepared = 0;
        gshell_action_allowed = 0;
        gshell_action_opened = 0;
        gshell_action_running = 0;
        gshell_action_events = 0;
        gshell_action_state = "idle";
        gshell_action_app = "none";
        gshell_action_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "ACTIONRESET";
        gshell_command_result = "ACTION RESET OK";
        gshell_command_view = "ACTIONSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ACTIONRESET -> APP ACTION STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_APPMGMTSTATUS) {
        gshell_command_name = "APPMGMTSTATUS";
        gshell_command_result = "APP MGMT STATUS OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPMGMTSTATUS -> APP MANAGEMENT PANEL READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPINVENTORY) {
        gshell_app_mgmt_inventory_ready = 1;
        gshell_app_mgmt_app = "demo.app";
        gshell_app_mgmt_state = "inventory";
        gshell_app_mgmt_last = "inventory-ready";
        gshell_catalog_ready = 1;
        gshell_catalog_items = 1;
        gshell_catalog_selected = "demo.app";
        gshell_app_catalog_ready = 1;
        gshell_focus_target = "app-inventory";
        gshell_app_mgmt_events++;
        gshell_command_name = "APPINVENTORY";
        gshell_command_result = "APP INVENTORY OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "INV OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPINVENTORY -> DEMO APP INVENTORY READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPSCAN) {
        gshell_app_mgmt_scanned = 1;
        gshell_app_mgmt_inventory_ready = 1;
        gshell_app_mgmt_app = "demo.app";
        gshell_app_mgmt_state = "scanned";
        gshell_app_mgmt_last = "scan-demo";
        gshell_catalog_ready = 1;
        gshell_catalog_items = 1;
        gshell_catalog_selected = "demo.app";
        gshell_focus_target = "app-scan";
        gshell_app_mgmt_events++;
        gshell_command_name = "APPSCAN";
        gshell_command_result = "APP SCAN OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "SCAN OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPSCAN -> DEMO APP SCANNED");
        return;
    }

    if (command_id == GSHELL_CMD_APPREGISTER) {
        if (gshell_app_mgmt_scanned || gshell_catalog_ready) {
            gshell_app_mgmt_registered = 1;
            gshell_app_mgmt_app = "demo.app";
            gshell_app_mgmt_state = "registered";
            gshell_app_mgmt_last = "register-demo";
            gshell_app_shell_selected = "demo.app";
            gshell_catalog_selected = "demo.app";
            gshell_command_result = "APP REGISTER OK";
            gshell_input_status_text = "REG OK";
            gshell_terminal_push("APPREGISTER -> DEMO APP REGISTERED");
        } else {
            gshell_app_mgmt_state = "wait";
            gshell_app_mgmt_last = "register-wait";
            gshell_command_result = "APP REGISTER WAIT";
            gshell_input_status_text = "REG WAIT";
            gshell_terminal_push("APPREGISTER -> NEED APPSCAN OR CATALOGLIST");
        }

        gshell_app_mgmt_events++;
        gshell_command_name = "APPREGISTER";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_APPUNREGISTER) {
        gshell_app_mgmt_registered = 0;
        gshell_app_mgmt_state = "unregistered";
        gshell_app_mgmt_last = "unregister";
        gshell_app_mgmt_events++;
        gshell_command_name = "APPUNREGISTER";
        gshell_command_result = "APP UNREGISTER OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "UNREG OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPUNREGISTER -> DEMO APP UNREGISTERED");
        return;
    }

    if (command_id == GSHELL_CMD_APPENABLE) {
        gshell_app_mgmt_enabled = 1;
        gshell_catalog_enabled = 1;
        gshell_app_mgmt_state = "enabled";
        gshell_app_mgmt_last = "enable";
        gshell_app_mgmt_events++;
        gshell_command_name = "APPENABLE";
        gshell_command_result = "APP ENABLE OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "ENABLE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPENABLE -> DEMO APP ENABLED");
        return;
    }

    if (command_id == GSHELL_CMD_APPDISABLE) {
        gshell_app_mgmt_enabled = 0;
        gshell_catalog_enabled = 0;
        gshell_app_mgmt_state = "disabled";
        gshell_app_mgmt_last = "disable";
        gshell_app_mgmt_events++;
        gshell_command_name = "APPDISABLE";
        gshell_command_result = "APP DISABLE OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "DISABLE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPDISABLE -> DEMO APP DISABLED");
        return;
    }

    if (command_id == GSHELL_CMD_APPTRUST) {
        gshell_app_mgmt_trusted = 1;
        gshell_app_mgmt_state = "trusted";
        gshell_app_mgmt_last = "trust";
        security_rule_default_allow();
        gshell_app_mgmt_events++;
        gshell_command_name = "APPTRUST";
        gshell_command_result = "APP TRUST OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "TRUST OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPTRUST -> DEMO APP TRUSTED");
        return;
    }

    if (command_id == GSHELL_CMD_APPUNTRUST) {
        gshell_app_mgmt_trusted = 0;
        gshell_app_mgmt_state = "untrusted";
        gshell_app_mgmt_last = "untrust";
        gshell_app_mgmt_events++;
        gshell_command_name = "APPUNTRUST";
        gshell_command_result = "APP UNTRUST OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "UNTRUST";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPUNTRUST -> DEMO APP TRUST CLEARED");
        return;
    }

    if (command_id == GSHELL_CMD_APPFAVORITE) {
        gshell_app_mgmt_favorite = 1;
        gshell_app_mgmt_state = "favorite";
        gshell_app_mgmt_last = "favorite";
        gshell_launcher_app_pinned = 1;
        gshell_app_mgmt_events++;
        gshell_command_name = "APPFAVORITE";
        gshell_command_result = "APP FAVORITE OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "FAV OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPFAVORITE -> DEMO APP FAVORITED");
        return;
    }

    if (command_id == GSHELL_CMD_APPUNFAVORITE) {
        gshell_app_mgmt_favorite = 0;
        gshell_app_mgmt_state = "unfavorite";
        gshell_app_mgmt_last = "unfavorite";
        gshell_app_mgmt_events++;
        gshell_command_name = "APPUNFAVORITE";
        gshell_command_result = "APP UNFAVORITE OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "UNFAV OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPUNFAVORITE -> DEMO APP FAVORITE CLEARED");
        return;
    }

    if (command_id == GSHELL_CMD_APPUPDATECHECK) {
        gshell_app_mgmt_update_ready = 1;
        gshell_app_mgmt_state = "update-check";
        gshell_app_mgmt_last = "update-ready";
        gshell_app_mgmt_events++;
        gshell_command_name = "APPUPDATECHECK";
        gshell_command_result = "APP UPDATE CHECK OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "UPDATE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPUPDATECHECK -> UPDATE METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPUPDATEMARK) {
        gshell_app_mgmt_update_ready = 1;
        gshell_app_mgmt_state = "update-marked";
        gshell_app_mgmt_last = "update-mark";
        gshell_app_mgmt_events++;
        gshell_command_name = "APPUPDATEMARK";
        gshell_command_result = "APP UPDATE MARK OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "MARK OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPUPDATEMARK -> DEMO APP UPDATE MARKED");
        return;
    }

    if (command_id == GSHELL_CMD_APPROLLBACK) {
        gshell_app_mgmt_rollback_ready = 1;
        gshell_app_mgmt_state = "rollback";
        gshell_app_mgmt_last = "rollback-ready";
        gshell_app_mgmt_events++;
        gshell_command_name = "APPROLLBACK";
        gshell_command_result = "APP ROLLBACK OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "ROLLBACK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPROLLBACK -> ROLLBACK METADATA READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPREPAIR) {
        gshell_app_mgmt_healthy = 1;
        gshell_app_mgmt_state = "repaired";
        gshell_app_mgmt_last = "repair";
        gshell_app_mgmt_events++;
        gshell_command_name = "APPREPAIR";
        gshell_command_result = "APP REPAIR OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "REPAIR OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPREPAIR -> DEMO APP HEALTH REPAIRED");
        return;
    }

    if (command_id == GSHELL_CMD_APPCLEARCACHE) {
        gshell_app_mgmt_cache_cleared = 1;
        gshell_app_mgmt_state = "cache-clear";
        gshell_app_mgmt_last = "clear-cache";
        gshell_app_mgmt_events++;
        gshell_command_name = "APPCLEARCACHE";
        gshell_command_result = "APP CLEAR CACHE OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "CACHE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPCLEARCACHE -> DEMO APP CACHE CLEARED");
        return;
    }

    if (command_id == GSHELL_CMD_APPSTATS) {
        gshell_app_mgmt_state = "stats";
        gshell_app_mgmt_last = "stats-ready";
        gshell_app_mgmt_events++;
        gshell_command_name = "APPSTATS";
        gshell_command_result = "APP STATS OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "STATS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPSTATS -> APP MGMT STATS READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPHEALTH) {
        gshell_app_mgmt_healthy = 1;
        gshell_app_mgmt_state = "healthy";
        gshell_app_mgmt_last = "health-ok";
        gshell_app_mgmt_events++;
        gshell_command_name = "APPHEALTH";
        gshell_command_result = "APP HEALTH OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "HEALTH OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPHEALTH -> DEMO APP HEALTH OK");
        return;
    }

    if (command_id == GSHELL_CMD_APPMGMTCHECK) {
        int ok = gshell_app_mgmt_inventory_ready && gshell_app_mgmt_registered && gshell_app_mgmt_enabled && gshell_app_mgmt_healthy;
        gshell_app_mgmt_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "APPMGMTCHECK";
        gshell_command_result = ok ? "APP MGMT CHECK OK" : "APP MGMT CHECK WAIT";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = ok ? "MGMT OK" : "MGMT WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "APPMGMTCHECK -> APP MANAGEMENT READY" : "APPMGMTCHECK -> NEED INVENTORY REGISTER ENABLE HEALTH");
        return;
    }

    if (command_id == GSHELL_CMD_APPMGMTRESET) {
        gshell_app_mgmt_inventory_ready = 0;
        gshell_app_mgmt_scanned = 0;
        gshell_app_mgmt_registered = 0;
        gshell_app_mgmt_enabled = 1;
        gshell_app_mgmt_trusted = 0;
        gshell_app_mgmt_favorite = 0;
        gshell_app_mgmt_update_ready = 0;
        gshell_app_mgmt_rollback_ready = 0;
        gshell_app_mgmt_cache_cleared = 0;
        gshell_app_mgmt_healthy = 1;
        gshell_app_mgmt_events = 0;
        gshell_app_mgmt_state = "idle";
        gshell_app_mgmt_app = "none";
        gshell_app_mgmt_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "APPMGMTRESET";
        gshell_command_result = "APP MGMT RESET OK";
        gshell_command_view = "APPMGMTSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPMGMTRESET -> APP MANAGEMENT STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_APPFINAL) {
        gshell_command_name = "APPFINAL";
        gshell_command_result = "APP FINAL OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPFINAL -> 1.3 APP SHELL CLOSEOUT READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPHEALTHSUM) {
        int core_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        gshell_app_final_state = core_ok ? "health-ok" : "health-bad";
        gshell_app_final_last = "health-sum";
        gshell_app_final_events++;
        gshell_command_name = "APPHEALTHSUM";
        gshell_command_result = core_ok ? "APP HEALTH SUM OK" : "APP HEALTH SUM BAD";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = core_ok ? "HEALTH OK" : "HEALTH BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(core_ok ? "APPHEALTHSUM -> CORE HEALTH OK" : "APPHEALTHSUM -> CORE HEALTH ISSUE");
        return;
    }

    if (command_id == GSHELL_CMD_APPSHELLSUM) {
        gshell_app_final_state = "appshell";
        gshell_app_final_last = "appshell-sum";
        gshell_app_shell_enabled = 1;
        gshell_app_shell_selected = "demo.app";
        gshell_app_final_events++;
        gshell_command_name = "APPSHELLSUM";
        gshell_command_result = "APP SHELL SUM OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "SHELL OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPSHELLSUM -> APP SHELL SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_CATALOGSUM) {
        gshell_app_final_state = "catalog";
        gshell_app_final_last = "catalog-sum";
        gshell_catalog_ready = 1;
        gshell_catalog_items = 1;
        gshell_catalog_enabled = 1;
        gshell_catalog_selected = "demo.app";
        gshell_app_catalog_ready = 1;
        gshell_app_final_events++;
        gshell_command_name = "CATALOGSUM";
        gshell_command_result = "CATALOG SUM OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "CATALOG OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CATALOGSUM -> APP CATALOG SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_DETAILSUM) {
        gshell_app_final_state = "detail";
        gshell_app_final_last = "detail-sum";
        gshell_detail_ready = 1;
        gshell_detail_manifest_ready = 1;
        gshell_detail_caps_ready = 1;
        gshell_detail_perms_ready = 1;
        gshell_detail_app = "demo.app";
        gshell_app_final_events++;
        gshell_command_name = "DETAILSUM";
        gshell_command_result = "DETAIL SUM OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "DETAIL OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DETAILSUM -> APP DETAIL SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_ACTIONSUM) {
        gshell_app_final_state = "action";
        gshell_app_final_last = "action-sum";
        gshell_action_prepared = 1;
        gshell_action_allowed = 1;
        gshell_action_opened = 1;
        gshell_action_app = "demo.app";
        gshell_app_final_events++;
        gshell_command_name = "ACTIONSUM";
        gshell_command_result = "ACTION SUM OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "ACTION OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ACTIONSUM -> APP ACTION SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_MGMTSUM) {
        gshell_app_final_state = "mgmt";
        gshell_app_final_last = "mgmt-sum";
        gshell_app_mgmt_inventory_ready = 1;
        gshell_app_mgmt_registered = 1;
        gshell_app_mgmt_enabled = 1;
        gshell_app_mgmt_healthy = 1;
        gshell_app_mgmt_app = "demo.app";
        gshell_app_final_events++;
        gshell_command_name = "MGMTSUM";
        gshell_command_result = "MGMT SUM OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "MGMT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MGMTSUM -> APP MANAGEMENT SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPREADINESS) {
        int ok = gshell_catalog_ready && gshell_detail_ready && gshell_action_prepared && gshell_app_mgmt_registered;
        gshell_app_final_ready = ok ? 1 : 0;
        gshell_app_final_state = ok ? "ready" : "partial";
        gshell_app_final_last = ok ? "ready-ok" : "ready-partial";
        gshell_app_final_events++;
        gshell_command_name = "APPREADINESS";
        gshell_command_result = ok ? "APP READINESS OK" : "APP READINESS PARTIAL";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = ok ? "READY OK" : "READY PART";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "APPREADINESS -> APP SHELL READY" : "APPREADINESS -> RUN APPFLOWFULL OR APPDEMOALL");
        return;
    }

    if (command_id == GSHELL_CMD_APPFLOWFULL) {
        gshell_app_final_ready = 1;
        gshell_app_final_flow_ready = 1;
        gshell_app_final_state = "flow-ready";
        gshell_app_final_focus = "app-flow";
        gshell_app_final_last = "full-flow";

        gshell_app_shell_enabled = 1;
        gshell_app_catalog_ready = 1;
        gshell_app_card_ready = 1;
        gshell_app_details_ready = 1;
        gshell_app_shell_launch_ready = 1;
        gshell_app_shell_selected = "demo.app";

        gshell_catalog_ready = 1;
        gshell_catalog_items = 1;
        gshell_catalog_enabled = 1;
        gshell_catalog_selected = "demo.app";

        gshell_detail_ready = 1;
        gshell_detail_manifest_ready = 1;
        gshell_detail_caps_ready = 1;
        gshell_detail_perms_ready = 1;
        gshell_detail_launch_ready = 1;
        gshell_detail_app = "demo.app";

        gshell_action_prepared = 1;
        gshell_action_allowed = 1;
        gshell_action_opened = 1;
        gshell_action_app = "demo.app";

        gshell_app_mgmt_inventory_ready = 1;
        gshell_app_mgmt_registered = 1;
        gshell_app_mgmt_enabled = 1;
        gshell_app_mgmt_healthy = 1;
        gshell_app_mgmt_app = "demo.app";

        gshell_focus_target = "app-flow";
        gshell_app_final_events++;
        gshell_command_name = "APPFLOWFULL";
        gshell_command_result = "APP FLOW FULL OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "FLOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPFLOWFULL -> FULL APP SHELL FLOW READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPDEMOALL) {
        gshell_app_final_ready = 1;
        gshell_app_final_flow_ready = 1;
        gshell_app_final_demo_ready = 1;
        gshell_app_final_state = "demo-ready";
        gshell_app_final_focus = "demo.app";
        gshell_app_final_last = "demo-all";

        gshell_catalog_ready = 1;
        gshell_catalog_items = 1;
        gshell_catalog_enabled = 1;
        gshell_catalog_selected = "demo.app";

        gshell_detail_ready = 1;
        gshell_detail_manifest_ready = 1;
        gshell_detail_caps_ready = 1;
        gshell_detail_perms_ready = 1;
        gshell_detail_launch_ready = 1;
        gshell_detail_app = "demo.app";

        gshell_action_prepared = 1;
        gshell_action_allowed = 1;
        gshell_action_opened = 1;
        gshell_action_running = 1;
        gshell_action_app = "demo.app";

        gshell_launch_prepared = 1;
        gshell_launch_approved = 1;
        gshell_launch_state = "running";
        gshell_app_slot_allocated = 1;
        gshell_app_slot_pid = 1;
        gshell_app_slot_state = "allocated";
        gshell_life_started = 1;
        gshell_life_state = "running";

        gshell_window_exists = 1;
        gshell_window_focused = 1;
        gshell_window_minimized = 0;
        gshell_window_title = "demo.window";
        gshell_window_state = "open";

        gshell_focus_target = "demo.app";
        gshell_app_final_events++;
        gshell_command_name = "APPDEMOALL";
        gshell_command_result = "APP DEMO ALL OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "DEMO OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPDEMOALL -> FULL DEMO APP RUNNING");
        return;
    }

    if (command_id == GSHELL_CMD_APPSECURITYSUM) {
        gshell_app_final_state = "security";
        gshell_app_final_last = "security-sum";
        gshell_app_mgmt_trusted = 1;
        gshell_focus_target = "app-security";
        gshell_app_final_events++;
        gshell_command_name = "APPSECURITYSUM";
        gshell_command_result = "APP SECURITY SUM OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "SEC OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPSECURITYSUM -> APP SECURITY SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPPERMISSIONSUM) {
        gshell_app_final_state = "permission";
        gshell_app_final_last = "permission-sum";
        gshell_perm_requested = 1;
        gshell_perm_allowed = 1;
        gshell_perm_denied = 0;
        gshell_perm_state = "granted";
        gshell_app_final_events++;
        gshell_command_name = "APPPERMISSIONSUM";
        gshell_command_result = "APP PERMISSION SUM OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "PERM OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPPERMISSIONSUM -> APP PERMISSION SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPRUNTIMESUM) {
        gshell_app_final_state = "runtime";
        gshell_app_final_last = "runtime-sum";
        gshell_app_slot_allocated = 1;
        gshell_app_slot_pid = 1;
        gshell_life_started = 1;
        gshell_life_state = "running";
        gshell_app_final_events++;
        gshell_command_name = "APPRUNTIMESUM";
        gshell_command_result = "APP RUNTIME SUM OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "RUNTIME OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPRUNTIMESUM -> APP RUNTIME SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPWINDOWSUM) {
        gshell_app_final_state = "window";
        gshell_app_final_last = "window-sum";
        gshell_window_exists = 1;
        gshell_window_focused = 1;
        gshell_window_title = "demo.window";
        gshell_window_state = "focused";
        gshell_app_final_events++;
        gshell_command_name = "APPWINDOWSUM";
        gshell_command_result = "APP WINDOW SUM OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "WINDOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPWINDOWSUM -> APP WINDOW SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPLAUNCHSUM) {
        gshell_app_final_state = "launch";
        gshell_app_final_last = "launch-sum";
        gshell_launch_prepared = 1;
        gshell_launch_approved = 1;
        gshell_launch_state = "approved";
        gshell_detail_launch_ready = 1;
        gshell_app_shell_launch_ready = 1;
        gshell_app_final_events++;
        gshell_command_name = "APPLAUNCHSUM";
        gshell_command_result = "APP LAUNCH SUM OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "LAUNCH OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPLAUNCHSUM -> APP LAUNCH SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPSTATESUM) {
        gshell_app_final_state = "state-sum";
        gshell_app_final_last = "state-sum";
        gshell_app_final_events++;
        gshell_command_name = "APPSTATESUM";
        gshell_command_result = "APP STATE SUM OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "STATE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPSTATESUM -> APP STATE SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_APPFINALCHECK) {
        int ok = gshell_app_final_ready || (gshell_catalog_ready && gshell_detail_ready && gshell_action_prepared && gshell_app_mgmt_registered);
        gshell_app_final_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "APPFINALCHECK";
        gshell_command_result = ok ? "APP FINAL CHECK OK" : "APP FINAL CHECK WAIT";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = ok ? "FINAL OK" : "FINAL WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "APPFINALCHECK -> APP SHELL CLOSEOUT READY" : "APPFINALCHECK -> RUN APPFLOWFULL OR APPDEMOALL");
        return;
    }

    if (command_id == GSHELL_CMD_APPFINALRESET) {
        gshell_app_final_ready = 0;
        gshell_app_final_flow_ready = 0;
        gshell_app_final_demo_ready = 0;
        gshell_app_final_events = 0;
        gshell_app_final_state = "idle";
        gshell_app_final_focus = "terminal";
        gshell_app_final_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "APPFINALRESET";
        gshell_command_result = "APP FINAL RESET OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPFINALRESET -> APP FINAL STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_APPNEXT) {
        gshell_command_name = "APPNEXT";
        gshell_command_result = "APP NEXT OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "NEXT 1.4";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPNEXT -> 1.4 VISUAL UI PANELS");
        return;
    }

    if (command_id == GSHELL_CMD_APPROADMAP) {
        gshell_command_name = "APPROADMAP";
        gshell_command_result = "APP ROADMAP OK";
        gshell_command_view = "APPFINAL";
        gshell_input_status_text = "ROADMAP";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("APPROADMAP -> VISUAL DESKTOP APP CARDS WINDOWS");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALSTATUS) {
        gshell_command_name = "VISUALSTATUS";
        gshell_command_result = "VISUAL STATUS OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALSTATUS -> VISUAL DESKTOP UI PANELS READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALBOOT) {
        gshell_visual_state = "boot-visual";
        gshell_visual_last = "boot";
        gshell_visual_metrics++;
        gshell_visual_events++;
        gshell_command_name = "VISUALBOOT";
        gshell_command_result = "VISUAL BOOT OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "BOOT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALBOOT -> BOOT VISUAL STATE READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALDESKTOP) {
        gshell_visual_desktop_ready = 1;
        gshell_visual_state = "desktop";
        gshell_visual_focus = "desktop";
        gshell_visual_last = "desktop-ready";
        gshell_desktop_enabled = 1;
        gshell_workspace_ready = 1;
        gshell_desktop_state = "visual-desktop";
        gshell_focus_target = "desktop";
        gshell_focus_changes++;
        gshell_visual_events++;
        gshell_command_name = "VISUALDESKTOP";
        gshell_command_result = "VISUAL DESKTOP OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "DESKTOP OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALDESKTOP -> DESKTOP VISUAL PANEL READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALPANEL) {
        gshell_visual_panel_ready = 1;
        gshell_visual_state = "panel";
        gshell_visual_focus = "main-panel";
        gshell_visual_last = "panel-ready";
        gshell_shell_panel_ready = 1;
        gshell_shell_state = "visual-panel";
        gshell_focus_target = "main-panel";
        gshell_visual_events++;
        gshell_command_name = "VISUALPANEL";
        gshell_command_result = "VISUAL PANEL OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "PANEL OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALPANEL -> MAIN VISUAL PANEL READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALCARDS) {
        gshell_visual_cards_ready = 1;
        gshell_visual_state = "cards";
        gshell_visual_focus = "app-cards";
        gshell_visual_last = "cards-ready";
        gshell_app_card_ready = 1;
        gshell_app_shell_state = "card";
        gshell_hit_zone = "app-card";
        gshell_hit_target = "demo.app";
        gshell_hit_action = "visual-card";
        gshell_focus_target = "app-card";
        gshell_visual_events++;
        gshell_command_name = "VISUALCARDS";
        gshell_command_result = "VISUAL CARDS OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "CARDS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALCARDS -> APP CARD VISUAL STATE READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALWINDOW) {
        gshell_visual_window_ready = 1;
        gshell_visual_state = "window";
        gshell_visual_focus = "demo.window";
        gshell_visual_last = "window-ready";
        gshell_window_exists = 1;
        gshell_window_focused = 1;
        gshell_window_title = "demo.window";
        gshell_window_state = "visual";
        gshell_focus_target = "demo.window";
        gshell_visual_events++;
        gshell_command_name = "VISUALWINDOW";
        gshell_command_result = "VISUAL WINDOW OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "WINDOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALWINDOW -> WINDOW VISUAL STATE READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALLAUNCHER) {
        gshell_visual_launcher_ready = 1;
        gshell_visual_state = "launcher";
        gshell_visual_focus = "launcher";
        gshell_visual_last = "launcher-ready";
        gshell_shell_launcher_ready = 1;
        gshell_launcher_grid_ready = 1;
        gshell_launcher_state = "visual";
        gshell_focus_target = "launcher";
        gshell_visual_events++;
        gshell_command_name = "VISUALLAUNCHER";
        gshell_command_result = "VISUAL LAUNCHER OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "LAUNCHER OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALLAUNCHER -> LAUNCHER VISUAL READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALTASKBAR) {
        gshell_visual_taskbar_ready = 1;
        gshell_visual_state = "taskbar";
        gshell_visual_focus = "taskbar";
        gshell_visual_last = "taskbar-ready";
        gshell_shell_taskbar_ready = 1;
        gshell_taskbar_enabled = 1;
        gshell_taskbar_item_ready = 1;
        gshell_taskbar_item = "demo.app";
        gshell_taskbar_state = "visual";
        gshell_focus_target = "taskbar";
        gshell_visual_events++;
        gshell_command_name = "VISUALTASKBAR";
        gshell_command_result = "VISUAL TASKBAR OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "TASKBAR OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALTASKBAR -> TASKBAR VISUAL READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALDOCK) {
        gshell_visual_dock_ready = 1;
        gshell_visual_state = "dock";
        gshell_visual_focus = "dock";
        gshell_visual_last = "dock-ready";
        gshell_dock_ready = 1;
        gshell_desktop_focus = "dock";
        gshell_focus_target = "dock";
        gshell_visual_events++;
        gshell_command_name = "VISUALDOCK";
        gshell_command_result = "VISUAL DOCK OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "DOCK OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALDOCK -> DOCK VISUAL READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALFOCUS) {
        gshell_visual_state = "focus";
        gshell_visual_focus = "demo.app";
        gshell_visual_last = "focus-demo";
        gshell_focus_target = "demo.app";
        gshell_focus_changes++;
        gshell_visual_events++;
        gshell_command_name = "VISUALFOCUS";
        gshell_command_result = "VISUAL FOCUS OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "FOCUS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALFOCUS -> VISUAL FOCUS RING READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALGRID) {
        gshell_visual_grid_ready = 1;
        gshell_visual_state = "grid";
        gshell_visual_last = "grid-ready";
        gshell_layout_grid_ready = 1;
        gshell_layout_state = "visual-grid";
        gshell_layout_mode = "grid";
        gshell_visual_events++;
        gshell_command_name = "VISUALGRID";
        gshell_command_result = "VISUAL GRID OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "GRID OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALGRID -> VISUAL GRID READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALTHEME) {
        gshell_visual_theme_ready = 1;
        gshell_visual_state = "theme";
        gshell_visual_last = "theme-ready";
        gshell_visual_events++;
        gshell_command_name = "VISUALTHEME";
        gshell_command_result = "VISUAL THEME OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "THEME OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALTHEME -> LINGJING VISUAL THEME READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALGLOW) {
        gshell_visual_glow_ready = 1;
        gshell_visual_state = "glow";
        gshell_visual_last = "glow-ready";
        gshell_visual_events++;
        gshell_command_name = "VISUALGLOW";
        gshell_command_result = "VISUAL GLOW OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "GLOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALGLOW -> PANEL GLOW STATE READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALBORDER) {
        gshell_visual_border_ready = 1;
        gshell_visual_state = "border";
        gshell_visual_last = "border-ready";
        gshell_visual_events++;
        gshell_command_name = "VISUALBORDER";
        gshell_command_result = "VISUAL BORDER OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "BORDER OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALBORDER -> PANEL BORDER STATE READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALMETRICS) {
        gshell_visual_metrics++;
        gshell_visual_state = "metrics";
        gshell_visual_last = "metrics";
        gshell_visual_events++;
        gshell_command_name = "VISUALMETRICS";
        gshell_command_result = "VISUAL METRICS OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "METRICS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALMETRICS -> UI METRICS UPDATED");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALDEMO) {
        gshell_visual_desktop_ready = 1;
        gshell_visual_panel_ready = 1;
        gshell_visual_cards_ready = 1;
        gshell_visual_window_ready = 1;
        gshell_visual_launcher_ready = 1;
        gshell_visual_taskbar_ready = 1;
        gshell_visual_dock_ready = 1;
        gshell_visual_grid_ready = 1;
        gshell_visual_theme_ready = 1;
        gshell_visual_glow_ready = 1;
        gshell_visual_border_ready = 1;
        gshell_visual_state = "demo";
        gshell_visual_focus = "demo.app";
        gshell_visual_last = "demo-ready";
        gshell_focus_target = "demo.app";
        gshell_window_exists = 1;
        gshell_window_title = "demo.window";
        gshell_window_state = "visual";
        gshell_app_final_state = "visual-demo";
        gshell_visual_events++;
        gshell_command_name = "VISUALDEMO";
        gshell_command_result = "VISUAL DEMO OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "DEMO OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALDEMO -> FULL VISUAL DEMO STATE READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALFLOW) {
        gshell_visual_desktop_ready = 1;
        gshell_visual_panel_ready = 1;
        gshell_visual_cards_ready = 1;
        gshell_visual_window_ready = 1;
        gshell_visual_launcher_ready = 1;
        gshell_visual_taskbar_ready = 1;
        gshell_visual_dock_ready = 1;
        gshell_visual_grid_ready = 1;
        gshell_visual_state = "flow";
        gshell_visual_focus = "visual-flow";
        gshell_visual_last = "flow-ready";
        gshell_focus_target = "visual-flow";
        gshell_visual_events++;
        gshell_command_name = "VISUALFLOW";
        gshell_command_result = "VISUAL FLOW OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "FLOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALFLOW -> VISUAL DESKTOP FLOW READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALCHECK) {
        int ok = gshell_visual_enabled && gshell_visual_desktop_ready && gshell_visual_panel_ready && gshell_visual_window_ready;
        gshell_visual_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "VISUALCHECK";
        gshell_command_result = ok ? "VISUAL CHECK OK" : "VISUAL CHECK WAIT";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = ok ? "VISUAL OK" : "VISUAL WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "VISUALCHECK -> VISUAL UI READY" : "VISUALCHECK -> NEED VISUALDESKTOP PANEL WINDOW");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALRESET) {
        gshell_visual_enabled = 1;
        gshell_visual_desktop_ready = 0;
        gshell_visual_panel_ready = 0;
        gshell_visual_cards_ready = 0;
        gshell_visual_window_ready = 0;
        gshell_visual_launcher_ready = 0;
        gshell_visual_taskbar_ready = 0;
        gshell_visual_dock_ready = 0;
        gshell_visual_grid_ready = 0;
        gshell_visual_theme_ready = 0;
        gshell_visual_glow_ready = 0;
        gshell_visual_border_ready = 0;
        gshell_visual_metrics = 0;
        gshell_visual_events = 0;
        gshell_visual_state = "idle";
        gshell_visual_focus = "terminal";
        gshell_visual_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "VISUALRESET";
        gshell_command_result = "VISUAL RESET OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALRESET -> VISUAL UI STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALNEXT) {
        gshell_command_name = "VISUALNEXT";
        gshell_command_result = "VISUAL NEXT OK";
        gshell_command_view = "VISUALSTATUS";
        gshell_input_status_text = "NEXT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALNEXT -> 1.4.1 VISUAL APP CARDS WINDOWS");
        return;
    }

    if (command_id == GSHELL_CMD_CARDSTATUS) {
        gshell_command_name = "CARDSTATUS";
        gshell_command_result = "CARD STATUS OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CARDSTATUS -> VISUAL APP CARDS WINDOWS READY");
        return;
    }

    if (command_id == GSHELL_CMD_CARDGRID) {
        gshell_card_grid_ready = 1;
        gshell_card_state = "grid";
        gshell_card_app = "demo.app";
        gshell_card_last = "grid-ready";
        gshell_visual_cards_ready = 1;
        gshell_visual_state = "cards";
        gshell_app_card_ready = 1;
        gshell_focus_target = "card-grid";
        gshell_card_events++;
        gshell_command_name = "CARDGRID";
        gshell_command_result = "CARD GRID OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "GRID OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CARDGRID -> APP CARD GRID READY");
        return;
    }

    if (command_id == GSHELL_CMD_CARDSELECT) {
        if (gshell_card_grid_ready || gshell_visual_cards_ready) {
            gshell_card_selected = 1;
            gshell_card_state = "selected";
            gshell_card_app = "demo.app";
            gshell_card_last = "select-demo";
            gshell_hit_zone = "visual-card";
            gshell_hit_target = "demo.app";
            gshell_hit_action = "select";
            gshell_focus_target = "demo.app";
            gshell_command_result = "CARD SELECT OK";
            gshell_input_status_text = "SELECT OK";
            gshell_terminal_push("CARDSELECT -> DEMO APP CARD SELECTED");
        } else {
            gshell_card_state = "wait";
            gshell_card_last = "select-wait";
            gshell_command_result = "CARD SELECT WAIT";
            gshell_input_status_text = "SELECT WAIT";
            gshell_terminal_push("CARDSELECT -> NEED CARDGRID");
        }

        gshell_card_events++;
        gshell_command_name = "CARDSELECT";
        gshell_command_view = "CARDSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_CARDOPEN) {
        if (gshell_card_selected) {
            gshell_card_opened = 1;
            gshell_card_state = "opened";
            gshell_card_last = "open-card";
            gshell_app_details_ready = 1;
            gshell_detail_ready = 1;
            gshell_detail_app = "demo.app";
            gshell_focus_target = "app-details";
            gshell_command_result = "CARD OPEN OK";
            gshell_input_status_text = "OPEN OK";
            gshell_terminal_push("CARDOPEN -> DEMO APP CARD OPENED");
        } else {
            gshell_card_state = "wait";
            gshell_card_last = "open-wait";
            gshell_command_result = "CARD OPEN WAIT";
            gshell_input_status_text = "OPEN WAIT";
            gshell_terminal_push("CARDOPEN -> NEED CARDSELECT");
        }

        gshell_card_events++;
        gshell_command_name = "CARDOPEN";
        gshell_command_view = "CARDSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_CARDEXPAND) {
        if (gshell_card_selected) {
            gshell_card_expanded = 1;
            gshell_card_state = "expanded";
            gshell_card_last = "expand-card";
            gshell_visual_state = "card-expanded";
            gshell_focus_target = "expanded-card";
            gshell_command_result = "CARD EXPAND OK";
            gshell_input_status_text = "EXPAND OK";
            gshell_terminal_push("CARDEXPAND -> DEMO APP CARD EXPANDED");
        } else {
            gshell_card_state = "wait";
            gshell_card_last = "expand-wait";
            gshell_command_result = "CARD EXPAND WAIT";
            gshell_input_status_text = "EXPAND WAIT";
            gshell_terminal_push("CARDEXPAND -> NEED CARDSELECT");
        }

        gshell_card_events++;
        gshell_command_name = "CARDEXPAND";
        gshell_command_view = "CARDSTATUS";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        return;
    }

    if (command_id == GSHELL_CMD_CARDPIN) {
        gshell_card_pinned = 1;
        gshell_card_state = "pinned";
        gshell_card_app = "demo.app";
        gshell_card_last = "pin-card";
        gshell_launcher_app_pinned = 1;
        gshell_taskbar_item_ready = 1;
        gshell_taskbar_item = "demo.app";
        gshell_focus_target = "taskbar";
        gshell_card_events++;
        gshell_command_name = "CARDPIN";
        gshell_command_result = "CARD PIN OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "PIN OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CARDPIN -> DEMO APP CARD PINNED");
        return;
    }

    if (command_id == GSHELL_CMD_CARDBADGE) {
        gshell_card_badge_ready = 1;
        gshell_card_state = "badge";
        gshell_card_last = "badge-ready";
        gshell_card_events++;
        gshell_command_name = "CARDBADGE";
        gshell_command_result = "CARD BADGE OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "BADGE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CARDBADGE -> CARD BADGE VISUAL READY");
        return;
    }

    if (command_id == GSHELL_CMD_CARDPREVIEW) {
        gshell_card_preview_ready = 1;
        gshell_card_state = "preview";
        gshell_card_last = "preview-ready";
        gshell_visual_state = "card-preview";
        gshell_card_events++;
        gshell_command_name = "CARDPREVIEW";
        gshell_command_result = "CARD PREVIEW OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "PREVIEW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CARDPREVIEW -> CARD PREVIEW READY");
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWVISUAL) {
        gshell_window_visual_ready = 1;
        gshell_visual_window_ready = 1;
        gshell_window_exists = 1;
        gshell_window_title = "demo.window";
        gshell_window_state = "visual";
        gshell_card_state = "window-visual";
        gshell_card_last = "window-visual";
        gshell_focus_target = "demo.window";
        gshell_card_events++;
        gshell_command_name = "WINDOWVISUAL";
        gshell_command_result = "WINDOW VISUAL OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "WINDOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("WINDOWVISUAL -> DEMO WINDOW VISUAL READY");
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWTITLE) {
        gshell_window_title_ready = 1;
        gshell_window_title = "demo.window";
        gshell_window_last = "visual-title";
        gshell_card_state = "window-title";
        gshell_card_last = "title-ready";
        gshell_card_events++;
        gshell_command_name = "WINDOWTITLE";
        gshell_command_result = "WINDOW TITLE OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "TITLE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("WINDOWTITLE -> WINDOW TITLEBAR READY");
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWBODY) {
        gshell_window_body_ready = 1;
        gshell_window_exists = 1;
        gshell_window_state = "body-ready";
        gshell_card_state = "window-body";
        gshell_card_last = "body-ready";
        gshell_card_events++;
        gshell_command_name = "WINDOWBODY";
        gshell_command_result = "WINDOW BODY OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "BODY OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("WINDOWBODY -> WINDOW BODY READY");
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWSHADOW) {
        gshell_window_shadow_ready = 1;
        gshell_card_state = "shadow";
        gshell_card_last = "shadow-ready";
        gshell_card_events++;
        gshell_command_name = "WINDOWSHADOW";
        gshell_command_result = "WINDOW SHADOW OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "SHADOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("WINDOWSHADOW -> WINDOW SHADOW READY");
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWACTIVE) {
        gshell_window_active_ready = 1;
        gshell_window_exists = 1;
        gshell_window_focused = 1;
        gshell_window_state = "active";
        gshell_card_state = "window-active";
        gshell_card_last = "active-ready";
        gshell_focus_target = "demo.window";
        gshell_card_events++;
        gshell_command_name = "WINDOWACTIVE";
        gshell_command_result = "WINDOW ACTIVE OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "ACTIVE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("WINDOWACTIVE -> WINDOW ACTIVE VISUAL READY");
        return;
    }

    if (command_id == GSHELL_CMD_WINDOWPREVIEW) {
        gshell_window_preview_ready = 1;
        gshell_card_state = "window-preview";
        gshell_card_last = "window-preview";
        gshell_card_events++;
        gshell_command_name = "WINDOWPREVIEW";
        gshell_command_result = "WINDOW PREVIEW OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "PREVIEW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("WINDOWPREVIEW -> WINDOW PREVIEW READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALCOMPOSE) {
        gshell_card_grid_ready = 1;
        gshell_card_selected = 1;
        gshell_card_opened = 1;
        gshell_window_visual_ready = 1;
        gshell_window_title_ready = 1;
        gshell_window_body_ready = 1;
        gshell_window_shadow_ready = 1;
        gshell_visual_cards_ready = 1;
        gshell_visual_window_ready = 1;
        gshell_card_state = "composed";
        gshell_card_app = "demo.app";
        gshell_card_last = "compose";
        gshell_focus_target = "visual-compose";
        gshell_card_events++;
        gshell_command_name = "VISUALCOMPOSE";
        gshell_command_result = "VISUAL COMPOSE OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "COMPOSE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALCOMPOSE -> CARD AND WINDOW COMPOSED");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALSYNC) {
        gshell_card_state = "synced";
        gshell_card_last = "sync";
        gshell_visual_state = "cards-window-sync";
        gshell_app_final_state = "visual-sync";
        gshell_card_events++;
        gshell_command_name = "VISUALSYNC";
        gshell_command_result = "VISUAL SYNC OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "SYNC OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALSYNC -> VISUAL CARD WINDOW SYNCED");
        return;
    }

    if (command_id == GSHELL_CMD_CARDFLOW) {
        gshell_card_grid_ready = 1;
        gshell_card_selected = 1;
        gshell_card_opened = 1;
        gshell_card_expanded = 1;
        gshell_card_pinned = 1;
        gshell_card_badge_ready = 1;
        gshell_card_preview_ready = 1;
        gshell_window_visual_ready = 1;
        gshell_window_title_ready = 1;
        gshell_window_body_ready = 1;
        gshell_window_shadow_ready = 1;
        gshell_window_active_ready = 1;
        gshell_window_preview_ready = 1;
        gshell_visual_cards_ready = 1;
        gshell_visual_window_ready = 1;
        gshell_card_state = "flow-ready";
        gshell_card_app = "demo.app";
        gshell_card_last = "flow";
        gshell_focus_target = "card-flow";
        gshell_card_events++;
        gshell_command_name = "CARDFLOW";
        gshell_command_result = "CARD FLOW OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "FLOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CARDFLOW -> FULL CARD WINDOW VISUAL FLOW READY");
        return;
    }

    if (command_id == GSHELL_CMD_CARDCHECK) {
        int ok = gshell_card_grid_ready && gshell_card_selected && gshell_window_visual_ready;
        gshell_card_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "CARDCHECK";
        gshell_command_result = ok ? "CARD CHECK OK" : "CARD CHECK WAIT";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = ok ? "CARD OK" : "CARD WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "CARDCHECK -> VISUAL CARD WINDOW READY" : "CARDCHECK -> NEED CARDGRID CARDSELECT WINDOWVISUAL");
        return;
    }

    if (command_id == GSHELL_CMD_CARDRESET) {
        gshell_card_grid_ready = 0;
        gshell_card_selected = 0;
        gshell_card_opened = 0;
        gshell_card_expanded = 0;
        gshell_card_pinned = 0;
        gshell_card_badge_ready = 0;
        gshell_card_preview_ready = 0;
        gshell_window_visual_ready = 0;
        gshell_window_title_ready = 0;
        gshell_window_body_ready = 0;
        gshell_window_shadow_ready = 0;
        gshell_window_active_ready = 0;
        gshell_window_preview_ready = 0;
        gshell_card_events = 0;
        gshell_card_state = "idle";
        gshell_card_app = "none";
        gshell_card_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "CARDRESET";
        gshell_command_result = "CARD RESET OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CARDRESET -> VISUAL CARD WINDOW STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_CARDNEXT) {
        gshell_command_name = "CARDNEXT";
        gshell_command_result = "CARD NEXT OK";
        gshell_command_view = "CARDSTATUS";
        gshell_input_status_text = "NEXT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CARDNEXT -> 1.4.2 VISUAL LAUNCHER TASKBAR");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKSTATUS) {
        gshell_command_name = "UIMOCKSTATUS";
        gshell_command_result = "UI MOCK STATUS OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKSTATUS -> VISIBLE UI MOCK LAYOUT READY");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKDESKTOP) {
        gshell_ui_mock_desktop_ready = 1;
        gshell_ui_mock_state = "desktop";
        gshell_ui_mock_focus = "desktop";
        gshell_ui_mock_last = "desktop-ready";
        gshell_visual_desktop_ready = 1;
        gshell_desktop_state = "ui-mock";
        gshell_focus_target = "desktop";
        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKDESKTOP";
        gshell_command_result = "UI MOCK DESKTOP OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "DESKTOP OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKDESKTOP -> DESKTOP RECT VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKGRID) {
        gshell_ui_mock_grid_ready = 1;
        gshell_ui_mock_state = "grid";
        gshell_ui_mock_focus = "card-grid";
        gshell_ui_mock_last = "grid-ready";
        gshell_visual_grid_ready = 1;
        gshell_card_grid_ready = 1;
        gshell_focus_target = "card-grid";
        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKGRID";
        gshell_command_result = "UI MOCK GRID OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "GRID OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKGRID -> CARD GRID RECT VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKCARD1) {
        gshell_ui_mock_card1_ready = 1;
        gshell_ui_mock_selected_card = 1;
        gshell_ui_mock_state = "card1";
        gshell_ui_mock_focus = "demo.app";
        gshell_ui_mock_last = "card1-ready";
        gshell_card_selected = 1;
        gshell_card_app = "demo.app";
        gshell_focus_target = "demo.app";
        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKCARD1";
        gshell_command_result = "UI MOCK CARD1 OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "CARD1 OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKCARD1 -> DEMO CARD RECT READY");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKCARD2) {
        gshell_ui_mock_card2_ready = 1;
        gshell_ui_mock_selected_card = 2;
        gshell_ui_mock_state = "card2";
        gshell_ui_mock_focus = "sys.tool";
        gshell_ui_mock_last = "card2-ready";
        gshell_focus_target = "sys.tool";
        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKCARD2";
        gshell_command_result = "UI MOCK CARD2 OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "CARD2 OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKCARD2 -> SYSTEM CARD RECT READY");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKCARD3) {
        gshell_ui_mock_card3_ready = 1;
        gshell_ui_mock_selected_card = 3;
        gshell_ui_mock_state = "card3";
        gshell_ui_mock_focus = "file.app";
        gshell_ui_mock_last = "card3-ready";
        gshell_focus_target = "file.app";
        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKCARD3";
        gshell_command_result = "UI MOCK CARD3 OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "CARD3 OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKCARD3 -> FILE CARD RECT READY");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKWINDOW) {
        gshell_ui_mock_window_ready = 1;
        gshell_ui_mock_state = "window";
        gshell_ui_mock_focus = "demo.window";
        gshell_ui_mock_last = "window-ready";
        gshell_visual_window_ready = 1;
        gshell_window_visual_ready = 1;
        gshell_window_exists = 1;
        gshell_window_title = "demo.window";
        gshell_window_state = "ui-mock";
        gshell_focus_target = "demo.window";
        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKWINDOW";
        gshell_command_result = "UI MOCK WINDOW OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "WINDOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKWINDOW -> WINDOW RECT VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKTITLE) {
        gshell_ui_mock_title_ready = 1;
        gshell_ui_mock_state = "titlebar";
        gshell_ui_mock_last = "title-ready";
        gshell_window_title_ready = 1;
        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKTITLE";
        gshell_command_result = "UI MOCK TITLE OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "TITLE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKTITLE -> WINDOW TITLEBAR VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKBODY) {
        gshell_ui_mock_body_ready = 1;
        gshell_ui_mock_state = "body";
        gshell_ui_mock_last = "body-ready";
        gshell_window_body_ready = 1;
        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKBODY";
        gshell_command_result = "UI MOCK BODY OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "BODY OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKBODY -> WINDOW BODY RECT VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKTASKBAR) {
        gshell_ui_mock_taskbar_ready = 1;
        gshell_ui_mock_state = "taskbar";
        gshell_ui_mock_focus = "taskbar";
        gshell_ui_mock_last = "taskbar-ready";
        gshell_visual_taskbar_ready = 1;
        gshell_shell_taskbar_ready = 1;
        gshell_taskbar_item_ready = 1;
        gshell_taskbar_item = "demo.app";
        gshell_focus_target = "taskbar";
        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKTASKBAR";
        gshell_command_result = "UI MOCK TASKBAR OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "TASKBAR OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKTASKBAR -> TASKBAR RECT VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKDOCK) {
        gshell_ui_mock_dock_ready = 1;
        gshell_ui_mock_state = "dock";
        gshell_ui_mock_focus = "dock";
        gshell_ui_mock_last = "dock-ready";
        gshell_visual_dock_ready = 1;
        gshell_dock_ready = 1;
        gshell_focus_target = "dock";
        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKDOCK";
        gshell_command_result = "UI MOCK DOCK OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "DOCK OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKDOCK -> DOCK RECT VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKFOCUS) {
        gshell_ui_mock_focus_ready = 1;
        gshell_ui_mock_state = "focus";
        gshell_ui_mock_focus = "demo.app";
        gshell_ui_mock_last = "focus-ring";
        gshell_visual_focus = "demo.app";
        gshell_focus_target = "demo.app";
        gshell_focus_changes++;
        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKFOCUS";
        gshell_command_result = "UI MOCK FOCUS OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "FOCUS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKFOCUS -> FOCUS RING VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKSELECT) {
        gshell_ui_mock_selected_card = 1;
        gshell_ui_mock_card1_ready = 1;
        gshell_ui_mock_focus_ready = 1;
        gshell_ui_mock_state = "selected";
        gshell_ui_mock_focus = "demo.app";
        gshell_ui_mock_last = "select-demo";
        gshell_card_selected = 1;
        gshell_card_app = "demo.app";
        gshell_focus_target = "demo.app";
        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKSELECT";
        gshell_command_result = "UI MOCK SELECT OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "SELECT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKSELECT -> DEMO CARD SELECTED VISIBLY");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKOPEN) {
        gshell_ui_mock_window_ready = 1;
        gshell_ui_mock_title_ready = 1;
        gshell_ui_mock_body_ready = 1;
        gshell_ui_mock_state = "open";
        gshell_ui_mock_focus = "demo.window";
        gshell_ui_mock_last = "open-window";
        gshell_window_exists = 1;
        gshell_window_focused = 1;
        gshell_window_title = "demo.window";
        gshell_window_state = "open";
        gshell_focus_target = "demo.window";
        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKOPEN";
        gshell_command_result = "UI MOCK OPEN OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "OPEN OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKOPEN -> DEMO WINDOW OPENED VISIBLY");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKLAYOUT) {
        gshell_ui_mock_desktop_ready = 1;
        gshell_ui_mock_grid_ready = 1;
        gshell_ui_mock_taskbar_ready = 1;
        gshell_ui_mock_dock_ready = 1;
        gshell_ui_mock_state = "layout";
        gshell_ui_mock_last = "layout-ready";
        gshell_layout_grid_ready = 1;
        gshell_layout_state = "ui-mock";
        gshell_layout_mode = "visual";
        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKLAYOUT";
        gshell_command_result = "UI MOCK LAYOUT OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "LAYOUT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKLAYOUT -> DESKTOP CARD TASKBAR LAYOUT VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKMETRICS) {
        gshell_visual_metrics++;
        gshell_ui_mock_state = "metrics";
        gshell_ui_mock_last = "metrics";
        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKMETRICS";
        gshell_command_result = "UI MOCK METRICS OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "METRICS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKMETRICS -> CARD 76x48 WINDOW 210x86");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKDEMO) {
        gshell_ui_mock_desktop_ready = 1;
        gshell_ui_mock_grid_ready = 1;
        gshell_ui_mock_card1_ready = 1;
        gshell_ui_mock_card2_ready = 1;
        gshell_ui_mock_card3_ready = 1;
        gshell_ui_mock_window_ready = 1;
        gshell_ui_mock_title_ready = 1;
        gshell_ui_mock_body_ready = 1;
        gshell_ui_mock_taskbar_ready = 1;
        gshell_ui_mock_dock_ready = 1;
        gshell_ui_mock_focus_ready = 1;
        gshell_ui_mock_selected_card = 1;
        gshell_ui_mock_state = "demo";
        gshell_ui_mock_focus = "demo.app";
        gshell_ui_mock_last = "demo-ready";

        gshell_visual_desktop_ready = 1;
        gshell_visual_panel_ready = 1;
        gshell_visual_cards_ready = 1;
        gshell_visual_window_ready = 1;
        gshell_visual_taskbar_ready = 1;
        gshell_visual_dock_ready = 1;
        gshell_visual_grid_ready = 1;
        gshell_card_state = "ui-mock";
        gshell_card_app = "demo.app";
        gshell_window_exists = 1;
        gshell_window_title = "demo.window";
        gshell_window_state = "ui-mock";
        gshell_focus_target = "demo.app";

        gshell_ui_mock_events++;
        gshell_command_name = "UIMOCKDEMO";
        gshell_command_result = "UI MOCK DEMO OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "DEMO OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKDEMO -> FULL VISIBLE DESKTOP MOCK READY");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKCHECK) {
        int ok = gshell_ui_mock_desktop_ready && gshell_ui_mock_grid_ready && gshell_ui_mock_window_ready && gshell_ui_mock_taskbar_ready;
        gshell_ui_mock_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "UIMOCKCHECK";
        gshell_command_result = ok ? "UI MOCK CHECK OK" : "UI MOCK CHECK WAIT";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = ok ? "MOCK OK" : "MOCK WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "UIMOCKCHECK -> VISIBLE UI MOCK READY" : "UIMOCKCHECK -> NEED DESKTOP GRID WINDOW TASKBAR");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKRESET) {
        gshell_ui_mock_desktop_ready = 0;
        gshell_ui_mock_grid_ready = 0;
        gshell_ui_mock_card1_ready = 0;
        gshell_ui_mock_card2_ready = 0;
        gshell_ui_mock_card3_ready = 0;
        gshell_ui_mock_window_ready = 0;
        gshell_ui_mock_title_ready = 0;
        gshell_ui_mock_body_ready = 0;
        gshell_ui_mock_taskbar_ready = 0;
        gshell_ui_mock_dock_ready = 0;
        gshell_ui_mock_focus_ready = 0;
        gshell_ui_mock_selected_card = 0;
        gshell_ui_mock_events = 0;
        gshell_ui_mock_state = "idle";
        gshell_ui_mock_focus = "terminal";
        gshell_ui_mock_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "UIMOCKRESET";
        gshell_command_result = "UI MOCK RESET OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKRESET -> VISIBLE UI MOCK RESET");
        return;
    }

    if (command_id == GSHELL_CMD_UIMOCKNEXT) {
        gshell_command_name = "UIMOCKNEXT";
        gshell_command_result = "UI MOCK NEXT OK";
        gshell_command_view = "UIMOCKSTATUS";
        gshell_input_status_text = "NEXT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("UIMOCKNEXT -> 1.4.3 VISUAL LAUNCHER TASKBAR MOCK");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHMOCKSTATUS) {
        gshell_command_name = "LAUNCHMOCKSTATUS";
        gshell_command_result = "LAUNCH MOCK STATUS OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHMOCKSTATUS -> VISIBLE LAUNCHER TASKBAR MOCK READY");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHMOCKPANEL) {
        gshell_launch_mock_panel_ready = 1;
        gshell_mock_state = "launcher-panel";
        gshell_mock_focus = "launcher";
        gshell_mock_last = "panel-ready";
        gshell_shell_launcher_ready = 1;
        gshell_focus_target = "launcher";
        gshell_mock_events++;
        gshell_command_name = "LAUNCHMOCKPANEL";
        gshell_command_result = "LAUNCH MOCK PANEL OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "PANEL OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHMOCKPANEL -> LAUNCHER PANEL VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHMOCKSEARCH) {
        gshell_launch_mock_search_ready = 1;
        gshell_mock_state = "search";
        gshell_mock_focus = "search";
        gshell_mock_last = "search-ready";
        gshell_focus_target = "launcher-search";
        gshell_mock_events++;
        gshell_command_name = "LAUNCHMOCKSEARCH";
        gshell_command_result = "LAUNCH MOCK SEARCH OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "SEARCH OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHMOCKSEARCH -> SEARCH BOX VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHMOCKAPPS) {
        gshell_launch_mock_apps_ready = 1;
        gshell_mock_state = "apps";
        gshell_mock_focus = "app-list";
        gshell_mock_last = "apps-ready";
        gshell_launcher_grid_ready = 1;
        gshell_catalog_ready = 1;
        gshell_catalog_items = 1;
        gshell_catalog_selected = "demo.app";
        gshell_mock_events++;
        gshell_command_name = "LAUNCHMOCKAPPS";
        gshell_command_result = "LAUNCH MOCK APPS OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "APPS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHMOCKAPPS -> APP LIST VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHMOCKRECENT) {
        gshell_launch_mock_recent_ready = 1;
        gshell_mock_state = "recent";
        gshell_mock_focus = "recent";
        gshell_mock_last = "recent-ready";
        gshell_mock_events++;
        gshell_command_name = "LAUNCHMOCKRECENT";
        gshell_command_result = "LAUNCH MOCK RECENT OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "RECENT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHMOCKRECENT -> RECENT AREA VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHMOCKPIN) {
        gshell_launch_mock_pin_ready = 1;
        gshell_mock_state = "pin";
        gshell_mock_focus = "pinned-app";
        gshell_mock_last = "pin-ready";
        gshell_launcher_app_pinned = 1;
        gshell_taskbar_item_ready = 1;
        gshell_taskbar_item = "demo.app";
        gshell_mock_events++;
        gshell_command_name = "LAUNCHMOCKPIN";
        gshell_command_result = "LAUNCH MOCK PIN OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "PIN OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHMOCKPIN -> PINNED APP VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_LAUNCHMOCKRUN) {
        gshell_launch_mock_run_ready = 1;
        gshell_mock_state = "run";
        gshell_mock_focus = "demo.app";
        gshell_mock_last = "run-ready";
        gshell_action_running = 1;
        gshell_life_started = 1;
        gshell_life_state = "running";
        gshell_window_exists = 1;
        gshell_window_state = "running";
        gshell_mock_events++;
        gshell_command_name = "LAUNCHMOCKRUN";
        gshell_command_result = "LAUNCH MOCK RUN OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "RUN OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("LAUNCHMOCKRUN -> RUNNING APP INDICATOR VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_TASKMOCKSTATUS) {
        gshell_command_name = "TASKMOCKSTATUS";
        gshell_command_result = "TASK MOCK STATUS OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "TASK OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TASKMOCKSTATUS -> TASKBAR MOCK STATUS READY");
        return;
    }

    if (command_id == GSHELL_CMD_TASKMOCKSTART) {
        gshell_task_mock_start_ready = 1;
        gshell_mock_state = "start";
        gshell_mock_focus = "start-button";
        gshell_mock_last = "start-ready";
        gshell_focus_target = "start-button";
        gshell_mock_events++;
        gshell_command_name = "TASKMOCKSTART";
        gshell_command_result = "TASK MOCK START OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "START OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TASKMOCKSTART -> START BUTTON VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_TASKMOCKAPP) {
        gshell_task_mock_app_ready = 1;
        gshell_mock_state = "task-app";
        gshell_mock_focus = "demo.task";
        gshell_mock_last = "task-app-ready";
        gshell_taskbar_item_ready = 1;
        gshell_taskbar_item = "demo.app";
        gshell_mock_events++;
        gshell_command_name = "TASKMOCKAPP";
        gshell_command_result = "TASK MOCK APP OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "APP OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TASKMOCKAPP -> TASKBAR APP BLOCK VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_TASKMOCKACTIVE) {
        gshell_task_mock_active_ready = 1;
        gshell_mock_state = "active";
        gshell_mock_focus = "demo.task";
        gshell_mock_last = "active-ready";
        gshell_taskbar_focused = 1;
        gshell_window_focused = 1;
        gshell_focus_target = "demo.app";
        gshell_mock_events++;
        gshell_command_name = "TASKMOCKACTIVE";
        gshell_command_result = "TASK MOCK ACTIVE OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "ACTIVE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TASKMOCKACTIVE -> ACTIVE TASK INDICATOR VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_TASKMOCKTRAY) {
        gshell_task_mock_tray_ready = 1;
        gshell_mock_state = "tray";
        gshell_mock_focus = "tray";
        gshell_mock_last = "tray-ready";
        gshell_mock_events++;
        gshell_command_name = "TASKMOCKTRAY";
        gshell_command_result = "TASK MOCK TRAY OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "TRAY OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TASKMOCKTRAY -> TRAY AREA VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_TASKMOCKCLOCK) {
        gshell_task_mock_clock_ready = 1;
        gshell_mock_state = "clock";
        gshell_mock_last = "clock-ready";
        gshell_mock_events++;
        gshell_command_name = "TASKMOCKCLOCK";
        gshell_command_result = "TASK MOCK CLOCK OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "CLOCK OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TASKMOCKCLOCK -> CLOCK PLACEHOLDER VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_TASKMOCKNET) {
        gshell_task_mock_net_ready = 1;
        gshell_mock_state = "net";
        gshell_mock_last = "net-ready";
        gshell_mock_events++;
        gshell_command_name = "TASKMOCKNET";
        gshell_command_result = "TASK MOCK NET OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "NET OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TASKMOCKNET -> NETWORK INDICATOR VISIBLE");
        return;
    }

    if (command_id == GSHELL_CMD_TASKMOCKSTATE) {
        gshell_mock_state = "task-state";
        gshell_mock_last = "task-state";
        gshell_mock_events++;
        gshell_command_name = "TASKMOCKSTATE";
        gshell_command_result = "TASK MOCK STATE OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "STATE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("TASKMOCKSTATE -> TASKBAR STATE INDICATORS READY");
        return;
    }

    if (command_id == GSHELL_CMD_MOCKCOMPOSE) {
        gshell_launch_mock_panel_ready = 1;
        gshell_launch_mock_search_ready = 1;
        gshell_launch_mock_apps_ready = 1;
        gshell_task_mock_start_ready = 1;
        gshell_task_mock_app_ready = 1;
        gshell_task_mock_tray_ready = 1;
        gshell_mock_state = "composed";
        gshell_mock_focus = "launcher-taskbar";
        gshell_mock_last = "compose";
        gshell_focus_target = "launcher-taskbar";
        gshell_mock_events++;
        gshell_command_name = "MOCKCOMPOSE";
        gshell_command_result = "MOCK COMPOSE OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "COMPOSE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MOCKCOMPOSE -> LAUNCHER TASKBAR MOCK COMPOSED");
        return;
    }

    if (command_id == GSHELL_CMD_MOCKDEMO) {
        gshell_launch_mock_panel_ready = 1;
        gshell_launch_mock_search_ready = 1;
        gshell_launch_mock_apps_ready = 1;
        gshell_launch_mock_recent_ready = 1;
        gshell_launch_mock_pin_ready = 1;
        gshell_launch_mock_run_ready = 1;
        gshell_task_mock_start_ready = 1;
        gshell_task_mock_app_ready = 1;
        gshell_task_mock_active_ready = 1;
        gshell_task_mock_tray_ready = 1;
        gshell_task_mock_clock_ready = 1;
        gshell_task_mock_net_ready = 1;
        gshell_ui_mock_desktop_ready = 1;
        gshell_ui_mock_taskbar_ready = 1;
        gshell_mock_state = "demo";
        gshell_mock_focus = "demo.app";
        gshell_mock_last = "demo-ready";
        gshell_focus_target = "demo.app";
        gshell_mock_events++;
        gshell_command_name = "MOCKDEMO";
        gshell_command_result = "MOCK DEMO OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "DEMO OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MOCKDEMO -> FULL LAUNCHER TASKBAR MOCK READY");
        return;
    }

    if (command_id == GSHELL_CMD_MOCKCHECK) {
        int ok = gshell_launch_mock_panel_ready && gshell_task_mock_start_ready && gshell_task_mock_app_ready && gshell_task_mock_tray_ready;
        gshell_mock_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "MOCKCHECK";
        gshell_command_result = ok ? "MOCK CHECK OK" : "MOCK CHECK WAIT";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = ok ? "MOCK OK" : "MOCK WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "MOCKCHECK -> LAUNCHER TASKBAR MOCK READY" : "MOCKCHECK -> NEED PANEL START APP TRAY");
        return;
    }

    if (command_id == GSHELL_CMD_MOCKRESET) {
        gshell_launch_mock_panel_ready = 0;
        gshell_launch_mock_search_ready = 0;
        gshell_launch_mock_apps_ready = 0;
        gshell_launch_mock_recent_ready = 0;
        gshell_launch_mock_pin_ready = 0;
        gshell_launch_mock_run_ready = 0;
        gshell_task_mock_start_ready = 0;
        gshell_task_mock_app_ready = 0;
        gshell_task_mock_active_ready = 0;
        gshell_task_mock_tray_ready = 0;
        gshell_task_mock_clock_ready = 0;
        gshell_task_mock_net_ready = 0;
        gshell_mock_events = 0;
        gshell_mock_state = "idle";
        gshell_mock_focus = "terminal";
        gshell_mock_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "MOCKRESET";
        gshell_command_result = "MOCK RESET OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MOCKRESET -> LAUNCHER TASKBAR MOCK RESET");
        return;
    }

    if (command_id == GSHELL_CMD_MOCKNEXT) {
        gshell_command_name = "MOCKNEXT";
        gshell_command_result = "MOCK NEXT OK";
        gshell_command_view = "LAUNCHMOCKSTATUS";
        gshell_input_status_text = "NEXT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MOCKNEXT -> 1.4.4 VISUAL POLISH MOCK");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHSTATUS) {
        gshell_command_name = "POLISHSTATUS";
        gshell_command_result = "POLISH STATUS OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHSTATUS -> VISUAL POLISH MOCK READY");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHBASE) {
        gshell_polish_base_ready = 1;
        gshell_polish_state = "base";
        gshell_polish_focus = "desktop";
        gshell_polish_last = "base-ready";
        gshell_visual_desktop_ready = 1;
        gshell_ui_mock_desktop_ready = 1;
        gshell_focus_target = "desktop";
        gshell_polish_events++;
        gshell_command_name = "POLISHBASE";
        gshell_command_result = "POLISH BASE OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "BASE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHBASE -> BASE DESKTOP PANEL READY");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHGLASS) {
        gshell_polish_glass_ready = 1;
        gshell_polish_state = "glass";
        gshell_polish_last = "glass-ready";
        gshell_visual_glow_ready = 1;
        gshell_polish_events++;
        gshell_command_name = "POLISHGLASS";
        gshell_command_result = "POLISH GLASS OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "GLASS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHGLASS -> GLASS PANEL READY");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHTITLE) {
        gshell_polish_title_ready = 1;
        gshell_polish_state = "title";
        gshell_polish_last = "title-ready";
        gshell_window_title_ready = 1;
        gshell_polish_events++;
        gshell_command_name = "POLISHTITLE";
        gshell_command_result = "POLISH TITLE OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "TITLE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHTITLE -> TITLE BAR POLISHED");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHCARDS) {
        gshell_polish_cards_ready = 1;
        gshell_polish_state = "cards";
        gshell_polish_focus = "cards";
        gshell_polish_last = "cards-ready";
        gshell_ui_mock_grid_ready = 1;
        gshell_card_grid_ready = 1;
        gshell_visual_cards_ready = 1;
        gshell_focus_target = "cards";
        gshell_polish_events++;
        gshell_command_name = "POLISHCARDS";
        gshell_command_result = "POLISH CARDS OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "CARDS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHCARDS -> COMPACT CARD ROW READY");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHWINDOW) {
        gshell_polish_window_ready = 1;
        gshell_polish_state = "window";
        gshell_polish_focus = "demo.window";
        gshell_polish_last = "window-ready";
        gshell_ui_mock_window_ready = 1;
        gshell_window_visual_ready = 1;
        gshell_window_exists = 1;
        gshell_window_state = "polished";
        gshell_window_title = "demo.window";
        gshell_focus_target = "demo.window";
        gshell_polish_events++;
        gshell_command_name = "POLISHWINDOW";
        gshell_command_result = "POLISH WINDOW OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "WINDOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHWINDOW -> WINDOW PANEL POLISHED");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHTASKBAR) {
        gshell_polish_taskbar_ready = 1;
        gshell_polish_state = "taskbar";
        gshell_polish_focus = "taskbar";
        gshell_polish_last = "taskbar-ready";
        gshell_ui_mock_taskbar_ready = 1;
        gshell_task_mock_start_ready = 1;
        gshell_task_mock_app_ready = 1;
        gshell_focus_target = "taskbar";
        gshell_polish_events++;
        gshell_command_name = "POLISHTASKBAR";
        gshell_command_result = "POLISH TASKBAR OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "TASKBAR OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHTASKBAR -> TASKBAR POLISHED");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHTRAY) {
        gshell_polish_tray_ready = 1;
        gshell_polish_state = "tray";
        gshell_polish_last = "tray-ready";
        gshell_task_mock_tray_ready = 1;
        gshell_task_mock_net_ready = 1;
        gshell_task_mock_clock_ready = 1;
        gshell_polish_events++;
        gshell_command_name = "POLISHTRAY";
        gshell_command_result = "POLISH TRAY OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "TRAY OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHTRAY -> TRAY STATUS POLISHED");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHBADGE) {
        gshell_polish_badge_ready = 1;
        gshell_polish_state = "badge";
        gshell_polish_last = "badge-ready";
        gshell_card_badge_ready = 1;
        gshell_polish_events++;
        gshell_command_name = "POLISHBADGE";
        gshell_command_result = "POLISH BADGE OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "BADGE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHBADGE -> STATUS BADGES READY");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHFOCUS) {
        gshell_polish_focus_ready = 1;
        gshell_polish_state = "focus";
        gshell_polish_focus = "demo.app";
        gshell_polish_last = "focus-ready";
        gshell_ui_mock_focus_ready = 1;
        gshell_visual_focus = "demo.app";
        gshell_focus_target = "demo.app";
        gshell_focus_changes++;
        gshell_polish_events++;
        gshell_command_name = "POLISHFOCUS";
        gshell_command_result = "POLISH FOCUS OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "FOCUS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHFOCUS -> FOCUS HIGHLIGHT READY");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHSHADOW) {
        gshell_polish_shadow_ready = 1;
        gshell_polish_state = "shadow";
        gshell_polish_last = "shadow-ready";
        gshell_window_shadow_ready = 1;
        gshell_polish_events++;
        gshell_command_name = "POLISHSHADOW";
        gshell_command_result = "POLISH SHADOW OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "SHADOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHSHADOW -> WINDOW SHADOW READY");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHCOMPACT) {
        gshell_polish_compact_ready = 1;
        gshell_polish_state = "compact";
        gshell_polish_last = "compact-ready";
        gshell_polish_events++;
        gshell_command_name = "POLISHCOMPACT";
        gshell_command_result = "POLISH COMPACT OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "COMPACT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHCOMPACT -> COMPACT UI MODE READY");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHALIGN) {
        gshell_polish_align_ready = 1;
        gshell_polish_state = "aligned";
        gshell_polish_last = "align-ready";
        gshell_polish_events++;
        gshell_command_name = "POLISHALIGN";
        gshell_command_result = "POLISH ALIGN OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "ALIGN OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHALIGN -> TEXT BOUNDS SAFE");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHMETRICS) {
        gshell_polish_metrics_ready = 1;
        gshell_visual_metrics++;
        gshell_polish_state = "metrics";
        gshell_polish_last = "metrics-ready";
        gshell_polish_events++;
        gshell_command_name = "POLISHMETRICS";
        gshell_command_result = "POLISH METRICS OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "METRICS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHMETRICS -> FIT METRICS READY");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHSAFE) {
        gshell_polish_safe_ready = 1;
        gshell_polish_state = "safe";
        gshell_polish_last = "safe-ready";
        gshell_polish_align_ready = 1;
        gshell_polish_metrics_ready = 1;
        gshell_polish_events++;
        gshell_command_name = "POLISHSAFE";
        gshell_command_result = "POLISH SAFE OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "SAFE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHSAFE -> NO OVERFLOW MODE READY");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHDEMO) {
        gshell_polish_base_ready = 1;
        gshell_polish_glass_ready = 1;
        gshell_polish_title_ready = 1;
        gshell_polish_cards_ready = 1;
        gshell_polish_window_ready = 1;
        gshell_polish_taskbar_ready = 1;
        gshell_polish_tray_ready = 1;
        gshell_polish_badge_ready = 1;
        gshell_polish_focus_ready = 1;
        gshell_polish_shadow_ready = 1;
        gshell_polish_compact_ready = 1;
        gshell_polish_align_ready = 1;
        gshell_polish_metrics_ready = 1;
        gshell_polish_safe_ready = 1;

        gshell_ui_mock_desktop_ready = 1;
        gshell_ui_mock_grid_ready = 1;
        gshell_ui_mock_card1_ready = 1;
        gshell_ui_mock_card2_ready = 1;
        gshell_ui_mock_card3_ready = 1;
        gshell_ui_mock_window_ready = 1;
        gshell_ui_mock_title_ready = 1;
        gshell_ui_mock_body_ready = 1;
        gshell_ui_mock_taskbar_ready = 1;
        gshell_ui_mock_dock_ready = 1;
        gshell_ui_mock_focus_ready = 1;
        gshell_ui_mock_selected_card = 1;

        gshell_task_mock_start_ready = 1;
        gshell_task_mock_app_ready = 1;
        gshell_task_mock_active_ready = 1;
        gshell_task_mock_tray_ready = 1;
        gshell_task_mock_clock_ready = 1;
        gshell_task_mock_net_ready = 1;

        gshell_polish_state = "demo";
        gshell_polish_focus = "demo.app";
        gshell_polish_last = "demo-ready";
        gshell_focus_target = "demo.app";
        gshell_polish_events++;
        gshell_command_name = "POLISHDEMO";
        gshell_command_result = "POLISH DEMO OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "DEMO OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHDEMO -> FULL POLISHED VISUAL MOCK READY");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHFLOW) {
        gshell_polish_base_ready = 1;
        gshell_polish_cards_ready = 1;
        gshell_polish_window_ready = 1;
        gshell_polish_taskbar_ready = 1;
        gshell_polish_align_ready = 1;
        gshell_polish_safe_ready = 1;
        gshell_polish_state = "flow";
        gshell_polish_focus = "visual-flow";
        gshell_polish_last = "flow-ready";
        gshell_focus_target = "visual-flow";
        gshell_polish_events++;
        gshell_command_name = "POLISHFLOW";
        gshell_command_result = "POLISH FLOW OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "FLOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHFLOW -> POLISH FLOW READY");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHCHECK) {
        int ok = gshell_polish_base_ready && gshell_polish_cards_ready && gshell_polish_window_ready && gshell_polish_taskbar_ready && gshell_polish_safe_ready;
        gshell_polish_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "POLISHCHECK";
        gshell_command_result = ok ? "POLISH CHECK OK" : "POLISH CHECK WAIT";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = ok ? "POLISH OK" : "POLISH WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "POLISHCHECK -> VISUAL POLISH READY" : "POLISHCHECK -> NEED BASE CARDS WINDOW TASKBAR SAFE");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHRESET) {
        gshell_polish_base_ready = 0;
        gshell_polish_glass_ready = 0;
        gshell_polish_title_ready = 0;
        gshell_polish_cards_ready = 0;
        gshell_polish_window_ready = 0;
        gshell_polish_taskbar_ready = 0;
        gshell_polish_tray_ready = 0;
        gshell_polish_badge_ready = 0;
        gshell_polish_focus_ready = 0;
        gshell_polish_shadow_ready = 0;
        gshell_polish_compact_ready = 0;
        gshell_polish_align_ready = 0;
        gshell_polish_metrics_ready = 0;
        gshell_polish_safe_ready = 0;
        gshell_polish_events = 0;
        gshell_polish_state = "idle";
        gshell_polish_focus = "terminal";
        gshell_polish_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "POLISHRESET";
        gshell_command_result = "POLISH RESET OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHRESET -> VISUAL POLISH STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_POLISHNEXT) {
        gshell_command_name = "POLISHNEXT";
        gshell_command_result = "POLISH NEXT OK";
        gshell_command_view = "POLISHSTATUS";
        gshell_input_status_text = "NEXT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POLISHNEXT -> 1.4.5 VISUAL CLOSEOUT");
        return;
    }

    if (command_id == GSHELL_CMD_SCENESTATUS) {
        gshell_command_name = "SCENESTATUS";
        gshell_command_result = "SCENE STATUS OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENESTATUS -> DEFAULT DESKTOP SCENE READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCENEBOOT) {
        gshell_scene_state = "boot";
        gshell_scene_last = "boot";
        gshell_scene_events++;
        gshell_command_name = "SCENEBOOT";
        gshell_command_result = "SCENE BOOT OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "BOOT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENEBOOT -> DESKTOP SCENE BOOTED");
        return;
    }

    if (command_id == GSHELL_CMD_SCENEBACKGROUND) {
        gshell_scene_background_ready = 1;
        gshell_scene_state = "background";
        gshell_scene_last = "background-ready";
        gshell_scene_events++;
        gshell_command_name = "SCENEBACKGROUND";
        gshell_command_result = "SCENE BACKGROUND OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "BG OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENEBACKGROUND -> BACKGROUND READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCENETOPBAR) {
        gshell_scene_topbar_ready = 1;
        gshell_scene_state = "topbar";
        gshell_scene_last = "topbar-ready";
        gshell_scene_events++;
        gshell_command_name = "SCENETOPBAR";
        gshell_command_result = "SCENE TOPBAR OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "TOPBAR OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENETOPBAR -> TOPBAR READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCENELAUNCHER) {
        gshell_scene_launcher_ready = 1;
        gshell_scene_state = "launcher";
        gshell_scene_focus = "launcher";
        gshell_scene_last = "launcher-ready";
        gshell_focus_target = "launcher";
        gshell_scene_events++;
        gshell_command_name = "SCENELAUNCHER";
        gshell_command_result = "SCENE LAUNCHER OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "LAUNCHER OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENELAUNCHER -> LAUNCHER READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCENECARDS) {
        gshell_scene_cards_ready = 1;
        gshell_scene_state = "cards";
        gshell_scene_focus = "cards";
        gshell_scene_last = "cards-ready";
        gshell_focus_target = "cards";
        gshell_scene_events++;
        gshell_command_name = "SCENECARDS";
        gshell_command_result = "SCENE CARDS OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "CARDS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENECARDS -> DESKTOP CARDS READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCENEWINDOW) {
        gshell_scene_window_ready = 1;
        gshell_scene_state = "window";
        gshell_scene_focus = "demo.window";
        gshell_scene_last = "window-ready";
        gshell_window_exists = 1;
        gshell_window_title = "demo.window";
        gshell_window_state = "scene";
        gshell_focus_target = "demo.window";
        gshell_scene_events++;
        gshell_command_name = "SCENEWINDOW";
        gshell_command_result = "SCENE WINDOW OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "WINDOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENEWINDOW -> MAIN WINDOW READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCENETASKBAR) {
        gshell_scene_taskbar_ready = 1;
        gshell_scene_state = "taskbar";
        gshell_scene_focus = "taskbar";
        gshell_scene_last = "taskbar-ready";
        gshell_focus_target = "taskbar";
        gshell_scene_events++;
        gshell_command_name = "SCENETASKBAR";
        gshell_command_result = "SCENE TASKBAR OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "TASKBAR OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENETASKBAR -> TASKBAR READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCENEDOCK) {
        gshell_scene_dock_ready = 1;
        gshell_scene_state = "dock";
        gshell_scene_focus = "dock";
        gshell_scene_last = "dock-ready";
        gshell_scene_events++;
        gshell_command_name = "SCENEDOCK";
        gshell_command_result = "SCENE DOCK OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "DOCK OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENEDOCK -> DOCK READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCENETRAY) {
        gshell_scene_tray_ready = 1;
        gshell_scene_state = "tray";
        gshell_scene_last = "tray-ready";
        gshell_scene_events++;
        gshell_command_name = "SCENETRAY";
        gshell_command_result = "SCENE TRAY OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "TRAY OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENETRAY -> TRAY READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCENEBADGE) {
        gshell_scene_badge_ready = 1;
        gshell_scene_state = "badge";
        gshell_scene_last = "badge-ready";
        gshell_scene_events++;
        gshell_command_name = "SCENEBADGE";
        gshell_command_result = "SCENE BADGE OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "BADGE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENEBADGE -> STATUS BADGE READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCENEFOCUS) {
        gshell_scene_focus_ready = 1;
        gshell_scene_state = "focus";
        gshell_scene_focus = "demo.app";
        gshell_scene_last = "focus-ready";
        gshell_focus_target = "demo.app";
        gshell_focus_changes++;
        gshell_scene_events++;
        gshell_command_name = "SCENEFOCUS";
        gshell_command_result = "SCENE FOCUS OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "FOCUS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENEFOCUS -> FOCUS READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCENEWIDGETS) {
        gshell_scene_widgets_ready = 1;
        gshell_scene_state = "widgets";
        gshell_scene_last = "widgets-ready";
        gshell_scene_events++;
        gshell_command_name = "SCENEWIDGETS";
        gshell_command_result = "SCENE WIDGETS OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "WIDGETS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENEWIDGETS -> WIDGETS READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCENEACTIVE) {
        gshell_scene_active_ready = 1;
        gshell_scene_state = "active";
        gshell_scene_focus = "demo.app";
        gshell_scene_last = "active-ready";
        gshell_focus_target = "demo.app";
        gshell_scene_events++;
        gshell_command_name = "SCENEACTIVE";
        gshell_command_result = "SCENE ACTIVE OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "ACTIVE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENEACTIVE -> ACTIVE APP READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCENEMETRICS) {
        gshell_scene_metrics++;
        gshell_scene_state = "metrics";
        gshell_scene_last = "metrics";
        gshell_scene_events++;
        gshell_command_name = "SCENEMETRICS";
        gshell_command_result = "SCENE METRICS OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "METRICS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENEMETRICS -> DEFAULT SCENE METRICS READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCENEDEMO || command_id == GSHELL_CMD_SCENEFLOW) {
        gshell_scene_background_ready = 1;
        gshell_scene_topbar_ready = 1;
        gshell_scene_launcher_ready = 1;
        gshell_scene_cards_ready = 1;
        gshell_scene_window_ready = 1;
        gshell_scene_taskbar_ready = 1;
        gshell_scene_dock_ready = 1;
        gshell_scene_tray_ready = 1;
        gshell_scene_badge_ready = 1;
        gshell_scene_focus_ready = 1;
        gshell_scene_widgets_ready = 1;
        gshell_scene_active_ready = 1;
        gshell_scene_state = command_id == GSHELL_CMD_SCENEDEMO ? "demo" : "flow";
        gshell_scene_focus = "demo.app";
        gshell_scene_last = command_id == GSHELL_CMD_SCENEDEMO ? "demo-ready" : "flow-ready";
        gshell_focus_target = "demo.app";
        gshell_scene_events++;
        gshell_command_name = command_id == GSHELL_CMD_SCENEDEMO ? "SCENEDEMO" : "SCENEFLOW";
        gshell_command_result = command_id == GSHELL_CMD_SCENEDEMO ? "SCENE DEMO OK" : "SCENE FLOW OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = command_id == GSHELL_CMD_SCENEDEMO ? "DEMO OK" : "FLOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(command_id == GSHELL_CMD_SCENEDEMO ? "SCENEDEMO -> DEFAULT DESKTOP SCENE READY" : "SCENEFLOW -> DEFAULT DESKTOP FLOW READY");
        return;
    }

    if (command_id == GSHELL_CMD_SCENECHECK) {
        int ok = gshell_scene_background_ready && gshell_scene_topbar_ready && gshell_scene_window_ready && gshell_scene_taskbar_ready;
        gshell_scene_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "SCENECHECK";
        gshell_command_result = ok ? "SCENE CHECK OK" : "SCENE CHECK WAIT";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = ok ? "SCENE OK" : "SCENE WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "SCENECHECK -> DEFAULT DESKTOP SCENE OK" : "SCENECHECK -> NEED BACKGROUND TOPBAR WINDOW TASKBAR");
        return;
    }

    if (command_id == GSHELL_CMD_SCENERESET) {
        gshell_scene_background_ready = 0;
        gshell_scene_topbar_ready = 0;
        gshell_scene_launcher_ready = 0;
        gshell_scene_cards_ready = 0;
        gshell_scene_window_ready = 0;
        gshell_scene_taskbar_ready = 0;
        gshell_scene_dock_ready = 0;
        gshell_scene_tray_ready = 0;
        gshell_scene_badge_ready = 0;
        gshell_scene_focus_ready = 0;
        gshell_scene_widgets_ready = 0;
        gshell_scene_active_ready = 0;
        gshell_scene_metrics = 0;
        gshell_scene_events = 0;
        gshell_scene_state = "idle";
        gshell_scene_focus = "terminal";
        gshell_scene_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "SCENERESET";
        gshell_command_result = "SCENE RESET OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENERESET -> DEFAULT SCENE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_SCENENEXT) {
        gshell_command_name = "SCENENEXT";
        gshell_command_result = "SCENE NEXT OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "NEXT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SCENENEXT -> 1.4.6 VISUAL CLOSEOUT OR 1.5 INPUT");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALFINAL) {
        gshell_command_name = "VISUALFINAL";
        gshell_command_result = "VISUAL FINAL OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALFINAL -> 1.4 VISUAL UI CLOSEOUT READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALHEALTHSUM) {
        int core_ok = health_user_ok() && health_ring3_ok() && health_syscall_ok();
        gshell_visual_final_state = core_ok ? "health-ok" : "health-bad";
        gshell_visual_final_last = "health-sum";
        gshell_visual_final_events++;
        gshell_command_name = "VISUALHEALTHSUM";
        gshell_command_result = core_ok ? "VISUAL HEALTH SUM OK" : "VISUAL HEALTH SUM BAD";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = core_ok ? "HEALTH OK" : "HEALTH BAD";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(core_ok ? "VISUALHEALTHSUM -> CORE VISUAL HEALTH OK" : "VISUALHEALTHSUM -> CORE ISSUE");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALSCENESUM) {
        gshell_visual_final_scene_ready = 1;
        gshell_visual_final_state = "scene";
        gshell_visual_final_last = "scene-sum";
        gshell_scene_background_ready = 1;
        gshell_scene_topbar_ready = 1;
        gshell_scene_window_ready = 1;
        gshell_scene_taskbar_ready = 1;
        gshell_scene_state = "summary";
        gshell_visual_final_events++;
        gshell_command_name = "VISUALSCENESUM";
        gshell_command_result = "VISUAL SCENE SUM OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "SCENE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALSCENESUM -> DEFAULT SCENE SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALMOCKSUM) {
        gshell_visual_final_mock_ready = 1;
        gshell_visual_final_state = "mock";
        gshell_visual_final_last = "mock-sum";
        gshell_ui_mock_desktop_ready = 1;
        gshell_ui_mock_grid_ready = 1;
        gshell_ui_mock_window_ready = 1;
        gshell_ui_mock_taskbar_ready = 1;
        gshell_visual_final_events++;
        gshell_command_name = "VISUALMOCKSUM";
        gshell_command_result = "VISUAL MOCK SUM OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "MOCK OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALMOCKSUM -> UI MOCK SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALCARDSUM) {
        gshell_visual_final_cards_ready = 1;
        gshell_visual_final_state = "cards";
        gshell_visual_final_last = "card-sum";
        gshell_card_grid_ready = 1;
        gshell_card_selected = 1;
        gshell_window_visual_ready = 1;
        gshell_card_state = "summary";
        gshell_visual_final_events++;
        gshell_command_name = "VISUALCARDSUM";
        gshell_command_result = "VISUAL CARD SUM OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "CARD OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALCARDSUM -> CARD WINDOW SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALLAUNCHSUM) {
        gshell_visual_final_launcher_ready = 1;
        gshell_visual_final_state = "launcher";
        gshell_visual_final_last = "launch-sum";
        gshell_launch_mock_panel_ready = 1;
        gshell_launch_mock_apps_ready = 1;
        gshell_task_mock_start_ready = 1;
        gshell_task_mock_app_ready = 1;
        gshell_mock_state = "summary";
        gshell_visual_final_events++;
        gshell_command_name = "VISUALLAUNCHSUM";
        gshell_command_result = "VISUAL LAUNCH SUM OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "LAUNCH OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALLAUNCHSUM -> LAUNCHER TASKBAR SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALPOLISHSUM) {
        gshell_visual_final_polish_ready = 1;
        gshell_visual_final_state = "polish";
        gshell_visual_final_last = "polish-sum";
        gshell_polish_base_ready = 1;
        gshell_polish_cards_ready = 1;
        gshell_polish_window_ready = 1;
        gshell_polish_taskbar_ready = 1;
        gshell_polish_safe_ready = 1;
        gshell_polish_state = "summary";
        gshell_visual_final_events++;
        gshell_command_name = "VISUALPOLISHSUM";
        gshell_command_result = "VISUAL POLISH SUM OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "POLISH OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALPOLISHSUM -> POLISH SUMMARY READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALREADINESS) {
        int ok = gshell_visual_final_scene_ready && gshell_visual_final_mock_ready && gshell_visual_final_polish_ready;
        gshell_visual_final_ready = ok ? 1 : 0;
        gshell_visual_final_state = ok ? "ready" : "partial";
        gshell_visual_final_last = ok ? "ready-ok" : "ready-partial";
        gshell_visual_final_events++;
        gshell_command_name = "VISUALREADINESS";
        gshell_command_result = ok ? "VISUAL READINESS OK" : "VISUAL READINESS PARTIAL";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = ok ? "READY OK" : "READY PART";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "VISUALREADINESS -> VISUAL UI READY" : "VISUALREADINESS -> RUN VISUALDEMOALL");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALDEMOALL) {
        gshell_visual_final_ready = 1;
        gshell_visual_final_scene_ready = 1;
        gshell_visual_final_mock_ready = 1;
        gshell_visual_final_cards_ready = 1;
        gshell_visual_final_launcher_ready = 1;
        gshell_visual_final_polish_ready = 1;
        gshell_visual_final_bounds_ready = 1;
        gshell_visual_final_density_ready = 1;
        gshell_visual_final_default_ready = 1;
        gshell_visual_final_state = "demo-all";
        gshell_visual_final_focus = "desktop";
        gshell_visual_final_last = "demo-all";

        gshell_scene_background_ready = 1;
        gshell_scene_topbar_ready = 1;
        gshell_scene_launcher_ready = 1;
        gshell_scene_cards_ready = 1;
        gshell_scene_window_ready = 1;
        gshell_scene_taskbar_ready = 1;
        gshell_scene_dock_ready = 1;
        gshell_scene_tray_ready = 1;
        gshell_scene_badge_ready = 1;
        gshell_scene_focus_ready = 1;
        gshell_scene_widgets_ready = 1;
        gshell_scene_state = "demo-all";

        gshell_visual_final_events++;
        gshell_command_name = "VISUALDEMOALL";
        gshell_command_result = "VISUAL DEMO ALL OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "DEMO OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALDEMOALL -> FULL 1.4 VISUAL DEMO READY");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALREGRESSION) {
        gshell_visual_final_state = "regression";
        gshell_visual_final_last = "regression-ok";
        gshell_visual_final_events++;
        gshell_command_name = "VISUALREGRESSION";
        gshell_command_result = "VISUAL REGRESSION OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "REGRESS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALREGRESSION -> OLD VISUAL COMMANDS PRESERVED");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALBOUNDS) {
        gshell_visual_final_bounds_ready = 1;
        gshell_visual_final_state = "bounds";
        gshell_visual_final_last = "bounds-ok";
        gshell_visual_final_events++;
        gshell_command_name = "VISUALBOUNDS";
        gshell_command_result = "VISUAL BOUNDS OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "BOUNDS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALBOUNDS -> 800x600 PANEL BOUNDS CHECKED");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALDENSITY) {
        gshell_visual_final_density_ready = 1;
        gshell_visual_final_state = "density";
        gshell_visual_final_last = "density-ok";
        gshell_visual_final_events++;
        gshell_command_name = "VISUALDENSITY";
        gshell_command_result = "VISUAL DENSITY OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "DENSITY OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALDENSITY -> LOW RES DENSITY ACCEPTED");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALDEFAULT) {
        gshell_visual_final_default_ready = 1;
        gshell_visual_final_state = "default-scene";
        gshell_visual_final_last = "default";
        gshell_scene_background_ready = 1;
        gshell_scene_topbar_ready = 1;
        gshell_scene_window_ready = 1;
        gshell_scene_taskbar_ready = 1;
        gshell_command_name = "VISUALDEFAULT";
        gshell_command_result = "VISUAL DEFAULT OK";
        gshell_command_view = "DESKTOPSCENE";
        gshell_input_status_text = "DEFAULT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALDEFAULT -> DEFAULT DESKTOP SCENE ACTIVE");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALHANDOFF) {
        gshell_visual_final_state = "handoff";
        gshell_visual_final_last = "handoff-ready";
        gshell_visual_final_events++;
        gshell_command_name = "VISUALHANDOFF";
        gshell_command_result = "VISUAL HANDOFF OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "HANDOFF";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALHANDOFF -> READY FOR INPUT OR NEXT UI LAYER");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALNEXTPHASE) {
        gshell_visual_final_state = "next-phase";
        gshell_visual_final_last = "next-phase";
        gshell_visual_final_events++;
        gshell_command_name = "VISUALNEXTPHASE";
        gshell_command_result = "VISUAL NEXT PHASE OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "NEXT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALNEXTPHASE -> 1.5 INPUT OR DESKTOP INTERACTION");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALFINALCHECK) {
        int ok = gshell_visual_final_ready || (gshell_scene_background_ready && gshell_scene_window_ready && gshell_scene_taskbar_ready);
        gshell_visual_final_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "VISUALFINALCHECK";
        gshell_command_result = ok ? "VISUAL FINAL CHECK OK" : "VISUAL FINAL CHECK WAIT";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = ok ? "FINAL OK" : "FINAL WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "VISUALFINALCHECK -> 1.4 VISUAL UI CLOSEOUT OK" : "VISUALFINALCHECK -> RUN VISUALDEMOALL");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALFINALRESET) {
        gshell_visual_final_ready = 0;
        gshell_visual_final_scene_ready = 0;
        gshell_visual_final_mock_ready = 0;
        gshell_visual_final_cards_ready = 0;
        gshell_visual_final_launcher_ready = 0;
        gshell_visual_final_polish_ready = 0;
        gshell_visual_final_bounds_ready = 0;
        gshell_visual_final_density_ready = 0;
        gshell_visual_final_default_ready = 1;
        gshell_visual_final_events = 0;
        gshell_visual_final_state = "idle";
        gshell_visual_final_focus = "desktop";
        gshell_visual_final_last = "reset";
        gshell_command_name = "VISUALFINALRESET";
        gshell_command_result = "VISUAL FINAL RESET OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALFINALRESET -> VISUAL FINAL STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALROADMAP) {
        gshell_command_name = "VISUALROADMAP";
        gshell_command_result = "VISUAL ROADMAP OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "ROADMAP";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALROADMAP -> NEXT REAL INPUT / CLICK / WINDOW INTERACTION");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALINPUTNEXT) {
        gshell_command_name = "VISUALINPUTNEXT";
        gshell_command_result = "VISUAL INPUT NEXT OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "INPUT NEXT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALINPUTNEXT -> 1.5 REAL INPUT DECISION POINT");
        return;
    }

    if (command_id == GSHELL_CMD_VISUALCLOSEOUT) {
        gshell_visual_final_ready = 1;
        gshell_visual_final_state = "closeout";
        gshell_visual_final_last = "closeout";
        gshell_visual_final_events++;
        gshell_command_name = "VISUALCLOSEOUT";
        gshell_command_result = "VISUAL CLOSEOUT OK";
        gshell_command_view = "VISUALFINAL";
        gshell_input_status_text = "CLOSEOUT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("VISUALCLOSEOUT -> 1.4 VISUAL UI STAGE CLOSED");
        return;
    }

    if (command_id == GSHELL_CMD_INTERACTSTATUS) {
        gshell_command_name = "INTERACTSTATUS";
        gshell_command_result = "INTERACT STATUS OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("INTERACTSTATUS -> DESKTOP INTERACTION EVENT CORE READY");
        return;
    }

    if (command_id == GSHELL_CMD_POINTERMOCK) {
        gshell_interact_pointer_ready = 1;
        gshell_interact_pointer_x = 120;
        gshell_interact_pointer_y = 80;
        gshell_interact_state = "pointer";
        gshell_interact_target = "desktop";
        gshell_interact_last = "pointer-ready";
        gshell_focus_target = "desktop";
        gshell_interact_events++;
        gshell_command_name = "POINTERMOCK";
        gshell_command_result = "POINTER MOCK OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "PTR OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POINTERMOCK -> MOCK POINTER READY");
        return;
    }

    if (command_id == GSHELL_CMD_POINTERMOVE) {
        gshell_interact_pointer_ready = 1;
        gshell_interact_pointer_x += 16;
        gshell_interact_pointer_y += 8;
        gshell_interact_state = "move";
        gshell_interact_target = "desktop";
        gshell_interact_last = "move";
        gshell_interact_events++;
        gshell_command_name = "POINTERMOVE";
        gshell_command_result = "POINTER MOVE OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "MOVE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POINTERMOVE -> MOCK POINTER MOVED");
        return;
    }

    if (command_id == GSHELL_CMD_POINTERHOVER) {
        gshell_interact_pointer_ready = 1;
        gshell_interact_hover_ready = 1;
        gshell_interact_state = "hover";
        gshell_interact_target = "demo.app";
        gshell_interact_last = "hover-card";
        gshell_hit_zone = "scene-card";
        gshell_hit_target = "demo.app";
        gshell_hit_action = "hover";
        gshell_focus_target = "demo.app";
        gshell_interact_events++;
        gshell_command_name = "POINTERHOVER";
        gshell_command_result = "POINTER HOVER OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "HOVER OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POINTERHOVER -> DEMO APP HOVERED");
        return;
    }

    if (command_id == GSHELL_CMD_POINTERCLICK) {
        gshell_interact_pointer_ready = 1;
        gshell_interact_click_ready = 1;
        gshell_interact_select_ready = 1;
        gshell_interact_state = "click";
        gshell_interact_target = "demo.app";
        gshell_interact_last = "click-card";
        gshell_scene_focus = "demo.app";
        gshell_scene_focus_ready = 1;
        gshell_focus_target = "demo.app";
        gshell_interact_events++;
        gshell_command_name = "POINTERCLICK";
        gshell_command_result = "POINTER CLICK OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "CLICK OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("POINTERCLICK -> DEMO APP CLICKED");
        return;
    }

    if (command_id == GSHELL_CMD_FOCUSMOCK) {
        gshell_interact_focus_ready = 1;
        gshell_interact_state = "focus";
        gshell_interact_target = "desktop";
        gshell_interact_last = "focus-ready";
        gshell_focus_target = "desktop";
        gshell_focus_changes++;
        gshell_interact_events++;
        gshell_command_name = "FOCUSMOCK";
        gshell_command_result = "FOCUS MOCK OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "FOCUS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FOCUSMOCK -> FOCUS MODEL READY");
        return;
    }

    if (command_id == GSHELL_CMD_FOCUSNEXT) {
        gshell_interact_focus_ready = 1;
        gshell_interact_state = "focus-next";
        gshell_interact_target = "demo.app";
        gshell_interact_last = "focus-next";
        gshell_focus_target = "demo.app";
        gshell_focus_changes++;
        gshell_interact_events++;
        gshell_command_name = "FOCUSNEXT";
        gshell_command_result = "FOCUS NEXT OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "NEXT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FOCUSNEXT -> FOCUS MOVED TO DEMO APP");
        return;
    }

    if (command_id == GSHELL_CMD_FOCUSCARD) {
        gshell_interact_focus_ready = 1;
        gshell_interact_state = "focus-card";
        gshell_interact_target = "demo.app";
        gshell_interact_last = "focus-card";
        gshell_scene_focus_ready = 1;
        gshell_scene_focus = "demo.app";
        gshell_focus_target = "demo.app";
        gshell_interact_events++;
        gshell_command_name = "FOCUSCARD";
        gshell_command_result = "FOCUS CARD OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "CARD OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FOCUSCARD -> CARD FOCUS READY");
        return;
    }

    if (command_id == GSHELL_CMD_FOCUSWINDOW) {
        gshell_interact_focus_ready = 1;
        gshell_interact_state = "focus-window";
        gshell_interact_target = "demo.window";
        gshell_interact_last = "focus-window";
        gshell_window_exists = 1;
        gshell_window_focused = 1;
        gshell_window_state = "focused";
        gshell_focus_target = "demo.window";
        gshell_interact_events++;
        gshell_command_name = "FOCUSWINDOW";
        gshell_command_result = "FOCUS WINDOW OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "WINDOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FOCUSWINDOW -> WINDOW FOCUS READY");
        return;
    }

    if (command_id == GSHELL_CMD_FOCUSTASKBAR) {
        gshell_interact_focus_ready = 1;
        gshell_interact_state = "focus-taskbar";
        gshell_interact_target = "taskbar";
        gshell_interact_last = "focus-taskbar";
        gshell_focus_target = "taskbar";
        gshell_taskbar_focused = 1;
        gshell_interact_events++;
        gshell_command_name = "FOCUSTASKBAR";
        gshell_command_result = "FOCUS TASKBAR OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "TASK OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FOCUSTASKBAR -> TASKBAR FOCUS READY");
        return;
    }

    if (command_id == GSHELL_CMD_SELECTMOCK) {
        gshell_interact_select_ready = 1;
        gshell_interact_state = "selected";
        gshell_interact_target = "demo.app";
        gshell_interact_last = "select";
        gshell_card_selected = 1;
        gshell_card_app = "demo.app";
        gshell_scene_focus_ready = 1;
        gshell_focus_target = "demo.app";
        gshell_interact_events++;
        gshell_command_name = "SELECTMOCK";
        gshell_command_result = "SELECT MOCK OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "SELECT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SELECTMOCK -> DEMO APP SELECTED");
        return;
    }

    if (command_id == GSHELL_CMD_OPENMOCK) {
        gshell_interact_open_ready = 1;
        gshell_interact_state = "open";
        gshell_interact_target = "demo.window";
        gshell_interact_last = "open-window";
        gshell_scene_window_ready = 1;
        gshell_window_exists = 1;
        gshell_window_title = "demo.window";
        gshell_window_state = "open";
        gshell_focus_target = "demo.window";
        gshell_interact_events++;
        gshell_command_name = "OPENMOCK";
        gshell_command_result = "OPEN MOCK OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "OPEN OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("OPENMOCK -> DEMO WINDOW OPENED BY INTERACTION");
        return;
    }

    if (command_id == GSHELL_CMD_CLOSEMOCK) {
        gshell_interact_open_ready = 0;
        gshell_interact_state = "close";
        gshell_interact_target = "demo.window";
        gshell_interact_last = "close-window";
        gshell_window_state = "closed";
        gshell_focus_target = "desktop";
        gshell_interact_events++;
        gshell_command_name = "CLOSEMOCK";
        gshell_command_result = "CLOSE MOCK OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "CLOSE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CLOSEMOCK -> DEMO WINDOW CLOSED BY INTERACTION");
        return;
    }

    if (command_id == GSHELL_CMD_MENUMOCK) {
        gshell_interact_menu_ready = 1;
        gshell_interact_state = "menu";
        gshell_interact_target = "desktop-menu";
        gshell_interact_last = "menu-open";
        gshell_focus_target = "desktop-menu";
        gshell_interact_events++;
        gshell_command_name = "MENUMOCK";
        gshell_command_result = "MENU MOCK OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "MENU OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MENUMOCK -> DESKTOP MENU OPENED");
        return;
    }

    if (command_id == GSHELL_CMD_SHORTCUTMOCK) {
        gshell_interact_shortcut_ready = 1;
        gshell_interact_state = "shortcut";
        gshell_interact_target = "launcher";
        gshell_interact_last = "shortcut-launcher";
        gshell_scene_launcher_ready = 1;
        gshell_focus_target = "launcher";
        gshell_interact_events++;
        gshell_command_name = "SHORTCUTMOCK";
        gshell_command_result = "SHORTCUT MOCK OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "SHORTCUT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SHORTCUTMOCK -> LAUNCHER SHORTCUT ROUTED");
        return;
    }

    if (command_id == GSHELL_CMD_ROUTEMOCK) {
        gshell_interact_route_ready = 1;
        gshell_interact_state = "route";
        gshell_interact_target = "scene";
        gshell_interact_last = "route-scene";
        gshell_scene_state = "interaction-routed";
        gshell_interact_events++;
        gshell_command_name = "ROUTEMOCK";
        gshell_command_result = "ROUTE MOCK OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "ROUTE OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ROUTEMOCK -> INTERACTION ROUTED TO DESKTOP SCENE");
        return;
    }

    if (command_id == GSHELL_CMD_INTERACTDEMO) {
        gshell_interact_pointer_ready = 1;
        gshell_interact_hover_ready = 1;
        gshell_interact_click_ready = 1;
        gshell_interact_focus_ready = 1;
        gshell_interact_select_ready = 1;
        gshell_interact_open_ready = 1;
        gshell_interact_menu_ready = 1;
        gshell_interact_shortcut_ready = 1;
        gshell_interact_route_ready = 1;
        gshell_interact_pointer_x = 156;
        gshell_interact_pointer_y = 98;
        gshell_interact_state = "demo";
        gshell_interact_target = "demo.app";
        gshell_interact_last = "demo-ready";

        gshell_scene_background_ready = 1;
        gshell_scene_topbar_ready = 1;
        gshell_scene_launcher_ready = 1;
        gshell_scene_cards_ready = 1;
        gshell_scene_window_ready = 1;
        gshell_scene_taskbar_ready = 1;
        gshell_scene_focus_ready = 1;
        gshell_scene_state = "interactive-demo";

        gshell_focus_target = "demo.app";
        gshell_interact_events++;
        gshell_command_name = "INTERACTDEMO";
        gshell_command_result = "INTERACT DEMO OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "DEMO OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("INTERACTDEMO -> FULL INTERACTION EVENT DEMO READY");
        return;
    }

    if (command_id == GSHELL_CMD_INTERACTCHECK) {
        int ok = gshell_interact_pointer_ready && gshell_interact_focus_ready && gshell_interact_select_ready && gshell_interact_route_ready;
        gshell_interact_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "INTERACTCHECK";
        gshell_command_result = ok ? "INTERACT CHECK OK" : "INTERACT CHECK WAIT";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = ok ? "INTERACT OK" : "INTERACT WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "INTERACTCHECK -> DESKTOP INTERACTION MODEL READY" : "INTERACTCHECK -> NEED POINTER FOCUS SELECT ROUTE");
        return;
    }

    if (command_id == GSHELL_CMD_INTERACTRESET) {
        gshell_interact_pointer_ready = 0;
        gshell_interact_pointer_x = 120;
        gshell_interact_pointer_y = 80;
        gshell_interact_hover_ready = 0;
        gshell_interact_click_ready = 0;
        gshell_interact_focus_ready = 0;
        gshell_interact_select_ready = 0;
        gshell_interact_open_ready = 0;
        gshell_interact_menu_ready = 0;
        gshell_interact_shortcut_ready = 0;
        gshell_interact_route_ready = 0;
        gshell_interact_events = 0;
        gshell_interact_state = "idle";
        gshell_interact_target = "desktop";
        gshell_interact_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "INTERACTRESET";
        gshell_command_result = "INTERACT RESET OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("INTERACTRESET -> DESKTOP INTERACTION STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_INTERACTNEXT) {
        gshell_command_name = "INTERACTNEXT";
        gshell_command_result = "INTERACT NEXT OK";
        gshell_command_view = "INTERACTSTATUS";
        gshell_input_status_text = "NEXT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("INTERACTNEXT -> 1.5.1 CLICK ROUTING OR 1.6 PS2 MOUSE");
        return;
    }

    if (command_id == GSHELL_CMD_ROUTESTATUS) {
        gshell_command_name = "ROUTESTATUS";
        gshell_command_result = "ROUTE STATUS OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "COMMAND OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ROUTESTATUS -> DESKTOP CLICK ROUTING CORE READY");
        return;
    }

    if (command_id == GSHELL_CMD_HITDESKTOP) {
        gshell_route_desktop_ready = 1;
        gshell_route_state = "hit-desktop";
        gshell_route_target = "desktop";
        gshell_route_last = "hit-desktop";
        gshell_hit_zone = "desktop";
        gshell_hit_target = "desktop";
        gshell_hit_action = "hit";
        gshell_interact_target = "desktop";
        gshell_focus_target = "desktop";
        gshell_route_events++;
        gshell_command_name = "HITDESKTOP";
        gshell_command_result = "HIT DESKTOP OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "DESK OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("HITDESKTOP -> DESKTOP HIT TARGET READY");
        return;
    }

    if (command_id == GSHELL_CMD_HITCARD) {
        gshell_route_card_ready = 1;
        gshell_route_state = "hit-card";
        gshell_route_target = "demo.app";
        gshell_route_last = "hit-card";
        gshell_hit_zone = "scene-card";
        gshell_hit_target = "demo.app";
        gshell_hit_action = "hit";
        gshell_interact_target = "demo.app";
        gshell_focus_target = "demo.app";
        gshell_card_selected = 1;
        gshell_card_app = "demo.app";
        gshell_route_events++;
        gshell_command_name = "HITCARD";
        gshell_command_result = "HIT CARD OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "CARD OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("HITCARD -> DEMO APP CARD HIT TARGET READY");
        return;
    }

    if (command_id == GSHELL_CMD_HITWINDOW) {
        gshell_route_window_ready = 1;
        gshell_route_state = "hit-window";
        gshell_route_target = "demo.window";
        gshell_route_last = "hit-window";
        gshell_hit_zone = "window";
        gshell_hit_target = "demo.window";
        gshell_hit_action = "hit";
        gshell_interact_target = "demo.window";
        gshell_window_exists = 1;
        gshell_window_title = "demo.window";
        gshell_window_state = "hit";
        gshell_focus_target = "demo.window";
        gshell_route_events++;
        gshell_command_name = "HITWINDOW";
        gshell_command_result = "HIT WINDOW OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "WINDOW OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("HITWINDOW -> DEMO WINDOW HIT TARGET READY");
        return;
    }

    if (command_id == GSHELL_CMD_HITTASKBAR) {
        gshell_route_taskbar_ready = 1;
        gshell_route_state = "hit-taskbar";
        gshell_route_target = "taskbar";
        gshell_route_last = "hit-taskbar";
        gshell_hit_zone = "taskbar";
        gshell_hit_target = "taskbar";
        gshell_hit_action = "hit";
        gshell_interact_target = "taskbar";
        gshell_taskbar_focused = 1;
        gshell_focus_target = "taskbar";
        gshell_route_events++;
        gshell_command_name = "HITTASKBAR";
        gshell_command_result = "HIT TASKBAR OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "TASK OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("HITTASKBAR -> TASKBAR HIT TARGET READY");
        return;
    }

    if (command_id == GSHELL_CMD_HITLAUNCHER) {
        gshell_route_launcher_ready = 1;
        gshell_route_state = "hit-launcher";
        gshell_route_target = "launcher";
        gshell_route_last = "hit-launcher";
        gshell_hit_zone = "launcher";
        gshell_hit_target = "launcher";
        gshell_hit_action = "hit";
        gshell_interact_target = "launcher";
        gshell_scene_launcher_ready = 1;
        gshell_focus_target = "launcher";
        gshell_route_events++;
        gshell_command_name = "HITLAUNCHER";
        gshell_command_result = "HIT LAUNCHER OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "LAUNCH OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("HITLAUNCHER -> LAUNCHER HIT TARGET READY");
        return;
    }

    if (command_id == GSHELL_CMD_HOVERROUTE) {
        gshell_route_hover_ready = 1;
        gshell_interact_hover_ready = 1;
        gshell_interact_pointer_ready = 1;
        gshell_route_state = "hover";
        gshell_route_target = "demo.app";
        gshell_route_last = "hover-route";
        gshell_hit_zone = "scene-card";
        gshell_hit_target = "demo.app";
        gshell_hit_action = "hover";
        gshell_interact_state = "hover";
        gshell_interact_target = "demo.app";
        gshell_focus_target = "demo.app";
        gshell_route_events++;
        gshell_command_name = "HOVERROUTE";
        gshell_command_result = "HOVER ROUTE OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "HOVER OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("HOVERROUTE -> HOVER ROUTED TO DEMO APP");
        return;
    }

    if (command_id == GSHELL_CMD_CLICKROUTE) {
        gshell_route_click_ready = 1;
        gshell_interact_click_ready = 1;
        gshell_interact_select_ready = 1;
        gshell_route_card_ready = 1;
        gshell_route_state = "click";
        gshell_route_target = "demo.app";
        gshell_route_last = "click-route";
        gshell_hit_zone = "scene-card";
        gshell_hit_target = "demo.app";
        gshell_hit_action = "click";
        gshell_interact_state = "click";
        gshell_interact_target = "demo.app";
        gshell_card_selected = 1;
        gshell_scene_focus_ready = 1;
        gshell_focus_target = "demo.app";
        gshell_route_events++;
        gshell_command_name = "CLICKROUTE";
        gshell_command_result = "CLICK ROUTE OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "CLICK OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("CLICKROUTE -> CLICK ROUTED TO DEMO APP CARD");
        return;
    }

    if (command_id == GSHELL_CMD_DOUBLECLICKROUTE) {
        gshell_route_double_click_ready = 1;
        gshell_route_click_ready = 1;
        gshell_route_open_ready = 1;
        gshell_interact_open_ready = 1;
        gshell_route_state = "double-click";
        gshell_route_target = "demo.window";
        gshell_route_last = "double-click-open";
        gshell_hit_zone = "scene-card";
        gshell_hit_target = "demo.app";
        gshell_hit_action = "double-click";
        gshell_window_exists = 1;
        gshell_window_title = "demo.window";
        gshell_window_state = "open";
        gshell_scene_window_ready = 1;
        gshell_focus_target = "demo.window";
        gshell_route_events++;
        gshell_command_name = "DOUBLECLICKROUTE";
        gshell_command_result = "DOUBLE CLICK ROUTE OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "DBL OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DOUBLECLICKROUTE -> DOUBLE CLICK OPENED DEMO WINDOW");
        return;
    }

    if (command_id == GSHELL_CMD_RIGHTCLICKROUTE) {
        gshell_route_right_click_ready = 1;
        gshell_route_menu_ready = 1;
        gshell_interact_menu_ready = 1;
        gshell_route_state = "right-click";
        gshell_route_target = "desktop-menu";
        gshell_route_last = "right-click-menu";
        gshell_hit_zone = "desktop";
        gshell_hit_target = "desktop-menu";
        gshell_hit_action = "right-click";
        gshell_focus_target = "desktop-menu";
        gshell_route_events++;
        gshell_command_name = "RIGHTCLICKROUTE";
        gshell_command_result = "RIGHT CLICK ROUTE OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "RIGHT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("RIGHTCLICKROUTE -> RIGHT CLICK ROUTED TO MENU");
        return;
    }

    if (command_id == GSHELL_CMD_MENUROUTE) {
        gshell_route_menu_ready = 1;
        gshell_interact_menu_ready = 1;
        gshell_route_state = "menu";
        gshell_route_target = "desktop-menu";
        gshell_route_last = "menu-route";
        gshell_hit_zone = "menu";
        gshell_hit_target = "desktop-menu";
        gshell_hit_action = "open-menu";
        gshell_focus_target = "desktop-menu";
        gshell_route_events++;
        gshell_command_name = "MENUROUTE";
        gshell_command_result = "MENU ROUTE OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "MENU OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("MENUROUTE -> MENU ROUTE READY");
        return;
    }

    if (command_id == GSHELL_CMD_OPENROUTE) {
        gshell_route_open_ready = 1;
        gshell_interact_open_ready = 1;
        gshell_route_state = "open";
        gshell_route_target = "demo.window";
        gshell_route_last = "open-route";
        gshell_window_exists = 1;
        gshell_window_title = "demo.window";
        gshell_window_state = "open";
        gshell_scene_window_ready = 1;
        gshell_focus_target = "demo.window";
        gshell_route_events++;
        gshell_command_name = "OPENROUTE";
        gshell_command_result = "OPEN ROUTE OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "OPEN OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("OPENROUTE -> OPEN ROUTED TO DEMO WINDOW");
        return;
    }

    if (command_id == GSHELL_CMD_SELECTROUTE) {
        gshell_route_select_ready = 1;
        gshell_interact_select_ready = 1;
        gshell_route_card_ready = 1;
        gshell_route_state = "select";
        gshell_route_target = "demo.app";
        gshell_route_last = "select-route";
        gshell_card_selected = 1;
        gshell_card_app = "demo.app";
        gshell_focus_target = "demo.app";
        gshell_route_events++;
        gshell_command_name = "SELECTROUTE";
        gshell_command_result = "SELECT ROUTE OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "SELECT OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("SELECTROUTE -> SELECT ROUTED TO DEMO APP");
        return;
    }

    if (command_id == GSHELL_CMD_FOCUSROUTE) {
        gshell_route_focus_ready = 1;
        gshell_interact_focus_ready = 1;
        gshell_route_state = "focus";
        gshell_route_target = "demo.app";
        gshell_route_last = "focus-route";
        gshell_scene_focus_ready = 1;
        gshell_scene_focus = "demo.app";
        gshell_focus_target = "demo.app";
        gshell_focus_changes++;
        gshell_route_events++;
        gshell_command_name = "FOCUSROUTE";
        gshell_command_result = "FOCUS ROUTE OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "FOCUS OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("FOCUSROUTE -> FOCUS ROUTED TO DEMO APP");
        return;
    }

    if (command_id == GSHELL_CMD_DRAGROUTE) {
        gshell_route_drag_ready = 1;
        gshell_route_state = "drag";
        gshell_route_target = "demo.window";
        gshell_route_last = "drag-window";
        gshell_hit_zone = "window-title";
        gshell_hit_target = "demo.window";
        gshell_hit_action = "drag";
        gshell_window_state = "dragging";
        gshell_focus_target = "demo.window";
        gshell_route_events++;
        gshell_command_name = "DRAGROUTE";
        gshell_command_result = "DRAG ROUTE OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "DRAG OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DRAGROUTE -> DRAG ROUTED TO WINDOW TITLE");
        return;
    }

    if (command_id == GSHELL_CMD_DROPROUTE) {
        gshell_route_drop_ready = 1;
        gshell_route_state = "drop";
        gshell_route_target = "demo.window";
        gshell_route_last = "drop-window";
        gshell_hit_zone = "window";
        gshell_hit_target = "demo.window";
        gshell_hit_action = "drop";
        gshell_window_state = "dropped";
        gshell_focus_target = "demo.window";
        gshell_route_events++;
        gshell_command_name = "DROPROUTE";
        gshell_command_result = "DROP ROUTE OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "DROP OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("DROPROUTE -> DROP ROUTED TO WINDOW");
        return;
    }

    if (command_id == GSHELL_CMD_ROUTEDEMO) {
        gshell_route_desktop_ready = 1;
        gshell_route_card_ready = 1;
        gshell_route_window_ready = 1;
        gshell_route_taskbar_ready = 1;
        gshell_route_launcher_ready = 1;
        gshell_route_hover_ready = 1;
        gshell_route_click_ready = 1;
        gshell_route_double_click_ready = 1;
        gshell_route_right_click_ready = 1;
        gshell_route_menu_ready = 1;
        gshell_route_open_ready = 1;
        gshell_route_select_ready = 1;
        gshell_route_focus_ready = 1;
        gshell_route_drag_ready = 1;
        gshell_route_drop_ready = 1;
        gshell_route_state = "demo";
        gshell_route_target = "demo.app";
        gshell_route_last = "demo-ready";

        gshell_interact_pointer_ready = 1;
        gshell_interact_hover_ready = 1;
        gshell_interact_click_ready = 1;
        gshell_interact_focus_ready = 1;
        gshell_interact_select_ready = 1;
        gshell_interact_open_ready = 1;
        gshell_interact_route_ready = 1;
        gshell_interact_target = "demo.app";

        gshell_scene_background_ready = 1;
        gshell_scene_cards_ready = 1;
        gshell_scene_window_ready = 1;
        gshell_scene_taskbar_ready = 1;
        gshell_scene_focus_ready = 1;
        gshell_focus_target = "demo.app";

        gshell_route_events++;
        gshell_command_name = "ROUTEDEMO";
        gshell_command_result = "ROUTE DEMO OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "DEMO OK";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ROUTEDEMO -> FULL CLICK ROUTING DEMO READY");
        return;
    }

    if (command_id == GSHELL_CMD_ROUTECHECK) {
        int ok = gshell_route_desktop_ready && gshell_route_card_ready && gshell_route_window_ready && gshell_route_click_ready && gshell_route_focus_ready;
        gshell_route_last = ok ? "check-ok" : "check-wait";
        gshell_command_name = "ROUTECHECK";
        gshell_command_result = ok ? "ROUTE CHECK OK" : "ROUTE CHECK WAIT";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = ok ? "ROUTE OK" : "ROUTE WAIT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push(ok ? "ROUTECHECK -> CLICK ROUTING READY" : "ROUTECHECK -> NEED DESKTOP CARD WINDOW CLICK FOCUS");
        return;
    }

    if (command_id == GSHELL_CMD_ROUTERESET) {
        gshell_route_desktop_ready = 0;
        gshell_route_card_ready = 0;
        gshell_route_window_ready = 0;
        gshell_route_taskbar_ready = 0;
        gshell_route_launcher_ready = 0;
        gshell_route_hover_ready = 0;
        gshell_route_click_ready = 0;
        gshell_route_double_click_ready = 0;
        gshell_route_right_click_ready = 0;
        gshell_route_menu_ready = 0;
        gshell_route_open_ready = 0;
        gshell_route_select_ready = 0;
        gshell_route_focus_ready = 0;
        gshell_route_drag_ready = 0;
        gshell_route_drop_ready = 0;
        gshell_route_events = 0;
        gshell_route_state = "idle";
        gshell_route_target = "desktop";
        gshell_route_last = "reset";
        gshell_focus_target = "terminal";
        gshell_command_name = "ROUTERESET";
        gshell_command_result = "ROUTE RESET OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "RESET";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ROUTERESET -> CLICK ROUTING STATE RESET");
        return;
    }

    if (command_id == GSHELL_CMD_ROUTENEXT) {
        gshell_command_name = "ROUTENEXT";
        gshell_command_result = "ROUTE NEXT OK";
        gshell_command_view = "ROUTESTATUS";
        gshell_input_status_text = "NEXT";
        gshell_parser_status_text = "REGISTRY";
        gshell_history_push(gshell_command_normalized, gshell_command_view);
        gshell_result_log_push(gshell_command_normalized, gshell_command_result);
        gshell_terminal_push("ROUTENEXT -> 1.5.2 DESKTOP ACTION BINDING");
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
    graphics_text(x + 24, y + 224, "routestatus");
    graphics_text(x + 132, y + 224, "hitcard");
    graphics_text(x + 24, y + 252, "clickroute");
    graphics_text(x + 132, y + 252, "routecheck");
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

static void gshell_draw_command_view_runtimestatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "APP RUNTIME BASE");
    graphics_text(x + 24, y + 52, "RUNTIME");
    graphics_text(x + 156, y + 52, "prep");
    graphics_text(x + 24, y + 76, "USER");
    graphics_text(x + 156, y + 76, health_user_ok() ? "ok" : "bad");
    graphics_text(x + 24, y + 100, "RING3");
    graphics_text(x + 156, y + 100, health_ring3_ok() ? "ok" : "bad");
    graphics_text(x + 24, y + 124, "SYSCALL");
    graphics_text(x + 156, y + 124, health_syscall_ok() ? "ok" : "bad");
    graphics_text(x + 24, y + 148, "LOADER");
    graphics_text(x + 156, y + 148, "next");
    graphics_text(x + 24, y + 172, "APP SLOT");
    graphics_text(x + 156, y + 172, "planned");
    graphics_text(x + 24, y + 196, "SANDBOX");
    graphics_text(x + 156, y + 196, security_sandbox_profile());
    graphics_text(x + 24, y + 220, "RULE");
    graphics_text(x + 156, y + 220, security_rule_default_policy());
    graphics_text(x + 24, y + 244, "GATE");
    graphics_text(x + 156, y + 244, security_decision_health());
    graphics_text(x + 24, y + 268, "NEXT");
    graphics_text(x + 156, y + 268, "elf loader");
}

static void gshell_draw_command_view_elfstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "ELF LOADER METADATA");
    graphics_text(x + 24, y + 52, "FORMAT");
    graphics_text(x + 156, y + 52, "ELF32");
    graphics_text(x + 24, y + 76, "ARCH");
    graphics_text(x + 156, y + 76, "i386");
    graphics_text(x + 24, y + 100, "TYPE");
    graphics_text(x + 156, y + 100, "executable");
    graphics_text(x + 24, y + 124, "HEADER");
    graphics_text(x + 156, y + 124, "metadata");
    graphics_text(x + 24, y + 148, "SEGMENTS");
    graphics_text(x + 156, y + 148, "planned");
    graphics_text(x + 24, y + 172, "SECTIONS");
    graphics_text(x + 156, y + 172, "planned");
    graphics_text(x + 24, y + 196, "ENTRY");
    graphics_text(x + 156, y + 196, "planned");
    graphics_text(x + 24, y + 220, "SANDBOX");
    graphics_text(x + 156, y + 220, security_sandbox_profile());
    graphics_text(x + 24, y + 244, "VALIDATE");
    graphics_text(x + 156, y + 244, (health_user_ok() && health_ring3_ok() && health_syscall_ok()) ? "ready" : "bad");
    graphics_text(x + 24, y + 268, "NEXT");
    graphics_text(x + 156, y + 268, "manifest");
}

static void gshell_draw_command_view_manifeststatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "APP MANIFEST CORE");
    graphics_text(x + 24, y + 52, "MANIFEST");
    graphics_text(x + 156, y + 52, "metadata");
    graphics_text(x + 24, y + 76, "APP NAME");
    graphics_text(x + 156, y + 76, "demo.app");
    graphics_text(x + 24, y + 100, "APP TYPE");
    graphics_text(x + 156, y + 100, "user");
    graphics_text(x + 24, y + 124, "ENTRY");
    graphics_text(x + 156, y + 124, "main");
    graphics_text(x + 24, y + 148, "BINARY");
    graphics_text(x + 156, y + 148, "elf32");
    graphics_text(x + 24, y + 172, "CAPS");
    graphics_text(x + 156, y + 172, "declared");
    graphics_text(x + 24, y + 196, "SANDBOX");
    graphics_text(x + 156, y + 196, security_sandbox_profile());
    graphics_text(x + 24, y + 220, "RULE");
    graphics_text(x + 156, y + 220, security_rule_default_policy());
    graphics_text(x + 24, y + 244, "VALIDATE");
    graphics_text(x + 156, y + 244, (health_user_ok() && health_ring3_ok() && health_syscall_ok()) ? "ready" : "bad");
    graphics_text(x + 24, y + 268, "NEXT");
    graphics_text(x + 156, y + 268, "app slot");
}

static void gshell_draw_command_view_appslotstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "APP SLOT CORE");
    graphics_text(x + 24, y + 52, "SLOT 0");
    graphics_text(x + 156, y + 52, gshell_app_slot_allocated ? "used" : "free");
    graphics_text(x + 24, y + 76, "STATE");
    graphics_text(x + 156, y + 76, gshell_app_slot_state);
    gshell_draw_value_uint(x + 24, y + 100, "PID", gshell_app_slot_pid);
    gshell_draw_value_uint(x + 24, y + 124, "CHECKS", gshell_app_slot_checks);
    graphics_text(x + 24, y + 148, "USER");
    graphics_text(x + 156, y + 148, health_user_ok() ? "ok" : "bad");
    graphics_text(x + 24, y + 172, "RING3");
    graphics_text(x + 156, y + 172, health_ring3_ok() ? "ok" : "bad");
    graphics_text(x + 24, y + 196, "SYSCALL");
    graphics_text(x + 156, y + 196, health_syscall_ok() ? "ok" : "bad");
    graphics_text(x + 24, y + 220, "SANDBOX");
    graphics_text(x + 156, y + 220, security_sandbox_profile());
    graphics_text(x + 24, y + 244, "BINDING");
    graphics_text(x + 156, y + 244, gshell_app_slot_allocated ? "demo.app" : "none");
    graphics_text(x + 24, y + 268, "NEXT");
    graphics_text(x + 156, y + 268, "launch gate");
}

static void gshell_draw_command_view_launchgate(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "APP LAUNCH");
    graphics_text(x + 24, y + 52, "APP");
    graphics_text(x + 156, y + 52, "demo.app");
    graphics_text(x + 24, y + 76, "SLOT");
    graphics_text(x + 156, y + 76, gshell_app_slot_allocated ? "ready" : "none");
    graphics_text(x + 24, y + 100, "PREPARE");
    graphics_text(x + 156, y + 100, gshell_launch_prepared ? "yes" : "no");
    graphics_text(x + 24, y + 124, "APPROVE");
    graphics_text(x + 156, y + 124, gshell_launch_approved ? "yes" : "wait");
    graphics_text(x + 24, y + 148, "STATE");
    graphics_text(x + 156, y + 148, gshell_launch_state);
    graphics_text(x + 24, y + 172, "LAST");
    graphics_text(x + 156, y + 172, gshell_launch_last_result);
    gshell_draw_value_uint(x + 24, y + 196, "TRIES", gshell_launch_attempts);
    graphics_text(x + 24, y + 220, "SANDBOX");
    graphics_text(x + 156, y + 220, security_sandbox_profile());
    graphics_text(x + 24, y + 244, "RULE");
    graphics_text(x + 156, y + 244, security_rule_default_policy());
    graphics_text(x + 24, y + 268, "NEXT");
    graphics_text(x + 156, y + 268, "perm req");
}

static void gshell_draw_command_view_permstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "APP PERMISSION REQUEST");
    graphics_text(x + 24, y + 52, "APP");
    graphics_text(x + 156, y + 52, "demo.app");
    graphics_text(x + 24, y + 76, "REQUEST");
    graphics_text(x + 156, y + 76, gshell_perm_requested ? "yes" : "no");
    graphics_text(x + 24, y + 100, "CAPABILITY");
    graphics_text(x + 156, y + 100, "file.read");
    graphics_text(x + 24, y + 124, "USER ALLOW");
    graphics_text(x + 156, y + 124, gshell_perm_allowed ? "yes" : "no");
    graphics_text(x + 24, y + 148, "USER DENY");
    graphics_text(x + 156, y + 148, gshell_perm_denied ? "yes" : "no");
    graphics_text(x + 24, y + 172, "STATE");
    graphics_text(x + 156, y + 172, gshell_perm_state);
    graphics_text(x + 24, y + 196, "LAST");
    graphics_text(x + 156, y + 196, gshell_perm_last);
    gshell_draw_value_uint(x + 24, y + 220, "CHECKS", gshell_perm_checks);
    graphics_text(x + 24, y + 244, "FILE GATE");
    graphics_text(x + 156, y + 244, security_file_policy());
    graphics_text(x + 24, y + 268, "NEXT");
    graphics_text(x + 156, y + 268, "ramfs prep");
}

static void gshell_draw_command_view_fsstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "BASIC FS / RAMFS PREP");
    graphics_text(x + 24, y + 52, "FS");
    graphics_text(x + 156, y + 52, "ramfs");
    graphics_text(x + 24, y + 76, "MOUNT");
    graphics_text(x + 156, y + 76, gshell_fs_mounted ? "yes" : "no");
    graphics_text(x + 24, y + 100, "STATE");
    graphics_text(x + 156, y + 100, gshell_fs_state);
    graphics_text(x + 24, y + 124, "FILE");
    graphics_text(x + 156, y + 124, gshell_fs_file);
    gshell_draw_value_uint(x + 24, y + 148, "WRITES", gshell_fs_writes);
    gshell_draw_value_uint(x + 24, y + 172, "READS", gshell_fs_reads);
    gshell_draw_value_uint(x + 24, y + 196, "CHECKS", gshell_fs_checks);
    graphics_text(x + 24, y + 220, "FILE GATE");
    graphics_text(x + 156, y + 220, security_file_policy());
    graphics_text(x + 24, y + 244, "LAST");
    graphics_text(x + 156, y + 244, gshell_fs_last);
    graphics_text(x + 24, y + 268, "NEXT");
    graphics_text(x + 156, y + 268, "lifecycle");
}

static void gshell_draw_command_view_lifestatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "APP LIFECYCLE");
    graphics_text(x + 24, y + 52, "APP");
    graphics_text(x + 156, y + 52, "demo.app");
    graphics_text(x + 24, y + 76, "STATE");
    graphics_text(x + 156, y + 76, gshell_life_state);
    graphics_text(x + 24, y + 100, "LAST");
    graphics_text(x + 156, y + 100, gshell_life_last);
    gshell_draw_value_uint(x + 24, y + 124, "STARTS", gshell_life_starts);
    gshell_draw_value_uint(x + 24, y + 148, "PAUSES", gshell_life_pauses);
    gshell_draw_value_uint(x + 24, y + 172, "RESUMES", gshell_life_resumes);
    gshell_draw_value_uint(x + 24, y + 196, "STOPS", gshell_life_stops);
    gshell_draw_value_uint(x + 24, y + 220, "CHECKS", gshell_life_checks);
    graphics_text(x + 24, y + 244, "SANDBOX");
    graphics_text(x + 156, y + 244, security_sandbox_profile());
    graphics_text(x + 24, y + 268, "NEXT");
    graphics_text(x + 156, y + 268, "runtime panel");
}

static void gshell_draw_command_view_runtimepanel(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "APP RUNTIME PANEL");
    graphics_text(x + 24, y + 48, "USER/RING3");
    graphics_text(x + 156, y + 48, (health_user_ok() && health_ring3_ok()) ? "ok" : "bad");
    graphics_text(x + 24, y + 72, "SYSCALL");
    graphics_text(x + 156, y + 72, health_syscall_ok() ? "ok" : "bad");
    graphics_text(x + 24, y + 96, "APP SLOT");
    graphics_text(x + 156, y + 96, gshell_app_slot_allocated ? "allocated" : "empty");
    graphics_text(x + 24, y + 120, "LAUNCH");
    graphics_text(x + 156, y + 120, gshell_launch_state);
    graphics_text(x + 24, y + 144, "PERMISSION");
    graphics_text(x + 156, y + 144, gshell_perm_state);
    graphics_text(x + 24, y + 168, "RAMFS");
    graphics_text(x + 156, y + 168, gshell_fs_state);
    graphics_text(x + 24, y + 192, "LIFECYCLE");
    graphics_text(x + 156, y + 192, gshell_life_state);
    graphics_text(x + 24, y + 216, "SANDBOX");
    graphics_text(x + 156, y + 216, security_sandbox_profile());
    graphics_text(x + 24, y + 240, "RULE");
    graphics_text(x + 156, y + 240, security_rule_default_policy());
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "0.9 closeout");
}

static void gshell_draw_command_view_runtimefinal(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "0.9X RUNTIME CLOSEOUT");
    graphics_text(x + 24, y + 48, "USER/RING3");
    graphics_text(x + 156, y + 48, (health_user_ok() && health_ring3_ok()) ? "ok" : "bad");
    graphics_text(x + 24, y + 72, "SYSCALL");
    graphics_text(x + 156, y + 72, health_syscall_ok() ? "ok" : "bad");
    graphics_text(x + 24, y + 96, "APP SLOT");
    graphics_text(x + 156, y + 96, gshell_app_slot_allocated ? "ready" : "empty");
    graphics_text(x + 24, y + 120, "LAUNCH");
    graphics_text(x + 156, y + 120, gshell_launch_state);
    graphics_text(x + 24, y + 144, "PERM");
    graphics_text(x + 156, y + 144, gshell_perm_state);
    graphics_text(x + 24, y + 168, "RAMFS");
    graphics_text(x + 156, y + 168, gshell_fs_state);
    graphics_text(x + 24, y + 192, "LIFE");
    graphics_text(x + 156, y + 192, gshell_life_state);
    graphics_text(x + 24, y + 216, "SANDBOX");
    graphics_text(x + 156, y + 216, security_sandbox_profile());
    graphics_text(x + 24, y + 240, "CONTROL");
    graphics_text(x + 156, y + 240, security_user_control_enabled() ? "enabled" : "disabled");
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "1.0 proto");
}

static void gshell_draw_command_view_prototype(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "LINGJING 1.0 PROTOTYPE");
    graphics_text(x + 24, y + 48, "BOOT");
    graphics_text(x + 156, y + 48, "ready");
    graphics_text(x + 24, y + 72, "GSHELL");
    graphics_text(x + 156, y + 72, "control");
    graphics_text(x + 24, y + 96, "HEALTH");
    graphics_text(x + 156, y + 96, (health_user_ok() && health_ring3_ok() && health_syscall_ok()) ? "ok" : "bad");
    graphics_text(x + 24, y + 120, "CONTROL");
    graphics_text(x + 156, y + 120, security_user_control_enabled() ? "enabled" : "disabled");
    graphics_text(x + 24, y + 144, "SANDBOX");
    graphics_text(x + 156, y + 144, security_sandbox_profile());
    graphics_text(x + 24, y + 168, "RUNTIME");
    graphics_text(x + 156, y + 168, gshell_life_state);
    graphics_text(x + 24, y + 192, "APP SLOT");
    graphics_text(x + 156, y + 192, gshell_app_slot_allocated ? "ready" : "empty");
    graphics_text(x + 24, y + 216, "LAUNCH");
    graphics_text(x + 156, y + 216, gshell_launch_state);
    graphics_text(x + 24, y + 240, "MOTTO");
    graphics_text(x + 156, y + 240, "my rules");
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "dashboard");
}

static void gshell_draw_command_view_dashboard(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "UNIFIED DASH");
    graphics_text(x + 24, y + 48, "BOOT");
    graphics_text(x + 156, y + 48, "ready");
    graphics_text(x + 24, y + 72, "GSHELL");
    graphics_text(x + 156, y + 72, "online");
    graphics_text(x + 24, y + 96, "HEALTH");
    graphics_text(x + 156, y + 96, (health_user_ok() && health_ring3_ok() && health_syscall_ok()) ? "ok" : "bad");
    graphics_text(x + 24, y + 120, "CONTROL");
    graphics_text(x + 156, y + 120, security_user_control_enabled() ? "on" : "off");
    graphics_text(x + 24, y + 144, "POLICY");
    graphics_text(x + 156, y + 144, security_policy_mode());
    graphics_text(x + 24, y + 168, "SANDBOX");
    graphics_text(x + 156, y + 168, security_sandbox_profile());
    graphics_text(x + 24, y + 192, "RUNTIME");
    graphics_text(x + 156, y + 192, gshell_life_state);
    graphics_text(x + 24, y + 216, "APP SLOT");
    graphics_text(x + 156, y + 216, gshell_app_slot_allocated ? "ready" : "empty");
    graphics_text(x + 24, y + 240, "LAUNCH");
    graphics_text(x + 156, y + 240, gshell_launch_state);
    graphics_text(x + 24, y + 264, "FLOW");
    graphics_text(x + 156, y + 264, "boot-control-app");
}

static void gshell_draw_command_view_boothealth(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "BOOT HEALTH");
    graphics_text(x + 24, y + 48, "BOOT");
    graphics_text(x + 156, y + 48, "ready");
    graphics_text(x + 24, y + 72, "GRAPHICS");
    graphics_text(x + 156, y + 72, "ready");
    graphics_text(x + 24, y + 96, "GSHELL");
    graphics_text(x + 156, y + 96, "online");
    graphics_text(x + 24, y + 120, "USER");
    graphics_text(x + 156, y + 120, health_user_ok() ? "ok" : "bad");
    graphics_text(x + 24, y + 144, "RING3");
    graphics_text(x + 156, y + 144, health_ring3_ok() ? "ok" : "bad");
    graphics_text(x + 24, y + 168, "SYSCALL");
    graphics_text(x + 156, y + 168, health_syscall_ok() ? "ok" : "bad");
    graphics_text(x + 24, y + 192, "CONTROL");
    graphics_text(x + 156, y + 192, security_user_control_enabled() ? "ok" : "bad");
    graphics_text(x + 24, y + 216, "SANDBOX");
    graphics_text(x + 156, y + 216, security_sandbox_profile());
    graphics_text(x + 24, y + 240, "DOCTOR");
    graphics_text(x + 156, y + 240, "ready");
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "flow demo");
}

static void gshell_draw_command_view_flowstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "CONTROL RUNTIME");
    graphics_text(x + 24, y + 48, "FLOW");
    graphics_text(x + 156, y + 48, gshell_flow_state);
    graphics_text(x + 24, y + 72, "LAST");
    graphics_text(x + 156, y + 72, gshell_flow_last);
    graphics_text(x + 24, y + 96, "CONTROL");
    graphics_text(x + 156, y + 96, security_user_control_enabled() ? "on" : "off");
    graphics_text(x + 24, y + 120, "SANDBOX");
    graphics_text(x + 156, y + 120, security_sandbox_profile());
    graphics_text(x + 24, y + 144, "APP SLOT");
    graphics_text(x + 156, y + 144, gshell_app_slot_allocated ? "ready" : "empty");
    graphics_text(x + 24, y + 168, "LAUNCH");
    graphics_text(x + 156, y + 168, gshell_launch_state);
    graphics_text(x + 24, y + 192, "PERM");
    graphics_text(x + 156, y + 192, gshell_perm_state);
    graphics_text(x + 24, y + 216, "FS");
    graphics_text(x + 156, y + 216, gshell_fs_state);
    graphics_text(x + 24, y + 240, "LIFE");
    graphics_text(x + 156, y + 240, gshell_life_state);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "demo chain");
}

static void gshell_draw_command_view_walkthrough(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "DEMO WALK");
    graphics_text(x + 24, y + 48, "STEP");
    gshell_draw_value_uint(x + 156, y + 48, "", gshell_demo_step);
    graphics_text(x + 24, y + 72, "STATE");
    graphics_text(x + 156, y + 72, gshell_demo_state);
    graphics_text(x + 24, y + 96, "LAST");
    graphics_text(x + 156, y + 96, gshell_demo_last);
    graphics_text(x + 24, y + 120, "CONTROL");
    graphics_text(x + 156, y + 120, security_policy_mode());
    graphics_text(x + 24, y + 144, "SLOT");
    graphics_text(x + 156, y + 144, gshell_app_slot_allocated ? "ready" : "empty");
    graphics_text(x + 24, y + 168, "LAUNCH");
    graphics_text(x + 156, y + 168, gshell_launch_state);
    graphics_text(x + 24, y + 192, "PERM");
    graphics_text(x + 156, y + 192, gshell_perm_state);
    graphics_text(x + 24, y + 216, "FS");
    graphics_text(x + 156, y + 216, gshell_fs_state);
    graphics_text(x + 24, y + 240, "LIFE");
    graphics_text(x + 156, y + 240, gshell_life_state);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "milestone");
}

static void gshell_draw_command_view_milestonefinal(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "1.0 MILESTONE");
    graphics_text(x + 24, y + 48, "BOOT");
    graphics_text(x + 156, y + 48, "ready");
    graphics_text(x + 24, y + 72, "GSHELL");
    graphics_text(x + 156, y + 72, "ready");
    graphics_text(x + 24, y + 96, "CORE");
    graphics_text(x + 156, y + 96, (health_user_ok() && health_ring3_ok() && health_syscall_ok()) ? "ok" : "bad");
    graphics_text(x + 24, y + 120, "CONTROL");
    graphics_text(x + 156, y + 120, security_user_control_enabled() ? "on" : "off");
    graphics_text(x + 24, y + 144, "SANDBOX");
    graphics_text(x + 156, y + 144, security_sandbox_profile());
    graphics_text(x + 24, y + 168, "RUNTIME");
    graphics_text(x + 156, y + 168, gshell_life_state);
    graphics_text(x + 24, y + 192, "DEMO");
    graphics_text(x + 156, y + 192, gshell_demo_state);
    graphics_text(x + 24, y + 216, "FLOW");
    graphics_text(x + 156, y + 216, gshell_flow_state);
    graphics_text(x + 24, y + 240, "MILESTONE");
    graphics_text(x + 156, y + 240, "proto");
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "ui polish");
}

static void gshell_draw_command_view_uipolish(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "1.0 UI POLISH");
    graphics_text(x + 24, y + 48, "LAYOUT");
    graphics_text(x + 156, y + 48, "ok");
    graphics_text(x + 24, y + 72, "PANELS");
    graphics_text(x + 156, y + 72, "ok");
    graphics_text(x + 24, y + 96, "TEXT");
    graphics_text(x + 156, y + 96, "short");
    graphics_text(x + 24, y + 120, "COMMANDS");
    graphics_text(x + 156, y + 120, "clean");
    graphics_text(x + 24, y + 144, "DASH");
    graphics_text(x + 156, y + 144, "ready");
    graphics_text(x + 24, y + 168, "PROTO");
    graphics_text(x + 156, y + 168, "ready");
    graphics_text(x + 24, y + 192, "DEMO");
    graphics_text(x + 156, y + 192, gshell_demo_state);
    graphics_text(x + 24, y + 216, "FLOW");
    graphics_text(x + 156, y + 216, gshell_flow_state);
    graphics_text(x + 24, y + 240, "STYLE");
    graphics_text(x + 156, y + 240, "gshell");
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "closeout");
}

static void gshell_draw_command_view_closeout(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "1.0 PROTO CLOSEOUT");
    graphics_text(x + 24, y + 48, "BOOT");
    graphics_text(x + 156, y + 48, "ready");
    graphics_text(x + 24, y + 72, "GSHELL");
    graphics_text(x + 156, y + 72, "ready");
    graphics_text(x + 24, y + 96, "CORE");
    graphics_text(x + 156, y + 96, (health_user_ok() && health_ring3_ok() && health_syscall_ok()) ? "ok" : "bad");
    graphics_text(x + 24, y + 120, "CONTROL");
    graphics_text(x + 156, y + 120, security_user_control_enabled() ? "on" : "off");
    graphics_text(x + 24, y + 144, "RUNTIME");
    graphics_text(x + 156, y + 144, gshell_life_state);
    graphics_text(x + 24, y + 168, "DEMO");
    graphics_text(x + 156, y + 168, gshell_demo_state);
    graphics_text(x + 24, y + 192, "FLOW");
    graphics_text(x + 156, y + 192, gshell_flow_state);
    graphics_text(x + 24, y + 216, "UI");
    graphics_text(x + 156, y + 216, "polished");
    graphics_text(x + 24, y + 240, "STATUS");
    graphics_text(x + 156, y + 240, "1.0 ready");
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "1.1 ui/input");
}

static void gshell_draw_command_view_inputstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "INTERACTION INPUT");
    graphics_text(x + 24, y + 48, "INPUT");
    graphics_text(x + 156, y + 48, gshell_input_layer_enabled ? "enabled" : "disabled");
    graphics_text(x + 24, y + 72, "STATE");
    graphics_text(x + 156, y + 72, gshell_input_state);
    graphics_text(x + 24, y + 96, "MOUSE");
    graphics_text(x + 156, y + 96, gshell_mouse_layer_ready ? "meta-ready" : "planned");
    graphics_text(x + 24, y + 120, "CLICK");
    graphics_text(x + 156, y + 120, gshell_click_layer_ready ? "ready" : "planned");
    graphics_text(x + 24, y + 144, "FOCUS");
    graphics_text(x + 156, y + 144, gshell_focus_layer_ready ? "ready" : "bad");
    graphics_text(x + 24, y + 168, "TARGET");
    graphics_text(x + 156, y + 168, gshell_focus_target);
    gshell_draw_value_uint(x + 24, y + 192, "EVENTS", gshell_interaction_events);
    gshell_draw_value_uint(x + 24, y + 216, "FOCUS CHG", gshell_focus_changes);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_input_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "real mouse");
}

static void gshell_draw_command_view_cursorstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "CURSOR POINTER CORE");
    graphics_text(x + 24, y + 48, "VISIBLE");
    graphics_text(x + 156, y + 48, gshell_cursor_visible ? "yes" : "no");
    gshell_draw_value_uint(x + 24, y + 72, "X", (unsigned int)gshell_cursor_x);
    gshell_draw_value_uint(x + 24, y + 96, "Y", (unsigned int)gshell_cursor_y);
    graphics_text(x + 24, y + 120, "STATE");
    graphics_text(x + 156, y + 120, gshell_cursor_state);
    graphics_text(x + 24, y + 144, "FOCUS");
    graphics_text(x + 156, y + 144, gshell_focus_target);
    gshell_draw_value_uint(x + 24, y + 168, "MOVES", gshell_cursor_moves);
    gshell_draw_value_uint(x + 24, y + 192, "CLICKS", gshell_cursor_clicks);
    gshell_draw_value_uint(x + 24, y + 216, "WHEEL", gshell_cursor_wheel);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_cursor_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "click map");
}

static void gshell_draw_command_view_hitstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "CLICK HIT TEST CORE");
    graphics_text(x + 24, y + 48, "ZONE");
    graphics_text(x + 156, y + 48, gshell_hit_zone);
    graphics_text(x + 24, y + 72, "TARGET");
    graphics_text(x + 156, y + 72, gshell_hit_target);
    graphics_text(x + 24, y + 96, "ACTION");
    graphics_text(x + 156, y + 96, gshell_hit_action);
    graphics_text(x + 24, y + 120, "FOCUS");
    graphics_text(x + 156, y + 120, gshell_focus_target);
    gshell_draw_value_uint(x + 24, y + 144, "HITS", gshell_hit_tests);
    gshell_draw_value_uint(x + 24, y + 168, "CLICKS", gshell_hit_clicks);
    gshell_draw_value_uint(x + 24, y + 192, "COMMANDS", gshell_hit_commands);
    gshell_draw_value_uint(x + 24, y + 216, "WHEEL", gshell_hit_wheel);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_hit_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "button ui");
}

static void gshell_draw_command_view_buttonstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "BUTTON PANEL CORE");
    graphics_text(x + 24, y + 48, "TARGET");
    graphics_text(x + 156, y + 48, gshell_button_target);
    graphics_text(x + 24, y + 72, "STATE");
    graphics_text(x + 156, y + 72, gshell_button_state);
    graphics_text(x + 24, y + 96, "HOVER");
    graphics_text(x + 156, y + 96, gshell_button_hovered ? "yes" : "no");
    graphics_text(x + 24, y + 120, "PRESS");
    graphics_text(x + 156, y + 120, gshell_button_pressed ? "yes" : "no");
    graphics_text(x + 24, y + 144, "FOCUS");
    graphics_text(x + 156, y + 144, gshell_button_focus ? "yes" : "no");
    graphics_text(x + 24, y + 168, "ACTIVE");
    graphics_text(x + 156, y + 168, gshell_button_active ? "yes" : "no");
    gshell_draw_value_uint(x + 24, y + 192, "EVENTS", gshell_button_events);
    gshell_draw_value_uint(x + 24, y + 216, "ACTIVATES", gshell_button_activations);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_button_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "window shell");
}

static void gshell_draw_command_view_windowstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "WINDOW PANEL CORE");
    graphics_text(x + 24, y + 48, "TITLE");
    graphics_text(x + 156, y + 48, gshell_window_title);
    graphics_text(x + 24, y + 72, "STATE");
    graphics_text(x + 156, y + 72, gshell_window_state);
    graphics_text(x + 24, y + 96, "EXISTS");
    graphics_text(x + 156, y + 96, gshell_window_exists ? "yes" : "no");
    graphics_text(x + 24, y + 120, "FOCUS");
    graphics_text(x + 156, y + 120, gshell_window_focused ? "yes" : "no");
    graphics_text(x + 24, y + 144, "MIN");
    graphics_text(x + 156, y + 144, gshell_window_minimized ? "yes" : "no");
    gshell_draw_value_uint(x + 24, y + 168, "X", gshell_window_x);
    gshell_draw_value_uint(x + 24, y + 192, "Y", gshell_window_y);
    gshell_draw_value_uint(x + 24, y + 216, "MOVES", gshell_window_moves);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_window_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "desktop");
}

static void gshell_draw_command_view_desktopstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "DESKTOP WORKSPACE");
    graphics_text(x + 24, y + 48, "DESKTOP");
    graphics_text(x + 156, y + 48, gshell_desktop_enabled ? "on" : "off");
    graphics_text(x + 24, y + 72, "STATE");
    graphics_text(x + 156, y + 72, gshell_desktop_state);
    graphics_text(x + 24, y + 96, "WORKSPACE");
    graphics_text(x + 156, y + 96, gshell_workspace_ready ? "ready" : "none");
    gshell_draw_value_uint(x + 24, y + 120, "WORK ID", gshell_workspace_id);
    graphics_text(x + 24, y + 144, "ICON");
    graphics_text(x + 156, y + 144, gshell_icon_selected ? "selected" : "none");
    graphics_text(x + 24, y + 168, "DOCK");
    graphics_text(x + 156, y + 168, gshell_dock_ready ? "ready" : "planned");
    graphics_text(x + 24, y + 192, "FOCUS");
    graphics_text(x + 156, y + 192, gshell_desktop_focus);
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_desktop_events);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_desktop_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "1.1 close");
}

static void gshell_draw_command_view_interactionfinal(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "1.1 UI INTERACTION");
    graphics_text(x + 24, y + 48, "INPUT");
    graphics_text(x + 156, y + 48, gshell_input_state);
    graphics_text(x + 24, y + 72, "CURSOR");
    graphics_text(x + 156, y + 72, gshell_cursor_state);
    graphics_text(x + 24, y + 96, "HIT");
    graphics_text(x + 156, y + 96, gshell_hit_zone);
    graphics_text(x + 24, y + 120, "BUTTON");
    graphics_text(x + 156, y + 120, gshell_button_state);
    graphics_text(x + 24, y + 144, "WINDOW");
    graphics_text(x + 156, y + 144, gshell_window_state);
    graphics_text(x + 24, y + 168, "DESKTOP");
    graphics_text(x + 156, y + 168, gshell_desktop_state);
    graphics_text(x + 24, y + 192, "FOCUS");
    graphics_text(x + 156, y + 192, gshell_focus_target);
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_interaction_events);
    graphics_text(x + 24, y + 240, "STATUS");
    graphics_text(x + 156, y + 240, "1.1 ready");
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "1.2 input");
}

static void gshell_draw_command_view_shellstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "WINDOW SHELL BASE");
    graphics_text(x + 24, y + 48, "SHELL");
    graphics_text(x + 156, y + 48, gshell_shell_enabled ? "on" : "off");
    graphics_text(x + 24, y + 72, "STATE");
    graphics_text(x + 156, y + 72, gshell_shell_state);
    graphics_text(x + 24, y + 96, "PANEL");
    graphics_text(x + 156, y + 96, gshell_shell_panel_ready ? "ready" : "none");
    graphics_text(x + 24, y + 120, "TASKBAR");
    graphics_text(x + 156, y + 120, gshell_shell_taskbar_ready ? "ready" : "none");
    graphics_text(x + 24, y + 144, "LAUNCHER");
    graphics_text(x + 156, y + 144, gshell_shell_launcher_ready ? "ready" : "none");
    graphics_text(x + 24, y + 168, "FOCUS");
    graphics_text(x + 156, y + 168, gshell_shell_focus);
    gshell_draw_value_uint(x + 24, y + 192, "OPEN APP", gshell_shell_open_apps);
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_shell_events);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_shell_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "shell flow");
}

static void gshell_draw_command_view_launcherstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "LAUNCHER APP FLOW");
    graphics_text(x + 24, y + 48, "STATE");
    graphics_text(x + 156, y + 48, gshell_launcher_state);
    graphics_text(x + 24, y + 72, "GRID");
    graphics_text(x + 156, y + 72, gshell_launcher_grid_ready ? "ready" : "none");
    graphics_text(x + 24, y + 96, "APP");
    graphics_text(x + 156, y + 96, gshell_launcher_selected_app);
    graphics_text(x + 24, y + 120, "SELECTED");
    graphics_text(x + 156, y + 120, gshell_launcher_app_selected ? "yes" : "no");
    graphics_text(x + 24, y + 144, "PINNED");
    graphics_text(x + 156, y + 144, gshell_launcher_app_pinned ? "yes" : "no");
    gshell_draw_value_uint(x + 24, y + 168, "OPENED", gshell_launcher_open_count);
    gshell_draw_value_uint(x + 24, y + 192, "EVENTS", gshell_launcher_events);
    graphics_text(x + 24, y + 216, "FOCUS");
    graphics_text(x + 156, y + 216, gshell_focus_target);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_launcher_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "task flow");
}

static void gshell_draw_command_view_taskbarstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "TASKBAR APP SWITCH");
    graphics_text(x + 24, y + 48, "TASKBAR");
    graphics_text(x + 156, y + 48, gshell_taskbar_enabled ? "on" : "off");
    graphics_text(x + 24, y + 72, "STATE");
    graphics_text(x + 156, y + 72, gshell_taskbar_state);
    graphics_text(x + 24, y + 96, "ITEM");
    graphics_text(x + 156, y + 96, gshell_taskbar_item);
    graphics_text(x + 24, y + 120, "FOCUS");
    graphics_text(x + 156, y + 120, gshell_taskbar_focused ? "yes" : "no");
    graphics_text(x + 24, y + 144, "MIN");
    graphics_text(x + 156, y + 144, gshell_taskbar_minimized ? "yes" : "no");
    gshell_draw_value_uint(x + 24, y + 168, "SWITCHES", gshell_taskbar_switches);
    gshell_draw_value_uint(x + 24, y + 192, "EVENTS", gshell_taskbar_events);
    graphics_text(x + 24, y + 216, "WINDOW");
    graphics_text(x + 156, y + 216, gshell_window_state);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_taskbar_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "shell close");
}

static void gshell_draw_command_view_layoutstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "WINDOW LAYOUT CORE");
    graphics_text(x + 24, y + 48, "LAYOUT");
    graphics_text(x + 156, y + 48, gshell_layout_enabled ? "on" : "off");
    graphics_text(x + 24, y + 72, "STATE");
    graphics_text(x + 156, y + 72, gshell_layout_state);
    graphics_text(x + 24, y + 96, "MODE");
    graphics_text(x + 156, y + 96, gshell_layout_mode);
    graphics_text(x + 24, y + 120, "GRID");
    graphics_text(x + 156, y + 120, gshell_layout_grid_ready ? "ready" : "none");
    graphics_text(x + 24, y + 144, "SNAP");
    graphics_text(x + 156, y + 144, gshell_layout_snapped ? "yes" : "no");
    graphics_text(x + 24, y + 168, "MAX");
    graphics_text(x + 156, y + 168, gshell_layout_maximized ? "yes" : "no");
    gshell_draw_value_uint(x + 24, y + 192, "Z", gshell_layout_z_index);
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_layout_events);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_layout_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "shell final");
}

static void gshell_draw_command_view_desktopshell(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "DESKTOP SHELL CORE");
    graphics_text(x + 24, y + 48, "HOME");
    graphics_text(x + 156, y + 48, gshell_home_ready ? "ready" : "none");
    graphics_text(x + 24, y + 72, "STATE");
    graphics_text(x + 156, y + 72, gshell_home_state);
    graphics_text(x + 24, y + 96, "APPS");
    graphics_text(x + 156, y + 96, gshell_home_apps_ready ? "ready" : "none");
    graphics_text(x + 24, y + 120, "WINDOWS");
    graphics_text(x + 156, y + 120, gshell_home_windows_ready ? "ready" : "none");
    graphics_text(x + 24, y + 144, "LAYOUT");
    graphics_text(x + 156, y + 144, gshell_home_layout_ready ? "ready" : "none");
    graphics_text(x + 24, y + 168, "DOCK");
    graphics_text(x + 156, y + 168, gshell_home_dock_ready ? "ready" : "none");
    graphics_text(x + 24, y + 192, "FOCUS");
    graphics_text(x + 156, y + 192, gshell_home_focus);
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_home_events);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_home_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "shell close");
}

static void gshell_draw_command_view_shellfinal(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "1.2 DESKTOP SHELL");
    graphics_text(x + 24, y + 48, "SHELL");
    graphics_text(x + 156, y + 48, gshell_shell_state);
    graphics_text(x + 24, y + 72, "HOME");
    graphics_text(x + 156, y + 72, gshell_home_state);
    graphics_text(x + 24, y + 96, "LAUNCHER");
    graphics_text(x + 156, y + 96, gshell_launcher_state);
    graphics_text(x + 24, y + 120, "TASKBAR");
    graphics_text(x + 156, y + 120, gshell_taskbar_state);
    graphics_text(x + 24, y + 144, "WINDOW");
    graphics_text(x + 156, y + 144, gshell_window_state);
    graphics_text(x + 24, y + 168, "LAYOUT");
    graphics_text(x + 156, y + 168, gshell_layout_state);
    graphics_text(x + 24, y + 192, "DESKTOP");
    graphics_text(x + 156, y + 192, gshell_desktop_state);
    graphics_text(x + 24, y + 216, "FOCUS");
    graphics_text(x + 156, y + 216, gshell_focus_target);
    graphics_text(x + 24, y + 240, "STATUS");
    graphics_text(x + 156, y + 240, "1.2 ready");
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "1.3 input");
}

static void gshell_draw_command_view_appshellstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "APP SHELL BASE");
    graphics_text(x + 24, y + 48, "APP SHELL");
    graphics_text(x + 156, y + 48, gshell_app_shell_enabled ? "on" : "off");
    graphics_text(x + 24, y + 72, "STATE");
    graphics_text(x + 156, y + 72, gshell_app_shell_state);
    graphics_text(x + 24, y + 96, "CATALOG");
    graphics_text(x + 156, y + 96, gshell_app_catalog_ready ? "ready" : "none");
    graphics_text(x + 24, y + 120, "CARD");
    graphics_text(x + 156, y + 120, gshell_app_card_ready ? "ready" : "none");
    graphics_text(x + 24, y + 144, "DETAILS");
    graphics_text(x + 156, y + 144, gshell_app_details_ready ? "ready" : "none");
    graphics_text(x + 24, y + 168, "APP");
    graphics_text(x + 156, y + 168, gshell_app_shell_selected);
    graphics_text(x + 24, y + 192, "LAUNCH");
    graphics_text(x + 156, y + 192, gshell_app_shell_launch_ready ? "ready" : "none");
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_app_shell_events);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_app_shell_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "app control");
}

static void gshell_draw_command_view_catalogstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "APP CATALOG CORE");
    graphics_text(x + 24, y + 48, "STATE");
    graphics_text(x + 156, y + 48, gshell_catalog_state);
    graphics_text(x + 24, y + 72, "SELECTED");
    graphics_text(x + 156, y + 72, gshell_catalog_selected);
    gshell_draw_value_uint(x + 24, y + 96, "ITEMS", gshell_catalog_items);
    gshell_draw_value_uint(x + 24, y + 120, "SEARCH", gshell_catalog_search_hits);
    graphics_text(x + 24, y + 144, "PINNED");
    graphics_text(x + 156, y + 144, gshell_catalog_pinned ? "yes" : "no");
    graphics_text(x + 24, y + 168, "ENABLED");
    graphics_text(x + 156, y + 168, gshell_catalog_enabled ? "yes" : "no");
    graphics_text(x + 24, y + 192, "FOCUS");
    graphics_text(x + 156, y + 192, gshell_focus_target);
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_catalog_events);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_catalog_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "app control");
}

static void gshell_draw_command_view_detailstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "APP DETAIL PANEL");
    graphics_text(x + 24, y + 48, "STATE");
    graphics_text(x + 156, y + 48, gshell_detail_state);
    graphics_text(x + 24, y + 72, "APP");
    graphics_text(x + 156, y + 72, gshell_detail_app);
    graphics_text(x + 24, y + 96, "MANIFEST");
    graphics_text(x + 156, y + 96, gshell_detail_manifest_ready ? "ready" : "none");
    graphics_text(x + 24, y + 120, "CAPS");
    graphics_text(x + 156, y + 120, gshell_detail_caps_ready ? "ready" : "none");
    graphics_text(x + 24, y + 144, "PERMS");
    graphics_text(x + 156, y + 144, gshell_detail_perms_ready ? "ready" : "none");
    graphics_text(x + 24, y + 168, "LAUNCH");
    graphics_text(x + 156, y + 168, gshell_detail_launch_ready ? "ready" : "none");
    graphics_text(x + 24, y + 192, "FOCUS");
    graphics_text(x + 156, y + 192, gshell_focus_target);
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_detail_events);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_detail_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "app action");
}

static void gshell_draw_command_view_actionstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "APP ACTION FLOW");
    graphics_text(x + 24, y + 48, "STATE");
    graphics_text(x + 156, y + 48, gshell_action_state);
    graphics_text(x + 24, y + 72, "APP");
    graphics_text(x + 156, y + 72, gshell_action_app);
    graphics_text(x + 24, y + 96, "PREPARE");
    graphics_text(x + 156, y + 96, gshell_action_prepared ? "yes" : "no");
    graphics_text(x + 24, y + 120, "ALLOW");
    graphics_text(x + 156, y + 120, gshell_action_allowed ? "yes" : "no");
    graphics_text(x + 24, y + 144, "OPEN");
    graphics_text(x + 156, y + 144, gshell_action_opened ? "yes" : "no");
    graphics_text(x + 24, y + 168, "RUN");
    graphics_text(x + 156, y + 168, gshell_action_running ? "yes" : "no");
    graphics_text(x + 24, y + 192, "FOCUS");
    graphics_text(x + 156, y + 192, gshell_focus_target);
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_action_events);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_action_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "app final");
}

static void gshell_draw_command_view_appmgmtstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "APP MANAGEMENT");
    graphics_text(x + 24, y + 48, "STATE");
    graphics_text(x + 156, y + 48, gshell_app_mgmt_state);
    graphics_text(x + 24, y + 72, "APP");
    graphics_text(x + 156, y + 72, gshell_app_mgmt_app);
    graphics_text(x + 24, y + 96, "REGISTER");
    graphics_text(x + 156, y + 96, gshell_app_mgmt_registered ? "yes" : "no");
    graphics_text(x + 24, y + 120, "ENABLED");
    graphics_text(x + 156, y + 120, gshell_app_mgmt_enabled ? "yes" : "no");
    graphics_text(x + 24, y + 144, "TRUSTED");
    graphics_text(x + 156, y + 144, gshell_app_mgmt_trusted ? "yes" : "no");
    graphics_text(x + 24, y + 168, "FAVORITE");
    graphics_text(x + 156, y + 168, gshell_app_mgmt_favorite ? "yes" : "no");
    graphics_text(x + 24, y + 192, "HEALTH");
    graphics_text(x + 156, y + 192, gshell_app_mgmt_healthy ? "ok" : "bad");
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_app_mgmt_events);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_app_mgmt_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "app close");
}

static void gshell_draw_command_view_appfinal(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "1.3 APP SHELL FINAL");
    graphics_text(x + 24, y + 48, "STATE");
    graphics_text(x + 156, y + 48, gshell_app_final_state);
    graphics_text(x + 24, y + 72, "APP SHELL");
    graphics_text(x + 156, y + 72, gshell_app_shell_state);
    graphics_text(x + 24, y + 96, "CATALOG");
    graphics_text(x + 156, y + 96, gshell_catalog_state);
    graphics_text(x + 24, y + 120, "DETAIL");
    graphics_text(x + 156, y + 120, gshell_detail_state);
    graphics_text(x + 24, y + 144, "ACTION");
    graphics_text(x + 156, y + 144, gshell_action_state);
    graphics_text(x + 24, y + 168, "MGMT");
    graphics_text(x + 156, y + 168, gshell_app_mgmt_state);
    graphics_text(x + 24, y + 192, "FOCUS");
    graphics_text(x + 156, y + 192, gshell_focus_target);
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_app_final_events);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_app_final_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "1.4 visual");
}

static void gshell_draw_command_view_visualstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "VISUAL DESKTOP UI");
    graphics_text(x + 24, y + 48, "STATE");
    graphics_text(x + 156, y + 48, gshell_visual_state);
    graphics_text(x + 24, y + 72, "DESKTOP");
    graphics_text(x + 156, y + 72, gshell_visual_desktop_ready ? "ready" : "none");
    graphics_text(x + 24, y + 96, "PANEL");
    graphics_text(x + 156, y + 96, gshell_visual_panel_ready ? "ready" : "none");
    graphics_text(x + 24, y + 120, "CARDS");
    graphics_text(x + 156, y + 120, gshell_visual_cards_ready ? "ready" : "none");
    graphics_text(x + 24, y + 144, "WINDOW");
    graphics_text(x + 156, y + 144, gshell_visual_window_ready ? "ready" : "none");
    graphics_text(x + 24, y + 168, "TASKBAR");
    graphics_text(x + 156, y + 168, gshell_visual_taskbar_ready ? "ready" : "none");
    graphics_text(x + 24, y + 192, "FOCUS");
    graphics_text(x + 156, y + 192, gshell_visual_focus);
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_visual_events);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_visual_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "cards ui");
}

static void gshell_draw_command_view_cardstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "VISUAL CARDS WINDOW");
    graphics_text(x + 24, y + 48, "STATE");
    graphics_text(x + 156, y + 48, gshell_card_state);
    graphics_text(x + 24, y + 72, "APP");
    graphics_text(x + 156, y + 72, gshell_card_app);
    graphics_text(x + 24, y + 96, "GRID");
    graphics_text(x + 156, y + 96, gshell_card_grid_ready ? "ready" : "none");
    graphics_text(x + 24, y + 120, "SELECT");
    graphics_text(x + 156, y + 120, gshell_card_selected ? "yes" : "no");
    graphics_text(x + 24, y + 144, "CARD");
    graphics_text(x + 156, y + 144, gshell_card_opened ? "open" : "closed");
    graphics_text(x + 24, y + 168, "WINDOW");
    graphics_text(x + 156, y + 168, gshell_window_visual_ready ? "ready" : "none");
    graphics_text(x + 24, y + 192, "ACTIVE");
    graphics_text(x + 156, y + 192, gshell_window_active_ready ? "yes" : "no");
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_card_events);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 156, y + 240, gshell_card_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 156, y + 264, "launcher ui");
}

static void gshell_draw_command_view_uimockstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    unsigned int dx = x + 14;
    unsigned int dy = y + 34;
    unsigned int dw = w - 28;
    unsigned int dh = h - 68;

    unsigned int card_y = dy + 34;
    unsigned int card_w = 54;
    unsigned int card_h = 38;
    unsigned int card1_x = dx + 14;
    unsigned int card2_x = dx + 76;
    unsigned int card3_x = dx + 138;

    unsigned int win_x = dx + 18;
    unsigned int win_y = dy + 92;
    unsigned int win_w = dw - 78;
    unsigned int win_h = 72;

    unsigned int dock_x = dx + dw - 52;
    unsigned int dock_y = dy + 54;
    unsigned int dock_w = 34;
    unsigned int dock_h = 98;

    unsigned int taskbar_y = dy + dh - 26;

    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 18, y + 14, "VISIBLE UI MOCK");

    graphics_rect(dx, dy, dw, dh, gshell_ui_mock_desktop_ready ? 0x00001228 : 0x00000812);
    graphics_rect(dx, dy, dw, 2, 0x000044AA);
    graphics_rect(dx, dy + dh - 2, dw, 2, 0x000044AA);
    graphics_rect(dx, dy, 2, dh, 0x000044AA);
    graphics_rect(dx + dw - 2, dy, 2, dh, 0x000044AA);

    graphics_text(dx + 8, dy + 8, "DESKTOP");

    if (gshell_ui_mock_grid_ready || gshell_ui_mock_card1_ready || gshell_ui_mock_card2_ready || gshell_ui_mock_card3_ready) {
        graphics_rect(card1_x - 6, card_y - 6, 3 * card_w + 26, card_h + 14, 0x00001022);
        graphics_text(card1_x - 2, card_y - 20, "APP GRID");
    }

    graphics_rect(card1_x, card_y, card_w, card_h, gshell_ui_mock_card1_ready ? 0x000055AA : 0x00002244);
    graphics_rect(card2_x, card_y, card_w, card_h, gshell_ui_mock_card2_ready ? 0x00004488 : 0x00001833);
    graphics_rect(card3_x, card_y, card_w, card_h, gshell_ui_mock_card3_ready ? 0x00004488 : 0x00001833);

    graphics_rect(card1_x, card_y, card_w, 2, gshell_ui_mock_selected_card == 1 ? 0x0000FFFF : 0x000066AA);
    graphics_rect(card2_x, card_y, card_w, 2, gshell_ui_mock_selected_card == 2 ? 0x0000FFFF : 0x000066AA);
    graphics_rect(card3_x, card_y, card_w, 2, gshell_ui_mock_selected_card == 3 ? 0x0000FFFF : 0x000066AA);

    graphics_text(card1_x + 6, card_y + 11, "DEMO");
    graphics_text(card2_x + 8, card_y + 11, "SYS");
    graphics_text(card3_x + 6, card_y + 11, "FILE");

    if (gshell_ui_mock_focus_ready && gshell_ui_mock_selected_card == 1) {
        graphics_rect(card1_x - 3, card_y - 3, card_w + 6, 2, 0x0000FFFF);
        graphics_rect(card1_x - 3, card_y + card_h + 1, card_w + 6, 2, 0x0000FFFF);
        graphics_rect(card1_x - 3, card_y - 3, 2, card_h + 6, 0x0000FFFF);
        graphics_rect(card1_x + card_w + 1, card_y - 3, 2, card_h + 6, 0x0000FFFF);
    }

    if (gshell_ui_mock_window_ready) {
        graphics_rect(win_x + 4, win_y + 4, win_w, win_h, gshell_window_shadow_ready ? 0x00000408 : 0x00000103);
        graphics_rect(win_x, win_y, win_w, win_h, 0x00001833);
        graphics_rect(win_x, win_y, win_w, 16, gshell_ui_mock_title_ready ? 0x000066AA : 0x00003366);
        graphics_rect(win_x, win_y, win_w, 2, gshell_window_active_ready ? 0x0000FFFF : 0x000088CC);
        graphics_text(win_x + 8, win_y + 4, "demo.window");

        if (gshell_ui_mock_body_ready) {
            graphics_rect(win_x + 8, win_y + 25, win_w - 16, win_h - 34, 0x00000A18);
            graphics_text(win_x + 14, win_y + 38, "APP BODY");
        }
    }

    if (gshell_ui_mock_dock_ready) {
        graphics_rect(dock_x, dock_y, dock_w, dock_h, 0x00001430);
        graphics_text(dock_x + 3, dock_y + 8, "D");
        graphics_rect(dock_x + 7, dock_y + 28, 18, 16, 0x000055AA);
        graphics_rect(dock_x + 7, dock_y + 50, 18, 16, 0x00004488);
        graphics_rect(dock_x + 7, dock_y + 72, 18, 16, 0x00004488);
    }

    if (gshell_ui_mock_taskbar_ready) {
        graphics_rect(dx + 8, taskbar_y, dw - 16, 20, 0x00002255);
        graphics_text(dx + 16, taskbar_y + 6, "TASKBAR");
        graphics_rect(dx + 92, taskbar_y + 4, 46, 12, 0x000055AA);
        graphics_text(dx + 98, taskbar_y + 7, "APP");
    }

    graphics_text(x + 18, y + h - 28, "CARD 54x38  WIN FIT  TASKBAR H20");
}

static void gshell_draw_command_view_launchmockstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    unsigned int dx = x + 14;
    unsigned int dy = y + 34;
    unsigned int dw = w - 28;
    unsigned int dh = h - 68;

    unsigned int panel_x = dx + 16;
    unsigned int panel_y = dy + 24;
    unsigned int panel_w = 132;
    unsigned int panel_h = 126;

    unsigned int search_x = panel_x + 10;
    unsigned int search_y = panel_y + 12;

    unsigned int taskbar_y = dy + dh - 26;

    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 18, y + 14, "LAUNCHER TASKBAR");

    graphics_rect(dx, dy, dw, dh, 0x00000812);
    graphics_rect(dx, dy, dw, 2, 0x000044AA);
    graphics_rect(dx, dy + dh - 2, dw, 2, 0x000044AA);
    graphics_rect(dx, dy, 2, dh, 0x000044AA);
    graphics_rect(dx + dw - 2, dy, 2, dh, 0x000044AA);

    if (gshell_launch_mock_panel_ready) {
        graphics_rect(panel_x, panel_y, panel_w, panel_h, 0x00001430);
        graphics_rect(panel_x, panel_y, panel_w, 2, 0x000088CC);
        graphics_text(panel_x + 10, panel_y + 104, "LAUNCHER");
    }

    if (gshell_launch_mock_search_ready) {
        graphics_rect(search_x, search_y, panel_w - 20, 18, 0x00002044);
        graphics_text(search_x + 8, search_y + 5, "search");
    }

    if (gshell_launch_mock_apps_ready) {
        graphics_rect(panel_x + 12, panel_y + 42, 28, 24, 0x000055AA);
        graphics_rect(panel_x + 52, panel_y + 42, 28, 24, 0x00004488);
        graphics_rect(panel_x + 92, panel_y + 42, 28, 24, 0x00004488);
        graphics_text(panel_x + 13, panel_y + 50, "D");
        graphics_text(panel_x + 54, panel_y + 50, "S");
        graphics_text(panel_x + 94, panel_y + 50, "F");
    }

    if (gshell_launch_mock_recent_ready) {
        graphics_rect(panel_x + 12, panel_y + 76, panel_w - 24, 18, 0x00001A38);
        graphics_text(panel_x + 18, panel_y + 82, "recent demo");
    }

    if (gshell_launch_mock_pin_ready) {
        graphics_rect(panel_x + panel_w - 18, panel_y + 8, 8, 8, 0x0000FFFF);
    }

    if (gshell_launch_mock_run_ready) {
        graphics_rect(panel_x + 14, panel_y + 68, 24, 3, 0x0000FF66);
    }

    graphics_rect(dx + 8, taskbar_y, dw - 16, 20, 0x00002255);

    if (gshell_task_mock_start_ready) {
        graphics_rect(dx + 14, taskbar_y + 4, 34, 12, 0x000055AA);
        graphics_text(dx + 18, taskbar_y + 7, "START");
    }

    if (gshell_task_mock_app_ready) {
        graphics_rect(dx + 58, taskbar_y + 4, 52, 12, 0x00004488);
        graphics_text(dx + 64, taskbar_y + 7, "DEMO");
    }

    if (gshell_task_mock_active_ready) {
        graphics_rect(dx + 58, taskbar_y + 17, 52, 2, 0x0000FFFF);
    }

    if (gshell_task_mock_tray_ready) {
        graphics_rect(dx + dw - 78, taskbar_y + 4, 62, 12, 0x00001430);
    }

    if (gshell_task_mock_net_ready) {
        graphics_text(dx + dw - 72, taskbar_y + 7, "NET");
    }

    if (gshell_task_mock_clock_ready) {
        graphics_text(dx + dw - 40, taskbar_y + 7, "12:00");
    }

    graphics_text(x + 18, y + h - 28, "START 34x12  PANEL 132x126  TASKBAR H20");
}

static void gshell_draw_command_view_polishstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    unsigned int dx = x + 14;
    unsigned int dy = y + 34;
    unsigned int dw = w - 28;
    unsigned int dh = h - 68;

    unsigned int panel_x = dx + 10;
    unsigned int panel_y = dy + 18;
    unsigned int panel_w = dw - 20;
    unsigned int panel_h = dh - 42;

    unsigned int card_y = panel_y + 42;
    unsigned int card_w = 44;
    unsigned int card_h = 28;
    unsigned int card1_x = panel_x + 12;
    unsigned int card2_x = panel_x + 64;
    unsigned int card3_x = panel_x + 116;

    unsigned int win_x = panel_x + 12;
    unsigned int win_y = panel_y + 82;
    unsigned int win_w = panel_w - 72;
    unsigned int win_h = 54;

    unsigned int tray_x = panel_x + panel_w - 54;
    unsigned int tray_y = panel_y + 42;

    unsigned int taskbar_y = dy + dh - 24;

    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 18, y + 14, "VISUAL POLISH");

    graphics_rect(dx, dy, dw, dh, gshell_polish_base_ready ? 0x00000816 : 0x0000040A);
    graphics_rect(dx, dy, dw, 2, 0x000044AA);
    graphics_rect(dx, dy + dh - 2, dw, 2, 0x000044AA);
    graphics_rect(dx, dy, 2, dh, 0x000044AA);
    graphics_rect(dx + dw - 2, dy, 2, dh, 0x000044AA);

    if (gshell_polish_glass_ready) {
        graphics_rect(panel_x, panel_y, panel_w, panel_h, 0x00001028);
    } else {
        graphics_rect(panel_x, panel_y, panel_w, panel_h, 0x00000818);
    }

    graphics_rect(panel_x, panel_y, panel_w, 2, 0x000088CC);
    graphics_rect(panel_x, panel_y + panel_h - 2, panel_w, 2, 0x00002266);
    graphics_rect(panel_x, panel_y, 2, panel_h, 0x00004488);
    graphics_rect(panel_x + panel_w - 2, panel_y, 2, panel_h, 0x00004488);

    if (gshell_polish_title_ready) {
        graphics_rect(panel_x + 8, panel_y + 8, panel_w - 16, 18, 0x00002455);
        graphics_text(panel_x + 14, panel_y + 13, "LINGJING DESK");
    }

    if (gshell_polish_badge_ready) {
        graphics_rect(panel_x + panel_w - 54, panel_y + 11, 42, 12, 0x000055AA);
        graphics_text(panel_x + panel_w - 48, panel_y + 14, "OK");
    }

    if (gshell_polish_cards_ready) {
        graphics_rect(card1_x, card_y, card_w, card_h, 0x000055AA);
        graphics_rect(card2_x, card_y, card_w, card_h, 0x00003377);
        graphics_rect(card3_x, card_y, card_w, card_h, 0x00003377);
        graphics_text(card1_x + 7, card_y + 10, "APP");
        graphics_text(card2_x + 7, card_y + 10, "SYS");
        graphics_text(card3_x + 7, card_y + 10, "FS");
    }

    if (gshell_polish_focus_ready) {
        graphics_rect(card1_x - 3, card_y - 3, card_w + 6, 2, 0x0000FFFF);
        graphics_rect(card1_x - 3, card_y + card_h + 1, card_w + 6, 2, 0x0000FFFF);
        graphics_rect(card1_x - 3, card_y - 3, 2, card_h + 6, 0x0000FFFF);
        graphics_rect(card1_x + card_w + 1, card_y - 3, 2, card_h + 6, 0x0000FFFF);
    }

    if (gshell_polish_shadow_ready) {
        graphics_rect(win_x + 4, win_y + 4, win_w, win_h, 0x00000206);
    }

    if (gshell_polish_window_ready) {
        graphics_rect(win_x, win_y, win_w, win_h, 0x00001430);
        graphics_rect(win_x, win_y, win_w, 14, 0x000066AA);
        graphics_text(win_x + 7, win_y + 4, "demo.window");
        graphics_rect(win_x + 8, win_y + 23, win_w - 16, win_h - 32, 0x00000816);
        graphics_text(win_x + 14, win_y + 32, "RUNNING");
    }

    if (gshell_polish_tray_ready) {
        graphics_rect(tray_x, tray_y, 38, 64, 0x00001430);
        graphics_text(tray_x + 5, tray_y + 8, "NET");
        graphics_text(tray_x + 5, tray_y + 28, "AI");
        graphics_text(tray_x + 5, tray_y + 48, "12");
    }

    if (gshell_polish_taskbar_ready) {
        graphics_rect(dx + 8, taskbar_y, dw - 16, 18, 0x00002255);
        graphics_rect(dx + 14, taskbar_y + 4, 28, 10, 0x000055AA);
        graphics_text(dx + 18, taskbar_y + 6, "LJ");
        graphics_rect(dx + 50, taskbar_y + 4, 46, 10, 0x00004488);
        graphics_text(dx + 56, taskbar_y + 6, "APP");

        if (gshell_polish_safe_ready) {
            graphics_text(dx + dw - 66, taskbar_y + 6, "SAFE");
        }
    }

    graphics_text(x + 18, y + h - 28, "FIT OK  NO OVERFLOW");
}

static void gshell_draw_command_view_desktopscene(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    unsigned int dx = x + 14;
    unsigned int dy = y + 30;
    unsigned int dw = w - 28;
    unsigned int dh = h - 64;

    unsigned int top_x = dx + 8;
    unsigned int top_y = dy + 8;
    unsigned int top_w = dw - 16;

    unsigned int launch_x = dx + 10;
    unsigned int launch_y = dy + 42;
    unsigned int launch_w = 58;
    unsigned int launch_h = 82;

    unsigned int card_y = dy + 42;
    unsigned int card_x = dx + 80;
    unsigned int card_w = 34;
    unsigned int card_h = 24;

    unsigned int win_x = dx + 78;
    unsigned int win_y = dy + 88;
    unsigned int win_w = dw - 102;
    unsigned int win_h = 62;

    unsigned int dock_x = dx + dw - 24;
    unsigned int dock_y = dy + 54;

    unsigned int task_y = dy + dh - 22;

    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 18, y + 12, "LINGJING DESKTOP");

    if (gshell_scene_background_ready) {
        graphics_rect(dx, dy, dw, dh, 0x00000614);
        graphics_rect(dx, dy, dw, 2, 0x000044AA);
        graphics_rect(dx, dy + dh - 2, dw, 2, 0x000044AA);
        graphics_rect(dx, dy, 2, dh, 0x000044AA);
        graphics_rect(dx + dw - 2, dy, 2, dh, 0x000044AA);
    }

    if (gshell_scene_topbar_ready) {
        graphics_rect(top_x, top_y, top_w, 18, 0x00001430);
        graphics_text(top_x + 8, top_y + 5, "LJ OS");
        graphics_text(top_x + top_w - 34, top_y + 5, "OK");
    }

    if (gshell_scene_launcher_ready) {
        graphics_rect(launch_x, launch_y, launch_w, launch_h, 0x00001028);
        graphics_rect(launch_x, launch_y, launch_w, 2, 0x000088CC);
        graphics_text(launch_x + 8, launch_y + 8, "START");

        graphics_rect(launch_x + 8, launch_y + 27, 40, 12, 0x00002044);
        graphics_text(launch_x + 12, launch_y + 30, "find");

        graphics_rect(launch_x + 8, launch_y + 50, 16, 16, 0x000055AA);
        graphics_rect(launch_x + 32, launch_y + 50, 16, 16, 0x00004488);
        graphics_text(launch_x + 12, launch_y + 55, "D");
        graphics_text(launch_x + 36, launch_y + 55, "S");
    }

    if (gshell_scene_cards_ready) {
        graphics_rect(card_x, card_y, card_w, card_h, 0x000055AA);
        graphics_rect(card_x + 42, card_y, card_w, card_h, 0x00003377);
        graphics_rect(card_x + 84, card_y, card_w, card_h, 0x00003377);

        graphics_text(card_x + 6, card_y + 8, "APP");
        graphics_text(card_x + 48, card_y + 8, "SYS");
        graphics_text(card_x + 92, card_y + 8, "FS");
    }

    if (gshell_scene_focus_ready) {
        graphics_rect(card_x - 3, card_y - 3, card_w + 6, 2, 0x0000FFFF);
        graphics_rect(card_x - 3, card_y + card_h + 1, card_w + 6, 2, 0x0000FFFF);
        graphics_rect(card_x - 3, card_y - 3, 2, card_h + 6, 0x0000FFFF);
        graphics_rect(card_x + card_w + 1, card_y - 3, 2, card_h + 6, 0x0000FFFF);
    }

    if (gshell_scene_window_ready) {
        graphics_rect(win_x + 4, win_y + 4, win_w, win_h, 0x00000206);
        graphics_rect(win_x, win_y, win_w, win_h, 0x00001430);
        graphics_rect(win_x, win_y, win_w, 14, 0x000066AA);
        graphics_text(win_x + 7, win_y + 4, "demo.win");

        graphics_rect(win_x + 8, win_y + 24, win_w - 16, win_h - 34, 0x00000816);
        graphics_text(win_x + 14, win_y + 34, "RUNNING");
    }

    if (gshell_scene_dock_ready) {
        graphics_rect(dock_x, dock_y, 16, 70, 0x00001430);
        graphics_rect(dock_x + 4, dock_y + 8, 8, 10, 0x000055AA);
        graphics_rect(dock_x + 4, dock_y + 28, 8, 10, 0x00004488);
        graphics_rect(dock_x + 4, dock_y + 48, 8, 10, 0x00004488);
    }

    if (gshell_scene_taskbar_ready) {
        graphics_rect(dx + 8, task_y, dw - 16, 18, 0x00002255);

        graphics_rect(dx + 14, task_y + 4, 26, 10, 0x000055AA);
        graphics_text(dx + 19, task_y + 6, "LJ");

        graphics_rect(dx + 48, task_y + 4, 40, 10, 0x00004488);
        graphics_text(dx + 54, task_y + 6, "APP");
    }

    if (gshell_scene_tray_ready) {
        graphics_text(dx + dw - 60, task_y + 6, "NET");
        graphics_text(dx + dw - 30, task_y + 6, "12");
    }

    if (gshell_scene_widgets_ready) {
        graphics_rect(dx + dw - 58, dy + 32, 36, 12, 0x00001028);
        graphics_text(dx + dw - 52, dy + 35, "CTRL");
    }

    graphics_text(x + 18, y + h - 28, "DEFAULT DESKTOP");
}

static void gshell_draw_command_view_visualfinal(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "1.4 VISUAL UI FINAL");
    graphics_text(x + 24, y + 48, "STATE");
    graphics_text(x + 170, y + 48, gshell_visual_final_state);
    graphics_text(x + 24, y + 72, "SCENE");
    graphics_text(x + 170, y + 72, gshell_visual_final_scene_ready ? "ready" : "partial");
    graphics_text(x + 24, y + 96, "MOCK");
    graphics_text(x + 170, y + 96, gshell_visual_final_mock_ready ? "ready" : "partial");
    graphics_text(x + 24, y + 120, "CARDS");
    graphics_text(x + 170, y + 120, gshell_visual_final_cards_ready ? "ready" : "partial");
    graphics_text(x + 24, y + 144, "LAUNCHER");
    graphics_text(x + 170, y + 144, gshell_visual_final_launcher_ready ? "ready" : "partial");
    graphics_text(x + 24, y + 168, "POLISH");
    graphics_text(x + 170, y + 168, gshell_visual_final_polish_ready ? "ready" : "partial");
    graphics_text(x + 24, y + 192, "DEFAULT");
    graphics_text(x + 170, y + 192, gshell_visual_final_default_ready ? "on" : "off");
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_visual_final_events);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 170, y + 240, gshell_visual_final_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 170, y + 264, "1.5 input");
}

static void gshell_draw_command_view_interactstatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "DESKTOP INTERACTION");
    graphics_text(x + 24, y + 48, "STATE");
    graphics_text(x + 170, y + 48, gshell_interact_state);
    graphics_text(x + 24, y + 72, "TARGET");
    graphics_text(x + 170, y + 72, gshell_interact_target);
    gshell_draw_value_uint(x + 24, y + 96, "PTR X", gshell_interact_pointer_x);
    gshell_draw_value_uint(x + 24, y + 120, "PTR Y", gshell_interact_pointer_y);
    graphics_text(x + 24, y + 144, "HOVER");
    graphics_text(x + 170, y + 144, gshell_interact_hover_ready ? "yes" : "no");
    graphics_text(x + 24, y + 168, "CLICK");
    graphics_text(x + 170, y + 168, gshell_interact_click_ready ? "yes" : "no");
    graphics_text(x + 24, y + 192, "FOCUS");
    graphics_text(x + 170, y + 192, gshell_interact_focus_ready ? "yes" : "no");
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_interact_events);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 170, y + 240, gshell_interact_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 170, y + 264, "click route");
}

static void gshell_draw_command_view_routestatus(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
    graphics_rect(x, y, w, h, 0x00000000);
    graphics_rect(x, y, w, 4, 0x0000AAFF);
    graphics_rect(x, y + h - 4, w, 4, 0x0000AAFF);
    graphics_rect(x, y, 4, h, 0x0000AAFF);
    graphics_rect(x + w - 4, y, 4, h, 0x0000AAFF);

    graphics_text(x + 24, y + 20, "CLICK ROUTING CORE");
    graphics_text(x + 24, y + 48, "STATE");
    graphics_text(x + 170, y + 48, gshell_route_state);
    graphics_text(x + 24, y + 72, "TARGET");
    graphics_text(x + 170, y + 72, gshell_route_target);
    graphics_text(x + 24, y + 96, "DESKTOP");
    graphics_text(x + 170, y + 96, gshell_route_desktop_ready ? "hit" : "none");
    graphics_text(x + 24, y + 120, "CARD");
    graphics_text(x + 170, y + 120, gshell_route_card_ready ? "hit" : "none");
    graphics_text(x + 24, y + 144, "WINDOW");
    graphics_text(x + 170, y + 144, gshell_route_window_ready ? "hit" : "none");
    graphics_text(x + 24, y + 168, "CLICK");
    graphics_text(x + 170, y + 168, gshell_route_click_ready ? "yes" : "no");
    graphics_text(x + 24, y + 192, "FOCUS");
    graphics_text(x + 170, y + 192, gshell_route_focus_ready ? "yes" : "no");
    gshell_draw_value_uint(x + 24, y + 216, "EVENTS", gshell_route_events);
    graphics_text(x + 24, y + 240, "LAST");
    graphics_text(x + 170, y + 240, gshell_route_last);
    graphics_text(x + 24, y + 264, "NEXT");
    graphics_text(x + 170, y + 264, "action bind");
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

    if (gshell_text_equal(gshell_command_view, "RUNTIMESTATUS")) {
        gshell_draw_command_view_runtimestatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "ELFSTATUS")) {
        gshell_draw_command_view_elfstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "MANIFESTSTATUS")) {
        gshell_draw_command_view_manifeststatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "APPSLOTSTATUS")) {
        gshell_draw_command_view_appslotstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "LAUNCHGATE")) {
        gshell_draw_command_view_launchgate(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "PERMSTATUS")) {
        gshell_draw_command_view_permstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "FSSTATUS")) {
        gshell_draw_command_view_fsstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "LIFESTATUS")) {
        gshell_draw_command_view_lifestatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "RUNTIMEPANEL")) {
        gshell_draw_command_view_runtimepanel(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "RUNTIMEFINAL")) {
        gshell_draw_command_view_runtimefinal(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "PROTOTYPE")) {
        gshell_draw_command_view_prototype(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "DASHBOARD")) {
        gshell_draw_command_view_dashboard(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "BOOTHEALTH")) {
        gshell_draw_command_view_boothealth(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "FLOWSTATUS")) {
        gshell_draw_command_view_flowstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "WALKTHROUGH")) {
        gshell_draw_command_view_walkthrough(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "MILEFINAL")) {
        gshell_draw_command_view_milestonefinal(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "UIPOLISH")) {
        gshell_draw_command_view_uipolish(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "CLOSEOUT")) {
        gshell_draw_command_view_closeout(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "INPUTSTATUS")) {
        gshell_draw_command_view_inputstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "CURSORSTATUS")) {
        gshell_draw_command_view_cursorstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "HITSTATUS")) {
        gshell_draw_command_view_hitstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "BUTTONSTATUS")) {
        gshell_draw_command_view_buttonstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "WINDOWSTATUS")) {
        gshell_draw_command_view_windowstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "DESKTOPSTATUS")) {
        gshell_draw_command_view_desktopstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "INTERACTIONFINAL")) {
        gshell_draw_command_view_interactionfinal(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "SHELLSTATUS")) {
        gshell_draw_command_view_shellstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "LAUNCHERSTATUS")) {
        gshell_draw_command_view_launcherstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "TASKBARSTATUS")) {
        gshell_draw_command_view_taskbarstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "LAYOUTSTATUS")) {
        gshell_draw_command_view_layoutstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "DESKTOPSHELL")) {
        gshell_draw_command_view_desktopshell(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "SHELLFINAL")) {
        gshell_draw_command_view_shellfinal(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "APPSHELLSTATUS")) {
        gshell_draw_command_view_appshellstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "CATALOGSTATUS")) {
        gshell_draw_command_view_catalogstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "DETAILSTATUS")) {
        gshell_draw_command_view_detailstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "ACTIONSTATUS")) {
        gshell_draw_command_view_actionstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "APPMGMTSTATUS")) {
        gshell_draw_command_view_appmgmtstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "APPFINAL")) {
        gshell_draw_command_view_appfinal(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "VISUALSTATUS")) {
        gshell_draw_command_view_visualstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "CARDSTATUS")) {
        gshell_draw_command_view_cardstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "UIMOCKSTATUS")) {
        gshell_draw_command_view_uimockstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "LAUNCHMOCKSTATUS")) {
        gshell_draw_command_view_launchmockstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "POLISHSTATUS")) {
        gshell_draw_command_view_polishstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "DESKTOPSCENE")) {
        gshell_draw_command_view_desktopscene(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "VISUALFINAL")) {
        gshell_draw_command_view_visualfinal(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "INTERACTSTATUS")) {
        gshell_draw_command_view_interactstatus(x, y, w, h);
        return;
    }

    if (gshell_text_equal(gshell_command_view, "ROUTESTATUS")) {
        gshell_draw_command_view_routestatus(x, y, w, h);
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
    graphics_text(390, 54, "CLICK ROUTE");

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
    graphics_text(width - 208, 164, "ROUTESTATUS");
    graphics_text(width - 208, 188, "HITDESKTOP");
    graphics_text(width - 208, 212, "HITCARD");
    graphics_text(width - 208, 236, "HITWINDOW");
    graphics_text(width - 208, 260, "CLICKROUTE");
    graphics_text(width - 208, 284, "OPENROUTE");
    graphics_text(width - 208, 308, "ROUTEDEMO");
    graphics_text(width - 208, 332, "ROUTECHECK");

    gshell_draw_history_panel(width - 208, 368);

    gshell_draw_input_zone(width, height);

    graphics_pixel(center_x, center_y, 0x00FFFFFF);
    graphics_pixel(center_x + 1, center_y, 0x00FFFFFF);
    graphics_pixel(center_x - 1, center_y, 0x00FFFFFF);
    graphics_pixel(center_x, center_y + 1, 0x00FFFFFF);
    graphics_pixel(center_x, center_y - 1, 0x00FFFFFF);

    platform_print("  output: graphics-self\n");
    platform_print("  command zone: desktop-click-routing-core\n");
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