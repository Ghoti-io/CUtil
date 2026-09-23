# Audit of the flag stamps that decide when this library recompiles.
#
# Run by `make check-stamps`. Reads the makefile's TEXT, deliberately: a rule
# inside an `ifeq` whose condition is false is not in make's rule database at
# all, so `make -p` reports it as covered. cutil's entire ASan and TSan half
# lives inside `ifeq ($(OS_NAME), Linux)`.
#
# Three arms, in increasing order of how quietly each one fails:
#
#   1. A compile or link rule that names no stamp. Builds with whatever flags
#      are in force and is never rebuilt when they change.
#   2. A rule that names a stamp belonging to another tree. Worse, because a
#      rule copied between trees keeps the source tree's stamp and then misses
#      exactly the flag changes it was put there to catch.
#   3. A rule whose recipe expands a variable its own stamp does not record.
#      The rule looks guarded and the first two arms agree. $(CC) was in no
#      stamp here, which made `make CC=clang` a silent no-op: it compiled
#      nothing, re-ran GCC's binaries, and the suite reported 538 tests
#      passing "under clang".
#
# Arm 3 needs a list of variables that are not flags. That list is the part of
# this file that rots, so its length is pinned and growing it has to be
# deliberate. POSIX awk only -- this host has mawk, not gawk.

BEGIN {
  # Automatic variables, make functions, and variables naming paths or file
  # lists rather than anything handed to the compiler. A path that matters is
  # tracked by make as a prerequisite; a flag is tracked only by a stamp.
  not_a_flag_text = "@ < ^ + ? * call filter filter-out patsubst wildcard" \
    " shell if foreach MAKE OBJ_DIR APP_DIR BUILD_DIR SUITE PROJECT BRANCH" \
    " EXE_EXTENSION LIB_EXTENSION TARGET LIBOBJECTS FLOAT_IDENTIFIER INCLUDE" \
    " ASAN_OBJ_DIR ASAN_APP_DIR ASAN_TARGET ASAN_LIBOBJECTS" \
    " TSAN_OBJ_DIR TSAN_APP_DIR TSAN_TARGET TSAN_LIBOBJECTS" \
    " FLAGS_STAMP ASAN_FLAGS_STAMP TSAN_FLAGS_STAMP"
  ignored = split(not_a_flag_text, names, " ")
  for (i = 1; i <= ignored; i++) not_a_flag[names[i]] = 1
  PINNED = 38
}

{
  line = $0
  while (line ~ /\\$/) {
    sub(/\\$/, "", line)
    if ((getline nxt) <= 0) break
    line = line " " nxt
  }
  n++; text[n] = line; lineno[n] = NR
}

