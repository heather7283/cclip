# cclip - clipboard manager for wayland
## Overview
cclip is a set of two tools - cclip itself and cclipd, a daemon
- cclipd daemon runs in the background, monitors wayland clipboard for changes and writes clibpoard content to a database
- cclip is a CLI tools for interacting with the database created by cclipd

cclip was heavily inspired by [cliphist](https://github.com/sentriz/cliphist) and attempts to fix some issues cliphist has. It's in development, so some features are missing and you might encounter bugs.

## Features
- You can select MIME types you wish to accept
- You can specify minimum size in bytes a clipboard entry must have to be saved
- Duplicate entries won't be saved twice
- Follows UNIX philosophy - communication via pipes, easily integrateble into scripts
- Preserves clipboard content byte-to-byte (just as cliphist does)
- Supports text, images, and any other MIME type, really
- Blazingly fa.. oh nvm it's written in C

## Building
> [!NOTE]
> Make sure you have **libwayland-client**, **libsqlite3** and **wayland-scanner** installed before proceeding.

cclip uses meson build system. To build cclip locally:
```
git clone https://github.com/heather7283/cclip.git
cd cclip
meson setup --buildtype=release build
meson compile -C build
```
Binaries will be available under build directory. If you wish to have cclipd and cclip binaries available system-wide, install them:
```
sudo meson install -C build
```

## Usage
> [!IMPORTANT]
> cclipd uses wlr_data_control_unstable_v1 wayland protocol for clipboard interaction. Your compositor must support it in order for you to use cclip.
> You can check if your compositor supports wlr_data_control_unstable_v1 [here](https://wayland.app/protocols/wlr-data-control-unstable-v1#compositor-support).

Run `cclipd -h` and `cclip -h` for description of command line arguments.

Generally, you want to start cclipd from your compositor's startup file. Example for Hyprland:
```
exec-once = cclipd -s 2 -t "image/png" -t "image/*" -t "text/plain;charset=utf-8" -t "text/*" -t "*"
```

cclip is best used with apps like [rofi](https://github.com/lbonn/rofi) or [fzf](https://github.com/junegunn/fzf). See [example script](examples/picker.sh) using fzf for picker and [chafa](https://github.com/hpjansson/chafa) for image previews (requires terminal emulator with sixel support):
```
cd examples
sh picker.sh
```

## Why another clipboard manager?
I have been using cliphist for quite some time and found it very useful, however, it has some annoying issues.

### MIME types mess
Consider the following scenario:
You are running `wl-paste --watch cliphist store` in the background. You copy an image from discord. Let's see which MIME types it offers:
```
$ wl-paste --list-types
image/png
text/html
```
Now, let's see what will be saved by cliphist by replacing `cliphist store` with `cat` in aforementioned command:
```
$ wl-paste --watch cat
<img src="https://cdn.discordapp.com/attachments/AAAAA/BBBBB/image.png?ex=CCC&is=DDD&hm=EEE&=">
```
As you can see, it saves `text/html` representation of copied data instead of `image/png`. When you recall this data with `cliphist decode` later and try to paste it somewhere, it will be pasted as an HTML tag.

cclip solves this issue by letting you choose which mime types to accept. Example:
```
cclipd -t 'image/png' -t 'image/*' -t 'text/*'
```
When launched with those arguments, cclipd will try to accept `image/png` if available, then anything that starts with `image/`, and fall back to `text/*` as last resort.

### Better neovim integration
Another cliphist issue I noticedd is visible when using nvim with `vim.o.clipboard = "unnamedplus"`. When you press `x` to delete a single character, it will be saved to cliphist database:
```
$ cliphist list
92675   f
92674   o
92672   b
92671   a
92670   r
92668   m
92666   g
92665   u
92664   s
```
In cclip, you can specify minimum size of database entry with -s argument:
```
cclipd -s 2
```
Will only save entries size of which is greater than or equal to 2 bytes. This will prevent single ASCII characters from being saved, but will still allow mulitbyte unicode sequences like kanji or emojis.

## TODOs
- [x] limiting number of database entries
- [x] man pages for cclipd and cclip

## Thanks
- [cliphist](https://github.com/sentriz/cliphist) - for original idea and inspiration
- [wayclip](https://git.sr.ht/~noocsharp/wayclip) - for showing how to work with wayland protocols
- [sqlite3](https://sqlite.org/index.html) - for their fast and efficient database library
- [meson](https://mesonbuild.com/) - for their amazing build system (I am never using make again)
