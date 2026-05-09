#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <pr-title>" >&2
  exit 1
fi

pr_title="$(echo "$1" | tr '[:upper:]' '[:lower:]')"

if [[ "${pr_title}" == major* ]]; then
  bump="major"
elif [[ "${pr_title}" == minor* ]]; then
  bump="minor"
elif [[ "${pr_title}" == fix* || "${pr_title}" == patch* || "${pr_title}" == path* ]]; then
  bump="patch"
else
  echo "Invalid PR title prefix. Must start with: major, minor, fix, patch, or path." >&2
  exit 1
fi

latest="$(git tag --list | sed -E 's/^v//' | grep -E '^[0-9]+\.[0-9]+\.[0-9]+$' | sort -t. -k1,1nr -k2,2nr -k3,3nr | head -n1 || true)"
if [[ -z "${latest}" ]]; then
  latest="0.0.0"
fi

IFS='.' read -r major minor patch <<< "${latest}"

case "${bump}" in
  major)
    major=$((major + 1))
    minor=0
    patch=0
    ;;
  minor)
    minor=$((minor + 1))
    patch=0
    ;;
  patch)
    patch=$((patch + 1))
    ;;
esac

version="${major}.${minor}.${patch}"
release_tag="v${version}"

if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
  {
    echo "version=${version}"
    echo "release_tag=${release_tag}"
  } >> "${GITHUB_OUTPUT}"
else
  echo "${version}"
fi
