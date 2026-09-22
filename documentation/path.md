# Paths

## 1. What it is

Two groups of functions behind one header.

The **lexical** group manipulates path strings and never touches the
filesystem: `join`, `normalize`, `dirname`, `basename`, `extension`,
`is_absolute`, `root_length`, `relative_to`, `to_native`, `to_posix`.

The **environment** group asks the operating system a question it alone can
answer: `cwd`, `home`, `config_dir`, `data_dir`, `cache_dir`, `temp_dir`,
`absolute`, `canonicalize`.

The split is structural rather than tidy-minded.  The lexical half is a set of
pure functions over strings, so its whole behaviour is a table of inputs and
expected outputs; the environment half depends on what the machine happens to
be, and a test for it can only assert properties.  Keeping them apart is what
lets the first half be tested exhaustively.

## 2. Why the flavour is a parameter

`GCU_Path_Flavor` is passed in.  It is not read from `#ifdef _WIN32`.

Section 11 of `CONVENTIONS.md` says that Linux is where everything is
verified, that Windows branches get written and marked `TODO(windows):`, and
that nobody claims they work.  That is the honest position for code that
cannot be run, and it is the position this module takes.

But the *rules* of Windows paths are not platform-specific in any deep sense.
Whether `C:x` is absolute, where the root of `\\server\share\x` ends, whether
`..` may climb above a drive - these are facts about a specification, and a
function that takes the specification as an argument can be checked anywhere.
Making the flavour a parameter converts most of this module from untestable
platform code into ordinary code with a test table, and `test/test-path.cpp`
runs every Windows case on Linux.

What is left genuinely platform-specific is the environment half, which really
does have to ask the host.  That part carries the `TODO(windows):` marker; the
lexical part does not need one.

`GCU_PATH_NATIVE` is an object-like macro resolving to one of the two real
flavours at compile time, rather than a third enumerator, so that no function
has to map it onto a real one at runtime and no `switch` can forget it.

## 3. Rooted is not absolute

The single most common Windows path bug, and the reason `root_length()` and
`is_absolute()` are separate functions:

