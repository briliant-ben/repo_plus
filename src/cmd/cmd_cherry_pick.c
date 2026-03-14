#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../git_ops.h"
#include "cmd.h"

void cmd_cherry_pick_usage(void) {
  printf("usage: rpp cherry-pick <rev> [<project>...]\n");
}

void cmd_cherry_pick_help(void) {
  cmd_cherry_pick_usage();
  printf("\nCherry-pick a specific revision across the workspace.\n");
}

int cmd_cherry_pick_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace\n");
    return 1;
  }

  if (argc < 2) {
    cmd_cherry_pick_usage();
    return 1;
  }

  const char *rev = argv[1];
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

    /* Check if the commit exists in the project */
    const char *check_args[] = {"cat-file", "-e", rev, NULL};
    if (git_run(worktree, check_args, NULL, NULL) == 0) {
      printf("project %s/\n", p->path);
      const char *cp_args[] = {"cherry-pick", rev, NULL};
      int rc = git_run(worktree, cp_args, NULL, NULL);
      if (rc != 0) {
        fprintf(stderr,
                "rpp cherry-pick: failed in %s. Please resolve manually.\n",
                p->path);
        errors++;
      }
    }
  }

  return errors > 0 ? 1 : 0;
}
