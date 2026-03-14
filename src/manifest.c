#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <dirent.h>
#include <errno.h>
#include <expat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "git_ops.h"
#include "manifest.h"

/* ------------------------------------------------------------------ */
/* Internal parse state                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
  Manifest *m;
  int in_manifest;
  /* current project being built */
  Project cur_proj;
  int in_project;
  /* current remote being built */
  Remote cur_remote;
  int in_remote;
  /* error flag */
  int error;
  char error_msg[256];
} ParseState;

/* ------------------------------------------------------------------ */
/* XML element handlers                                                 */
/* ------------------------------------------------------------------ */

static void copy_attr(char *dst, size_t dsz, const char **attrs,
                      const char *key) {
  for (int i = 0; attrs[i]; i += 2) {
    if (strcmp(attrs[i], key) == 0) {
      strncpy(dst, attrs[i + 1], dsz - 1);
      dst[dsz - 1] = '\0';
      return;
    }
  }
  dst[0] = '\0';
}

static const char *find_attr(const char **attrs, const char *key) {
  for (int i = 0; attrs[i]; i += 2) {
    if (strcmp(attrs[i], key) == 0)
      return attrs[i + 1];
  }
  return NULL;
}

static int parse_include(Manifest *m, const char *include_name);

static void on_start(void *data, const char *el, const char **attrs) {
  ParseState *ps = (ParseState *)data;
  Manifest *m = ps->m;

  if (strcmp(el, "manifest") == 0) {
    ps->in_manifest = 1;
    return;
  }

  if (!ps->in_manifest)
    return;

  if (strcmp(el, "include") == 0) {
    const char *name = find_attr(attrs, "name");
    if (name) {
      if (parse_include(m, name) != 0) {
        ps->error = 1;
        snprintf(ps->error_msg, sizeof(ps->error_msg), "failed to include %s",
                 name);
      }
    }
    return;
  }

  if (strcmp(el, "remote") == 0) {
    memset(&ps->cur_remote, 0, sizeof(ps->cur_remote));
    copy_attr(ps->cur_remote.name, sizeof(ps->cur_remote.name), attrs, "name");
    copy_attr(ps->cur_remote.fetch, sizeof(ps->cur_remote.fetch), attrs,
              "fetch");
    copy_attr(ps->cur_remote.push_url, sizeof(ps->cur_remote.push_url), attrs,
              "pushurl");
    copy_attr(ps->cur_remote.revision, sizeof(ps->cur_remote.revision), attrs,
              "revision");
    copy_attr(ps->cur_remote.review, sizeof(ps->cur_remote.review), attrs,
              "review");
    copy_attr(ps->cur_remote.alias, sizeof(ps->cur_remote.alias), attrs,
              "alias");
    ps->in_remote = 1;
    return;
  }

  if (strcmp(el, "default") == 0) {
    copy_attr(m->def.remote_name, sizeof(m->def.remote_name), attrs, "remote");
    copy_attr(m->def.revision, sizeof(m->def.revision), attrs, "revision");
    copy_attr(m->def.dest_branch, sizeof(m->def.dest_branch), attrs,
              "dest-branch");
    copy_attr(m->def.upstream, sizeof(m->def.upstream), attrs, "upstream");
    const char *sj = find_attr(attrs, "sync-j");
    if (sj)
      m->def.sync_j = atoi(sj);
    const char *sjm = find_attr(attrs, "sync-j-max");
    if (sjm)
      m->def.sync_j_max = atoi(sjm);
    const char *sc = find_attr(attrs, "sync-c");
    if (sc)
      m->def.sync_c = (strcmp(sc, "true") == 0);
    const char *ss = find_attr(attrs, "sync-s");
    if (ss)
      m->def.sync_s = (strcmp(ss, "true") == 0);
    const char *st = find_attr(attrs, "sync-tags");
    m->def.sync_tags = (!st || strcmp(st, "false") != 0) ? 1 : 0;
    return;
  }

  if (strcmp(el, "project") == 0) {
    memset(&ps->cur_proj, 0, sizeof(ps->cur_proj));
    ps->cur_proj.sync_tags = 1; /* default: sync tags */

    copy_attr(ps->cur_proj.name, sizeof(ps->cur_proj.name), attrs, "name");
    copy_attr(ps->cur_proj.path, sizeof(ps->cur_proj.path), attrs, "path");
    copy_attr(ps->cur_proj.revision, sizeof(ps->cur_proj.revision), attrs,
              "revision");
    copy_attr(ps->cur_proj.remote_name, sizeof(ps->cur_proj.remote_name), attrs,
              "remote");
    copy_attr(ps->cur_proj.groups, sizeof(ps->cur_proj.groups), attrs,
              "groups");
    copy_attr(ps->cur_proj.upstream, sizeof(ps->cur_proj.upstream), attrs,
              "upstream");
    copy_attr(ps->cur_proj.dest_branch, sizeof(ps->cur_proj.dest_branch), attrs,
              "dest-branch");

    /* Apply defaults */
    if (ps->cur_proj.path[0] == '\0')
      snprintf(ps->cur_proj.path, MAX_PATH, "%s", ps->cur_proj.name);
    if (ps->cur_proj.revision[0] == '\0')
      snprintf(ps->cur_proj.revision, MAX_REVISION, "%s", m->def.revision);
    if (ps->cur_proj.remote_name[0] == '\0')
      snprintf(ps->cur_proj.remote_name, MAX_NAME, "%s", m->def.remote_name);

    const char *sc = find_attr(attrs, "sync-c");
    if (sc)
      ps->cur_proj.sync_c = (strcmp(sc, "true") == 0);
    const char *ss_v = find_attr(attrs, "sync-s");
    if (ss_v)
      ps->cur_proj.sync_s = (strcmp(ss_v, "true") == 0);
    const char *st = find_attr(attrs, "sync-tags");
    if (st)
      ps->cur_proj.sync_tags = (strcmp(st, "false") != 0);
    const char *cd = find_attr(attrs, "clone-depth");
    if (cd)
      ps->cur_proj.clone_depth = atoi(cd);

    ps->in_project = 1;
    return;
  }

  if (strcmp(el, "copyfile") == 0 && ps->in_project) {
    CopyFile *cf = calloc(1, sizeof(*cf));
    if (!cf)
      return;
    copy_attr(cf->src, sizeof(cf->src), attrs, "src");
    copy_attr(cf->dest, sizeof(cf->dest), attrs, "dest");
    cf->next = ps->cur_proj.copyfiles;
    ps->cur_proj.copyfiles = cf;
    return;
  }

  if (strcmp(el, "linkfile") == 0 && ps->in_project) {
    LinkFile *lf = calloc(1, sizeof(*lf));
    if (!lf)
      return;
    copy_attr(lf->src, sizeof(lf->src), attrs, "src");
    copy_attr(lf->dest, sizeof(lf->dest), attrs, "dest");
    lf->next = ps->cur_proj.linkfiles;
    ps->cur_proj.linkfiles = lf;
    return;
  }
}

