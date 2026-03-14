#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../git_ops.h"
#include "cmd.h"

void cmd_stage_usage(void) { printf("usage: rpp stage -i [<project>...]\n"); }

void cmd_stage_help(void) {
  cmd_stage_usage();
  printf("\nInteract with the git index (staging area) in projects with "
         "uncommitted changes.\n"
         "Options:\n"
         "  -i, --interactive    Interactively add files to the index\n");
}

int cmd_stage_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace\n");
    return 1;
  }

  int interactive = 0;
  static struct option long_opts[] = {{"interactive", no_argument, NULL, 'i'},
                                      {NULL, 0, NULL, 0}};

  int opt;
  optind = 1;
  while ((opt = getopt_long(argc, argv, "i", long_opts, NULL)) != -1) {
    switch (opt) {
    case 'i':
      interactive = 1;
      break;
    default:
      cmd_stage_usage();
      return 1;
    }
  }

  if (!interactive) {
    fprintf(stderr,
            "error: Only interactive staging (-i) is currently supported\n");
    return 1;
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

    /* Check if there are changes */
    const char *status_args[] = {"status", "--porcelain", NULL};
    char *output = NULL;
    int rc = git_run(worktree, status_args, &output, NULL);

    if (rc == 0 && output && output[0] != '\0') {
      printf("\nproject %s/\n", p->path);
      const char *add_args[] = {"add", "-i", NULL};
      /* Call git run interactively with no captured output */
      int add_rc = git_run(worktree, add_args, NULL, NULL);
      if (add_rc != 0) {
        errors++;
      }
    }
    free(output);
  }

  return errors > 0 ? 1 : 0;
}
