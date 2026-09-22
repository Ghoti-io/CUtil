# Files

## 1. What it is

Two operations, and the handle that connects them.

`gcu_file_read()` reads an entire file into memory.  `gcu_file_temp_create()`
opens a uniquely named temporary file, and `gcu_file_temp_commit()` moves it
over a destination so that no reader ever sees a partly written file.
`gcu_file_write_atomic()` is the two of those for the common case where the
whole content is already in memory.

It is built on `path.h` and adds nothing of its own about path syntax.

## 2. Prior art

**The code quoted in this section has since been replaced.**  It is kept
because it is the specification this module was written against, and because
a reader deserves to see what was actually wrong rather than take the claim on
trust.  What happened when the libraries converted is at the end, and it
corrects one thing this section originally overstated.

The suite had written this several times before.

**Reading**:  `chron/src/zone/zonedb.c`, `cjelly/src/format/image.c`,
`ctang/src/tang.c`, `model/src/obj/obj_load.c`, `text/src/text_file_io.c` and
`text/src/yaml/yaml_file_io.c` each contained a whole-file reader.  Two of
them had independently converged on the same signature -

```c
GCHRON_Result gchron_zone_read_file(const char * path, size_t max_bytes,
    const GCHRON_Allocator * allocator, void ** out_data, size_t * out_len);
```

- a byte cap and an allocator, which is a strong signal about what the API
wants.  `cjelly`'s has neither, which is the untrusted-input hole the cap
exists to close.

**Atomic replacement**:  `text/src/text_file_io.c` and
`text/src/yaml/yaml_file_io.c` held the same mkstemp/fdopen/rename sequence
twice, and the copies had drifted apart.  On the `fdopen` failure path:

```c
/* text_file_io.c */              /* yaml_file_io.c */
_close(fd);                       _close(fd);
remove(temp_path);                free(temp_path);
free(temp_path);
```

The second forgot to remove the file it had just created, in **both** its
branches.  Neither copy called `fsync`, and both carried a comment claiming
the content was safe once `fflush` had returned, which is not what `fflush`
does.

### What the conversion found

All six readers above are now seams onto this module:  `text` converted in
`d4763d4`, `chron` in `e0ed78f`, `model` in `83d7c65`, `ctang` in `a2c3bf5`
and `cjelly` in `ec913e5`.

The conversion turned up a defect neither this section nor the duplication
itself had predicted, and it is the one that mattered - in three of the six
copies, which had never shared a line of code:

- **YAML could not read a stream.**  Its copy read with `fseek`/`ftell`/
  `fread`, so a pipe, a FIFO, `/dev/stdin` or anything under `/proc` was
  refused outright with "Failed to seek file" - while `text`'s README had
  claimed otherwise for all three formats for months.  That is the only
  difference between the copies a caller could actually see, and it is
  precisely the failure section 3 describes.  `text` has a test for it that
  fails against the previous implementation.
- **Neither could `cjelly`, and it said the wrong thing about it.**  Its copy
  sized with `fseek`/`ftell` in the same way, so the same inputs came back
  empty - and an image reader cannot tell an empty buffer from a bad one, so
  the failure was reported as a corrupt image rather than as a file that could
  not be read that way.  It too has a test that fails against the old reader.
- **`ctang` sized with `ftell` on a stream opened in text mode**, which
  over-reports on Windows by the number of line endings.  The tail of the
  buffer was then whatever the allocator last left there, and it was handed to
  the compiler as source.  That copy also ignored what `fread` returned and
  leaked the `FILE *` when the allocation failed.

Three copies, written separately, with the same shape at the bottom of each.
That is a better argument for one implementation than the duplication itself
was:  duplication predicts that the copies will *differ*, and this section
originally went looking on exactly those grounds - but what was actually wrong
is the thing all three agreed on.

Two further differences turned out to be smaller than this section originally
called them, and the record should say so.  It said "three defects"; two of
the three are better described as divergences between copies than as bugs
anyone met, and `text`'s tests for both hold against the old code as well:

- YAML never applied its own `max_total_bytes` to the *file*, so an over-large
  document was read into memory in full and refused afterwards by the parser.
  The answer was the same either way; the limit had simply already been spent
  by the time it was applied.  A capped read closes that by construction.
- The missing `remove()` on the `fdopen` failure path sits on a path that is
  close to unreachable.  It was a real difference between two copies of one
  function, which is the argument for there being one function - but not an
  incident.

