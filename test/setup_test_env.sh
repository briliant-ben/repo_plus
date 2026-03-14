#!/usr/bin/env bash
# setup_test_env.sh - Create a local test repo workspace
#
# After running this script:
#   cd test_repo/workspace
#   repo-plus init -u file://path/to/test_repo/server/manifest.git
#   repo-plus sync
#
set -euo pipefail

TESTROOT="$(cd "$(dirname "$0")" && pwd)/test_repo"
SERVER="$TESTROOT/server"
WORKSPACE="$TESTROOT/workspace"

echo "==> Cleaning up old test environment..."
rm -rf "$TESTROOT"

echo "==> Creating directory structure..."
mkdir -p "$SERVER"
mkdir -p "$WORKSPACE"

# ------------------------------------------------------------------ #
# Create bare repos on the "server"                                   #
# ------------------------------------------------------------------ #

# Helper: init a bare repo and populate it with one initial commit
init_bare_repo() {
    local name="$1"
    local bare_path="$SERVER/${name}.git"

    git init --bare -b main "$bare_path" -q || git init --bare "$bare_path" -q
    # Also set default branch to main if -b didn't work (older git)
    git -C "$bare_path" symbolic-ref HEAD refs/heads/main


    # Create a temp clone, add a file, push back
    local tmp="$(mktemp -d)"
    git clone "$bare_path" "$tmp" -q
    git -C "$tmp" config user.email "test@test.com"
    git -C "$tmp" config user.name  "Test User"

    mkdir -p "$tmp/src"
    echo "# $name" > "$tmp/src/README.md"
    echo "/* $name placeholder */" > "$tmp/src/main.c"
    git -C "$tmp" add -A
    git -C "$tmp" commit -q -m "Initial commit for $name"
    git -C "$tmp" push -q origin main 2>/dev/null || \
        git -C "$tmp" push -q origin HEAD:main

    rm -rf "$tmp"
    echo "  Created $bare_path"
}

echo "==> Initializing server-side bare repositories..."
init_bare_repo "project_a"
init_bare_repo "project_b"
init_bare_repo "project_c"

# ------------------------------------------------------------------ #
# Create the manifest repo                                             #
# ------------------------------------------------------------------ #

echo "==> Creating manifest repository..."
git init --bare "$SERVER/manifest.git" -q

TMP_MANIFEST="$(mktemp -d)"
git clone "$SERVER/manifest.git" "$TMP_MANIFEST" -q
git -C "$TMP_MANIFEST" config user.email "test@test.com"
git -C "$TMP_MANIFEST" config user.name  "Test User"

# Write default.xml
cat > "$TMP_MANIFEST/default.xml" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<manifest>
  <remote name="local" fetch="file://$TESTROOT/server" revision="main"/>
  <default remote="local" revision="main" sync-j="2"/>

  <project name="project_a" path="apps/project_a"/>
  <project name="project_b" path="apps/project_b"/>
  <project name="project_c" path="libs/project_c"/>
</manifest>
EOF

git -C "$TMP_MANIFEST" add default.xml
git -C "$TMP_MANIFEST" commit -q -m "Add default manifest"
git -C "$TMP_MANIFEST" push -q origin main 2>/dev/null || \
    git -C "$TMP_MANIFEST" push -q origin HEAD:main

rm -rf "$TMP_MANIFEST"
echo "  Created $SERVER/manifest.git with default.xml"

# ------------------------------------------------------------------ #
# Summary                                                              #
# ------------------------------------------------------------------ #

echo ""
echo "==> Test environment ready!"
echo ""
echo "Server repos:"
ls "$SERVER/"
echo ""
echo "To test repo-plus:"
echo "  cd $WORKSPACE"
echo "  repo-plus init -u file://$SERVER/manifest.git"
echo "  repo-plus sync"
echo "  repo-plus status"
echo "  repo-plus list"
echo "  repo-plus forall -c 'echo \$REPO_PROJECT'"
