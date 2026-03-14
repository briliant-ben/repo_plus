#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../color.h"
#include "../git_ops.h"
#include "cmd.h"

/* ------------------------------------------------------------------ */
/* branches                                                           */
/* ------------------------------------------------------------------ */

void cmd_branches_usage(void) {
  printf("usage: rpp branches [<project>...]\n");
}

void cmd_branches_help(void) {
  cmd_branches_usage();
  printf("\nView current topic branches.\n");
}

int cmd_branches_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace\n");
    return 1;
  }

  int n_filter = argc - 1;
  char **filter = argv + 1;
  Manifest *m = ctx->manifest;

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

    /* Get all branches for the project */
    const char *args[] = {"branch", NULL};
    char *output = NULL;
    int rc = git_run(worktree, args, &output, NULL);
    if (rc == 0 && output) {
      /* Parse output and display branches. Let's make a simple output for
       * Phase 1. */
      char *line = strtok(output, "\n");
      int has_branches = 0;

      while (line) {
        /* Skip detached head or remote branches if any show up, but 'git
         * branch' usually shows local */
        /* In a more advanced version, we'd aggregate branches across projects.
         */
        if (!has_branches) {
          color_printf(COLOR_HEADER, "\nproject %s/ :\n", p->path);
          has_branches = 1;
        }
        if (line[0] == '*') {
          color_printf(COLOR_ADDED, "  %s\n", line);
        } else {
          printf("  %s\n", line);
        }
        line = strtok(NULL, "\n");
      }
    }

    if (output)
      free(output);
  }

  return 0;
}
