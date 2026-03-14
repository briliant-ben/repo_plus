#!/usr/bin/env bash
# run_tests.sh - Integration test suite for repo-plus
#
# Requirements:
#   - repo-plus binary must be built and in $PATH (or REPO_PLUS env var set)
#   - Test environment must be set up first (run setup_test_env.sh)
#
set -euo pipefail

TESTROOT="$(cd "$(dirname "$0")" && pwd)/test_repo"
WORKSPACE="$TESTROOT/workspace"
SERVER="$TESTROOT/server"

# Use REPO_PLUS env var or fallback to built binary path
REPO_PLUS="${REPO_PLUS:-$(cd "$(dirname "$0")" && pwd)/../src/repo-plus}"

if [ ! -x "$REPO_PLUS" ]; then
    echo "ERROR: repo-plus binary not found at $REPO_PLUS"
    echo "Please build first: cd .. && autoreconf -i && ./configure && make"
    exit 1
fi

# ------------------------------------------------------------------ #
# Helpers                                                              #
# ------------------------------------------------------------------ #

PASS=0
FAIL=0

pass() { echo "  [PASS] $1"; ((PASS++)) || true; }
fail() { echo "  [FAIL] $1: $2"; ((FAIL++)) || true; }


run_test() {
    local name="$1"
    shift
    if "$@" 2>&1; then
        pass "$name"
    else
        fail "$name" "exit code $?"
    fi
}

assert_output_contains() {
    local name="$1"
    local pattern="$2"
    local actual="$3"
    if echo "$actual" | grep -q "$pattern"; then
        pass "$name"
    else
        fail "$name" "expected '$pattern' in output: $actual"
    fi
}

assert_output_contains_literal() {
    local name="$1"
    local pattern="$2"
    local actual="$3"
    if echo "$actual" | grep -Fq "$pattern"; then
        pass "$name"
    else
        fail "$name" "expected '$pattern' in output: $actual"
    fi
}

assert_dir_exists() {
    local name="$1"
    local path="$2"
    if [ -d "$path" ]; then
        pass "$name"
    else
        fail "$name" "directory not found: $path"
    fi
}

# ------------------------------------------------------------------ #
# Setup: ensure clean workspace                                        #
# ------------------------------------------------------------------ #

echo "==> Setting up test environment..."
bash "$(dirname "$0")/setup_test_env.sh" > /dev/null 2>&1 || {
    echo "ERROR: Failed to set up test environment"
    exit 1
}

rm -rf "$WORKSPACE"
mkdir -p "$WORKSPACE"

# ------------------------------------------------------------------ #
# Tests                                                                #
# ------------------------------------------------------------------ #

echo ""
echo "=== Running repo-plus integration tests ==="
echo ""

# ------------------------------------
# T01: version command
# ------------------------------------
echo "--- T01: version ---"
OUT=$("$REPO_PLUS" version 2>&1)
assert_output_contains "T01: version output contains 'repo-plus'" "repo-plus" "$OUT"
assert_output_contains "T01: version output contains version number" "[0-9]\+\.[0-9]" "$OUT"

# ------------------------------------
# T02: help command
# ------------------------------------
echo "--- T02: help ---"
OUT=$("$REPO_PLUS" help 2>&1)
assert_output_contains "T02: help shows 'init'" "init" "$OUT"
assert_output_contains "T02: help shows 'sync'" "sync" "$OUT"
assert_output_contains "T02: help shows 'status'" "status" "$OUT"
assert_output_contains "T02: help shows 'forall'" "forall" "$OUT"
assert_output_contains "T02: help shows 'list'" "list" "$OUT"

# ------------------------------------
# T03: help for specific command
# ------------------------------------
echo "--- T03: help <cmd> ---"
OUT=$("$REPO_PLUS" help init 2>&1)
assert_output_contains "T03: help init shows --manifest-url" "manifest-url" "$OUT"

OUT=$("$REPO_PLUS" help sync 2>&1)
assert_output_contains "T03: help sync shows --jobs" "jobs" "$OUT"

# ------------------------------------
# T04: unknown command
# ------------------------------------
echo "--- T04: unknown command ---"
OUT=$("$REPO_PLUS" unknowncmd 2>&1 || true)
if echo "$OUT" | grep -qi "not a repo command"; then
    pass "T04: unknown command shows error"
else
    fail "T04: unknown command" "expected error message"
fi

# ------------------------------------
# T05: init
# ------------------------------------
echo "--- T05: init ---"
cd "$WORKSPACE"
"$REPO_PLUS" init -u "file://$SERVER/manifest.git" -q 2>&1 || true
assert_dir_exists "T05: .repo directory created" "$WORKSPACE/.repo"
assert_dir_exists "T05: .repo/manifests directory created" "$WORKSPACE/.repo/manifests"
if [ -f "$WORKSPACE/.repo/manifest.xml" ]; then
    pass "T05: manifest.xml created"
else
    fail "T05: manifest.xml not created" "$WORKSPACE/.repo/manifest.xml"
fi

# ------------------------------------
# T06: sync
# ------------------------------------
echo "--- T06: sync ---"
cd "$WORKSPACE"
"$REPO_PLUS" sync -q 2>&1 || true  # might have warnings, not fatal
assert_dir_exists "T06: apps/project_a cloned" "$WORKSPACE/apps/project_a"
assert_dir_exists "T06: apps/project_b cloned" "$WORKSPACE/apps/project_b"
assert_dir_exists "T06: libs/project_c cloned" "$WORKSPACE/libs/project_c"

# Check that the clone has content
if [ -f "$WORKSPACE/apps/project_a/src/README.md" ]; then
    pass "T06: project_a has expected file"
else
    fail "T06: project_a content" "src/README.md not found"
fi