One deliberate behaviour change came with the conversion rather than out of
it:  `text` now commits the bytes before the rename instead of leaving them to
writeback.  The hand-written version flushed stdio and renamed, which survives
a killed process but not a power loss, and these parsers are usually pointed
at configuration files.  That is `GCU_FILE_SYNC_FULL` being the zero value,
doing what section 6 says it is for.

## 3. Reading in chunks, not by size

`gcu_file_read()` never asks how large the file is.  It reads until the read
stops producing bytes.

Sizing first - seek to the end, tell, allocate, read - is the obvious
implementation and it returns nothing at all for any file whose size the
kernel does not know in advance.  Every file under `/proc` reports zero;
`/proc/version` on this machine reports a size of 0 and yields 213 bytes.
Pipes and character devices behave the same way.  `FileRead.WorksOnAFileThat
ReportsNoSize` exists because that failure is silent: the call succeeds and
hands back an empty buffer.

The buffer always carries a NUL one byte past `out_len`, which is not counted
in it.  A text caller can use the result as a C string with no copy; a binary
caller ignores it.  `ReadKeepsEmbeddedNulBytes` checks that the added
terminator is not confused with content, since the length is what delimits the
data and the NUL is only a convenience.

`max_bytes` is a promise and not a truncation, in the same sense as section 5
of `CONVENTIONS.md`.  A file over the limit yields `GCU_FILE_ERR_LIMIT` and
nothing is allocated.  The read never holds more than one byte over the limit
while discovering this, which is the byte that proves the file is too big.

## 4. The handle, and the bug it makes impossible

The two hand-written copies handed back a bare `char *` and a `FILE *`,
leaving every error path to remember `remove()`.  One of nine such paths
forgot.  That is not carelessness so much as a shape that invites it: cleanup
is a thing to remember rather than a thing to call.  Section 2 records how
little that particular omission cost in practice - the path is close to
unreachable - which is the argument for the shape rather than against it.  The
same shape is what let the *reachable* difference between those copies go
unnoticed for months.

`GCU_File_Temp` is disposed of by exactly one of two named functions:

- `gcu_file_temp_commit()` - close, move into place, done.
- `gcu_file_temp_abort()` - close, delete, done.

Both leave the handle zeroed, and `abort` accepts a zeroed handle.  So `abort`
can sit on an unconditional cleanup path without anybody tracking whether
`commit` already ran, which is what removes the class of mistake rather than
just this instance of it.  `TempAbortIsSafeOnAHandleThatWasNeverOpenedOr
IsAlreadySpent` pins all five cases: NULL, zeroed, after a failed create,
twice, and after a commit.

A failed `commit` removes its own temporary too, leaves the destination
exactly as it was, and still spends the handle.

## 5. What atomic means here, and what it needs

`rename()` is atomic with respect to the directory entry:  a reader opening
the destination gets the whole old file or the whole new one, never a mixture,
and never a zero-length window in between.  That holds **only if the temporary
file is on the same filesystem**, because a rename across filesystems is a
copy followed by a delete, and a copy is not atomic.

This is why `gcu_file_temp_create()` takes a directory and why
`gcu_file_write_atomic()` passes the destination's own.  Reaching for the
system temporary directory instead would work on a developer's laptop, where
everything is one filesystem, and fail in production where `/tmp` is separate.
`WriteAtomicDoesNotUseTheSystemTemporaryDirectory` makes that observable by
pointing `$TMPDIR` at nothing: the correct implementation never looks there.

## 6. Durability, stated exactly

`fflush()` moves bytes out of stdio and into the kernel.  It does not put them
on a disk.  A rename recorded while the content behind it is not leaves a file
that exists, has the right name, and is empty.

`GCU_FILE_SYNC_FULL` is the zero value, so a caller who does not think about
it gets the durable behaviour:

1. `fflush` the stream.
2. `fsync` it - **a failure here is reported**, because these are the bytes.
3. `fclose`, before the rename, because Windows will not move an open file.
4. Rename.
5. `fsync` the destination's directory - **a failure here is not reported**.

Step 5 is deliberately weaker.  Several filesystems refuse `fsync` on a
directory outright, and failing a replacement that has otherwise completed
would be worse than the promise being softer.  The promise is therefore:  with
`SYNC_FULL`, the content is on the disk before the rename is attempted, and
the directory entry is committed on a best-effort basis.  A crash in the
window between steps 4 and 5 can leave the old file in place, but cannot leave
a truncated or empty new one.

