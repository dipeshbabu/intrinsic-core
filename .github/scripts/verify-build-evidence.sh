#!/usr/bin/env bash
set -euo pipefail

fix_commit=$1
build_run_id=$2
workflow_commit=$3

actual_head=$(gh api "repos/$GITHUB_REPOSITORY/actions/runs/$build_run_id" --jq .head_sha)
test "$actual_head" = "$workflow_commit"
gh api "repos/$GITHUB_REPOSITORY/contents/.github/workflows/verify-bugfix.yml?ref=$workflow_commit" \
  -H 'Accept: application/vnd.github.raw+json' > "$RUNNER_TEMP/build-$build_run_id.yml"
grep -Fxq "          ref: $fix_commit" "$RUNNER_TEMP/build-$build_run_id.yml"
gh api "repos/$GITHUB_REPOSITORY/actions/runs/$build_run_id/jobs" > "$RUNNER_TEMP/build-$build_run_id.json"
jq -e '[.jobs[].steps[] | select(.name == "Regression tests" or .name == "Build the published CI artifacts" or .name == "Build all targets")] | length == 3 and all(.[]; .status == "completed" and .conclusion == "success")' "$RUNNER_TEMP/build-$build_run_id.json"
echo "Verified builds for $fix_commit: https://github.com/$GITHUB_REPOSITORY/actions/runs/$build_run_id"
