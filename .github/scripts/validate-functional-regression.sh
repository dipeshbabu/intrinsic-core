#!/usr/bin/env bash
set -euo pipefail

expected_commit=$1
source_file=$2
failure_pattern=$3
shift 3
regression_targets=("$@")
base_commit=979c8ff1f71b0c8a63d546adce8d974b7c39e482
source_path="intrinsic_sdk/intrinsic/math/python/$source_file"

test "$(git rev-parse HEAD)" = "$expected_commit"
git merge-base --is-ancestor "$base_commit" HEAD
git diff --check "$base_commit" HEAD
mapfile -t changed_python_files < <(git diff --name-only "$base_commit" HEAD -- '*.py')
test "${#changed_python_files[@]}" -gt 0
pipx run --spec pyink==25.12.0 pyink --check --line-length=80 --pyink-indentation=2 --pyink-use-majority-quotes "${changed_python_files[@]}"
pipx run --spec isort==6.0.1 isort --check-only --profile=google --project intrinsic "${changed_python_files[@]}"

unset -v ANDROID_HOME ANDROID_SDK_HOME ANDROID_SDK_ROOT
bazel test //intrinsic_sdk/intrinsic/math/python:all \
  --spawn_strategy=processwrapper-sandbox --strategy=TestRunner=linux-sandbox \
  --test_output=errors

trap 'git restore -- "$source_path"' EXIT
git show "$base_commit:$source_path" > "$source_path"
set +e
bazel test "${regression_targets[@]}" \
  --spawn_strategy=processwrapper-sandbox --strategy=TestRunner=linux-sandbox \
  --test_output=errors 2>&1 | tee "$RUNNER_TEMP/$expected_commit-regression.log"
test_status=${PIPESTATUS[0]}
set -e
test "$test_status" = 3
grep -E "$failure_pattern" "$RUNNER_TEMP/$expected_commit-regression.log"
git restore -- "$source_path"
git diff --exit-code -- "$source_path"
