cclip(1)

# NAME

cclip - command line interface for cclip database

# SYNOPSIS

*cclip* [_OPTIONS_] _ACTION_ _ACTION_ARG_

# DESCRIPTION

*cclip* is a program that lets users interact with a database
created by *cclipd*(1).

# OPTIONS

*-d* _DB_PATH_
	Path to database file. Database will NOT be created if it doesn't exist.++
Default is *$XDG_DATA_HOME/cclip/db.sqlite3* if *XDG_DATA_HOME* is set, or
	*$HOME/.local/share/cclip/db.sqlite3* otherwise.

*-s* Enables secure_delete option.
Deleted content with be overwritten with zeroes.
Use this to delete sensitive data like passwords.

*-h* Will cause *cclipd* to print help message and then exit.

*-V* Will cause *cclipd* to print its version and then exit.

# ACTIONS

*cclip* accepts exactly one of the following actions:

*list* [_FIELDS_]
	Print information about all database entries to stdout.
	Fields in output are separated with tabs.
	Output format can be controlled by specifying
	comma-separated list of one or more fields.++
Available fields: rowid, preview, mime_type, data_size, timestamp.++
Empty _FIELDS_ is equivalent to *rowid,mime_type,preview*.

*get* [_ID_]
	Print saved clipboard entry with specified id to stdout.++
If _ID_ is not specified, *cclip* will try to read id from stdin.

*delete* [_ID_]
	Delete entry with specified id from database.++
If _ID_ is not specified, *cclip* will try to read id from stdin.++
If entry with specified id does not exist in database,
	nothing is done and warning is printed to stderr.

*wipe*
	Remove _all_ entries from database.

*vacuum*
	Rebuilds the database file, repacking it into a minimal amount of space.

# EXAMPLES

Get most recently saved PNG image and display it using *chafa*(1):

```
cclip list id,mime | awk '$2 == "image/png" {print $1; exit}' | cclip get | chafa
```

Interactive picker using *fzf*(1):

```
#!/bin/sh

set -- $(cclip list | fzf --with-nth 3 -d "$(printf '\t')")
id="$1"
mime="$2"
[ -n "$id" ] && cclip get "$id" | wl-copy --type "$mime";
```

Example above can be simplifed if you let *wl-copy*(1) figure out MIME type:

```
cclip list | fzf --with-nth 3 -d "$(printf '\t')" | awk '{print $1}' | cclip get | wl-copy
```

# EXIT STATUS

*cclip* exits 0 after successful operation and 1 if it encounters an error.

# BUGS

Please report bugs to https://github.com/heather7283/cclip/issues.

When reporting bugs:

- Provide backtrace with symbols if you experience a crash.
- Describe steps necessary to reproduce the issue.

# SEE ALSO

*cclipd*(1), *wl-clipboard*(1), *fzf*(1)