`GCU_FILE_SYNC_NONE` skips steps 2 and 5.  The rename is still atomic; only
the durability is given up.  That is the right choice for output that can be
regenerated and the wrong one for anything else.

Neither copy in `text` did any of this: both flushed stdio and renamed, which
survives a killed process but not a power loss.  It is the part of section 2
that most deserved to be one decision rather than an omission repeated per
call site, and converting `text` changed the behaviour of all three of its
formats accordingly.

## 7. Taking the name and the file in one step

`mkstemp()` chooses a name, creates the file, and fails if the name is already
taken - all in one operation, with the file opened `0600`.

Choosing a name and then opening it is the classic local privilege-escalation
shape: between the two steps, anything that can write to the directory can put
a symbolic link at that name, and the open follows it.  In a world-writable
`/tmp` that is a real attack and not a theoretical one, which is why the
Windows branch pairs `_wmktemp_s` with `_O_CREAT | _O_EXCL` rather than
trusting the name it was handed.

`TempCreateOpensAFileOnlyItsOwnerCanRead` asserts the mode, because the
permissions are the half that a refactor can quietly lose:  the file is
usually the place a secret is being written *through*.

That is the temporary file, for its whole life.  What the *destination* ends
up with is the next section, and is a separate question - it was not always,
and that is what the next section is about.

## 8. Permissions, and who decides them

For a while this module answered that question by not asking it.  The
destination was renamed from a `mkstemp` temporary and inherited its `0600`,
so five libraries got owner-only files without anybody choosing that:
`cjelly`'s captured screenshots came out `-rw-------` where the `fopen` they
replaced had made them `-rw-rw-r--`.  The behaviour was not obviously wrong -
it is the conservative direction - but it was not written down anywhere, and a
contract nobody stated is one nobody can be said to have accepted.

The design says permissions are the caller's business.  An access model cannot
be described honestly across platforms:  POSIX derives a new file's
permissions from a process-global umask, Windows has no umask at all and
inherits access control entries from the parent directory.  A library that
claimed to model both would be lying on at least one.  So this module does not
model them - but a caller can only take on a responsibility it has been told
about, which is what the silence prevented.

`GCU_File_Perms` is therefore three values and not a `mode_t`.  It asks the
only question that can be answered on both platforms:

| Value | The finished file |
| --- | --- |
| `GCU_FILE_PERMS_PRIVATE` | Owner-only.  The zero value. |
| `GCU_FILE_PERMS_DEFAULT` | What an ordinary `fopen()` here would have made. |
| `GCU_FILE_PERMS_PRESERVE` | What the destination already had, else `DEFAULT`. |

`PRIVATE` is the zero value for the same reason `SYNC_FULL` is:  a caller who
does not think about it should not publish something by omission.  It also
means the change that introduced this enum altered nobody's behaviour - the
five libraries that had already converted kept exactly what they had, and got
a way to say otherwise.

`PRESERVE` is the one worth arguing for.  Replacing a file is not the same act
as creating one:  somebody who runs `chmod 600` on a configuration file has
said something, and rewriting that file is not an occasion to un-say it.  The
reverse holds too - a file deliberately made group-readable should not become
private because it was edited.  Neither of the other two values can express
"leave this as the operator left it".

### Asking rather than computing

`DEFAULT` does not calculate `0666 & ~umask`.  It creates an empty file beside
the temporary, asks the kernel what mode it got, removes it, and uses that.

Two reasons, and the second is the one that decided it.  The umask is
process-global and the only portable way to read it is to set it -
`umask(0)` and then put it back - which is a window in which every other
thread creating a file gets `0666`.  And the answer is not a function of the
umask anyway.  A *default ACL* on the containing directory overrides the umask
entirely; on this machine, a directory carrying one produced a `0664` file
under `umask 0077`.  A computed answer is wrong on exactly the systems whose
administrator went to the trouble of configuring one, which is the worst
possible place to be wrong.

The probe is opened `O_CREAT | O_EXCL`, for the same reason the temporary file
is:  a name in a directory other people can write to is not a file until it
has been created as one.  `TheProbeRefusesANameSomebodyElseAlreadyHolds`
plants a symbolic link at the name the probe will use and checks that the
target is untouched and the call fails.

### Before the rename, not after

The permissions are applied while the temporary is still a temporary, through
its own descriptor, immediately before the move.  The rename is the moment the
file becomes reachable under a name somebody else knows, so doing it
afterwards would leave a window in which the destination exists with the
*wrong* permissions - and on a file whose content is already complete.

