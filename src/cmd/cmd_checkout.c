#include <getopt.h>
#include <stdio.h>
#include <string.h>

#include "../git_ops.h"
#include "cmd.h"

/* ------------------------------------------------------------------ */
/* checkout                                                           */
/* ------------------------------------------------------------------ */

void cmd_checkout_usage(void) {
  printf("usage: rpp checkout <branch> [<project>...]\n");
}

void cmd_checkout_help(void) {
  cmd_checkout_usage();
  printf("\nCheckout a branch for development in each specified project.\n");
}

int cmd_checkout_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace\n");
    return 1;
  }

  if (argc < 2) {
    fprintf(stderr, "repo checkout: branch name required\n");
    cmd_checkout_usage();
    return 1;
  }

  const char *branch = argv[1];
  int n_filter = argc - 2;
  char **filter = argv + 2;
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

    /* Original repo doesn't checkout if branch doesn't exist, but 'git
     * checkout' will fail gracefully */
    int rc = git_checkout(worktree, branch);
    if (rc == 0) {
      printf("project %s/ : switched to branch %s\n", p->path, branch);
    } else {
      /* Just ignore errors here to mimic repo behavior where a branch might not
         exist in all projects. For a true replica, we could check if branch
         exists first, but `git_checkout` simply fails if not present. */
      errors++;
    }
  }

  return 0; /* Original repo doesn't fail globally if a branch is missing in
               some projects */
}