static void on_end(void *data, const char *el) {
  ParseState *ps = (ParseState *)data;
  Manifest *m = ps->m;

  if (strcmp(el, "remote") == 0 && ps->in_remote) {
    if (m->remote_count >= MAX_REMOTES) {
      snprintf(ps->error_msg, sizeof(ps->error_msg),
               "Too many remotes (max %d)", MAX_REMOTES);
      ps->error = 1;
      return;
    }
    m->remotes[m->remote_count++] = ps->cur_remote;
    ps->in_remote = 0;
    return;
  }

  if (strcmp(el, "project") == 0 && ps->in_project) {
    /* Grow project array if needed */
    if (m->project_count >= m->project_cap) {
      int new_cap = m->project_cap == 0 ? 64 : m->project_cap * 2;
      Project *np = realloc(m->projects, new_cap * sizeof(Project));
      if (!np) {
        ps->error = 1;
        snprintf(ps->error_msg, sizeof(ps->error_msg), "OOM");
        return;
      }
      m->projects = np;
      m->project_cap = new_cap;
    }
    m->projects[m->project_count++] = ps->cur_proj;
    ps->in_project = 0;
    return;
  }

  if (strcmp(el, "manifest") == 0)
    ps->in_manifest = 0;
}

/* ------------------------------------------------------------------ */
/* Include-file support                                                 */
/* ------------------------------------------------------------------ */

static int parse_file(Manifest *m, const char *filepath);

static int parse_include(Manifest *m, const char *include_name) {
  char manifests_dir[MAX_PATH * 2];
  char include_path[MAX_PATH * 4];

  snprintf(manifests_dir, sizeof(manifests_dir), "%s/manifests", m->repodir);
  snprintf(include_path, sizeof(include_path), "%s/%s", manifests_dir,
           include_name);

  return parse_file(m, include_path);
}

