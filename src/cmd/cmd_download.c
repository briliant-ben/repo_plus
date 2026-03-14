#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../git_ops.h"
#include "cmd.h"

void cmd_download_usage(void) {
  printf("usage: rpp download <project> <change>[/<patchset>]\n");
}

void cmd_download_help(void) {
  cmd_download_usage();
  printf("\nDownload a specific change from Gerrit and apply it to the local "
         "workspace.\n");
}

int cmd_download_run(CmdContext *ctx, int argc, char **argv) {
  if (!ctx->manifest) {
    fprintf(stderr, "error: not in a rpp workspace\n");
    return 1;
  }

  if (argc < 3) {
    cmd_download_usage();
    return 1;
  }

  const char *proj_name = argv[1];
  const char *change_arg = argv[2];

  Manifest *m = ctx->manifest;
  Project *target_proj = NULL;

  for (int i = 0; i < m->project_count; i++) {
    if (strcmp(m->projects[i].name, proj_name) == 0 ||
        strcmp(m->projects[i].path, proj_name) == 0) {
      target_proj = &m->projects[i];
      break;
    }
  }

  if (!target_proj) {
    fprintf(stderr, "repo download: project '%s' not found\n", proj_name);
    return 1;
  }

  /* Parse change and patchset */
  char change_str[64];
  strncpy(change_str, change_arg, sizeof(change_str) - 1);
  change_str[sizeof(change_str) - 1] = '\0';

  char *patchset = strchr(change_str, '/');
  if (patchset) {
    *patchset = '\0';
    patchset++;
  } else {
    patchset = "1"; /* default patchset */
  }

  int change_num = atoi(change_str);
  if (change_num <= 0) {
    fprintf(stderr, "repo download: invalid change number '%s'\n", change_str);
    return 1;
  }

  char worktree[2048];
  snprintf(worktree, sizeof(worktree), "%s/%s", ctx->topdir, target_proj->path);

  char *remote_cmd_args[] = {"config", "--get", "remote.origin.url", NULL};
  char *remote_url = NULL;
  git_run(worktree, (const char **)remote_cmd_args, &remote_url, NULL);
  /* Use origin as remote by default */
  const char *remote_name = "origin";

  /* Construct the refspec */
  char refspec[256];
  snprintf(refspec, sizeof(refspec), "refs/changes/%02d/%d/%s",
           change_num % 100, change_num, patchset);

  printf("Downloading %s to project %s/\n", refspec, target_proj->path);

  /* Fetch the change */
  const char *fetch_args[] = {"fetch", remote_name, refspec, NULL};
  int fetch_rc = git_run(worktree, fetch_args, NULL, NULL);

  if (fetch_rc != 0) {
    fprintf(stderr,
            "repo download: failed to fetch %s from %s for project %s\n",
            refspec, remote_name, target_proj->path);
    free(remote_url);
    return 1;
  }

  /* Cherry-pick FETCH_HEAD */
  printf("Applying %s via cherry-pick...\n", refspec);
  const char *cp_args[] = {"cherry-pick", "FETCH_HEAD", NULL};
  int cp_rc = git_run(worktree, cp_args, NULL, NULL);

  if (cp_rc != 0) {
    fprintf(stderr, "repo download: failed to cherry-pick FETCH_HEAD. Please "
                    "resolve conflicts.\n");
    free(remote_url);
    return 1;
  }

  printf("Successfully downloaded %s to %s\n", change_arg, target_proj->path);
  free(remote_url);
  return 0;
}