A failure to apply them fails the call and leaves the destination alone.  A
file with permissions other than the ones asked for is a different file from
the one requested, and quietly handing it over would be the same class of
silence this section exists to end.

### What is deliberately missing

Ownership, ACLs, extended attributes, inherited groups, the read-only
attribute on Windows, and anything expressed as a `mode_t`.  Those are the
caller's, with its own platform's API.  What changed is that the caller is now
told what it is starting from.

On Windows none of this is implemented:  `_wchmod` moves only the read-only
attribute, and the entries that actually decide access are inherited from the
destination's directory when the file is created - so `DEFAULT` and `PRESERVE`
already hold there and `PRIVATE` does **not**.  Making it hold needs
`SetSecurityInfo` and a constructed DACL.  Like the rest of the Windows branch
in this module, it has never been compiled or run.

## 9. Encoding

Paths are UTF-8, converted to UTF-16 at the Win32 boundary, exactly as in
`path.h` - `_wfopen`, not `fopen`.  The conversion helpers live in
`src/path_internal.h` as `static inline` so that `path.c` and `file.c` share
one implementation rather than each carrying a copy of Windows-only code
nobody here can compile.  A second copy of that is precisely the thing this
module exists to stop.

## 10. What it does not do

- **No streams.**  Section 5 of `CONVENTIONS.md` gives parsing libraries their
  own `<PREFIX>_Stream`.  This is not that:  it is whole-file reading and
  whole-file replacement, which is what the six existing copies were doing.
- **No directory creation.**  A write into a directory that does not exist
  fails; it does not build the path.
- **No locking.**  Two processes replacing the same file race, and the loser's
  content is simply gone.  Advisory locking is a separate decision with a
  separate set of platform lies attached to it.
- **No partial reads or writes.**  There is no seek, no offset and no append.

## 11. Testing

`test/test-file.cpp`, 36 tests, clean under ASan+UBSan and under Valgrind with
`--leak-check=full`.

Checked by sabotage.  Each invariant was broken in the source and the suite
confirmed to fail:

| Sabotage | Tests failing |
| --- | --- |
| a read over the limit truncates instead of refusing | 2 |
| the buffer is not NUL-terminated past its length | 1 |
| outputs are written even when the read failed | 2 |
| `abort` leaves the temporary file on disk | 1 |
| `abort` does not empty the handle | 1 (a double free) |
| `commit` leaves its temporary behind when the rename fails | 1 |
| `write_atomic` uses the system temporary directory | 1 |
| the temporary file is left world-readable | 1 |
| the read is sized by seeking rather than read in chunks | 1 |
| `PRIVATE` widens the file instead of leaving it alone | 1 |
| `DEFAULT` uses a hardcoded `0644` rather than asking | 3 |
| `PRESERVE` stats the destination and discards the answer | 2 |
| the probe file is left behind | 1 |
| the probe follows a name that is already there | 1 |
| a probe that could not be made is ignored | 1 |
| a failure to settle the permissions does not fail the commit | 1 |
| an unrecognised `GCU_File_Perms` is accepted rather than refused | 1 |

Two of those failed nothing on the first attempt.  There was no test in which
`temp_create` succeeded and the *rename* then failed - the only path on which
`commit` has litter of its own to remove - and no test that could tell where
`write_atomic` put its temporary file.  Both gaps are now closed by the two
tests named in sections 4 and 5, and both were found by the sabotage rather
than by reading the code.

The permissions work repeated that.  The first test written for "the
permissions could not be settled" made the directory unwritable, which also
breaks the `rename`, so the call failed either way and the test passed against
a build that ignored the failure entirely.  It was replaced by one that blocks
only the probe, by planting a symbolic link at the name the probe will take -
which reaches the branch in isolation and pins the `O_EXCL` at the same time.

Two mutations are recorded here as *not* caught, rather than left to look like
coverage:

- **Applying the permissions after the rename instead of before.**  The
  finished file has the same mode either way; what differs is a window in
  which the destination is readable with the wrong permissions.  A
  single-threaded test cannot observe it, and the mutation survives every
  assertion in the suite.  It is section 8's reasoning, not a tested property.
- **Ignoring what `fchmod` returns.**  It cannot be made to fail for a
  descriptor the test owns, and a read-only filesystem is not something a test
  may assume it can arrange.  The neighbouring failure - a probe that cannot
  be created - *is* covered, so the reporting path around it is exercised even
  though this one call's return value is not.
