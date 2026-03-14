#ifndef GIT_OPS_H
#define GIT_OPS_H

#include <stddef.h>

/*
 * git_ops.h - Wrappers around git commands
 *
 * All functions execute git as a subprocess.  On success they return 0;
 * on failure they return non-zero and print an error to stderr.
 */

/*
 * Run git with the given argv in the given working directory.
 * If output != NULL, stdout is captured and stored in a malloc'd buffer
 * that the caller must free().
 * Returns the exit code of git.
 */
int git_run(const char *cwd, const char *const *args, char **output,
            size_t *output_len);

/*
 * Clone a remote URL into dest_path.
 * opts->depth   : shallow clone depth (0 = full)
 * opts->branch  : which branch to clone (NULL = default)
 * opts->mirror  : 1 to clone as bare mirror
 * opts->quiet   : 1 to suppress git output
 */
typedef struct {
  int depth;
  const char *branch;
  int mirror;
  int bare;
  int quiet;
  const char *reference; /* --reference path */
} GitCloneOpts;

int git_clone(const char *url, const char *dest_path, const GitCloneOpts *opts);

/*
 * Fetch from a remote inside an existing working tree.
 * remote_name : name of the configured remote (e.g. "origin")
 * refspec     : optional refspec, NULL to use configured refspec
 * prune       : 1 to pass --prune
 * depth       : >0 for --depth=N, 0 for full
 */
int git_fetch(const char *worktree, const char *remote_name,
              const char *refspec, int prune, int depth);

/*
 * Checkout (detach HEAD) to the given revision inside worktree.
 */
int git_checkout(const char *worktree, const char *revision);

/*
 * Return the current HEAD commit SHA1 (40 hex chars + NUL).
 * out must be at least 41 bytes.
 */
int git_rev_parse(const char *worktree, const char *rev, char *out,
                  size_t out_sz);

/*
 * Return the current branch name.
 * Returns NULL if detached HEAD.
 * Caller must free the returned string.
 */
char *git_current_branch(const char *worktree);

/*
 * Create a new branch at worktree and optionally switch to it.
 * revision: the starting point (NULL = current HEAD)
 */
int git_branch_create(const char *worktree, const char *branch,
                      const char *revision, int checkout);

/*
 * Delete a branch in worktree.
 * force: pass -D instead of -d
 */
int git_branch_delete(const char *worktree, const char *branch, int force);

/*
 * Run `git status --porcelain` and return the output.
 * Caller must free the returned string.
 */
char *git_status_porcelain(const char *worktree);

/*
 * Run `git diff` (working tree vs. index) and return output.
 * Caller must free.
 */
char *git_diff(const char *worktree, int stat_only);

/*
 * Initialize a bare repository at path.
 */
int git_init_bare(const char *path);

/*
 * Set a git config key=value inside worktree (or bare repo).
 */
int git_config_set(const char *repo_path, const char *key, const char *value);

/*
 * Get a git config value. Caller frees returned string.
 */
char *git_config_get(const char *repo_path, const char *key);

#endif /* GIT_OPS_H */
