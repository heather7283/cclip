#!/bin/sh

set -- $1
id="$1"
mime="$2"
preview="$3"

chafa_wrapper() {
    chafa \
        -f sixels \
        --align center \
        --scale max \
        --passthrough none \
        --view-size "${FZF_PREVIEW_COLUMNS}x${FZF_PREVIEW_LINES}"
}

case "$mime" in
    (image/*) # preview images with chafa
        cclip get "$id" | chafa_wrapper
        ;;
    (text/*) # simply print text
        cclip get "$id"
        ;;
    (*) # print preview for unknown types
        echo "$preview"
        ;;
esac

