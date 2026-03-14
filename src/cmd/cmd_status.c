#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../git_ops.h"
#include "../tasks.h"
#include "cmd.h"

/* ------------------------------------------------------------------ */
/* Status output format (mimics original repo)                          */
/*                                                                      */
/* project <path>/        branch <branch>                               */
/*  -m     some/file.c                                                  */
/*                                                                      */
/* First column  (index vs HEAD):   A M D R C T U -                    */
/* Second column (worktree vs idx): m d - (lower case)                 */
/* ------------------------------------------------------------------ */

typedef struct {
  const Project *proj;
  const char *topdir;
  int quiet;
} StatusArg;

/* Parse --porcelain output and print in repo-style format.
 * Returns 1 if the project is clean, 0 if dirty. */
static int print_project_status(const Project *p, const char *topdir,
                                int quiet) {
  char worktree[2048];
  snprintf(worktree, sizeof(worktree), "%s/%s", topdir, p->path);

  char *porcelain = git_status_porcelain(worktree);
  if (!porcelain)
    return 1; /* treat as clean if can't query */

  int is_clean = (porcelain[0] == '\0');

  if (!is_clean) {
    char *branch = git_current_branch(worktree);
    printf("project %s/", p->path);
    if (branch)
      printf("  (branch %s)", branch);
    putchar('\n');
    free(branch);

    /* Print each changed file */
    char *line = porcelain;
    while (*line) {
      char *nl = strchr(line, '\n');
      if (nl)
        *nl = '\0';

      if (strlen(line) >= 3) {
        char idx_status = line[0];  /* X */
        char work_status = line[1]; /* Y */
        const char *filename = line + 3;

        /* Map to repo-style display */
        char col1 = (idx_status == ' ' || idx_status == '?') ? '-' : idx_status;
        char col2;
        switch (work_status) {
        case 'M':
          col2 = 'm';
          break;
        case 'D':
          col2 = 'd';
          break;
        case '?':
          col2 = '-';
          break; /* untracked */
        default:
          col2 = '-';
          break;
        }
        printf(" %c%c     %s\n", col1, col2, filename);
      }

      if (!nl)
        break;
      line = nl + 1;
    }
  } else if (!quiet) {
    /* nothing to do */
  }

  free(porcelain);
  return is_clean ? 1 : 0;
}

static int status_one(void *arg) {
  StatusArg *sa = (StatusArg *)arg;
  print_project_status(sa->proj, sa->topdir, sa->quiet);
  return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void cmd_status_usage(void) {
  printf("usage: rpp status [-j N] [-q] [<project>...]\n");
}

void cmd_status_help(void) {
  cmd_status_usage();
  printf("\nShow the working tree status of each project.\n\n"
         "Options:\n"
         "  -j, --jobs N   parallel status queries (default: cpu count)\n"
         "  -q, --quiet    only show projects with changes\n"
         "\n"
         "Status columns:\n"
         "  First  : index vs HEAD  (A=added M=modified D=deleted R=renamed "
         "-=clean)\n"
         "  Second : work  vs index (m=modified d=deleted -=clean/new)\n");
}

int cmd_status_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace\n");
    return 1;
  }

  int jobs = task_cpu_count();
  int quiet = ctx->quiet;

  static struct option long_opts[] = {{"jobs", required_argument, NULL, 'j'},
                                      {"quiet", no_argument, NULL, 'q'},
                                      {NULL, 0, NULL, 0}};

  int opt;
  optind = 1;
  while ((opt = getopt_long(argc, argv, "j:q", long_opts, NULL)) != -1) {
    switch (opt) {
    case 'j':
      jobs = atoi(optarg);
      break;
    case 'q':
      quiet = 1;
      break;
    default:
      cmd_status_usage();
      return 1;
    }
  }

  Manifest *m = ctx->manifest;
  int n_filter = (optind <= argc) ? (argc - optind) : 0;
  char **filter = argv + (optind <= argc ? optind : argc);

  /* Collect projects */
  StatusArg *args = malloc(m->project_count * sizeof(StatusArg));
  int n = 0;

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
    args[n].proj = p;
    args[n].topdir = ctx->topdir;
    args[n].quiet = quiet;
    n++;
  }

  /* Run in parallel – but for status we use jobs=1 to preserve output order.
   * (Alternatively we could collect output per-project and print sequentially.)
   */
  if (jobs > 1) {
    /* For simplicity, run sequentially to preserve output ordering */
    jobs = 1;
  }

  TaskPool *pool = task_pool_create(jobs);
  for (int i = 0; i < n; i++)
    task_pool_submit(pool, status_one, &args[i]);
  task_pool_wait(pool);
  task_pool_destroy(pool);

  free(args);
  return 0;
}
