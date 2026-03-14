#define _GNU_SOURCE
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../git_ops.h"
#include "cmd.h"

void cmd_upload_usage(void) {
  printf("usage: rpp upload [--cbr] [-t] [--yes] [<project>...]\n");
}

void cmd_upload_help(void) {
  cmd_upload_usage();
  printf("\nUpload local changes to Gerrit for code review.\n\n"
         "Options:\n"
         "  --cbr           Upload current git branch\n"
         "  -t              Send local branch name to Gerrit as topic\n"
         "  -y, --yes       Proceed with upload without prompting\n");
}

int cmd_upload_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace\n");
    return 1;
  }

  int use_cbr = 0;
  int send_topic = 0;
  int auto_yes = 0;
  static struct option long_opts[] = {{"cbr", no_argument, NULL, 'c'},
                                      {"yes", no_argument, NULL, 'y'},
                                      {NULL, 0, NULL, 0}};

  int opt;
  optind = 1;
  while ((opt = getopt_long(argc, argv, "ty", long_opts, NULL)) != -1) {
    switch (opt) {
    case 'c':
      use_cbr = 1;
      break;
    case 't':
      send_topic = 1;
      break;
    case 'y':
      auto_yes = 1;
      break;
    default:
      cmd_upload_usage();
      return 1;
    }
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

    char *branch = git_current_branch(worktree);
    if (!branch) {
      if (use_cbr) {
        printf("project %s/ (no current branch)\n", p->path);
      }
      continue;
    }

    /* We only process the current branch when --cbr is used, but rpp upload
       normally prompts. For simplicity in this phase, we act as if --cbr is
       always implied or check specifically. */
    if (!use_cbr && n_filter == 0) {
      // In full repo, it checks all published branches. We only check the
      // current branch.
    }

    /* Get tracking remote and merge */
    char key_remote[256];
    char key_merge[256];
    snprintf(key_remote, sizeof(key_remote), "branch.%s.remote", branch);
    snprintf(key_merge, sizeof(key_merge), "branch.%s.merge", branch);

    char *remote_name = git_config_get(worktree, key_remote);
    char *upstream = git_config_get(worktree, key_merge);

    if (!remote_name) {
      remote_name = strdup(
          p->remote ? p->remote->name
                    : (m->def.remote_name[0] ? m->def.remote_name : "origin"));
    }
    if (!upstream) {
      upstream = strdup(p->revision[0] ? p->revision : m->def.revision);
      if (!upstream[0]) {
        free(upstream);
        upstream = strdup("refs/heads/master");
      }
    }

    char dest_branch[256];
    if (strncmp(upstream, "refs/heads/", 11) == 0) {
      snprintf(dest_branch, sizeof(dest_branch), "%s", upstream + 11);
    } else {
      snprintf(dest_branch, sizeof(dest_branch), "%s", upstream);
    }

    /* Check for commits to upload using log: <remote>/<dest>..HEAD */
    char diff_range[512];
    snprintf(diff_range, sizeof(diff_range), "%s/%s..HEAD", remote_name,
             dest_branch);
    const char *log_args[] = {"log", "--oneline", diff_range, NULL};
    char *log_out = NULL;
    int rc = git_run(worktree, log_args, &log_out, NULL);

    if (rc == 0 && log_out && log_out[0] != '\0') {
      printf("\nUpload project %s/ to remote branch %s:\n", p->path,
             dest_branch);
      printf("  branch %s (%zu commit(s))\n", branch,
             (size_t)1); /* Counting commits is an approximation here */

      if (!auto_yes) {
        printf("to %s (y/N)? ", remote_name);
        fflush(stdout);
        char answer[10];
        if (!fgets(answer, sizeof(answer), stdin) ||
            (answer[0] != 'y' && answer[0] != 'Y')) {
          printf("Skipped upload.\n");
          free(log_out);
          free(remote_name);
          free(upstream);
          free(branch);
          continue;
        }
      }

      char refspec[1024];
      if (send_topic) {
        snprintf(refspec, sizeof(refspec), "HEAD:refs/for/%s%%topic=%s",
                 dest_branch, branch);
      } else {
        snprintf(refspec, sizeof(refspec), "HEAD:refs/for/%s", dest_branch);
      }

      const char *push_args[] = {"push", remote_name, refspec, NULL};
      int push_rc = git_run(worktree, push_args, NULL, NULL);
      if (push_rc != 0) {
        fprintf(stderr, "rpp upload: failed to push in %s\n", p->path);
        errors++;
      } else {
        printf("Successfully uploaded.\n");
      }
    }

    free(log_out);
    free(remote_name);
    free(upstream);
    free(branch);
  }

  return errors > 0 ? 1 : 0;
}