static int parse_file(Manifest *m, const char *filepath) {
  FILE *fp = fopen(filepath, "r");
  if (!fp) {
    fprintf(stderr, "manifest: cannot open %s: %s\n", filepath,
            strerror(errno));
    return -1;
  }

  XML_Parser parser = XML_ParserCreate("UTF-8");
  ParseState ps = {
      .m = m, .in_manifest = 0, .in_project = 0, .in_remote = 0, .error = 0};
  XML_SetUserData(parser, &ps);
  XML_SetStartElementHandler(parser, on_start);
  XML_SetEndElementHandler(parser, on_end);

  char buf[4096];
  size_t bytes;
  int ok = 1;
  while ((bytes = fread(buf, 1, sizeof(buf), fp)) > 0) {
    if (XML_Parse(parser, buf, (int)bytes, 0) == XML_STATUS_ERROR) {
      fprintf(stderr, "manifest: XML error in %s: %s\n", filepath,
              XML_ErrorString(XML_GetErrorCode(parser)));
      ok = 0;
      break;
    }
  }
  if (ok)
    XML_Parse(parser, "", 0, 1);

  XML_ParserFree(parser);
  fclose(fp);

  if (!ok || ps.error) {
    fprintf(stderr, "manifest: parse error: %s\n",
            ps.error_msg[0] ? ps.error_msg : "(unknown)");
    return -1;
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/* Remote resolution                                                    */
/* ------------------------------------------------------------------ */

static Remote *find_remote(Manifest *m, const char *name) {
  for (int i = 0; i < m->remote_count; i++) {
    if (strcmp(m->remotes[i].name, name) == 0)
      return &m->remotes[i];
    if (m->remotes[i].alias[0] && strcmp(m->remotes[i].alias, name) == 0)
      return &m->remotes[i];
  }
  return NULL;
}

/* Build full URL: remote->fetch + "/" + project->name */
static void build_project_url(Manifest *m, Project *p) {
  Remote *r = find_remote(m, p->remote_name);
  if (!r) {
    fprintf(stderr, "manifest: unknown remote '%s' for project '%s'\n",
            p->remote_name, p->name);
    return;
  }
  p->remote = r;

  /* Strip trailing slash from fetch URL */
  char fetch[MAX_URL];
  strncpy(fetch, r->fetch, MAX_URL - 1);
  fetch[MAX_URL - 1] = '\0';
  size_t flen = strlen(fetch);
  if (flen > 0 && fetch[flen - 1] == '/')
    fetch[flen - 1] = '\0';

  snprintf(p->fetch_url, MAX_URL, "%s/%s", fetch, p->name);

  /* If project has no revision set, inherit from remote then default */
  if (p->revision[0] == '\0') {
    if (r->revision[0])
      snprintf(p->revision, MAX_REVISION, "%s", r->revision);
    else
      snprintf(p->revision, MAX_REVISION, "%s", m->def.revision);
  }
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int manifest_find_repodir(char *out, size_t out_sz) {
  char cwd[MAX_PATH];
  if (!getcwd(cwd, sizeof(cwd)))
    return -1;

  char path[MAX_PATH + 10];
  char *p = cwd + strlen(cwd);

  while (1) {
    snprintf(path, sizeof(path), "%s/.repo", cwd);
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
      strncpy(out, path, out_sz - 1);
      out[out_sz - 1] = '\0';
      return 0;
    }
    /* Move up one level */
    p = strrchr(cwd, '/');
    if (!p || p == cwd)
      break;
    *p = '\0';
  }
  return -1;
}

static int matches_groups(const char *proj_groups_raw,
                          const char *active_groups_raw) {
  if (!active_groups_raw || active_groups_raw[0] == '\0') {
    active_groups_raw = "default";
  }

  char p_groups[1024];
  if (!proj_groups_raw || proj_groups_raw[0] == '\0') {
    strcpy(p_groups, "default");
  } else {
    strncpy(p_groups, proj_groups_raw, sizeof(p_groups) - 1);
    p_groups[sizeof(p_groups) - 1] = '\0';
  }

  char a_groups[1024];
  strncpy(a_groups, active_groups_raw, sizeof(a_groups) - 1);
  a_groups[sizeof(a_groups) - 1] = '\0';

  int has_positive_match = 0;
  int has_negative_match = 0;

  char *saveptr_a;
  char *a_group = strtok_r(a_groups, ",", &saveptr_a);
  while (a_group) {
    if (strcmp(a_group, "all") == 0) {
      has_positive_match = 1;
    } else {
      int is_negative = (a_group[0] == '-');
      const char *target = is_negative ? a_group + 1 : a_group;

      char p_copy[1024];
      strcpy(p_copy, p_groups);
      char *saveptr_p;
      char *p_group = strtok_r(p_copy, ",", &saveptr_p);
      while (p_group) {
        if (strcmp(p_group, target) == 0) {
          if (is_negative)
            has_negative_match = 1;
          else
            has_positive_match = 1;
        }
        p_group = strtok_r(NULL, ",", &saveptr_p);
      }
    }
    a_group = strtok_r(NULL, ",", &saveptr_a);
  }

  if (has_negative_match)
    return 0;
  return has_positive_match;
}

Manifest *manifest_load(const char *repodir) {
  Manifest *m = calloc(1, sizeof(*m));
  if (!m)
    return NULL;

  strncpy(m->repodir, repodir, MAX_PATH - 1);
  m->def.sync_j = 0;
  m->def.sync_tags = 1;

  /* topdir is parent of .repo */
  strncpy(m->topdir, repodir, MAX_PATH - 1);
  char *slash = strrchr(m->topdir, '/');
  if (slash)
    *slash = '\0';

  /* Path to manifest.xml */
  char manifest_path[MAX_PATH * 2];
  snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.xml", repodir);

  if (parse_file(m, manifest_path) != 0) {
    manifest_free(m);
    return NULL;
  }

  /* Support local manifests */
  char local_manifests_dir[MAX_PATH * 2];
  snprintf(local_manifests_dir, sizeof(local_manifests_dir),
           "%s/local_manifests", repodir);

  DIR *d = opendir(local_manifests_dir);
  if (d) {
    struct dirent **namelist;
    int n = scandir(local_manifests_dir, &namelist, NULL, alphasort);
    if (n >= 0) {
      for (int i = 0; i < n; i++) {
        const char *dn = namelist[i]->d_name;
        size_t len = strlen(dn);
        if (len > 4 && strcmp(dn + len - 4, ".xml") == 0) {
          char lpath[MAX_PATH * 3];
          snprintf(lpath, sizeof(lpath), "%s/%s", local_manifests_dir, dn);
          parse_file(m, lpath); /* Ignore errors for local manifests per
                                   official repo behavior, or print warning */
        }
        free(namelist[i]);
      }
      free(namelist);
    }
    closedir(d);
  }

  /* Resolve project URLs */
  for (int i = 0; i < m->project_count; i++) {
    build_project_url(m, &m->projects[i]);
  }

  /* Filter by groups */
  char manifests_git[1024];
  snprintf(manifests_git, sizeof(manifests_git), "%s/manifests", repodir);
  char *active_groups = git_config_get(manifests_git, "manifest.groups");
  if (!active_groups)
    active_groups = strdup("default");

  int keep_count = 0;
  for (int i = 0; i < m->project_count; i++) {
    Project *p = &m->projects[i];
    if (matches_groups(p->groups, active_groups)) {
      m->projects[keep_count++] = m->projects[i];
    } else {
      /* Free associated data */
      CopyFile *cf = p->copyfiles;
      while (cf) {
        CopyFile *next = cf->next;
        free(cf);
        cf = next;
      }
      LinkFile *lf = p->linkfiles;
      while (lf) {
        LinkFile *next = lf->next;
        free(lf);
        lf = next;
      }
    }
  }
  m->project_count = keep_count;
  free(active_groups);

  return m;
}

void manifest_free(Manifest *m) {
  if (!m)
    return;
  for (int i = 0; i < m->project_count; i++) {
    Project *p = &m->projects[i];
    CopyFile *cf = p->copyfiles;
    while (cf) {
      CopyFile *next = cf->next;
      free(cf);
      cf = next;
    }
    LinkFile *lf = p->linkfiles;
    while (lf) {
      LinkFile *next = lf->next;
      free(lf);
      lf = next;
    }
  }
  free(m->projects);
  free(m);
}

int manifest_resolve_project_url(Manifest *m, Project *p) {
  build_project_url(m, p);
  return p->fetch_url[0] ? 0 : -1;
}

Project *manifest_find_project_by_name(Manifest *m, const char *name) {
  for (int i = 0; i < m->project_count; i++) {
    if (strcmp(m->projects[i].name, name) == 0)
      return &m->projects[i];
  }
  return NULL;
}

Project *manifest_find_project_by_path(Manifest *m, const char *path) {
  for (int i = 0; i < m->project_count; i++) {
    if (strcmp(m->projects[i].path, path) == 0)
      return &m->projects[i];
  }
  return NULL;
}
