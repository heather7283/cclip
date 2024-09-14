#!/usr/bin/env bash

IFS=$'\t' read -r id mime preview <<<"$1"

chafa_wrapper() {
    chafa \
        -f sixels \
        --align center \
        --scale max \
        --optimize 9 \
        --view-size "${FZF_PREVIEW_COLUMNS}x${FZF_PREVIEW_LINES}"
}

# images
if [[ "$mime" =~ ^image/.*$ ]]; then
  cclip get "$id" | chafa_wrapper
# hex colors
elif [[ "$preview" =~ ^(#|0x)?[0-9a-fA-F]{6,8}$ ]]; then
  item="${preview/#\#/}"
  r="$((16#${item:0:2}))"
  g="$((16#${item:2:2}))"
  b="$((16#${item:4:2}))"
  echo "#$item"
  echo "$r $g $b"
  printf "\033[48;2;${r};${g};${b}m %*s \033[0m" "$FZF_PREVIEW_COLUMNS" ""
# youtube
elif [[ "$preview" =~ ^https:\/\/www.youtube.com\/watch\?v= ]]; then
  url="${preview#https://www.youtube.com/watch?v=}"
  url="${url%&*}"
  curl --no-progress-meter "https://img.youtube.com/vi/$url/3.jpg" | chafa_wrapper
# youtube
elif [[ "$preview" =~ ^https:\/\/youtu.be\/ ]]; then
  url="${preview#https://youtu.be/}"
  url="${url%\?*}"
  curl --no-progress-meter "https://img.youtube.com/vi/$url/3.jpg" | chafa_wrapper
# fallback
else
  cclip get "$id" | sed -e "s/.\{${FZF_PREVIEW_COLUMNS}\}/&\n/g"
fi

