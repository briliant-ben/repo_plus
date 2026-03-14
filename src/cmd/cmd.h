#ifndef CMD_H
#define CMD_H

#include "../manifest.h"

/*
 * cmd.h - Subcommand interface
 *
 * Each subcommand implements three functions:
 *   cmd_<name>_usage()  - print usage string
 *   cmd_<name>_help()   - print full help
 *   cmd_<name>()        - execute
 */

/* Context passed to every command */
typedef struct {
  Manifest *manifest; /* loaded manifest (NULL if not in a workspace) */
  char repodir[1024]; /* .repo/ path */
  char topdir[1024];  /* workspace root */
  int verbose;
  int quiet;
  int color; /* 0: never, 1: always, 2: auto */
} CmdContext;

/* Subcommand descriptor */
typedef struct {
  const char *name;
  const char *summary;
  void (*usage)(void);
  void (*help)(void);
  int (*run)(CmdContext *ctx, int argc, char **argv);
} SubCommand;

/* All registered commands */
extern const SubCommand *all_commands[];
extern int all_commands_count;

/* Find a command by name */
const SubCommand *cmd_find(const char *name);

/* Print global help */
void cmd_print_help(int all);

/* --- Individual command declarations --- */

/* help */
void cmd_help_usage(void);
void cmd_help_help(void);
int cmd_help_run(CmdContext *ctx, int argc, char **argv);

/* version */
void cmd_version_usage(void);
void cmd_version_help(void);
int cmd_version_run(CmdContext *ctx, int argc, char **argv);

/* init */
void cmd_init_usage(void);
void cmd_init_help(void);
int cmd_init_run(CmdContext *ctx, int argc, char **argv);

/* sync */
void cmd_sync_usage(void);
void cmd_sync_help(void);
int cmd_sync_run(CmdContext *ctx, int argc, char **argv);

/* status */
void cmd_status_usage(void);
void cmd_status_help(void);
int cmd_status_run(CmdContext *ctx, int argc, char **argv);

/* forall */
void cmd_forall_usage(void);
void cmd_forall_help(void);
int cmd_forall_run(CmdContext *ctx, int argc, char **argv);

/* list */
void cmd_list_usage(void);
void cmd_list_help(void);
int cmd_list_run(CmdContext *ctx, int argc, char **argv);

/* start */
void cmd_start_usage(void);
void cmd_start_help(void);
int cmd_start_run(CmdContext *ctx, int argc, char **argv);

/* diff */
void cmd_diff_usage(void);
void cmd_diff_help(void);
int cmd_diff_run(CmdContext *ctx, int argc, char **argv);

/* abandon */
void cmd_abandon_usage(void);
void cmd_abandon_help(void);
int cmd_abandon_run(CmdContext *ctx, int argc, char **argv);

/* branches */
void cmd_branches_usage(void);
void cmd_branches_help(void);
int cmd_branches_run(CmdContext *ctx, int argc, char **argv);

/* checkout */
void cmd_checkout_usage(void);
void cmd_checkout_help(void);
int cmd_checkout_run(CmdContext *ctx, int argc, char **argv);

/* info */
void cmd_info_usage(void);
void cmd_info_help(void);
int cmd_info_run(CmdContext *ctx, int argc, char **argv);

void cmd_rebase_usage(void);
void cmd_rebase_help(void);
int cmd_rebase_run(CmdContext *ctx, int argc, char **argv);

void cmd_prune_usage(void);
void cmd_prune_help(void);
int cmd_prune_run(CmdContext *ctx, int argc, char **argv);

void cmd_stage_usage(void);
void cmd_stage_help(void);
int cmd_stage_run(CmdContext *ctx, int argc, char **argv);

void cmd_cherry_pick_usage(void);
void cmd_cherry_pick_help(void);
int cmd_cherry_pick_run(CmdContext *ctx, int argc, char **argv);

void cmd_upload_usage(void);
void cmd_upload_help(void);
int cmd_upload_run(CmdContext *ctx, int argc, char **argv);

void cmd_download_usage(void);
void cmd_download_help(void);
int cmd_download_run(CmdContext *ctx, int argc, char **argv);

void cmd_grep_usage(void);
void cmd_grep_help(void);
int cmd_grep_run(CmdContext *ctx, int argc, char **argv);

#endif /* CMD_H */
