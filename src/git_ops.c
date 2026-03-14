#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "git_ops.h"

/* ------------------------------------------------------------------ */
/* Internal helpers                                                     */
/* ------------------------------------------------------------------ */

#define MAX_GIT_ARGS 64

/* Build a NULL-terminated argv starting with "git" */
static void build_argv(const char *const *args, const char **argv, int *argc) {
  int i = 0;
  argv[i++] = "git";
  for (int j = 0; args[j] && i < MAX_GIT_ARGS - 1; j++)
    argv[i++] = args[j];
  argv[i] = NULL;
  *argc = i;
}

/*
 * Fork-and-exec git.
 * cwd      : working directory (NULL = inherit)
 * args     : NULL-terminated list (without "git" at front)
 * output   : if non-NULL, capture stdout; caller must free
 * Returns process exit code (0 = success).
 */
int git_run(const char *cwd, const char *const *args, char **output,
            size_t *output_len) {
  const char *argv[MAX_GIT_ARGS];
  int argc;
  build_argv(args, argv, &argc);

  int pipefd[2] = {-1, -1};
  if (output) {
    if (pipe(pipefd) != 0) {
      perror("git_run: pipe");
      return -1;
    }
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("git_run: fork");
    if (output) {
      close(pipefd[0]);
      close(pipefd[1]);
    }
    return -1;
  }

  if (pid == 0) {
    /* Child */
    if (cwd && chdir(cwd) != 0) {
      perror(cwd);
      _exit(127);
    }
    if (output) {
      dup2(pipefd[1], STDOUT_FILENO);
      close(pipefd[0]);
      close(pipefd[1]);
    }
    execvp("git", (char *const *)argv);
    perror("git");
    _exit(127);
  }

  /* Parent */
  char *buf = NULL;
  size_t buf_len = 0;

  if (output) {
    close(pipefd[1]);
    /* Read all output */
    char tmp[4096];
    ssize_t n;
    while ((n = read(pipefd[0], tmp, sizeof(tmp))) > 0) {
      char *nb = realloc(buf, buf_len + n + 1);
      if (!nb) {
        free(buf);
        buf = NULL;
        buf_len = 0;
        break;
      }
      buf = nb;
      memcpy(buf + buf_len, tmp, n);
      buf_len += n;
      buf[buf_len] = '\0';
    }
    close(pipefd[0]);
  }

  int status;
  waitpid(pid, &status, 0);
  int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

  if (output) {
    *output = buf;
    if (output_len)
      *output_len = buf_len;
  }
  return rc;
}

/* ------------------------------------------------------------------ */
/* Clone                                                                */
/* ------------------------------------------------------------------ */

int git_clone(const char *url, const char *dest_path,
              const GitCloneOpts *opts) {
  const char *args[32];
  int i = 0;
  char depth_str[32];

  args[i++] = "clone";
  if (opts) {
    if (opts->quiet)
      args[i++] = "-q";
    if (opts->mirror)
      args[i++] = "--mirror";
    else if (opts->bare)
      args[i++] = "--bare";
    if (opts->branch) {
      args[i++] = "-b";
      args[i++] = opts->branch;
    }
    if (opts->depth > 0) {
      snprintf(depth_str, sizeof(depth_str), "--depth=%d", opts->depth);
      args[i++] = depth_str;
    }
    if (opts->reference) {
      args[i++] = "--reference";
      args[i++] = opts->reference;
    }
  }
  args[i++] = url;
  args[i++] = dest_path;
  args[i] = NULL;

  return git_run(NULL, args, NULL, NULL);
}

/* ------------------------------------------------------------------ */
/* Fetch                                                                */
/* ------------------------------------------------------------------ */

int git_fetch(const char *worktree, const char *remote_name,
              const char *refspec, int prune, int depth) {
  const char *args[16];
  int i = 0;
  char depth_str[32];

  args[i++] = "fetch";
  if (prune)
    args[i++] = "--prune";
  if (depth > 0) {
    snprintf(depth_str, sizeof(depth_str), "--depth=%d", depth);
    args[i++] = depth_str;
  }
  args[i++] = remote_name;
  if (refspec)
    args[i++] = refspec;
  args[i] = NULL;

  return git_run(worktree, args, NULL, NULL);
}

