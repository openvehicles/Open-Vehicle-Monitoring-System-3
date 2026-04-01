#!/usr/bin/env bash
# install-hooks.sh — Install git hooks from scripts/hooks/ into .git/hooks/.
#
# Run once after cloning:
#   bash scripts/install-hooks.sh

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HOOKS_SRC="$REPO_ROOT/scripts/hooks"
HOOKS_DST="$REPO_ROOT/.git/hooks"

for hook in "$HOOKS_SRC"/*; do
    name=$(basename "$hook")
    dest="$HOOKS_DST/$name"
    cp "$hook" "$dest"
    chmod +x "$dest"
    echo "Installed: .git/hooks/$name"
done

echo "Done."