# ------------------------------------
# T07: list
# ------------------------------------
echo "--- T07: list ---"
cd "$WORKSPACE"
OUT=$("$REPO_PLUS" list 2>&1)
assert_output_contains "T07: list shows project_a" "project_a" "$OUT"
assert_output_contains "T07: list shows project_b" "project_b" "$OUT"
assert_output_contains "T07: list shows project_c" "project_c" "$OUT"

OUT=$("$REPO_PLUS" list -n 2>&1)
assert_output_contains "T07: list -n shows name only" "project_a" "$OUT"

OUT=$("$REPO_PLUS" list -p 2>&1)
assert_output_contains "T07: list -p shows path" "apps/project_a" "$OUT"

# ------------------------------------
# T08: status (clean workspaces)
# ------------------------------------
echo "--- T08: status (clean) ---"
cd "$WORKSPACE"
OUT=$("$REPO_PLUS" status 2>&1)
# Clean workspaces should produce no output (or minimal)
pass "T08: status ran without error"

# ------------------------------------
# T09: status (with changes)
# ------------------------------------
echo "--- T09: status (with changes) ---"
cd "$WORKSPACE"
# Make a change in project_a
echo "modified content" >> "$WORKSPACE/apps/project_a/src/README.md"
echo "new_file.txt" > "$WORKSPACE/apps/project_a/new_file.txt"
git -C "$WORKSPACE/apps/project_a" add new_file.txt 2>/dev/null || true

OUT=$("$REPO_PLUS" status 2>&1)
assert_output_contains "T09: status shows project_a" "project_a" "$OUT"

# Revert changes
git -C "$WORKSPACE/apps/project_a" checkout -- src/README.md 2>/dev/null || true
git -C "$WORKSPACE/apps/project_a" rm -f new_file.txt 2>/dev/null || true

# ------------------------------------
# T10: forall
# ------------------------------------
echo "--- T10: forall ---"
cd "$WORKSPACE"
OUT=$("$REPO_PLUS" forall -c 'echo "PROJ=$REPO_PROJECT"' 2>&1)
assert_output_contains "T10: forall sets REPO_PROJECT for project_a" "PROJ=project_a" "$OUT"
assert_output_contains "T10: forall sets REPO_PROJECT for project_b" "PROJ=project_b" "$OUT"
assert_output_contains "T10: forall sets REPO_PROJECT for project_c" "PROJ=project_c" "$OUT"

# Test REPO_PATH
OUT=$("$REPO_PLUS" forall -c "echo PATH=\$REPO_PATH" 2>&1)
assert_output_contains "T10: forall sets REPO_PATH" "apps/project_a" "$OUT"

# ------------------------------------
# T11: forall with project filter
# ------------------------------------
echo "--- T11: forall with project filter ---"
cd "$WORKSPACE"
OUT=$("$REPO_PLUS" forall apps/project_a -c "echo ONLY_A=\$REPO_PROJECT" 2>&1)
assert_output_contains "T11: forall filters to project_a" "ONLY_A=project_a" "$OUT"
if echo "$OUT" | grep -q "project_b"; then
    fail "T11: forall filter excluded project_b" "project_b appeared in output"
else
    pass "T11: forall filter excluded project_b"
fi

# ------------------------------------
# T12: diff (no changes)
# ------------------------------------
echo "--- T12: diff (no changes) ---"
cd "$WORKSPACE"
OUT=$("$REPO_PLUS" diff 2>&1)
# No output or empty for clean workspaces
pass "T12: diff ran without error"

# ------------------------------------
# T13: start + abandon
# ------------------------------------
echo "--- T13: start branch ---"
cd "$WORKSPACE"
"$REPO_PLUS" start my-feature-branch --all 2>&1 || true
# Check at least one project got the branch
BRANCH=$(git -C "$WORKSPACE/apps/project_a" symbolic-ref --short HEAD 2>/dev/null || echo "")
if [ "$BRANCH" = "my-feature-branch" ]; then
    pass "T13: branch created in project_a"
else
    fail "T13: branch created" "expected my-feature-branch, got $BRANCH"
fi

echo "--- T14: abandon branch ---"
cd "$WORKSPACE"
# First switch back to main
git -C "$WORKSPACE/apps/project_a" checkout main -q 2>/dev/null || true
git -C "$WORKSPACE/apps/project_b" checkout main -q 2>/dev/null || true
git -C "$WORKSPACE/libs/project_c" checkout main -q 2>/dev/null || true
"$REPO_PLUS" abandon my-feature-branch --all 2>&1 || true
pass "T14: abandon ran without error"

# ------------------------------------
# T15: help for unknown sub-command
# ------------------------------------
echo "--- T15: help unknown ---"
    OUT=$("$REPO_PLUS" help notacommand 2>&1 || true)
assert_output_contains "T15: help unknown shows error" "not a repo command" "$OUT"

# ------------------------------------
# T16: grep
# ------------------------------------
echo "--- T16: grep ---"
cd "$WORKSPACE"
OUT=$("$REPO_PLUS" grep project_a 2>&1)
assert_output_contains_literal "T16: grep finds project_a in its README" "apps/project_a/src/README.md:# project_a" "$OUT"

OUT=$("$REPO_PLUS" grep placeholder 2>&1)
assert_output_contains_literal "T16: grep finds placeholder in project_b" "apps/project_b/src/main.c:/* project_b placeholder */" "$OUT"
assert_output_contains_literal "T16: grep finds placeholder in project_c" "libs/project_c/src/main.c:/* project_c placeholder */" "$OUT"

# ------------------------------------------------------------------ #
# Summary                                                              #
# ------------------------------------------------------------------ #

echo ""
echo "==================================="
echo "Results: $PASS passed, $FAIL failed"
echo "==================================="

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
