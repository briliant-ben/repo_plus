#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../git_ops.h"
#include "cmd.h"

/* ------------------------------------------------------------------ */
/* grep                                                               */
/* ------------------------------------------------------------------ */

void cmd_grep_usage(void) {
  printf("usage: rpp grep <pattern> [<git-grep-options>...]\n");
}

void cmd_grep_help(void) {
  cmd_grep_usage();
  printf("\nSearch for a pattern across projects.\n");
}

int cmd_grep_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace\n");
    return 1;
  }

  if (argc < 2) {
    fprintf(stderr, "rpp grep: pattern required\n");
    cmd_grep_usage();
    return 1;
  }

  const char *git_args[64];
  int git_argc = 0;
  git_args[git_argc++] = "grep";
  for (int i = 1; i < argc; i++) {
    if (git_argc < 63) {
      git_args[git_argc++] = argv[i];
    }
  }
  git_args[git_argc] = NULL;

  Manifest *m = ctx->manifest;
  int errors = 0;
  int found_any = 0;

  for (int i = 0; i < m->project_count; i++) {
    Project *p = &m->projects[i];

    char worktree[2048];
    snprintf(worktree, sizeof(worktree), "%s/%s", ctx->topdir, p->path);

    char *output = NULL;
    int rc = git_run(worktree, git_args, &output, NULL);
    if (rc == 0 && output) {
      char *line = output;
      char *token = strtok(line, "\n");
      while (token) {
        printf("%s/%s\n", p->path, token);
        token = strtok(NULL, "\n");
        found_any = 1;
      }
    } else if (rc != 0 && rc != 1) {
      // 1 means no match, other codes mean error
      errors++;
    }
    if (output) free(output);
  }

  if (!found_any) {
    return 1;
  }

  return errors > 0 ? 1 : 0;
}
