#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#else
#define PACKAGE_VERSION "0.1.0"
#endif

#include "cmd/cmd.h"
#include "manifest.h"

/* ------------------------------------------------------------------ */
/* Command table                                                        */
/* ------------------------------------------------------------------ */

static const SubCommand CMD_HELP = {
    .name = "help",
    .summary = "Display help about rpp and its commands",
    .usage = cmd_help_usage,
    .help = cmd_help_help,
    .run = cmd_help_run,
};

static const SubCommand CMD_VERSION = {
    .name = "version",
    .summary = "Display the version of rpp",
    .usage = cmd_version_usage,
    .help = cmd_version_help,
    .run = cmd_version_run,
};

static const SubCommand CMD_INIT = {
    .name = "init",
    .summary = "Initialize a repo client checkout in the current directory",
    .usage = cmd_init_usage,
    .help = cmd_init_help,
    .run = cmd_init_run,
};

static const SubCommand CMD_SYNC = {
    .name = "sync",
    .summary = "Update working tree to the latest revision",
    .usage = cmd_sync_usage,
    .help = cmd_sync_help,
    .run = cmd_sync_run,
};

static const SubCommand CMD_STATUS = {
    .name = "status",
    .summary = "Show the working tree status",
    .usage = cmd_status_usage,
    .help = cmd_status_help,
    .run = cmd_status_run,
};

static const SubCommand CMD_FORALL = {
    .name = "forall",
    .summary = "Run a shell command in each project",
    .usage = cmd_forall_usage,
    .help = cmd_forall_help,
    .run = cmd_forall_run,
};

static const SubCommand CMD_LIST = {
    .name = "list",
    .summary = "List projects and their associated directories",
    .usage = cmd_list_usage,
    .help = cmd_list_help,
    .run = cmd_list_run,
};

static const SubCommand CMD_START = {
    .name = "start",
    .summary = "Start a new branch for development",
    .usage = cmd_start_usage,
    .help = cmd_start_help,
    .run = cmd_start_run,
};

static const SubCommand CMD_DIFF = {
    .name = "diff",
    .summary = "Show changes between commit, index and work tree",
    .usage = cmd_diff_usage,
    .help = cmd_diff_help,
    .run = cmd_diff_run,
};

static const SubCommand CMD_ABANDON = {
    .name = "abandon",
    .summary = "Permanently abandon a development branch",
    .usage = cmd_abandon_usage,
    .help = cmd_abandon_help,
    .run = cmd_abandon_run,
};

static const SubCommand CMD_BRANCHES = {
    .name = "branches",
    .summary = "View current topic branches",
    .usage = cmd_branches_usage,
    .help = cmd_branches_help,
    .run = cmd_branches_run,
};

static const SubCommand CMD_CHECKOUT = {
    .name = "checkout",
    .summary = "Checkout a branch for development",
    .usage = cmd_checkout_usage,
    .help = cmd_checkout_help,
    .run = cmd_checkout_run,
};

static const SubCommand CMD_INFO = {
    .name = "info",
    .summary =
        "Get info on the manifest branch, current branch or unmerged branches",
    .usage = cmd_info_usage,
    .help = cmd_info_help,
    .run = cmd_info_run,
};

static const SubCommand CMD_REBASE = {
    .name = "rebase",
    .summary = "Rebase local branches onto their upstream equivalents",
    .usage = cmd_rebase_usage,
    .help = cmd_rebase_help,
    .run = cmd_rebase_run,
};

static const SubCommand CMD_PRUNE = {
    .name = "prune",
    .summary = "Prune (delete) already merged topics",
    .usage = cmd_prune_usage,
    .help = cmd_prune_help,
    .run = cmd_prune_run,
};

static const SubCommand CMD_STAGE = {
    .name = "stage",
    .summary = "Interact with the git index (staging area)",
    .usage = cmd_stage_usage,
    .help = cmd_stage_help,
    .run = cmd_stage_run,
};

static const SubCommand CMD_CHERRY_PICK = {
    .name = "cherry-pick",
    .summary = "Cherry-pick a specific revision across the workspace",
    .usage = cmd_cherry_pick_usage,
    .help = cmd_cherry_pick_help,
    .run = cmd_cherry_pick_run,
};

static const SubCommand CMD_UPLOAD = {
    .name = "upload",
    .summary = "Upload changes for code review",
    .usage = cmd_upload_usage,
    .help = cmd_upload_help,
    .run = cmd_upload_run,
};

