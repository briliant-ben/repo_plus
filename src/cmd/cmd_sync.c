#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../git_ops.h"
#include "../tasks.h"
#include "cmd.h"

/* ------------------------------------------------------------------ */
/* Per-project sync work                                                */
/* ------------------------------------------------------------------ */

typedef struct {
  const Project *proj;
  const char *topdir;
  const char *repodir;
  int depth;
  int quiet;
  int force_sync;
  int detach;
  int current_branch;
  int prune;
} SyncArg;

static int mkdir_p_sync(const char *path) {
  char tmp[1024];
  strncpy(tmp, path, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = '\0';

  for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      mkdir(tmp, 0755);
      *p = '/';
    }
  }
  return mkdir(tmp, 0755);
}

static int sync_one(void *arg) {
  SyncArg *sa = (SyncArg *)arg;
  const Project *p = sa->proj;
  int rc = 0;

  /* Determine worktree path */
  char worktree[2048];
  snprintf(worktree, sizeof(worktree), "%s/%s", sa->topdir, p->path);

  /* Determine git dir inside .repo/projects/ */
  char gitdir[1024];
  {
    /* Replace '/' with '_' in project name for the storage dir */
    char proj_escaped[512];
    strncpy(proj_escaped, p->name, sizeof(proj_escaped) - 1);
    proj_escaped[sizeof(proj_escaped) - 1] = '\0';
    for (char *c = proj_escaped; *c; c++)
      if (*c == '/')
        *c = '_';
    snprintf(gitdir, sizeof(gitdir), "%s/projects/%s.git", sa->repodir,
             proj_escaped);
  }

  struct stat st;
  int is_new = (stat(worktree, &st) != 0);

  if (is_new) {
    /* Object-store sharing: clone to bare repo first */
    struct stat bare_st;
    if (stat(gitdir, &bare_st) != 0) {
      if (!sa->quiet)
        printf("Initializing project objects for %s ...\n", p->name);
      
      /* Make sure parent dir of gitdir exists */
      char parent_gitdir[1024];
      strncpy(parent_gitdir, gitdir, sizeof(parent_gitdir) - 1);
      parent_gitdir[sizeof(parent_gitdir) - 1] = '\0';
      char *last_slash = strrchr(parent_gitdir, '/');
      if (last_slash) {
        *last_slash = '\0';
        mkdir_p_sync(parent_gitdir);
      }

      GitCloneOpts bare_opts = {
          .depth = sa->depth,
          .bare = 1,
          .quiet = sa->quiet,
      };
      git_clone(p->fetch_url, gitdir, &bare_opts);
    }

    /* First time: create parent dirs and clone */
    mkdir_p_sync(worktree);

    if (!sa->quiet)
      printf("Cloning %s ...\n", p->path);

    GitCloneOpts opts = {
        .depth = sa->depth,
        .branch = p->revision[0] ? p->revision : NULL,
        .quiet = sa->quiet,
        .reference = gitdir,
    };
    rc = git_clone(p->fetch_url, worktree, &opts);
    if (rc != 0) {
      fprintf(stderr, "rpp sync: failed to clone %s from %s\n", p->path,
              p->fetch_url);
    }
  } else {
    /* Existing checkout: fetch + checkout */
    if (!sa->quiet)
      printf("Fetching %s ...\n", p->path);

    rc = git_fetch(worktree, "origin", sa->current_branch && p->revision[0] ? p->revision : NULL, sa->prune, sa->depth);
    if (rc != 0) {
      fprintf(stderr, "rpp sync: fetch failed for %s\n", p->path);
      return rc;
    }

    /* Determine target revision */
    const char *rev = p->revision[0] ? p->revision : "origin/HEAD";

    if (sa->detach) {
      /* Detach HEAD to manifest revision */
      char origin_rev[512];
      snprintf(origin_rev, sizeof(origin_rev), "origin/%s", rev);

      /* Try origin/<rev> first, then bare rev */
      if (git_checkout(worktree, origin_rev) != 0)
        rc = git_checkout(worktree, rev);
    } else {
      /* Merge/rebase local branch onto new remote commits */
      char *branch = git_current_branch(worktree);
      if (branch) {
        /* Try: git merge --ff-only origin/<revision> */
        char origin_rev[512];
        snprintf(origin_rev, sizeof(origin_rev), "origin/%s", rev);
        const char *args[] = {"merge", "--ff-only", origin_rev, NULL};
        rc = git_run(worktree, args, NULL, NULL);
        if (rc != 0) {
          fprintf(stderr,
                  "rpp sync: cannot fast-forward %s, try 'rpp sync -d'\n",
                  p->path);
        }
        free(branch);
      } else {
        /* Detached HEAD – just checkout */
        char origin_rev[512];
        snprintf(origin_rev, sizeof(origin_rev), "origin/%s", rev);
        if (git_checkout(worktree, origin_rev) != 0)
          rc = git_checkout(worktree, rev);
      }
    }
  }

  (void)gitdir; /* reserved for future object-store sharing */
  return rc;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void cmd_sync_usage(void) {
  printf("usage: rpp sync [-j N] [-d] [-q] [-c] [--prune] [<project>...]\n");
}

void cmd_sync_help(void) {
  cmd_sync_usage();
  printf("\nFetch and update all (or specified) projects.\n\n"
         "Options:\n"
         "  -j, --jobs N       parallel fetch jobs (default: 4)\n"
         "  -d, --detach       detach projects to manifest revision\n"
         "  -q, --quiet        suppress output\n"
         "  -f, --force-sync   overwrite existing checkouts\n"
         "  -c, --current-branch fetch only current manifest branch\n"
         "  --prune            delete refs that no longer exist on the remote\n"
         "  --depth N          shallow clone depth\n"
         "\n"
         "Projects can be specified by name or path.\n");
}

int cmd_sync_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace (run 'rpp init' first)\n");
    return 1;
  }

  int jobs = 4;
  int detach = 0;
  int quiet = ctx->quiet;
  int force_sync = 0;
  int depth = 0;

  int current_branch = 0;
  int prune = 0;

  static struct option long_opts[] = {{"jobs", required_argument, NULL, 'j'},
                                      {"detach", no_argument, NULL, 'd'},
                                      {"quiet", no_argument, NULL, 'q'},
                                      {"force-sync", no_argument, NULL, 'f'},
                                      {"current-branch", no_argument, NULL, 'c'},
                                      {"prune", no_argument, NULL, 'p'},
                                      {"depth", required_argument, NULL, 'D'},
                                      {NULL, 0, NULL, 0}};

  int opt;
  optind = 1;
  while ((opt = getopt_long(argc, argv, "j:dqfcD:p", long_opts, NULL)) != -1) {
    switch (opt) {
    case 'j':
      jobs = atoi(optarg);
      break;
    case 'd':
      detach = 1;
      break;
    case 'q':
      quiet = 1;
      break;
    case 'f':
      force_sync = 1;
      break;
    case 'c':
      current_branch = 1;
      break;
    case 'p':
      prune = 1;
      break;
    case 'D':
      depth = atoi(optarg);
      break;
    default:
      cmd_sync_usage();
      return 1;
    }
  }

  Manifest *m = ctx->manifest;
  /* getopt may leave optind > argc when no arguments given */
  int n_filter = (optind <= argc) ? (argc - optind) : 0;
  char **filter = argv + (optind <= argc ? optind : argc);

  /* Build the list of projects to sync */
  Project **to_sync = malloc(m->project_count * sizeof(Project *));
  int n_sync = 0;
  if (!to_sync)
    return 1;

  for (int i = 0; i < m->project_count; i++) {
    Project *p = &m->projects[i];
    if (p->fetch_url[0] == '\0')
      continue; /* skip unresolved */

    if (n_filter == 0) {
      to_sync[n_sync++] = p;
    } else {
      for (int j = 0; j < n_filter; j++) {
        if (strcmp(p->name, filter[j]) == 0 ||
            strcmp(p->path, filter[j]) == 0) {
          to_sync[n_sync++] = p;
          break;
        }
      }
    }
  }

  if (!quiet)
    printf("Syncing %d project(s) with %d job(s) ...\n", n_sync, jobs);

  /* Build SyncArg array */
  SyncArg *args = malloc(n_sync * sizeof(SyncArg));
  if (!args) {
    free(to_sync);
    return 1;
  }

  for (int i = 0; i < n_sync; i++) {
    args[i].proj = to_sync[i];
    args[i].topdir = ctx->topdir;
    args[i].repodir = ctx->repodir;
    args[i].depth = depth;
    args[i].quiet = quiet;
    args[i].force_sync = force_sync;
    args[i].detach = detach;
    args[i].current_branch = current_branch;
    args[i].prune = prune;
  }

  /* Run in parallel */
  TaskPool *pool = task_pool_create(jobs);
  if (!pool) {
    free(args);
    free(to_sync);
    return 1;
  }

  for (int i = 0; i < n_sync; i++)
    task_pool_submit(pool, sync_one, &args[i]);

  int errors = task_pool_wait(pool);
  task_pool_destroy(pool);

  free(args);
  free(to_sync);

  if (errors > 0) {
    fprintf(stderr, "rpp sync: %d project(s) failed to sync\n", errors);
    return 1;
  }

  if (!quiet)
    printf("Sync complete.\n");

  return 0;
}
