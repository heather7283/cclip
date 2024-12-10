#!/bin/sh

# Ctrl+D to delete entry under cursor from database
# Ctrl+R to reload input
# Enter to copy entry under cursor into clipboard

list_cmd="cclip list rowid,mime_type,preview"
export FZF_DEFAULT_COMMAND="$list_cmd"

fzf \
  --ignore-case \
  --no-multi \
  --with-nth 3 \
  --delimiter "$(printf '\t')" \
  --scheme history \
  --preview 'exec bash ./previewer.sh {}' \
  --bind "ctrl-d:execute-silent(cclip -s delete {1})+reload(${list_cmd})" \
  --bind "ctrl-r:reload(${list_cmd})" \
  --bind "enter:become(cclip get {1} | wl-copy -t {2})"

