#ifndef MANIFEST_H
#define MANIFEST_H

#include <stddef.h>

/* Maximum lengths */
#define MAX_NAME        256
#define MAX_URL         1024
#define MAX_PATH        1024
#define MAX_REVISION    256
#define MAX_GROUPS      512
#define MAX_PROJECTS    4096
#define MAX_REMOTES     64

/* Forward declarations */
typedef struct Remote Remote;
typedef struct Project Project;
typedef struct Manifest Manifest;
typedef struct CopyFile CopyFile;
typedef struct LinkFile LinkFile;

/* Remote element from manifest */
struct Remote {
    char name[MAX_NAME];
    char fetch[MAX_URL];       /* base fetch URL */
    char push_url[MAX_URL];    /* optional push URL */
    char revision[MAX_REVISION]; /* default revision */
    char review[MAX_URL];      /* Gerrit review URL */
    char alias[MAX_NAME];      /* remote alias */
};

/* copyfile / linkfile elements */
struct CopyFile {
    char src[MAX_PATH];
    char dest[MAX_PATH];
    struct CopyFile *next;
};

struct LinkFile {
    char src[MAX_PATH];
    char dest[MAX_PATH];
    struct LinkFile *next;
};

/* Project element from manifest */
struct Project {
    char name[MAX_NAME];          /* project name (e.g. platform/build) */
    char path[MAX_PATH];          /* local checkout path (e.g. build) */
    char revision[MAX_REVISION];  /* branch/tag/sha1 */
    char remote_name[MAX_NAME];   /* which remote to use */
    char groups[MAX_GROUPS];      /* comma-separated groups */
    char upstream[MAX_REVISION];  /* upstream branch for rebase */
    char dest_branch[MAX_REVISION]; /* dest branch for upload */
    int  clone_depth;             /* shallow clone depth, 0=full */
    int  sync_c;                  /* sync only current branch? */
    int  sync_s;                  /* sync submodules? */
    int  sync_tags;               /* sync tags? 1 by default */

    CopyFile *copyfiles;
    LinkFile *linkfiles;

    /* Computed at load time */
    char fetch_url[MAX_URL];      /* full URL = remote->fetch + "/" + name */
    Remote *remote;               /* pointer to resolved remote */
};

/* Default element from manifest */
typedef struct {
    char remote_name[MAX_NAME];
    char revision[MAX_REVISION];
    char dest_branch[MAX_REVISION];
    char upstream[MAX_REVISION];
    int  sync_j;       /* parallel jobs, 0=unset */
    int  sync_j_max;
    int  sync_c;
    int  sync_s;
    int  sync_tags;    /* 1 by default */
} ManifestDefault;

/* Top-level manifest */
struct Manifest {
    char repodir[MAX_PATH];    /* path to .repo dir */
    char topdir[MAX_PATH];     /* workspace root (parent of .repo) */

    Remote  remotes[MAX_REMOTES];
    int     remote_count;

    ManifestDefault def;

    Project *projects;
    int      project_count;
    int      project_cap;
};

/* Public API */
Manifest *manifest_load(const char *repodir);
void      manifest_free(Manifest *m);

/* Resolve project URL from remote */
int manifest_resolve_project_url(Manifest *m, Project *p);

/* Find project by name or path */
Project *manifest_find_project_by_name(Manifest *m, const char *name);
Project *manifest_find_project_by_path(Manifest *m, const char *path);

/* Find .repo directory by searching upward from cwd */
int  manifest_find_repodir(char *out, size_t out_sz);

#endif /* MANIFEST_H */
