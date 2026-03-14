#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../git_ops.h"
#include "cmd.h"

void cmd_prune_usage(void) { printf("usage: rpp prune [<project>...]\n"); }

void cmd_prune_help(void) {
  cmd_prune_usage();
  printf("\nPrune (delete) local branches that are already merged.\n");
}

int cmd_prune_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace\n");
    return 1;
  }

  int n_filter = argc - 1;
  char **filter = argv + 1;
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

    /* Get currently checked out branch to skip deleting it */
    char *current_branch = git_current_branch(worktree);

    /* Get all merged branches */
    const char *args[] = {"branch", "--merged", NULL};
    char *output = NULL;
    int rc = git_run(worktree, args, &output, NULL);
    if (rc != 0 || !output) {
      free(current_branch);
      free(output);
      continue; /* Skip if error */
    }

    /* Parse output */
    char *line = strtok(output, "\n");
    while (line) {
      /* Skip lines starting with '*' (current branch) or whitespace alone */
      if (line[0] == '*' || line[0] == ' ') {
        char *br = line + 2;
        /* Trim leading spaces */
        while (*br == ' ')
          br++;

        if (br[0] != '\0' &&
            (current_branch == NULL || strcmp(br, current_branch) != 0)) {
          /* It is a merged branch and not the current branch */
          const char *del_args[] = {"branch", "-d", br, NULL};
          int d_rc = git_run(worktree, del_args, NULL, NULL);
          if (d_rc == 0) {
            printf("project %s/ : Pruned branch %s\n", p->path, br);
          } else {
            fprintf(stderr, "project %s/ : Failed to prune branch %s\n",
                    p->path, br);
            errors++;
          }
        }
      }
      line = strtok(NULL, "\n");
    }

    free(output);
    free(current_branch);
  }

  return errors > 0 ? 1 : 0;
}
