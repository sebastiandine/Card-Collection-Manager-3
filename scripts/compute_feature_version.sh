#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <branch-name> <commit-sha>" >&2
  exit 1
fi

branch_name="$1"
commit_sha="$2"

branch_safe="$(echo "${branch_name}" | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9._-]+/-/g; s/^-+|-+$//g')"
short_sha="$(echo "${commit_sha}" | cut -c1-7)"

if [[ -z "${branch_safe}" ]]; then
  branch_safe="branch"
fi

echo "${branch_safe}-${short_sha}"
