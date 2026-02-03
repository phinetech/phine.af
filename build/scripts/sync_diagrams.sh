#!/usr/bin/env bash
set -euo pipefail

# Sync Mermaid diagram sources into Markdown targets.
#
# Why: GitHub Markdown can't "include" other files, so we keep a single
# source-of-truth (*.mmd) and inject it into README/docs between markers.

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

src_rel="docs/diagrams/architecture.mmd"
src="$repo_root/$src_rel"

if [[ ! -f "$src" ]]; then
  echo "ERROR: source diagram not found: $src" >&2
  exit 1
fi

begin="%% BEGIN DIAGRAM: $src_rel"
end="%% END DIAGRAM: $src_rel"

sync_one() {
  local target_rel="$1"
  local target="$repo_root/$target_rel"

  if [[ ! -f "$target" ]]; then
    echo "ERROR: target not found: $target_rel" >&2
    exit 1
  fi

  local tmp
  tmp="$(mktemp)"

  awk -v begin="$begin" -v end="$end" -v src="$src" '
    BEGIN { in_block = 0 }
    $0 == begin {
      print $0
      while ((getline line < src) > 0) print line
      close(src)
      in_block = 1
      next
    }
    $0 == end {
      in_block = 0
      print $0
      next
    }
    in_block == 1 { next }
    { print }
  ' "$target" > "$tmp"

  mv "$tmp" "$target"
  echo "Synced: $target_rel"
}

sync_one "README.md"
sync_one "docs/architecture/overview.md"
