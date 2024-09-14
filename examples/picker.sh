#!/usr/bin/env bash

IFS=$'\t' read -r id mime preview < <(cclip list | fzf \
  --no-multi \
  --with-nth 3 \
  --delimiter $'\t' \
  --scheme history \
  --preview 'exec bash ./picker-preview.sh {}')

if [ -n "$id" ]; then
    cclip get "$id" | wl-copy --type "$mime"
fi