| Form | Example | Root | Absolute | Relative to |
| --- | --- | --- | --- | --- |
| Drive, rooted | `C:\x` | `C:\` | yes | nothing |
| Drive, relative | `C:x` | `C:` | **no** | drive C's own current directory |
| Rooted, no drive | `\x` | `\` | **no** | the current drive |
| UNC share | `\\srv\shr\x` | `\\srv\shr` | yes | nothing |
| Extended | `\\?\C:\x` | `\\?\C:\` | yes | nothing |

Three of the five have a non-empty root.  Only three of the five are absolute,
and they are not the same three.  Code that tests "does it start with a
separator or a drive letter" gets two of these wrong.

The distinction reaches further than a predicate.  `normalize()` must let `..`
climb above `C:` - because that names drive C's current directory, which has a
parent - while refusing to let it climb above `C:\`, `\`, or a UNC share,
which do not.  `gcu_path_normalize(W, "C:..")` is `C:..`, and
`gcu_path_normalize(W, "C:\\..")` is `C:\`.

## 4. Lexical is not canonical

`normalize()` and `absolute()` resolve `.` and `..` by text.  `canonicalize()`
resolves them by asking the filesystem, which means following symbolic links.

They disagree, and the disagreement is not a defect in either.  If `/a/b` is a
symbolic link to `/c`, then `/a/b/..` *says* `/a` and *reaches* `/`.  Both
questions have callers: a build system printing a path wants what it says, and
a program deciding whether it may open a file wants what it reaches.

The security consequence is stated on both functions in the header, and is the
reason they are two functions with two names rather than one function with a
flag.  **A containment check written on lexical paths is not a containment
check.**  Normalising `root + "/" + user_supplied` and confirming the result
still begins with `root` passes any input that a symbolic link inside the tree
carries back out of it.  The check has to be made on canonicalised paths, at
which point it also has to cope with the path not existing yet - which is why
`absolute()` exists as a separate, cheaper, more permissive answer.

## 5. The buffer contract, and why nothing truncates

Every lexical function writes into a caller-supplied buffer:

- `out = NULL`, `out_size = 0` measures, returning the length that would be
  needed.
- A buffer that cannot hold the result *and* its terminator returns
  `GCU_PATH_ERR_LIMIT` and is left byte-for-byte untouched.
- `GCU_PATH_ERR_LIMIT` is the one case where an output parameter is written on
  failure: `out_len` receives the required length.  Section 5 of
  `CONVENTIONS.md` says outputs are written only on success, and this is a
  deliberate exception, because reporting the size needed is the entire
  purpose of the failure.

Truncation is never an option.  Section 5 makes the general case - a truncated
line that still parses is worse than an error - and paths make it sharper
still: a truncated path is usually still a *valid* path, and it names a
different file.  A silent truncation is therefore not a degraded answer but a
confidently wrong one.

There are no allocating variants of the lexical calls, because every lexical
result is bounded by its inputs: a join is never longer than its arguments
plus a separator, and a normalisation is never longer than its input.  A
caller can always size a buffer up front.  The environment calls allocate,
because the length of an answer from the operating system is not knowable
before it arrives.

`relative_to()` is the one exception in the other direction: it takes an
allocator, because it must hold two normalised paths at once to compare them
and neither of them is the answer.  The alternative was a fixed component
limit, which is `MAX_PATH` in a smaller costume.  The memory is released
before it returns.

## 6. Normalising without a stack

Resolving `..` normally wants a stack: walk left to right, push components,
pop on `..`.  The stack's size depends on the input, which would mean an
allocation in the most-used function in the module.

Iterating **right to left** removes the need for one.  Going that way, a `..`
simply increments a counter, and the next ordinary component it meets is
cancelled and the counter decremented.  The state is one integer.  Whatever
count remains at the far left is the number of leading `..` the result needs -
zero if the path has a root to stop at, and exactly that many if it does not.

The cost is that the result is discovered right to left while it must be
written left to right.  The module pays it by walking the path twice: once to
total the length, and once to place the bytes from the right-hand end
backwards, which it can do because the first pass established where that end
is.

Two passes over the same data with different arithmetic is a real risk, since
the measuring pass computes a length by formula and the writing pass computes
it by placing bytes.  `test/test-path.cpp` asserts the two agree on every case
it runs - the shared helper checks the reported length against the written
string on every call - and a corpus test walks a table of shapes through it.
Adding one to the formula alone fails thirteen tests.

## 7. Directories, not home

`home()` exists, and is usually the wrong function to call.

Code that appends `/.myapp` to it is correct on Linux and wrong on both
Windows and macOS.  `config_dir()`, `data_dir()` and `cache_dir()` answer the
question that was actually being asked, and are right on each:

| | Linux / BSD | macOS | Windows |
| --- | --- | --- | --- |
| config | `$XDG_CONFIG_HOME`, else `~/.config` | `~/Library/Application Support` | `%APPDATA%` |
| data | `$XDG_DATA_HOME`, else `~/.local/share` | `~/Library/Application Support` | `%LOCALAPPDATA%` |
| cache | `$XDG_CACHE_HOME`, else `~/.cache` | `~/Library/Caches` | `%LOCALAPPDATA%` |
| temp | `$TMPDIR`, else `/tmp` | same | `GetTempPath` |

An `XDG_*` value that is not absolute is ignored, as the specification
requires, because a relative cache directory lands wherever the process
happened to be started and nothing could find it again.  An exported-but-empty
value is treated as unset, because that is what a shell almost always means by
it.

None of these create the directory, and none of them guarantee it exists.

`home()` itself consults `$HOME` first and the password database second.  The
fallback is not decoration: `$HOME` is routinely absent in daemons,
containers and cron jobs, and under `sudo` it may name the invoking user
rather than the effective one.

Every one of these strips trailing separators, so that the result joins
cleanly.  That matters most on Windows, where `GetTempPath` *always* returns
one.  On Linux nothing normally supplies one, which meant the trimming was
initially dead code that no test executed - a sabotage removing it entirely
left the suite green.  `UserDirectories.ATrailingSeparatorInTheEnvironmentIsRemoved`
sets `$TMPDIR` deliberately so that the code runs.

## 8. There is no chdir

Deliberately.

A process has one working directory, shared by every thread in it.  There is
no per-thread version, so two threads cannot hold different ones, and a
library that changes it reaches into its host application and silently alters
the meaning of every relative path in the program - including paths belonging
to code that has never heard of this library, and including paths already
captured by another thread mid-call.

The fix is to make it unnecessary rather than to document it.  Anything that
resolves a relative path should be given the base directory to resolve it
against, which is what `join()` is for.  Reading the working directory is
fine, and `cwd()` does that.

## 9. Encoding

Paths here are UTF-8 `char *`.

On Windows the wide Win32 calls are used throughout and converted at the
boundary - `GetCurrentDirectoryW`, not `GetCurrentDirectoryA`.  The ANSI
entry points go through the process code page, which cannot represent every
filename the filesystem accepts; a path that merely passes through one comes
back naming a different file, or nothing at all.  This is invisible until
somebody has an accented character in a directory name, at which point it is
a bug report nobody can reproduce.

## 10. Matching names

`gcu_path_match()` answers whether a name matches a shell pattern.  It is here
rather than in `file.h` because it is a question about a string:  nothing is
opened, nothing is resolved, and a pattern is never expanded into the set of
files that exist.  Walking a directory and testing each entry is
`gcu_dir_read()` plus this, which keeps the matching testable without a
filesystem.

Section 11 used to say there was no globbing, and gave the reason it would be
safe to add:  a glob matcher needs no regular-expression engine.  That still
holds, and is why this could be written at all - `cutil` is the root of the
suite's dependency graph and must not grow a dependency on `regex`.  Going via
a regular expression would also have been *more* work, not less:  every literal
run would need its metacharacters escaped, `[!...]` mapped to `[^...]`, and `*`
constrained not to cross a separator.

### Two rules that keep a pattern inside its component

A single `*` does not cross a separator, and neither does `?`.  A caller who
writes `src` `/` `*.c` means the files in `src`, not everything beneath it.
`**` crosses, which is the spelling every tool that needed both settled on.

A character set does not match a separator either, however it is written.
Without that rule `a[!x]b` would match `a/b`, and a pattern that looked like it
described one component would quietly describe two - the same hole the star
rule closes, through a different door.

### Two backtrack points, not one

The classic algorithm remembers only the most recent `*` and is provably
enough when every star is equal.  These are not equal:  a `*` that runs out at
a separator it may not cross must be able to fall back to an earlier `**`,
which may.  So the matcher keeps both, and tries the narrower one first.

The cost is bounded by pattern length times path length.  That is worth the
care because patterns come from configuration files and sometimes from users,
and a matcher that can be made exponential by a short string is a denial of
service with a friendly face.  `RunsInBoundedTimeOnAPatternBuiltToBlowUp`
fails if that ever regresses.

### The backslash cannot be both things

`\` escapes the next character under `GCU_PATH_POSIX`.  Under
`GCU_PATH_WINDOWS` it separates components, and one character cannot be both
that and the escape for the next one - so there is no escape character in that
flavour.  This was a real defect before it was a documented rule:  the first
implementation treated `\` as an escape in both, which meant no Windows
pattern containing a separator could match anything.

Case folding is opt-in through a flag and is ASCII-only, and it is *not*
applied automatically for the Windows flavour.  What Windows folds is a
property of the volume and the version, and folding UTF-8 correctly is a
Unicode question rather than a path one.  A flag that says exactly what it does
is honest; one that claimed to match the filesystem would not be.

## 11. What it does not do

Three entries that stood here have been built:  globbing is section 10,
directory iteration is `dir.h`, and file I/O is `file.h`.  The directory entry
said "`chron` is currently the only caller in the suite that walks a
directory", which was true and was the wrong reason to wait - one caller in
this suite says nothing about what a consumer outside it expects to find.
- **No permissions or ownership.**  POSIX mode bits are not Windows ACLs and
  POSIX uids are not Windows SIDs.  An abstraction over them is either
  dishonest about what it did or so reduced that nothing can use it, and
  nothing in the suite has asked for one.
- **No case folding beyond ASCII.**  Windows comparison in `relative_to()`
  ignores case for `A`-`Z` only.  Correct Unicode folding is locale- and
  version-dependent and would not make the comparison agree with the
  filesystem in every case anyway.

## 12. Testing

`test/test-path.cpp`, 58 tests.  Every Windows case runs on Linux, for the
reason in section 2.

The tests were checked by sabotage rather than by assumption - each invariant
was broken in the source and the suite confirmed to fail:

| Sabotage | Tests failing |
| --- | --- |
| `C:` treated as rooted rather than drive-relative | 4 |
| `is_absolute()` accepts `C:x` | 1 |
| leading `..` dropped from relative paths | 1 |
| a too-small buffer truncates instead of refusing | 3 |
| an extended `\\?\` path gets normalised | 1 |
| Windows component comparison becomes case-sensitive | 1 |
| `normalize()`'s measured length off by one | 13 |
| trailing separator not trimmed from environment values | 1 |

The last of those failed nothing on the first attempt, which is what led to
the test named in section 7.

One surviving mutation is deliberate.  Removing the `flavor == GCU_PATH_POSIX`
early return in `path_emit_separators()` changes no output, because for POSIX
the rewriting loop is already the identity: `is_separator(POSIX, '\\')` is
false, so a backslash is copied rather than rewritten.  It was confirmed
equivalent by diffing both variants over a corpus rather than by argument.
The early return stays as a statement of intent and a fast path; no test can
catch its removal, and none should be written to pretend otherwise.
