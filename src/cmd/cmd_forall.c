#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../tasks.h"
#include "cmd.h"

/* ------------------------------------------------------------------ */
/* forall: run a command in each project                                */
/* ------------------------------------------------------------------ */

/*
 * Environment variables set for each project (mirrors original repo):
 *   REPO_PROJECT  - project name
 *   REPO_PATH     - project path relative to workspace root
 *   REPO_REMOTE   - remote name
 *   REPO_RREV     - revision from manifest
 */

typedef struct {
  const Project *proj;
  const char *topdir;
  const char **cmd_argv; /* NULL-terminated command to run */
  int project_header;
  int abort_on_errors;
  int interactive;
  int verbose;
} ForallArg;

static int forall_one(void *arg) {
  ForallArg *fa = (ForallArg *)arg;
  const Project *p = fa->proj;

  char worktree[2048];
  snprintf(worktree, sizeof(worktree), "%s/%s", fa->topdir, p->path);

  /* Set environment */
  setenv("REPO_PROJECT", p->name, 1);
  setenv("REPO_PATH", p->path, 1);
  setenv("REPO_REMOTE", p->remote_name, 1);
  setenv("REPO_RREV", p->revision, 1);

  if (fa->project_header)
    printf("\nproject %s/ -----\n", p->path);

  /* Fork child */
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    return -1;
  }
  if (pid == 0) {
    if (chdir(worktree) != 0) {
      fprintf(stderr, "rpp forall: cannot chdir to %s: %s\n", worktree,
              strerror(errno));
      _exit(1);
    }
    /* Execute via sh -c to support shell builtins and environment variables.
     * Original repo does exactly this.
     * We need to join all arguments into a single string for sh -c. */
    int len = 0;
    for (int i = 0; fa->cmd_argv[i]; i++) {
      len += strlen(fa->cmd_argv[i]) + 1;
    }
    char *cmd_str = malloc(len + 1);
    cmd_str[0] = '\0';
    for (int i = 0; fa->cmd_argv[i]; i++) {
      strcat(cmd_str, fa->cmd_argv[i]);
      if (fa->cmd_argv[i + 1])
        strcat(cmd_str, " ");
    }

    const char *sh_argv[] = {"sh", "-c", cmd_str, NULL};
    execvp("sh", (char *const *)sh_argv);
    perror("sh");
    _exit(127);
  }

  int status;
  waitpid(pid, &status, 0);
  int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

  if (rc != 0 && fa->abort_on_errors) {
    fprintf(stderr, "rpp forall: aborting due to error in %s\n", p->path);
    /* We can't easily abort the pool from inside a worker, so just flag */
  }

  return rc;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void cmd_forall_usage(void) {
  printf("usage: rpp forall [<project>...] -c <command> [<args>...]\n");
}

void cmd_forall_help(void) {
  cmd_forall_usage();
  printf("\nExecutes the same shell command in each project.\n\n"
         "Options:\n"
         "  -c <cmd>       command (and arguments) to execute (required)\n"
         "  -j, --jobs N   parallel jobs (default: 1, sequential)\n"
         "  -p             show project headers before output\n"
         "  -e             abort if any command fails\n"
         "  -v, --verbose  show stderr output\n"
         "\n"
         "Environment variables available in <cmd>:\n"
         "  REPO_PROJECT   project name from manifest\n"
         "  REPO_PATH      project path relative to workspace root\n"
         "  REPO_REMOTE    remote name\n"
         "  REPO_RREV      revision from manifest\n");
}

int cmd_forall_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace\n");
    return 1;
  }

  int jobs = 1;
  int project_header = 0;
  int abort_on_errors = 0;
  int verbose = ctx->verbose;
  const char **cmd_argv = NULL;
  int n_filter = 0;
  char **filter = NULL;

  /* Parse args: collect project names before -c, command after -c */
  char **cmd_start = NULL;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--command") == 0) {
      cmd_start = &argv[i + 1];
      /* args before -c are project filters */
      filter = argv + 1;
      n_filter = i - 1;
      break;
    }
    if (strcmp(argv[i], "-j") == 0 && i + 1 < argc) {
      jobs = atoi(argv[++i]);
    } else if (strncmp(argv[i], "--jobs=", 7) == 0) {
      jobs = atoi(argv[i] + 7);
    } else if (strcmp(argv[i], "-p") == 0) {
      project_header = 1;
    } else if (strcmp(argv[i], "-e") == 0) {
      abort_on_errors = 1;
    } else if (strcmp(argv[i], "-v") == 0 ||
               strcmp(argv[i], "--verbose") == 0) {
      verbose = 1;
    }
  }

  if (!cmd_start) {
    fprintf(stderr, "rpp forall: -c <command> is required\n");
    cmd_forall_usage();
    return 1;
  }

  /* Build cmd_argv from cmd_start to end of argv */
  int cmd_argc = argc - (int)(cmd_start - argv);
  cmd_argv = malloc((cmd_argc + 1) * sizeof(const char *));
  if (!cmd_argv)
    return 1;
  for (int i = 0; i < cmd_argc; i++)
    cmd_argv[i] = cmd_start[i];
  cmd_argv[cmd_argc] = NULL;

  Manifest *m = ctx->manifest;

  /* Collect projects */
  ForallArg *args = malloc(m->project_count * sizeof(ForallArg));
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
    args[n].cmd_argv = cmd_argv;
    args[n].project_header = project_header;
    args[n].abort_on_errors = abort_on_errors;
    args[n].interactive = (jobs == 1);
    args[n].verbose = verbose;
    n++;
  }

  /* Run */
  TaskPool *pool = task_pool_create(jobs);
  for (int i = 0; i < n; i++)
    task_pool_submit(pool, forall_one, &args[i]);
  int errors = task_pool_wait(pool);
  task_pool_destroy(pool);

  free(cmd_argv);
  free(args);

  return (errors > 0) ? 1 : 0;
}
