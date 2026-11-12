#!/usr/bin/env bash
# ci-test.sh — run the host test suite for CI.
#
# Usage: scripts/ci-test.sh [make variable assignments...]
#   e.g. scripts/ci-test.sh CXX=g++-13
#        scripts/ci-test.sh TEST_TARGETS="target-a target-b"
# All arguments are forwarded to `make test`.
#
# Retry policy (CI inventory D1): exactly two targets are evidenced
# load-sensitive — test-run-prepared-sound (threaded assert) and
# test-run-file-web-http (TimeoutError against its own local test
# server). Both have passed in the same configurations in which
# they have failed, and both pass standalone. If the suite fails
# and the set of failed targets is a subset of those two, each
# failed target is retried ONCE on its own. A second failure fails
# the job. Any other failing target fails the job immediately,
# with no retry: a retry must never mask a new failure elsewhere.
#
# Visibility: every retry emits a GitHub warning annotation and a
# job-summary line naming the target, so flake frequency stays
# countable from run history. Nothing is suppressed: the first
# attempt's full log stays in the job log above the retry.
#
# Trend record: when FLAKE_LOG is set (the CI test jobs set it),
# each retry fire also appends one tab-separated line —
# UTC timestamp, GitHub job id, GitHub run id, target — to that
# file, which the job uploads as a workflow artifact. That turns
# individual annotations into a trend record without making trend
# volume a failure condition.
set -u

FLAKY_TARGETS="test-run-prepared-sound test-run-file-web-http"

record_flake_fire() {
  local target_name="$1"
  local log_path="${FLAKE_LOG:-}"
  local log_dir
  [ -n "$log_path" ] || return 0
  log_dir="$(dirname "$log_path")"
  mkdir -p "$log_dir" 2>/dev/null || return 0
  printf '%s\t%s\t%s\t%s\n' \
    "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
    "${GITHUB_JOB:-local}" \
    "${GITHUB_RUN_ID:-local}" \
    "$target_name" >> "$log_path" 2>/dev/null || true
}

log="$(mktemp)"
trap 'rm -f "$log"' EXIT

make test "$@" 2>&1 | tee "$log"
status=${PIPESTATUS[0]}
if [ "$status" -eq 0 ]; then
  exit 0
fi

# Failing targets from make's own error lines, e.g.
#   make[2]: *** [Makefile:1965: test-run-prepared-sound] Error 134
# The roll-up goals (test, test-all) also appear as error lines
# whenever any leaf target fails; they carry no information of
# their own and are dropped — only leaf targets are classified.
failed="$(grep -oE '\*\*\* \[Makefile:[0-9]+: [^]]+\] Error' "$log" \
  | sed -E 's/.*\[Makefile:[0-9]+: ([^]]+)\] Error/\1/' | sort -u \
  | grep -v -x -e test -e test-all || true)"

if [ -z "$failed" ]; then
  echo "::error::ci-test: suite failed (exit $status) with no identifiable failing target; not retrying."
  exit "$status"
fi

for target in $failed; do
  case " $FLAKY_TARGETS " in
    *" $target "*) ;;
    *)
      echo "::error::ci-test: $target failed and is not on the evidenced flaky list; not retrying."
      exit "$status"
      ;;
  esac
done

retry_status=0
for target in $failed; do
  echo "::warning title=Flaky target retry::$target failed in the suite run; retrying once per the D1 policy. A second failure fails the job."
  if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    echo "- ⚠️ Flaky-target retry: \`$target\` failed in the suite run and was retried once (D1). A second failure fails the job." >> "$GITHUB_STEP_SUMMARY"
  fi
  record_flake_fire "$target"
  echo "ci-test: retrying $target once..."
  if make test "$@" TEST_TARGETS="$target"; then
    echo "ci-test: $target passed on retry."
  else
    echo "::error::ci-test: $target failed again on its single retry; treating as a real failure."
    retry_status=1
  fi
done
exit "$retry_status"