/* ------------------------------------------------------------------ */
/* Checkout                                                             */
/* ------------------------------------------------------------------ */

int git_checkout(const char *worktree, const char *revision) {
  const char *args[] = {"checkout", revision, NULL};
  return git_run(worktree, args, NULL, NULL);
}

/* ------------------------------------------------------------------ */
/* Rev-parse                                                            */
/* ------------------------------------------------------------------ */

int git_rev_parse(const char *worktree, const char *rev, char *out,
                  size_t out_sz) {
  const char *args[] = {"rev-parse", rev, NULL};
  char *buf = NULL;
  int rc = git_run(worktree, args, &buf, NULL);
  if (rc == 0 && buf) {
    /* strip trailing newline */
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
      buf[len - 1] = '\0';
    strncpy(out, buf, out_sz - 1);
    out[out_sz - 1] = '\0';
  }
  free(buf);
  return rc;
}

/* ------------------------------------------------------------------ */
/* Current branch                                                       */
/* ------------------------------------------------------------------ */

char *git_current_branch(const char *worktree) {
  const char *args[] = {"symbolic-ref", "--short", "HEAD", NULL};
  char *buf = NULL;
  int rc = git_run(worktree, args, &buf, NULL);
  if (rc != 0) {
    free(buf);
    return NULL;
  }
  if (buf) {
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
      buf[len - 1] = '\0';
  }
  return buf;
}

/* ------------------------------------------------------------------ */
/* Branch create / delete                                               */
/* ------------------------------------------------------------------ */

int git_branch_create(const char *worktree, const char *branch,
                      const char *revision, int do_checkout) {
  if (do_checkout) {
    /* git checkout -b <branch> [revision] */
    const char *args[8];
    int i = 0;
    args[i++] = "checkout";
    args[i++] = "-b";
    args[i++] = branch;
    if (revision)
      args[i++] = revision;
    args[i] = NULL;
    return git_run(worktree, args, NULL, NULL);
  } else {
    /* git branch <branch> [revision] */
    const char *args[8];
    int i = 0;
    args[i++] = "branch";
    args[i++] = branch;
    if (revision)
      args[i++] = revision;
    args[i] = NULL;
    return git_run(worktree, args, NULL, NULL);
  }
}

int git_branch_delete(const char *worktree, const char *branch, int force) {
  const char *args[] = {"branch", force ? "-D" : "-d", branch, NULL};
  return git_run(worktree, args, NULL, NULL);
}

/* ------------------------------------------------------------------ */
/* Status                                                               */
/* ------------------------------------------------------------------ */

char *git_status_porcelain(const char *worktree) {
  const char *args[] = {"status", "--porcelain", NULL};
  char *buf = NULL;
  int rc = git_run(worktree, args, &buf, NULL);
  if (rc != 0) {
    free(buf);
    return NULL;
  }
  return buf;
}

/* ------------------------------------------------------------------ */
/* Diff                                                                 */
/* ------------------------------------------------------------------ */

char *git_diff(const char *worktree, int stat_only, int use_color) {
  const char *args[8];
  int i = 0;
  args[i++] = "diff";
  if (stat_only)
    args[i++] = "--stat";
  if (use_color)
    args[i++] = "--color=always";
  args[i] = NULL;

  char *buf = NULL;
  git_run(worktree, args, &buf, NULL);
  return buf;
}

/* ------------------------------------------------------------------ */
/* Init bare                                                            */
/* ------------------------------------------------------------------ */

int git_init_bare(const char *path) {
  const char *args[] = {"init", "--bare", path, NULL};
  return git_run(NULL, args, NULL, NULL);
}

/* ------------------------------------------------------------------ */
/* Config                                                               */
/* ------------------------------------------------------------------ */

int git_config_set(const char *repo_path, const char *key, const char *value) {
  const char *args[] = {"config", key, value, NULL};
  return git_run(repo_path, args, NULL, NULL);
}

char *git_config_get(const char *repo_path, const char *key) {
  const char *args[] = {"config", "--get", key, NULL};
  char *buf = NULL;
  int rc = git_run(repo_path, args, &buf, NULL);
  if (rc != 0) {
    free(buf);
    return NULL;
  }
  if (buf) {
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
      buf[len - 1] = '\0';
  }
  return buf;
}
