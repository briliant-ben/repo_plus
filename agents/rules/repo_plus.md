---
description: Repo Plus Project Specific Rules
---
# Repo Plus Development Guidelines

1. **Objective**: This project (`repo_plus` / `oper`) is a C-language enhancement of the standard Android `repo` tool. It provides a pure C implementation for speed, integrated easily via `apt`.
2. **Architecture**:
    * The tool parses xml manifests.
    * The tool manages multple git repositories concurrently.
    * The codebase should be structured into logical components: e.g., manifest parsing (`manifest.c`), git operations (`git_ops.c`), concurrency/task management (`tasks.c`).
3. **Dependencies**:
    * Use standard POSIX APIs for directory operations.
    * Consider using standard libraries like `libxml2` or `expat` for XML parsing (add to configure.ac checks).
    * Avoid heavy, unnecessary dependencies to keep it lightweight.
