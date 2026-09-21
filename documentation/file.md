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

The suite had written this several times before.

**Reading**:  `chron/src/zone/zonedb.c`, `cjelly/src/format/image.c`,
`ctang/src/tang.c`, `model/src/obj/obj_load.c`, `text/src/text_file_io.c` and
`text/src/yaml/yaml_file_io.c` each contain a whole-file reader.  Two of them
had independently converged on the same signature -

```c
GCHRON_Result gchron_zone_read_file(const char * path, size_t max_bytes,
    const GCHRON_Allocator * allocator, void ** out_data, size_t * out_len);
```

- a byte cap and an allocator, which is a strong signal about what the API
wants.  `cjelly`'s has neither, which is the untrusted-input hole the cap
exists to close.

**Atomic replacement**:  `text/src/text_file_io.c` and
`text/src/yaml/yaml_file_io.c` hold the same mkstemp/fdopen/rename sequence
twice, and the copies have already drifted apart.  On the `fdopen` failure
path:

```c
/* text_file_io.c:147 */          /* yaml_file_io.c:108 */
_close(fd);                       _close(fd);
remove(temp_path);                free(temp_path);
free(temp_path);
```

The second forgets to remove the file it just created, in **both** its
branches, and leaks a `.tmpXXXXXX` onto the disk every time `fdopen` fails.
Neither copy calls `fsync`, and both carry a comment claiming the content is
safe once `fflush` has returned, which is not what `fflush` does.

Three defects in about 120 duplicated lines.  Those files are in another
repository and are untouched; converting them is a separate job for whoever
owns `text`.

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

The two hand-written copies hand back a bare `char *` and a `FILE *`, leaving
every error path to remember `remove()`.  One of nine such paths forgot.  That
is not carelessness so much as a shape that invites it: cleanup is a thing to
remember rather than a thing to call.

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

Neither copy in `text` does any of this, which is the third of the three
defects in section 2, and the one that most deserves to be a single decision
rather than an omission repeated per call site.

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

## 8. Encoding

Paths are UTF-8, converted to UTF-16 at the Win32 boundary, exactly as in
`path.h` - `_wfopen`, not `fopen`.  The conversion helpers live in
`src/path_internal.h` as `static inline` so that `path.c` and `file.c` share
one implementation rather than each carrying a copy of Windows-only code
nobody here can compile.  A second copy of that is precisely the thing this
module exists to stop.

## 9. What it does not do

- **No streams.**  Section 5 of `CONVENTIONS.md` gives parsing libraries their
  own `<PREFIX>_Stream`.  This is not that:  it is whole-file reading and
  whole-file replacement, which is what the six existing copies were doing.
- **No directory creation.**  A write into a directory that does not exist
  fails; it does not build the path.
- **No locking.**  Two processes replacing the same file race, and the loser's
  content is simply gone.  Advisory locking is a separate decision with a
  separate set of platform lies attached to it.
- **No partial reads or writes.**  There is no seek, no offset and no append.

## 10. Testing

`test/test-file.cpp`, 26 tests, clean under ASan+UBSan and under Valgrind with
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

Two of those failed nothing on the first attempt.  There was no test in which
`temp_create` succeeded and the *rename* then failed - the only path on which
`commit` has litter of its own to remove - and no test that could tell where
`write_atomic` put its temporary file.  Both gaps are now closed by the two
tests named in sections 4 and 5, and both were found by the sabotage rather
than by reading the code.