END {
  if (ignored != PINNED) {
    printf "stamp-audit: the not-a-flag list is %d entries, pinned at %d.\n", \
      ignored, PINNED > "/dev/stderr"
    print  "    Every name on it is a variable arm 3 will not ask about." > "/dev/stderr"
    print  "    Growing it silences the arm one variable at a time. Update" > "/dev/stderr"
    print  "    PINNED in the same commit, and say in the message why the" > "/dev/stderr"
    print  "    new name is not a flag." > "/dev/stderr"
    exit 1
  }

  # Pass one: every variable assignment, so that a stamp built out of other
  # variables can be resolved. A stamp line reading '$(COMMON_STAMP_TEXT)
  # $(CFLAGS)' records everything those two reach, and asking the text
  # literally would say it records neither $(CC) nor -O3.
  # Assignments indented inside an ifeq look exactly like recipe lines, so
  # track whether we are in a recipe rather than keying on the leading tab.
  # cutil sets OS_SPECIFIC_COMPILE_FLAGS in four such blocks, one per
  # platform, and skipping them would make the stamp look as though it does
  # not reach a variable it does.
  in_recipe = 0
  for (i = 1; i <= n; i++) {
    if (text[i] ~ /^[^\t #][^:=]*:[^=]/ &&
        text[i] !~ /^(ifeq|ifneq|ifdef|ifndef|else|endif|define|endef)/) in_recipe = 1
    else if (text[i] !~ /^[ \t]/ && text[i] ~ /[^ \t]/) in_recipe = 0
    if (in_recipe) continue
    stripped = text[i]; sub(/^[ \t]+/, "", stripped)
    if (match(stripped, /^[A-Za-z_][A-Za-z0-9_]*[ \t]*:?\+?=/)) {
      eq = index(stripped, "=")
      dname = substr(stripped, 1, eq - 1)
      sub(/[ \t:?+]*$/, "", dname)
      def[dname] = def[dname] " " substr(stripped, eq + 1)
    }
  }

  # Pass two: what each stamp records, resolved through those definitions.
  pending = ""
  for (i = 1; i <= n; i++) {
    if (pending != "" && text[i] ~ /^[ \t]*@printf .*> \$@\.new/) {
      stamp_text[pending] = resolve(text[i]); pending = ""; continue
    }
    if (text[i] ~ /^\$\([A-Z_]*FLAGS_STAMP\):/) {
      pending = substr(text[i], 3, index(text[i], ")") - 3)
    }
  }

  # Pass three: the rules.
  for (i = 1; i <= n; i++) {
    if (text[i] ~ /^\t/) { recipe = recipe "\n" text[i]; continue }
    check()
    if (text[i] ~ /^[^\t #][^:=]*:[^=]/ &&
        text[i] !~ /^(ifeq|ifneq|ifdef|ifndef|else|endif|define|endef)/) {
      at = lineno[i]
      target = substr(text[i], 1, index(text[i], ":") - 1)
      prereq = substr(text[i], index(text[i], ":") + 1)
      recipe = ""
    } else target = ""
  }
  check()

  if (bad) exit 1
  printf "check-stamps: %d compile rules, every one stamped for its own tree\n", seen
  printf "check-stamps: %d recipe variables, every one recorded by its stamp\n", vars
}

function check(   want, rest, name, cut) {
  if (target == "" || recipe !~ /\$\$?\((CC|CXX|CLANG)\)/ || recipe ~ /-fsyntax-only/) return
  seen++
  want = "FLAGS_STAMP"
  if (target ~ /ASAN/) want = "ASAN_FLAGS_STAMP"
  else if (target ~ /TSAN/) want = "TSAN_FLAGS_STAMP"

  if (prereq !~ ("\\$\\(" want "\\)")) {
    bad++
    printf "Makefile:%d: %s\n", at, target > "/dev/stderr"
    if (prereq ~ /FLAGS_STAMP/)
      printf "    names a stamp, but not %s\n", want > "/dev/stderr"
    else
      printf "    names no flag stamp; it wants %s\n", want > "/dev/stderr"
    return
  }

  # Arm 3: every variable the recipe expands must appear in that stamp's text.
  round++
  rest = recipe
  while (match(rest, /\$\$?\([A-Za-z_][A-Za-z0-9_-]*/)) {
    name = substr(rest, RSTART, RLENGTH)
    rest = substr(rest, RSTART + RLENGTH)
    cut = index(name, "(")
    name = substr(name, cut + 1)
    if (name in not_a_flag) continue
    if (asked[name] == round) continue
    asked[name] = round
    vars++
    if (index(stamp_text[want], "$(" name ")") == 0) {
      bad++
      printf "Makefile:%d: %s\n", at, target > "/dev/stderr"
      printf "    expands $(%s), which %s does not record\n", name, want > "/dev/stderr"
    }
  }
}

# Append the definition of every variable the text mentions, repeatedly, so
# that "is $(CC) recorded" is asked of everything the stamp reaches rather
# than of the one line it is spelled on. Appending rather than substituting:
# the question is only ever whether a name is reachable.
function resolve(t,   pass, rest, name, cut, add) {
  for (pass = 0; pass < 6; pass++) {
    rest = t; add = ""
    while (match(rest, /\$\([A-Za-z_][A-Za-z0-9_-]*\)/)) {
      name = substr(rest, RSTART + 2, RLENGTH - 3)
      rest = substr(rest, RSTART + RLENGTH)
      if (name in def && index(t, def[name]) == 0) add = add " " def[name]
    }
    if (add == "") break
    t = t add
  }
  return t
}