static const SubCommand CMD_DOWNLOAD = {
    .name = "download",
    .summary = "Download and checkout a change",
    .usage = cmd_download_usage,
    .help = cmd_download_help,
    .run = cmd_download_run,
};

static const SubCommand CMD_GREP = {
    .name = "grep",
    .summary = "Search for a pattern across projects",
    .usage = cmd_grep_usage,
    .help = cmd_grep_help,
    .run = cmd_grep_run,
};

/* NULL-terminated table */
const SubCommand *all_commands[] = {
    &CMD_HELP,     &CMD_VERSION,     &CMD_INIT,   &CMD_SYNC,     &CMD_STATUS,
    &CMD_FORALL,   &CMD_LIST,        &CMD_START,  &CMD_DIFF,     &CMD_ABANDON,
    &CMD_BRANCHES, &CMD_CHECKOUT,    &CMD_INFO,   &CMD_REBASE,   &CMD_PRUNE,
    &CMD_STAGE,    &CMD_CHERRY_PICK, &CMD_UPLOAD, &CMD_DOWNLOAD, &CMD_GREP,
    NULL};

int all_commands_count = 20;

/* ------------------------------------------------------------------ */
/* Command lookup                                                        */
/* ------------------------------------------------------------------ */

const SubCommand *cmd_find(const char *name) {
  for (int i = 0; all_commands[i]; i++) {
    if (strcmp(all_commands[i]->name, name) == 0)
      return all_commands[i];
  }
  return NULL;
}

void cmd_print_help(int all_flag) {
  (void)all_flag;
  printf("Usage: rpp [-p|--paginate] COMMAND [ARGS]\n\n");
  printf("Available commands:\n");
  for (int i = 0; all_commands[i]; i++) {
    printf("  %-15s %s\n", all_commands[i]->name, all_commands[i]->summary);
  }
  printf("\nRun `rpp help <command>` for command-specific details.\n");
}

/* ------------------------------------------------------------------ */
/* Main                                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[]) {
  /* Parse global flags */
  int verbose = 0;
  int quiet = 0;
  int paginate = 0;

  int i = 1;
  while (i < argc && argv[i][0] == '-') {
    if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--paginate") == 0) {
      paginate = 1;
      i++;
    } else if (strcmp(argv[i], "--no-pager") == 0) {
      paginate = 0;
      i++;
    } else if (strcmp(argv[i], "-v") == 0 ||
               strcmp(argv[i], "--verbose") == 0) {
      verbose = 1;
      i++;
    } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
      quiet = 1;
      i++;
    } else if (strcmp(argv[i], "--version") == 0) {
      /* Handle --version as a global flag too */
      printf("rpp version " PACKAGE_VERSION "\n");
      return 0;
    } else {
      /* Unknown global flag – stop scanning */
      break;
    }
  }
  (void)paginate;

  /* Subcommand name */
  if (i >= argc) {
    cmd_print_help(0);
    return 1;
  }
  const char *cmd_name = argv[i];
  int sub_argc = argc - i;
  char **sub_argv = argv + i;

  /* Find the subcommand */
  const SubCommand *sc = cmd_find(cmd_name);
  if (!sc) {
    fprintf(stderr, "rpp: '%s' is not a rpp command.  See 'rpp help'.\n",
            cmd_name);
    return 1;
  }

  /* Build context: find .repo and load manifest */
  CmdContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.verbose = verbose;
  ctx.quiet = quiet;

  /* init command does not need an existing manifest */
  int needs_manifest =
      (strcmp(cmd_name, "init") != 0 && strcmp(cmd_name, "help") != 0 &&
       strcmp(cmd_name, "version") != 0);

  if (needs_manifest) {
    if (manifest_find_repodir(ctx.repodir, sizeof(ctx.repodir)) != 0) {
      fprintf(stderr,
              "error: manifest missing or unreadable -- please run init\n");
      return 1;
    }

    ctx.manifest = manifest_load(ctx.repodir);
    if (!ctx.manifest) {
      fprintf(stderr, "error: failed to load manifest from %s\n", ctx.repodir);
      return 1;
    }

    snprintf(ctx.topdir, sizeof(ctx.topdir), "%s", ctx.manifest->topdir);
  }

  /* Execute */
  int rc = sc->run(&ctx, sub_argc, sub_argv);

  if (ctx.manifest)
    manifest_free(ctx.manifest);

  return rc;
}
