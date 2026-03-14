#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../color.h"
#include "../git_ops.h"
#include "cmd.h"

/* ------------------------------------------------------------------ */
/* list                                                                 */
/* ------------------------------------------------------------------ */

void cmd_list_usage(void) {
  printf("usage: rpp list [-n] [-p] [-f] [<project>...]\n");
}

void cmd_list_help(void) {
  cmd_list_usage();
  printf("\nList all projects (or specified projects) in the manifest.\n\n"
         "Options:\n"
         "  -n, --name-only    show only project names\n"
         "  -p, --path-only    show only project paths\n"
         "  -f, --full         show full project info (name + path + remote)\n"
         "\n"
         "Default output format:  <path> : <name>\n");
}

int cmd_list_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace\n");
    return 1;
  }

  int name_only = 0, path_only = 0, full = 0;

  static struct option long_opts[] = {{"name-only", no_argument, NULL, 'n'},
                                      {"path-only", no_argument, NULL, 'p'},
                                      {"full", no_argument, NULL, 'f'},
                                      {NULL, 0, NULL, 0}};

  int opt;
  optind = 1;
  while ((opt = getopt_long(argc, argv, "npf", long_opts, NULL)) != -1) {
    switch (opt) {
    case 'n':
      name_only = 1;
      break;
    case 'p':
      path_only = 1;
      break;
    case 'f':
      full = 1;
      break;
    default:
      cmd_list_usage();
      return 1;
    }
  }

  int n_filter = (optind <= argc) ? (argc - optind) : 0;
  char **filter = argv + (optind <= argc ? optind : argc);
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

    if (name_only)
      printf("%s\n", p->name);
    else if (path_only)
      printf("%s\n", p->path);
    else if (full)
      printf("%-40s %-40s %s\n", p->path, p->name, p->remote_name);
    else
      printf("%s : %s\n", p->path, p->name);
  }

  return 0;
}

/* ------------------------------------------------------------------ */
/* start                                                                */
/* ------------------------------------------------------------------ */

void cmd_start_usage(void) {
  printf("usage: rpp start <branch> [--all | <project>...]\n");
}

void cmd_start_help(void) {
  cmd_start_usage();
  printf("\nCreate a new branch in each specified project.\n\n"
         "Options:\n"
         "  --all          apply to all projects in the manifest\n"
         "  <project>...   apply only to the specified projects\n");
}

int cmd_start_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace\n");
    return 1;
  }

  int all_projects = 0;
  optind = 1;

  /* First arg must be branch name */
  if (argc < 2) {
    fprintf(stderr, "rpp start: branch name required\n");
    cmd_start_usage();
    return 1;
  }

  const char *branch = argv[1];
  int errors = 0;
  Manifest *m = ctx->manifest;

  /* Remaining args are project filters */
  int n_filter = argc - 2;
  char **filter = argv + 2;

  if (n_filter > 0 && strcmp(filter[0], "--all") == 0) {
    all_projects = 1;
    n_filter = 0;
  }

  for (int i = 0; i < m->project_count; i++) {
    Project *p = &m->projects[i];

    if (!all_projects && n_filter > 0) {
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
    } else if (!all_projects && n_filter == 0) {
      /* No filter and not --all: apply to current project only */
      /* For simplicity, apply to all if no args given */
      /* (Original repo requires --all or explicit projects) */
    }

    char worktree[2048];
    snprintf(worktree, sizeof(worktree), "%s/%s", ctx->topdir, p->path);

    int rc = git_branch_create(worktree, branch,
                               p->revision[0] ? p->revision : NULL, 1);
    if (rc != 0) {
      fprintf(stderr, "rpp start: failed to create branch '%s' in %s\n",
              branch, p->path);
      errors++;
    } else {
      char key_remote[256];
      char key_merge[256];
      snprintf(key_remote, sizeof(key_remote), "branch.%s.remote", branch);
      snprintf(key_merge, sizeof(key_merge), "branch.%s.merge", branch);

      const char *remote = p->remote_name[0] ? p->remote_name : "origin";
      git_config_set(worktree, key_remote, remote);

      const char *rev = p->revision[0] ? p->revision : m->def.revision;
      if (!rev[0])
        rev = "refs/heads/master";

      char ref_rev[512];
      if (strncmp(rev, "refs/", 5) == 0) {
        snprintf(ref_rev, sizeof(ref_rev), "%s", rev);
      } else {
        snprintf(ref_rev, sizeof(ref_rev), "refs/heads/%s", rev);
      }
      git_config_set(worktree, key_merge, ref_rev);

      printf("project %s/ : switched to branch %s\n", p->path, branch);
    }
  }

  return errors > 0 ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* diff                                                                 */
/* ------------------------------------------------------------------ */

void cmd_diff_usage(void) { printf("usage: rpp diff [<project>...]\n"); }

void cmd_diff_help(void) {
  cmd_diff_usage();
  printf("\nShow changes in the working tree (git diff) for each project.\n");
}

int cmd_diff_run(CmdContext *ctx, int argc, char **argv) {
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

    int use_color = (ctx->color == 1);
    char *diff = git_diff(worktree, 0, use_color);
    if (diff && diff[0] != '\0') {
      color_printf(COLOR_HEADER, "\nproject %s/\n", p->path);
      printf("%s", diff);
    }
    free(diff);
  }

  return 0;
}

/* ------------------------------------------------------------------ */
/* abandon                                                              */
/* ------------------------------------------------------------------ */

void cmd_abandon_usage(void) {
  printf("usage: rpp abandon <branch> [<project>...]\n");
}

void cmd_abandon_help(void) {
  cmd_abandon_usage();
  printf("\nDelete a branch in each project permanently.\n");
}

int cmd_abandon_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace\n");
    return 1;
  }

  if (argc < 2) {
    fprintf(stderr, "rpp abandon: branch name required\n");
    cmd_abandon_usage();
    return 1;
  }

  const char *branch = argv[1];
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

    int rc = git_branch_delete(worktree, branch, 1 /* force */);
    if (rc == 0)
      printf("Abandoned branch '%s' in %s\n", branch, p->path);
    else
      errors++;
  }

  return errors > 0 ? 1 : 0;
}
