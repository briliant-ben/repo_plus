#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../color.h"
#include "../git_ops.h"
#include "cmd.h"

/* ------------------------------------------------------------------ */
/* info                                                               */
/* ------------------------------------------------------------------ */

void cmd_info_usage(void) {
  printf("usage: rpp info [-d|--local-only] [-o|--overview] [<project>...]\n");
}

void cmd_info_help(void) {
  cmd_info_usage();
  printf("\nGet info on the manifest branch, current branch or unmerged "
         "branches.\n\n"
         "Options:\n"
         "  -d, --local-only    Disable all remote operations\n"
         "  -o, --overview     Show an overview of current branches\n");
}

int cmd_info_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace\n");
    return 1;
  }

  int local_only = 0;
  int overview = 0;

  static struct option long_opts[] = {{"local-only", no_argument, NULL, 'd'},
                                      {"overview", no_argument, NULL, 'o'},
                                      {NULL, 0, NULL, 0}};

  int opt;
  optind = 1;
  while ((opt = getopt_long(argc, argv, "do", long_opts, NULL)) != -1) {
    switch (opt) {
    case 'd':
      local_only = 1;
      break;
    case 'o':
      overview = 1;
      break;
    default:
      cmd_info_usage();
      return 1;
    }
  }

  int n_filter = (optind <= argc) ? (argc - optind) : 0;
  char **filter = argv + (optind <= argc ? optind : argc);
  Manifest *m = ctx->manifest;

  printf("Manifest branch: %s\n",
         m->def.revision[0] ? m->def.revision : "refs/heads/master");
  printf("Manifest merge branch: %s\n",
         "refs/heads/master"); /* Simplification for now */
  printf("Manifest server: %s\n",
         m->def.remote_name[0] ? m->def.remote_name : "origin");
  color_printf(COLOR_HEADER, "----------------------------\n");

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

    color_printf(COLOR_HEADER, "Project: %s\n", p->name);
    printf("Mount path: %s\n", p->path);
    char *curr_branch = git_current_branch(worktree);
    printf("Current revision: %s\n", curr_branch ? curr_branch : "(detached)");
    if (curr_branch)
      free(curr_branch);

    /* Just print local branches for now as basic info */
    if (overview) {
      printf("Local Branches: \n");
      const char *args[] = {"branch", NULL};
      char *output = NULL;
      if (git_run(worktree, args, &output, NULL) == 0 && output) {
        printf("%s", output);
      }
      if (output)
        free(output);
    }
    printf("----------------------------\n");

    (void)local_only; /* Unused for basic version */
  }

  return 0;
}
