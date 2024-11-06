cclipd(1)

# NAME

cclipd - clipboard manager daemon for wayland

# SYNOPSIS

*cclipd* [_OPTIONS_]

# DESCRIPTION

*cclipd* is a daemon that monitors wayland clipboard for changes and writes
clipboard content to database. Saved entries can be later recalled with
*cclip*(1). Behaviour of *cclipd* is controlled with command line options.

# OPTIONS

*-d* _DB_PATH_
	Path to database file. Database will be created if it doesn't exist.++
Default is *$XDG_DATA_HOME/cclip/db.sqlite3* if *XDG_DATA_HOME* is set, or
	*$HOME/.local/share/cclip/db.sqlite3* otherwise.

*-t* _PATTERN_
	MIME type pattern to accept. Can be supplied multiple times.
	Each offered MIME type will be matched against all patterns with *fnmatch*(3)
	in the same order they were supplied on command line. First offered MIME type
	to match will be accepted (see *EXAMPLES* for details).++
Default is *\**.

*-s* _SIZE_
	If size of clipboard data in bytes is less than _SIZE_,
	it will not be saved.++
Default is *1* (accept everything).

*-c* _ENTRIES_
	Maximum number of database entries. When number of entries exceeds _ENTRIES_,
	oldest entry will be deleted when new one is added.++
Default is *1000*.

*-p* Will tell *cclipd* to also monitor primary selection for changes, which is
disabled by default.

*-e* Will make *cclipd* error out if database does not exist instead of creating it.

*-v* Will increase verbosity of output.

*-h* Will cause *cclipd* to print help message and then exit.

*-V* Will cause *cclipd* to print its version and then exit.

# EXIT STATUS

As a daemon, *cclipd* normally does not exit unless killed with a signal.
However, *cclipd* will exit with code 1 if it encounters a critical error.
In this case, error message will also be printed to stderr.

# SIGNALS

*cclipd* has special handling for the following signals:

*SIGINT*, *SIGTERM*
	Will cause *cclipd* to exit cleanly with code 0

*SIGUSR1*
	Will cause *cclipd* to close and reopen database connection.

# EXAMPLES

Try to accept *image/png* MIME type if available,
then try to accept anything that starts with *image/*,
and finally fall back to *text/plain;charset=utf-8* if nothing matched:

```
cclipd -t "image/png" -t "image/*" -t "text/plain;charset=utf-8"

```

Accept only images greater than or equal to 1024 bytes in size
and store up to 500 database entries:

```
cclipd -t "image/*" -s 1024 -c 500
```

Accept only text from both clipboard and primary selection,
saving to ~/.cache/cclip/mydb:

```
cclipd -p -t "text/\*" -d ~/.cache/cclip/mydb
```

Accept whichever MIME type we are offered first, with verbose logging:

```
cclipd -v -t "\*"
```

# COMPOSITOR SUPPORT

*cclipd* uses wlr_data_control_unstable_v1 wayland protocol for clipboard
interaction. Your compositor must support it in order for you to use *cclipd*.
You can check if your compositor supports wlr_data_control_unstable_v1 here:

https://wayland.app/protocols/wlr-data-control-unstable-v1#compositor-support

# SECURITY

Since *cclipd* stores clipboard data in an unencrypted sqlite database,
everyone who has read acces to database file can read its contents.
This can become an issue if you store sensitive data in clipboard.
One can delete sensitive data from database using *delete* or *wipe*
actions provided by *cclip*(1) with *-s* option.

# KNOWN BUGS

When copying things into clipboard in rapid succession, *cclipd*
might fail to save all of them into database. Can be reproduced
with something like this:


```
cclipd -v -d /tmp/testdb
```

then, in another terminal:


```
for i in $(seq 10); do echo "test ${i}" | wl-copy; done
cclip -d /tmp/testdb list
```

You will notice that only 3-4 entries end up in database.
I am unsure what causes it, and judging by *cclipd* logs
it seems that all offers are read successfully, but data gets lost
somewhere on database insertion step.

# REPORTING BUGS

Please report bugs to https://github.com/heather7283/cclip/issues.

When reporting bugs:

- Attach output of *cclipd* with double *-v* flag (trace log level).
- Provide backtrace with symbols if you experience a crash.
- Describe steps necessary to reproduce the issue.

# SEE ALSO

*cclip*(1), *wl-clipboard*(1)

