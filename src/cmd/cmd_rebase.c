#define _GNU_SOURCE
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../git_ops.h"
#include "cmd.h"

void cmd_rebase_usage(void) {
  printf("usage: rpp rebase [--auto-stash] [<project>...]\n");
}

void cmd_rebase_help(void) {
  cmd_rebase_usage();
  printf("\nRebase local branches onto their upstream equivalents.\n\n"
         "Options:\n"
         "  --auto-stash    Stash local modifications before rebase, and "
         "restore them after\n");
}

int cmd_rebase_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace\n");
    return 1;
  }

  int auto_stash = 0;
  static struct option long_opts[] = {{"auto-stash", no_argument, NULL, 'a'},
                                      {NULL, 0, NULL, 0}};

  int opt;
  optind = 1;
  while ((opt = getopt_long(argc, argv, "a", long_opts, NULL)) != -1) {
    switch (opt) {
    case 'a':
      auto_stash = 1;
      break;
    default:
      cmd_rebase_usage();
      return 1;
    }
  }

  int n_filter = (optind <= argc) ? (argc - optind) : 0;
  char **filter = argv + (optind <= argc ? optind : argc);
  Manifest *m = ctx->manifest;
  int errors = 0;

  for (int i = 0; i < m->project_count; i++) {
    Project *p = &m->projects[i];

    if (n_filter > 0) {
      int match = 0;
      for (int j = 0; j < n_filter; j++) {
        if (strcmp(p->name, filter[j]) == 0 ||
            strcmp(p->path, filter[j]) == 0) {
          match = 1;
          break;
        }
      }
      if (!match)
        continue;
    }

    char worktree[2048];
    snprintf(worktree, sizeof(worktree), "%s/%s", ctx->topdir, p->path);

    char *branch = git_current_branch(worktree);
    if (!branch) {
      /* Detached HEAD, nothing to rebase onto */
      continue;
    }

    /* Get upstream merge branch */
    char key_merge[256];
    snprintf(key_merge, sizeof(key_merge), "branch.%s.merge", branch);
    char *upstream = git_config_get(worktree, key_merge);

    if (!upstream) {
      /* Fallback to manifest revision if no upstream config */
      upstream = strdup(p->revision[0] ? p->revision : m->def.revision);
      if (!upstream[0]) {
        free(upstream);
        upstream = strdup("refs/heads/master");
      }
    }

    /* Convert refs/heads/main to origin/main for rebasing */
    char rebase_target[512];
    if (strncmp(upstream, "refs/heads/", 11) == 0) {
      snprintf(rebase_target, sizeof(rebase_target), "origin/%s",
               upstream + 11);
    } else {
      snprintf(rebase_target, sizeof(rebase_target), "origin/%s", upstream);
    }

    printf("project %s/: rebasing %s onto %s\n", p->path, branch,
           rebase_target);

    const char *args[8];
    int idx = 0;
    args[idx++] = "rebase";
    if (auto_stash) {
      args[idx++] = "--autostash";
    }
    args[idx++] = rebase_target;
    args[idx] = NULL;

    int rc = git_run(worktree, args, NULL, NULL);
    if (rc != 0) {
      fprintf(stderr, "repo rebase: failed in %s\n", p->path);
      errors++;
    }

    free(upstream);
    free(branch);
  }

  return errors > 0 ? 1 : 0;
}
