CXX := g++
CXXFLAGS := -pedantic-errors -Wall -Wextra -Werror -Wno-error=unused-function -Wfatal-errors -std=c++20 -O1 -g $(EXTRA_CXXFLAGS)
CC := cc
# GHOTIIO_CUTIL_BUILD enables DLL export on Windows (checked by GCU_API).
# GHOTIIO_CUTIL_TEST_BUILD would export internals for testing (checked by
# GCU_INTERNAL_API); it is deliberately not set here, so the shipped library
# exports its public API and nothing else.
CFLAGS := -pedantic-errors -Wall -Wextra -Werror -Wno-error=unused-function -Wfatal-errors -std=c17 -O3 -g -fvisibility=hidden -DGHOTIIO_CUTIL_BUILD $(EXTRA_CFLAGS)
# -DGHOTIIO_CUTIL_ENABLE_MEMORY_DEBUG
LDFLAGS := -L /usr/lib -lstdc++ -lm $(EXTRA_LDFLAGS)


SUITE := ghoti.io
PROJECT := cutil

# The version of this library. MINOR_VERSION carries the minor and the patch as
# one dotted string; the two are split out below for the places that need three
# separate integers. See CONVENTIONS.md section 4.
MAJOR_VERSION := 0
MINOR_VERSION := 0.0
VERSION_MINOR_ONLY := $(word 1,$(subst ., ,$(MINOR_VERSION)))
VERSION_PATCH_ONLY := $(or $(word 2,$(subst ., ,$(MINOR_VERSION))),0)
# Substituted into the .pc file; an empty Version: field makes every
# pkg-config version constraint fail.
VERSION := $(MAJOR_VERSION).$(MINOR_VERSION)

# Names this build everywhere: the .pc file, the install directory, the soname
# and the symbol token. It defaults to the major version, so an ordinary build
# of 1.x is "-1" and two majors cannot be loaded into one process by mistake.
# Override it for a build that wants its own identity:  make BRANCH=-dev
BRANCH ?= -$(MAJOR_VERSION)

# What the library reports as its version. The branch is appended only when it
# is not the default, so an ordinary build says "1.2.3" and an overridden one
# says "1.2.3-dev". Computed before BUILD=debug rewrites BRANCH below.
ifeq ($(BRANCH),-$(MAJOR_VERSION))
VERSION_STRING := $(VERSION)
else
VERSION_STRING := $(VERSION)$(BRANCH)
endif

# If BUILD is debug, append -debug.
#
# "override" because BRANCH may have come from the command line, and a
# command-line variable otherwise wins over a plain assignment here: without it
# `make BRANCH=-dev BUILD=debug` produced a debug build carrying the release
# token, whose symbols collide with the release build's.
ifeq ($(BUILD),debug)
    override BRANCH := $(BRANCH)-debug
    override VERSION_STRING := $(VERSION_STRING)-debug
endif

# The symbol namespace token. Derived from BRANCH so that the token inside every
# exported symbol is the same one that names the .pc file, the install directory
# and the shared library: "-dev" -> ghotiio_cutil_dev, "-1.0" -> ghotiio_cutil_1_0.
# Hand-writing it is the failure mode: every version then exports identical
# symbols, and two of them cannot be loaded into one process.
LIBVER_SYMBOL := $(shell echo "ghotiio_$(PROJECT)$(BRANCH)" | sed 's/[.-]/_/g')

BUILD_DIR = $(BUILD)
BASE_NAME_PREFIX := lib$(SUITE)-$(PROJECT)$(BRANCH)
BASE_NAME := $(BASE_NAME_PREFIX).so
SO_NAME := $(BASE_NAME).$(MAJOR_VERSION)


# PKG_CONFIG_PATH names where this project's own .pc file is installed, and the
# platform block below overwrites it to say so. Remember what the environment
# asked for first, so dependency lookup can still honour it further down.
PKG_CONFIG_PATH_ENV := $(PKG_CONFIG_PATH)

# Detect OS
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), Linux)
	OS_NAME := Linux
	BUILD := ./build/linux
	LIB_EXTENSION := so
	OS_SPECIFIC_COMPILE_FLAGS := -fPIC
	OS_SPECIFIC_LINK_FLAGS := -shared -fPIC
	OS_SPECIFIC_LIBRARY_NAME_FLAG := -Wl,-soname,$(SO_NAME)
	TARGET := $(SO_NAME).$(MINOR_VERSION)
	EXE_EXTENSION :=
	# Additional Linux-specific variables
	PKG_CONFIG_PATH := /usr/local/share/pkgconfig
	INCLUDE_INSTALL_PATH := /usr/local/include
	LIB_INSTALL_PATH := /usr/local/lib
	PC_INCLUDE_DIR := $(INCLUDE_INSTALL_PATH)/$(SUITE)/$(PROJECT)$(BRANCH)
	PC_LIB_DIR := $(LIB_INSTALL_PATH)/$(SUITE)

else ifeq ($(UNAME_S), Darwin)
	OS_NAME := Mac
	BUILD := ./build/mac
	LIB_EXTENSION := dylib
	OS_SPECIFIC_COMPILE_FLAGS :=
	OS_SPECIFIC_LINK_FLAGS := -shared
	OS_SPECIFIC_LIBRARY_NAME_FLAG := -Wl,-install_name,$(BASE_NAME_PREFIX).dylib
	TARGET := $(BASE_NAME_PREFIX).dylib
	EXE_EXTENSION :=
	# Additional macOS-specific variables
	PC_INCLUDE_DIR := $(INCLUDE_INSTALL_PATH)/$(SUITE)/$(PROJECT)$(BRANCH)
	PC_LIB_DIR := $(LIB_INSTALL_PATH)/$(SUITE)

else ifeq ($(findstring MINGW32_NT,$(UNAME_S)),MINGW32_NT)  # 32-bit Windows
	OS_NAME := Windows
	BUILD := ./build/win32
	LIB_EXTENSION := dll
	OS_SPECIFIC_COMPILE_FLAGS :=
	OS_SPECIFIC_LINK_FLAGS := -shared
	OS_SPECIFIC_LIBRARY_NAME_FLAG = -Wl,--out-implib,$(APP_DIR)/$(BASE_NAME_PREFIX).dll.a
	TARGET := $(BASE_NAME_PREFIX).dll
	EXE_EXTENSION := .exe
	# Additional Windows-specific variables
	# This is the path to the pkg-config files on MSYS2
	PKG_CONFIG_PATH := /mingw32/lib/pkgconfig
	INCLUDE_INSTALL_PATH := /mingw32/include
	LIB_INSTALL_PATH := /mingw32/lib
	BIN_INSTALL_PATH := /mingw32/bin
	# Windows paths for .pc so gcc invoked by mingw32-make can resolve -I/-L (lazy: only when install runs)
	PC_INCLUDE_DIR = $(shell cygpath -m $(INCLUDE_INSTALL_PATH)/$(SUITE)/$(PROJECT)$(BRANCH))
	PC_LIB_DIR = $(shell cygpath -m $(LIB_INSTALL_PATH)/$(SUITE))

else ifeq ($(findstring MINGW64_NT,$(UNAME_S)),MINGW64_NT)  # 64-bit Windows
	OS_NAME := Windows
	BUILD := ./build/win64
	LIB_EXTENSION := dll
	OS_SPECIFIC_COMPILE_FLAGS :=
	OS_SPECIFIC_LINK_FLAGS := -shared
	OS_SPECIFIC_LIBRARY_NAME_FLAG = -Wl,--out-implib,$(APP_DIR)/$(BASE_NAME_PREFIX).dll.a
	TARGET := $(BASE_NAME_PREFIX).dll
	EXE_EXTENSION := .exe
	# Additional Windows-specific variables
	# This is the path to the pkg-config files on MSYS2
	PKG_CONFIG_PATH := /mingw64/lib/pkgconfig
	INCLUDE_INSTALL_PATH := /mingw64/include
	LIB_INSTALL_PATH := /mingw64/lib
	BIN_INSTALL_PATH := /mingw64/bin
	# Windows paths for .pc so gcc invoked by mingw32-make can resolve -I/-L (lazy: only when install runs)
	PC_INCLUDE_DIR = $(shell cygpath -m $(INCLUDE_INSTALL_PATH)/$(SUITE)/$(PROJECT)$(BRANCH))
	PC_LIB_DIR = $(shell cygpath -m $(LIB_INSTALL_PATH)/$(SUITE))

else
    $(error Unsupported OS: $(UNAME_S))

endif

# ---------------------------------------------------------------------------
# Installation prefix
#
# Defaults to the system location chosen above. Override it to install
# somewhere else - the suite's bootstrap installs every library into a local
# prefix so that each build resolves its dependencies through pkg-config,
# exactly as a consumer would, rather than through a second code path that
# only in-tree builds exercise. See CONVENTIONS.md section 1.
#
#     make install PREFIX=/path/to/prefix
# ---------------------------------------------------------------------------
ifdef PREFIX
INCLUDE_INSTALL_PATH := $(PREFIX)/include
LIB_INSTALL_PATH := $(PREFIX)/lib
BIN_INSTALL_PATH := $(PREFIX)/bin
PKG_CONFIG_PATH := $(PREFIX)/share/pkgconfig
ifeq ($(OS_NAME), Windows)
PC_INCLUDE_DIR = $(shell cygpath -m $(INCLUDE_INSTALL_PATH)/$(SUITE)/$(PROJECT)$(BRANCH))
PC_LIB_DIR = $(shell cygpath -m $(LIB_INSTALL_PATH)/$(SUITE))
else
PC_INCLUDE_DIR := $(INCLUDE_INSTALL_PATH)/$(SUITE)/$(PROJECT)$(BRANCH)
PC_LIB_DIR := $(LIB_INSTALL_PATH)/$(SUITE)
endif
# A non-system prefix has no /etc/ld.so.conf.d, and writing to it would need
# root anyway. Everything built here carries an rpath to the prefix instead.
LDCONF_INSTALL_PATH :=
endif

# Dependencies are looked up along the inherited PKG_CONFIG_PATH as well as the
# install location chosen above, so that exporting PKG_CONFIG_PATH works as the
# errors below say it does. The inherited value comes first: it is an explicit
# request for this build, where the install location may be only a default.
PKG_CONFIG_LOOKUP_PATH := $(if $(PKG_CONFIG_PATH_ENV),$(PKG_CONFIG_PATH_ENV):)$(PKG_CONFIG_PATH)

ifdef PREFIX
# So that a library, a test or an example finds its Ghoti.io dependencies in the
# prefix at run time without LD_LIBRARY_PATH.
LDFLAGS += -Wl,-rpath,$(LIB_INSTALL_PATH)/$(SUITE)
endif


OBJ_DIR := $(BUILD)/objects
GEN_DIR := $(BUILD)/generated
APP_DIR := $(BUILD)/apps

INCLUDE := -I include/ -I $(BUILD_DIR)/include/
LIBOBJECTS := \
  $(OBJ_DIR)/allocator.o \
	$(OBJ_DIR)/array.o \
	$(OBJ_DIR)/atomic.o \
	$(OBJ_DIR)/barrier.o \
	$(OBJ_DIR)/cond.o \
	$(OBJ_DIR)/dir.o \
	$(OBJ_DIR)/error.o \
	$(OBJ_DIR)/env.o \
	$(OBJ_DIR)/file.o \
	$(OBJ_DIR)/filelock.o \
	$(OBJ_DIR)/hash.o \
	$(OBJ_DIR)/library.o \
	$(OBJ_DIR)/memory.o \
	$(OBJ_DIR)/mmap.o \
	$(OBJ_DIR)/once.o \
	$(OBJ_DIR)/path.o \
	$(OBJ_DIR)/pool.o \
	$(OBJ_DIR)/random.o \
	$(OBJ_DIR)/rwlock.o \
	$(OBJ_DIR)/semaphore.o \
	$(OBJ_DIR)/sequencer.o \
	$(OBJ_DIR)/string.o \
	$(OBJ_DIR)/subprocess.o \
	$(OBJ_DIR)/thread.o \
	$(OBJ_DIR)/tls.o \
	$(OBJ_DIR)/type.o \
	$(OBJ_DIR)/utf.o \
	$(OBJ_DIR)/vector.o

TESTFLAGS := `PKG_CONFIG_PATH=$(PKG_CONFIG_LOOKUP_PATH) pkg-config --libs --cflags gtest`

# The checks `make test` runs besides the tests themselves. Named in a
# variable so that a build which cannot satisfy them can clear it: the
# coverage target does, because --coverage links the gcov runtime, whose
# mangle_path check-symbols is right to reject in a shipping library and
# wrong to reject in an instrumented one. Spelled as text's TEST_GATES is.
TEST_GATES ?= check-symbols check-win32-parse check-clang

# Used by check-clang. Empty when clang is not installed, which that
# target reports rather than failing over.
CLANG := $(shell command -v clang 2>/dev/null)

# Sources whose #ifdef _WIN32 bodies are parse-checked. Add a file here in
# the same commit that gives it a Windows branch, or the branch ships
# untokenised.
WIN32_PARSE_SOURCES := src/cond.c src/once.c src/rwlock.c src/error.c src/tls.c src/env.c src/library.c src/filelock.c src/mmap.c src/subprocess.c



CUTILLIBRARY := -L $(APP_DIR) -l$(SUITE)-$(PROJECT)$(BRANCH)


all: $(APP_DIR)/$(TARGET) ## Build the shared library

####################################################################
# Dependency Inclusion
####################################################################
# Compiler-generated .d files (see -MMD -MP -MF in compile commands).
TEST_NAMES := test-macros test-type test-cond test-once test-rwlock test-error test-utf test-tls test-env test-library test-filelock test-atomic test-barrier test-mmap test-subprocess test-memory test-memory-inline test-hash test-mutex test-random test-semaphore test-string test-thread test-vector test-array test-allocator test-safemath test-safemath-portable test-pool test-sequencer test-path test-file test-dir
TEST_BINARIES := $(foreach t,$(TEST_NAMES),$(APP_DIR)/$(t)$(EXE_EXTENSION))
TEST_DEPFILES := $(addprefix $(APP_DIR)/,$(TEST_NAMES:%=%.d))
DEPFILES := $(LIBOBJECTS:.o=.d) $(TEST_DEPFILES)
-include $(DEPFILES)

####################################################################
# Floating Point Type Identification
####################################################################
FLOAT_IDENTIFIER := $(APP_DIR)/float_identifier$(EXE_EXTENSION)
$(FLOAT_IDENTIFIER): \
		src/float_identifier.c \
		src/float.h.template
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/include/$(SUITE)/$(PROJECT):
	@mkdir -p $@

# The generated half of libver.h: the namespace token and version string.
# libver_gen.h is regenerated on every build and rewritten only when its content
# changes, so a variable given on the command line - make MAJOR_VERSION=2, or
# make BRANCH=-dev - takes effect. Keying the rule on the Makefile's timestamp
# alone left the previous token and version baked into the build, and nothing
# said so.
.PHONY: force-libver
force-libver:

$(BUILD_DIR)/include/$(SUITE)/$(PROJECT)/libver_gen.h: \
		force-libver \
		$(BUILD_DIR)/include/$(SUITE)/$(PROJECT)
	@if [ -z "$(LIBVER_SYMBOL)" ]; then \
		printf "### LIBVER_SYMBOL is empty ###\n" >&2; \
		printf "Every exported symbol would lose its version namespace, and two\n" >&2; \
		printf "versions of this library could not be loaded into one process.\n" >&2; \
		exit 1; \
	fi
	@printf '%s\n' \
		'// Generated by the Makefile. Do not edit; see CONVENTIONS.md section 4.' \
		'#ifndef GHOTIIO_CUTIL_LIBVER_GEN_H' \
		'#define GHOTIIO_CUTIL_LIBVER_GEN_H' \
		'' \
		'/** The symbol namespace for this build, from the Makefile'"'"'s BRANCH. */' \
		'#define GHOTIIO_CUTIL_NAME $(LIBVER_SYMBOL)' \
		'' \
		'/** Human-readable version of this build. */' \
		'#define GHOTIIO_CUTIL_VERSION "$(VERSION_STRING)"' \
		'' \
		'/** The same version as three integers. */' \
		'#define GHOTIIO_CUTIL_VERSION_MAJOR $(MAJOR_VERSION)' \
		'#define GHOTIIO_CUTIL_VERSION_MINOR $(VERSION_MINOR_ONLY)' \
		'#define GHOTIIO_CUTIL_VERSION_PATCH $(VERSION_PATCH_ONLY)' \
		'' \
		'#endif // GHOTIIO_CUTIL_LIBVER_GEN_H' > $@.tmp
	@if cmp -s $@.tmp $@; then rm -f $@.tmp; else mv $@.tmp $@; fi

$(BUILD_DIR)/include/$(SUITE)/$(PROJECT)/float.h: \
		src/float.h.template \
		$(FLOAT_IDENTIFIER) \
		$(BUILD_DIR)/include/$(SUITE)/$(PROJECT)
	@f32="$$($(FLOAT_IDENTIFIER) 32)"; f64="$$($(FLOAT_IDENTIFIER) 64)"; \
	if [ -z "$$f32" ] || [ -z "$$f64" ]; then \
		printf "### $(FLOAT_IDENTIFIER) produced no type name ###\n" >&2; \
		printf "Without it float.h defines GCU_float32_t/GCU_float64_t as nothing,\n" >&2; \
		printf "and the first file to include type.h fails with a syntax error that\n" >&2; \
		printf "says nothing about this step. Delete $(FLOAT_IDENTIFIER) and rebuild.\n" >&2; \
		exit 1; \
	fi; \
	sed "s/FLOAT32/$$f32/; s/FLOAT64/$$f64/" src/float.h.template > $@

####################################################################
# Object Files
####################################################################

# Pattern rule: compile .c to .o and generate dependency file (compiler tracks headers).
# float.h is generated; ensure it exists before compiling any .c that may include it (e.g. type.h).
# Makefile is a real prerequisite, not decoration: every flag these objects
# were built with comes from this file, and `make` otherwise sees a .o newer
# than its .c and reuses it after a flag change. That is silent and it is
# specifically dangerous for the instrumented trees, where the flags *are* the
# semantics -- a sanitizer arm rebuilt without the flag you just added reports
# clean because the check was never compiled in. Two sessions in this
# workspace measured "no hazard" that way on 2026-09-22 before noticing they
# were comparing a binary with itself. The cost is a full rebuild whenever
# this file changes, which is the correct price.
$(OBJ_DIR)/%.o: src/%.c Makefile | $(BUILD_DIR)/include/$(SUITE)/$(PROJECT)/float.h $(BUILD_DIR)/include/$(SUITE)/$(PROJECT)/libver_gen.h
	@printf "\n### Compiling $@ ###\n"
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -MMD -MP -MF $(@:.o=.d) -o $@ $(OS_SPECIFIC_COMPILE_FLAGS)

# Extra source dependencies not seen by the compiler (included via macros).
$(OBJ_DIR)/hash.o: src/hash.template.c
$(OBJ_DIR)/vector.o: src/vector.template.c

####################################################################
# Shared Library
####################################################################

$(APP_DIR)/$(TARGET): \
		$(LIBOBJECTS)
	@printf "\n### Compiling Ghoti.io CUtil Shared Library ###\n"
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(OS_SPECIFIC_LINK_FLAGS) -o $@ $^ $(LDFLAGS) $(OS_SPECIFIC_LIBRARY_NAME_FLAG)

ifeq ($(OS_NAME), Linux)
	@ln -f -s $(TARGET) $(APP_DIR)/$(SO_NAME)
	@ln -f -s $(SO_NAME) $(APP_DIR)/$(BASE_NAME)
endif

####################################################################
# Unit Tests
####################################################################

# Test executables: compile with -MMD -MP -MF so dependency files are generated and -included.
# The plugin the dynamic-loading tests load.  Built as its own shared object
# with default visibility -- not linked into anything -- so that those tests
# open a real library with known symbols instead of reopening cutil, which is
# already in the process and would let a broken loader look like it worked.
#
# One per tree.  The ASan tree gets its own rather than borrowing the ordinary
# one, so the object the loader opens was built with the same flags as the
# process opening it.
$(APP_DIR)/libtest-plugin.$(LIB_EXTENSION): test/test-plugin.c
	@printf "\n### Compiling Test Plugin ###\n"
	@mkdir -p $(@D)
	$(CC) $(filter-out -fvisibility=hidden,$(CFLAGS)) -shared -fPIC -o $@ $<

$(APP_DIR)/libtest-plugin-broken.$(LIB_EXTENSION): test/test-plugin-broken.c
	@printf "\n### Compiling Test Plugin (unresolvable) ###\n"
	@mkdir -p $(@D)
	$(CC) $(filter-out -fvisibility=hidden,$(CFLAGS)) -shared -fPIC -o $@ $<

# Extra flags for one test, looked up by name.  Spelled this way because the
# ASan and TSan binaries are built from a generated rule that cannot carry a
# per-test flag any other way -- and a test needing a define in one tree and
# not the others is how test-library first built clean and then failed only
# under ASan.  Adding a test here covers every tree at once.
TEST_CPPFLAGS_test-library = \
	-DGCU_TEST_PLUGIN_PATH='"$(1)/libtest-plugin.$(LIB_EXTENSION)"' \
	-DGCU_TEST_BROKEN_PLUGIN_PATH='"$(1)/libtest-plugin-broken.$(LIB_EXTENSION)"'
TEST_PREREQS_test-library  = $(1)/libtest-plugin.$(LIB_EXTENSION) \
	$(1)/libtest-plugin-broken.$(LIB_EXTENSION)

# The lock tests create files; they go in the build tree, not the source tree.
TEST_CPPFLAGS_test-filelock = -DGCU_TEST_LOCK_DIR='"$(1)"'
TEST_CPPFLAGS_test-mmap     = -DGCU_TEST_MMAP_DIR='"$(1)"'
TEST_CPPFLAGS_test-subprocess = -DGCU_TEST_SUBPROCESS_DIR='"$(1)"'

$(APP_DIR)/test-library$(EXE_EXTENSION): test/test-library.cpp \
		$(call TEST_PREREQS_test-library,$(APP_DIR)) | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Library Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(call TEST_CPPFLAGS_test-library,$(APP_DIR)) \
		-MMD -MP -MF $(APP_DIR)/test-library.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-mmap$(EXE_EXTENSION): test/test-mmap.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Mmap Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(call TEST_CPPFLAGS_test-mmap,$(APP_DIR)) \
		-MMD -MP -MF $(APP_DIR)/test-mmap.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-subprocess$(EXE_EXTENSION): test/test-subprocess.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Subprocess Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(call TEST_CPPFLAGS_test-subprocess,$(APP_DIR)) \
		-MMD -MP -MF $(APP_DIR)/test-subprocess.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-barrier$(EXE_EXTENSION): test/test-barrier.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Barrier Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-barrier.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-atomic$(EXE_EXTENSION): test/test-atomic.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Atomic Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-atomic.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-filelock$(EXE_EXTENSION): test/test-filelock.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling File Lock Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(call TEST_CPPFLAGS_test-filelock,$(APP_DIR)) \
		-MMD -MP -MF $(APP_DIR)/test-filelock.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-env$(EXE_EXTENSION): test/test-env.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Env Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-env.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-tls$(EXE_EXTENSION): test/test-tls.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling TLS Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-tls.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-utf$(EXE_EXTENSION): test/test-utf.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling UTF Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-utf.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-error$(EXE_EXTENSION): test/test-error.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Error Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-error.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-rwlock$(EXE_EXTENSION): test/test-rwlock.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling RWLock Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-rwlock.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-once$(EXE_EXTENSION): test/test-once.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Once Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-once.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-cond$(EXE_EXTENSION): test/test-cond.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Condition Variable Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-cond.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-macros$(EXE_EXTENSION): test/test-macros.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Macros Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-macros.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-type$(EXE_EXTENSION): test/test-type.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Types Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-type.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-memory$(EXE_EXTENSION): test/test-memory.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Memory Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-memory.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-memory-inline$(EXE_EXTENSION): test/test-memory-inline.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Inline Memory Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-memory-inline.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-hash$(EXE_EXTENSION): test/test-hash.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Hash Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-hash.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-mutex$(EXE_EXTENSION): test/test-mutex.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Mutex Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-mutex.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-pool$(EXE_EXTENSION): test/test-pool.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Pool Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-pool.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-sequencer$(EXE_EXTENSION): test/test-sequencer.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Sequencer Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-sequencer.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-file$(EXE_EXTENSION): test/test-file.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling File Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-file.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-dir$(EXE_EXTENSION): test/test-dir.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Directory Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-dir.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-path$(EXE_EXTENSION): test/test-path.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Path Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-path.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-random$(EXE_EXTENSION): test/test-random.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Random Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-random.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-semaphore$(EXE_EXTENSION): test/test-semaphore.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Semaphore Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-semaphore.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-string$(EXE_EXTENSION): test/test-string.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling String Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-string.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-thread$(EXE_EXTENSION): test/test-thread.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Thread Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-thread.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-vector$(EXE_EXTENSION): test/test-vector.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Vector Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-vector.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-array$(EXE_EXTENSION): test/test-array.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Array Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-array.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-allocator$(EXE_EXTENSION): test/test-allocator.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Allocator Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-allocator.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-safemath$(EXE_EXTENSION): test/test-safemath.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Safe Math Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -MMD -MP -MF $(APP_DIR)/test-safemath.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

# The same cases against the portable body of each operation. It includes
# test-safemath.cpp, so -I test/ is needed to find it, and it has to be
# rebuilt when that file changes.
$(APP_DIR)/test-safemath-portable$(EXE_EXTENSION): test/test-safemath-portable.cpp \
		test/test-safemath.cpp | $(APP_DIR)/$(TARGET)
	@printf "\n### Compiling Safe Math Test (portable body) ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -I test/ -MMD -MP -MF $(APP_DIR)/test-safemath-portable.d -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

####################################################################
# Commands
####################################################################

# General commands
.PHONY: clean cloc docs docs-pdf coverage check-symbols check-win32-parse check-clang test-tsan
# Release build commands
.PHONY: all install test test-asan test-ubsan test-watch uninstall watch
# Debug build commands
.PHONY: all-debug install-debug test-debug test-watch-debug uninstall-debug watch-debug

watch: ## Watch the file directory for changes and compile the target
	@while true; do \
		make all BUILD=$(BUILD); \
		printf "\033[0;32m"; \
		printf "#########################\n"; \
		printf "# Waiting for changes.. #\n"; \
		printf "#########################\n"; \
		printf "\033[0m"; \
		inotifywait -qr -e modify -e create -e delete -e move src include test Makefile --exclude '/\.'; \
		done

test-watch: ## Watch the file directory for changes and run the unit tests
	@while true; do \
		make test BUILD=$(BUILD); \
		printf "\033[0;32m"; \
		printf "#########################\n"; \
		printf "# Waiting for changes.. #\n"; \
		printf "#########################\n"; \
		printf "\033[0m"; \
		inotifywait -qr -e modify -e create -e delete -e move src include test Makefile --exclude '/\.'; \
		done

####################################################################
# Symbol namespace check
####################################################################

check-symbols: ## Fail if any exported symbol lacks the version namespace
check-symbols: $(APP_DIR)/$(TARGET)
ifeq ($(OS_NAME), Linux)
# mangle_path is gcov's, not ours: a --coverage build links it into the library
# and it is the only symbol libgcov exports whose name does not begin with an
# underscore, so it is the only one the '^_' filter above misses. Without this
# line `make coverage` fails here - after the instrumented build and before the
# clean that would undo it - leaving instrumented objects that a later plain
# `make` silently links.
	@leaked=$$(nm -D --defined-only $(APP_DIR)/$(TARGET) \
		| awk '$$2 ~ /^[TDBR]$$/ {print $$3}' \
		| grep -v '^$(LIBVER_SYMBOL)_' \
		| grep -v '^_' \
		| grep -v '^mangle_path$$' || true); \
	if [ -n "$$leaked" ]; then \
		printf "\033[0;31m\n### Exported symbols missing the $(LIBVER_SYMBOL)_ namespace ###\033[0m\n" >&2; \
		printf "%s\n" "$$leaked" >&2; \
		printf "\nEach needs a '#define <name> GHOTIIO_CUTIL(<name>)' line in the header\n" >&2; \
		printf "that declares it. Without one, two versions of this library cannot be\n" >&2; \
		printf "loaded into the same process. See CONVENTIONS.md section 4.\n" >&2; \
		exit 1; \
	fi
	@unexported=$$(awk '/^#if DOXYGEN/{d=1} d==0 && /^[a-z_][A-Za-z0-9_ ]*\**[[:space:]]*gcu_[a-z0-9_]+[[:space:]]*\(/{print FILENAME": "$$0} /^#endif/{d=0}' \
		include/$(SUITE)/$(PROJECT)/*.h | grep -vE 'typedef|static inline' || true); \
	if [ -n "$$unexported" ]; then \
		printf "\033[0;31m\n### Public declarations without GCU_API ###\033[0m\n" >&2; \
		printf "%s\n" "$$unexported" >&2; \
		printf "\nThe library builds with -fvisibility=hidden, so these are not exported\n" >&2; \
		printf "and a consumer linking the .so gets an undefined reference.\n" >&2; \
		exit 1; \
	fi
	@split=$$(nm -D --undefined-only $(APP_DIR)/$(TARGET) \
		| awk '{print $$2}' | grep '^$(LIBVER_SYMBOL)_' || true); \
	if [ -n "$$split" ]; then \
		printf "\033[0;31m\n### Renamed but undefined - a split symbol ###\033[0m\n" >&2; \
		printf "%s\n" "$$split" >&2; \
		printf "\nA translation unit referenced the namespaced name while the one that\n" >&2; \
		printf "defines it did not see the rename - usually an internal header that\n" >&2; \
		printf "declares or defines something without including macros.h first.\n" >&2; \
		exit 1; \
	fi
	@nomacros=$$(find include src -name '*.h' \
		! -name 'libver.h' ! -name 'libver_gen.h' ! -name 'namespace.h' ! -name 'macros.h' \
		-exec grep -L '#include <ghoti.io/cutil/macros.h>' {} + || true); \
	if [ -n "$$nomacros" ]; then \
		printf "\033[0;31m\n### Headers that do not include macros.h ###\033[0m\n" >&2; \
		printf "%s\n" "$$nomacros" >&2; \
		printf "\nEvery header must include <ghoti.io/cutil/macros.h> before it declares\n" >&2; \
		printf "anything, so that the renames in namespace.h are already in effect. A\n" >&2; \
		printf "header that skips it can name a type before that type has been renamed,\n" >&2; \
		printf "producing two different types under one spelling.\n" >&2; \
		printf "See CONVENTIONS.md section 4.\n" >&2; \
		exit 1; \
	fi
	@badguards=$$(find include src -name '*.h' -exec awk 'FNR==1{d=0} !d && /^#ifndef/{print $$2; d=1}' {} + \
		| awk '$$1 !~ /^GHOTI_IO_GCU_/ {print $$1}' || true); \
	if [ -n "$$badguards" ]; then \
		printf "\033[0;31m\n### Include guards with the wrong prefix ###\033[0m\n" >&2; \
		printf "%s\n" "$$badguards" >&2; \
		printf "\nGuards mirror the path: GHOTI_IO_GCU_<PATH>_H. A guard without the\n" >&2; \
		printf "library token is one rename away from colliding with another library's.\n" >&2; \
		exit 1; \
	fi
	@dupguards=$$(find include src -name '*.h' -exec awk 'FNR==1{d=0} !d && /^#ifndef/{print $$2; d=1}' {} + \
		| sort | uniq -d || true); \
	if [ -n "$$dupguards" ]; then \
		printf "\033[0;31m\n### Headers sharing an include guard ###\033[0m\n" >&2; \
		printf "%s\n" "$$dupguards" >&2; \
		printf "\nTwo headers with one guard means whichever is included second is\n" >&2; \
		printf "silently empty. Guards mirror the path: GHOTI_IO_GCU_<PATH>_H.\n" >&2; \
		exit 1; \
	fi
	@printf "\033[0;32mEvery exported symbol carries the $(LIBVER_SYMBOL)_ namespace.\033[0m\n"
	@printf "\033[0;32mEvery public declaration carries GCU_API.\033[0m\n"
	@printf "\033[0;32mEvery header includes macros.h.\033[0m\n"
	@printf "\033[0;32mEvery include guard is unique and correctly prefixed.\033[0m\n"
else
	@printf "check-symbols: skipped (Linux only)\n"
endif

# Parse the headers' `#ifdef _WIN32` branches on this compiler.
#
# Those branches are never tokenised by a Linux build, so a syntax error in
# one survives indefinitely -- GCU_MAYBE_UNUSED's Windows arm was a syntax
# error from the initial commit until it was written down. This compiles a TU
# that *uses* every Windows-only macro against stub declarations; it catches
# the syntax-error class and says nothing about semantics.
# Compile everything with clang as well as gcc.
#
# Not about supporting a second compiler for its own sake. clang's UBSan
# diagnoses things gcc's does not -- `&data[0]` on a null pointer is undefined
# and only clang reports it, with no gcc flag that turns it on
# (-fsanitize=undefined and -fsanitize=pointer-overflow were both measured
# silent) -- and libFuzzer needs clang too. A library that has quietly stopped
# compiling under it cannot be instrumented at all without local patches,
# which is exactly how two such defects sat in the hash template while every
# run of this suite executed the line.
#
# -fsyntax-only rather than a real compile: it is half a second, and it still
# sees the driver-level diagnostics, including a link flag that has found its
# way onto a compile line.
check-clang: ## Check that the library still compiles under clang
	@printf "\n### Compiling under clang ###\n"
ifeq ($(CLANG),)
	@printf "check-clang: skipped (clang is not installed)\n"
else
	$(CLANG) -fsyntax-only $(CFLAGS) $(INCLUDE) $(OS_SPECIFIC_COMPILE_FLAGS) \
		$(patsubst $(OBJ_DIR)/%.o,src/%.c,$(LIBOBJECTS))
endif

check-win32-parse: ## Parse-check the headers' Windows branches
	@printf "\n### Parse-checking Windows branches ###\n"
	$(CC) -fsyntax-only $(filter-out -fvisibility=hidden -DGHOTIIO_CUTIL_BUILD,$(CFLAGS)) \
		-include test/win32-stubs/force.h -I test/win32-stubs $(INCLUDE) \
		test/win32-stubs/parse-check.c
	@printf "### Parse-checking Windows sources ###\n"
	$(CC) -fsyntax-only -D_WIN32 $(filter-out -fvisibility=hidden,$(CFLAGS)) \
		-include test/win32-stubs/force.h -I test/win32-stubs $(INCLUDE) \
		$(WIN32_PARSE_SOURCES)

test: ## Make and run the Unit tests
# Both the prerequisites and the run lines are derived from TEST_NAMES, so
# adding a test to that list is all it takes to have it built and executed.
# They used to be hand-maintained in parallel, which made it possible to add a
# test that was compiled but never run.
test: $(APP_DIR)/$(TARGET) $(TEST_BINARIES) $(TEST_GATES)
	@printf "\033[0;32m"
	@printf "############################\n"
	@printf "### Running normal tests ###\n"
	@printf "############################\n"
	@printf "\033[0m"
	@for t in $(TEST_BINARIES); do \
		printf "\n--- $$t ---\n"; \
		env LD_LIBRARY_PATH="$(APP_DIR)" $$t --gtest_brief=1 || exit 1; \
	done

####################################################################
# Sanitizer builds (ASan + UBSan)
####################################################################
#
# This library had no sanitizer target at all, which meant its memory
# behaviour was only ever asserted by tests that pass or fail on values -
# and a use-after-free reads back plausible values. The sibling compress
# library, which did have one, turned out to have three of them the moment
# the target was made to run.
#
# The instrumented objects live in their own tree so that an ordinary `make`
# can never link them by mistake.

# One list, used twice.  Two things were wrong here, and the second is the one
# that made the first invisible:
#
#   1. -fno-sanitize-recover was absent entirely, so UBSan printed its
#      diagnostic and returned 0.  The target reported the bug and passed.
#      Measured: a signed-overflow and a float-cast-overflow in one program
#      both printed and the process exited 0.
#   2. gcc's `undefined` group does not include float-cast-overflow (clang's
#      does), so `(int)1e30` was not even diagnosed.  Naming the check in
#      -fsanitize= and forgetting it in -fno-sanitize-recover= reproduces
#      failure 1 for that one check, which is why both flags read one variable
#      rather than two lists that can drift apart.
#
# Deliberately NOT here: float-divide-by-zero, which IEEE defines and which
# would fire on correct code that records an inf.  bounds-strict and
# pointer-overflow are already in gcc's `undefined` and add nothing.
UBSAN_CHECKS := undefined,float-cast-overflow

ASAN_UBSAN_FLAGS := -fsanitize=address,$(UBSAN_CHECKS) \
	-fno-sanitize-recover=$(UBSAN_CHECKS) \
	-fno-omit-frame-pointer -g

ASAN_BUILD_DIR := $(BUILD)-asan
ASAN_OBJ_DIR := $(ASAN_BUILD_DIR)/objects
ASAN_APP_DIR := $(ASAN_BUILD_DIR)/apps
ASAN_TARGET := $(BASE_NAME_PREFIX)-asan.so

ASAN_CFLAGS := $(CFLAGS) $(ASAN_UBSAN_FLAGS)
ASAN_CXXFLAGS := $(CXXFLAGS) $(ASAN_UBSAN_FLAGS)
ASAN_LDFLAGS := $(LDFLAGS) $(ASAN_UBSAN_FLAGS)
ASAN_LIBOBJECTS := $(patsubst $(OBJ_DIR)/%,$(ASAN_OBJ_DIR)/%,$(LIBOBJECTS))
ASAN_TEST_BINARIES := \
	$(foreach t,$(TEST_NAMES),$(ASAN_APP_DIR)/$(t)$(EXE_EXTENSION))
ASAN_CUTILLIBRARY := -L $(ASAN_APP_DIR) -l$(SUITE)-$(PROJECT)$(BRANCH)-asan

# The ASan runtime insists on being initialised before anything it has to
# intercept, and refuses to start rather than run uninstrumented if some other
# library got there first:
#
#   ASan runtime does not come first in initial library list
#
# A desktop session that sets LD_PRELOAD for its own reasons is enough to
# trigger that, and every test then aborts before gtest gets control. Naming
# the runtime here replaces whatever was inherited and puts it first.
#
# Only under gcc, though. clang links its own runtime into the executable, and
# preloading gcc's on top of that gets "Your application is linked against
# incompatible ASan runtimes" on the first binary -- which is what
# `make test-asan CC=clang` used to do. An empty preload is right there: it
# still replaces whatever the desktop inherited, which is the entire purpose
# of setting it, and clang's runtime is already first in the executable's own
# NEEDED list.
#
# The compiler has to be identified by asking it, not by looking at the answer
# to -print-file-name: clang on this distribution resolves libasan.so through
# the gcc installation and hands back gcc's absolute path, so the two replies
# are byte-identical and a path test cannot tell them apart.
ASAN_CC_IS_CLANG := $(shell $(CC) -dM -E -x c /dev/null 2>/dev/null | grep -c __clang__)
ifeq ($(ASAN_CC_IS_CLANG),0)
ASAN_RUNTIME := $(shell $(CC) -print-file-name=libasan.so)
else
ASAN_RUNTIME :=
endif

# float.h and libver_gen.h are plain generated headers, so the instrumented
# build reuses the ones the ordinary build made rather than building a second
# copy of the generator. That also keeps the generator itself uninstrumented:
# it is a build tool that runs at build time, and instrumenting it only made
# it inherit the startup problem described above.
$(ASAN_OBJ_DIR)/%.o: src/%.c Makefile \
		| $(BUILD_DIR)/include/$(SUITE)/$(PROJECT)/float.h \
		  $(BUILD_DIR)/include/$(SUITE)/$(PROJECT)/libver_gen.h
	@printf "\n### Compiling (ASan+UBSan): $@ ###\n"
	@mkdir -p $(@D)
	$(CC) $(ASAN_CFLAGS) $(INCLUDE) -c $< -o $@ $(OS_SPECIFIC_COMPILE_FLAGS)

$(ASAN_OBJ_DIR)/hash.o: src/hash.template.c
$(ASAN_OBJ_DIR)/vector.o: src/vector.template.c

$(ASAN_APP_DIR)/$(ASAN_TARGET): $(ASAN_LIBOBJECTS)
	@printf "\n### Linking (ASan+UBSan) $@ ###\n"
	@mkdir -p $(@D)
	$(CC) $(ASAN_CFLAGS) $(OS_SPECIFIC_LINK_FLAGS) -o $@ $^ $(ASAN_LDFLAGS)

# One rule per test, generated from TEST_NAMES for the same reason the ordinary
# test list is: a hand-maintained second list is a list that can silently omit
# a test.
$(ASAN_APP_DIR)/libtest-plugin.$(LIB_EXTENSION): test/test-plugin.c
	@printf "\n### Compiling (ASan+UBSan) Test Plugin ###\n"
	@mkdir -p $(@D)
	$(CC) $(filter-out -fvisibility=hidden,$(ASAN_CFLAGS)) -shared -fPIC -o $@ $<

$(ASAN_APP_DIR)/libtest-plugin-broken.$(LIB_EXTENSION): test/test-plugin-broken.c
	@printf "\n### Compiling (ASan+UBSan) Test Plugin (unresolvable) ###\n"
	@mkdir -p $(@D)
	$(CC) $(filter-out -fvisibility=hidden,$(ASAN_CFLAGS)) -shared -fPIC -o $@ $<

define ASAN_TEST_RULE
$(ASAN_APP_DIR)/$(1)$(EXE_EXTENSION): test/$(1).cpp \
		$(call TEST_PREREQS_$(1),$(ASAN_APP_DIR)) \
		| $(ASAN_APP_DIR)/$(ASAN_TARGET)
	@printf "\n### Compiling (ASan+UBSan) $$@ ###\n"
	@mkdir -p $$(@D)
	$$(CXX) $$(ASAN_CXXFLAGS) $$(INCLUDE) -I test/ \
		$(call TEST_CPPFLAGS_$(1),$(ASAN_APP_DIR)) \
		-o $$@ $$< $$(ASAN_LDFLAGS) $$(TESTFLAGS) $$(ASAN_CUTILLIBRARY)
endef
$(foreach t,$(TEST_NAMES),$(eval $(call ASAN_TEST_RULE,$(t))))

test-asan: ## Make and run the Unit tests under AddressSanitizer + UBSan
test-asan: $(ASAN_APP_DIR)/$(ASAN_TARGET) $(ASAN_TEST_BINARIES)
ifeq ($(OS_NAME), Linux)
	@printf "\033[0;36m"
	@printf "#######################################\n"
	@printf "### Running tests with ASan + UBSan ###\n"
	@printf "#######################################\n"
	@printf "\033[0m"
	@for t in $(ASAN_TEST_BINARIES); do \
		printf "\n--- $$t ---\n"; \
		env LD_LIBRARY_PATH="$(ASAN_APP_DIR)" LD_PRELOAD="$(ASAN_RUNTIME)" \
			ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
			UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
			$$t --gtest_brief=1 || exit 1; \
	done
	@printf "\033[0;32m\nAll tests passed with ASan + UBSan.\033[0m\n"
else
	@printf "\033[0;31mSanitizer builds are currently only supported on Linux.\033[0m\n"
	@exit 1
endif

test-ubsan: ## Alias for test-asan (ASan and UBSan run together)
test-ubsan: test-asan

# ---------------------------------------------------------------------------
# ThreadSanitizer
#
# ASan and UBSan assert what a single thread does with memory. They say
# nothing about two threads reaching the same memory without a lock between
# them, which is the defect this library's concurrent modules are most likely
# to have: the sibling compress library's thread pool shared its shutdown flag
# across threads as a plain bool, and nothing in its test suite could see
# that. (That pool has since been replaced by this one, and its job queue by
# this library's sequencer, which is why both are on the list below.)
#
# TSan cannot be combined with ASan, so it gets its own tree, built the same
# way and kept beside the ordinary and instrumented ones.
#
# It is driven by its own list rather than by TEST_NAMES. hash and vector ship
# a mutex that the README describes as the caller's responsibility to use, so
# their tests may race deliberately, and auditing them should not gate a run
# that is here to watch the synchronisation primitives themselves. Add a name
# here once its test is expected to be clean under TSan.

TSAN_TEST_NAMES := test-mutex test-cond test-barrier test-once test-rwlock test-tls test-atomic test-semaphore test-thread test-pool test-sequencer

TSAN_FLAGS := -fsanitize=thread -fno-omit-frame-pointer -g

TSAN_BUILD_DIR := $(BUILD)-tsan
TSAN_OBJ_DIR := $(TSAN_BUILD_DIR)/objects
TSAN_APP_DIR := $(TSAN_BUILD_DIR)/apps
TSAN_TARGET := $(BASE_NAME_PREFIX)-tsan.so

TSAN_CFLAGS := $(CFLAGS) $(TSAN_FLAGS)
TSAN_CXXFLAGS := $(CXXFLAGS) $(TSAN_FLAGS)
TSAN_LDFLAGS := $(LDFLAGS) $(TSAN_FLAGS)
TSAN_LIBOBJECTS := $(patsubst $(OBJ_DIR)/%,$(TSAN_OBJ_DIR)/%,$(LIBOBJECTS))
TSAN_TEST_BINARIES := \
	$(foreach t,$(TSAN_TEST_NAMES),$(TSAN_APP_DIR)/$(t)$(EXE_EXTENSION))
TSAN_CUTILLIBRARY := -L $(TSAN_APP_DIR) -l$(SUITE)-$(PROJECT)$(BRANCH)-tsan

# Deliberately NOT preloaded, unlike the ASan runtime.
#
# The two sanitizers look alike here but are not. ASan checks its own
# initialisation order and refuses to start if anything got in front of it, so
# an LD_PRELOAD inherited from a desktop session has to be replaced; that is
# what the ASAN_RUNTIME comment describes. TSan makes no such check. Being
# first in the executable's own NEEDED list is enough, and it stays first
# whatever the environment preloads - verified by planting a race and watching
# TSan still report it with an unrelated .so preloaded.
#
# Preloading it is therefore not merely redundant, it is harmful: the shell
# that system() spawns inherits the preload into an uninstrumented binary and
# dies of SIGSEGV with no diagnostic at all, so system() returns a raw wait
# status of 11 whatever it was asked to run. Nothing in cutil's suite execs
# today - test/test-thread.cpp forks and _exit()s without one - so this is a
# trap laid for the first test that shells out rather than a present failure.
# compress hit it for real: 135 failures, none of them races (compress e035f44).
#
# LD_PRELOAD is cleared rather than left unset so that a value inherited from
# the environment cannot reintroduce the problem.

$(TSAN_OBJ_DIR)/%.o: src/%.c Makefile \
		| $(BUILD_DIR)/include/$(SUITE)/$(PROJECT)/float.h \
		  $(BUILD_DIR)/include/$(SUITE)/$(PROJECT)/libver_gen.h
	@printf "\n### Compiling (TSan): $@ ###\n"
	@mkdir -p $(@D)
	$(CC) $(TSAN_CFLAGS) $(INCLUDE) -c $< -o $@ $(OS_SPECIFIC_COMPILE_FLAGS)

$(TSAN_OBJ_DIR)/hash.o: src/hash.template.c
$(TSAN_OBJ_DIR)/vector.o: src/vector.template.c

$(TSAN_APP_DIR)/$(TSAN_TARGET): $(TSAN_LIBOBJECTS)
	@printf "\n### Linking (TSan) $@ ###\n"
	@mkdir -p $(@D)
	$(CC) $(TSAN_CFLAGS) $(OS_SPECIFIC_LINK_FLAGS) -o $@ $^ $(TSAN_LDFLAGS)

define TSAN_TEST_RULE
$(TSAN_APP_DIR)/$(1)$(EXE_EXTENSION): test/$(1).cpp \
		| $(TSAN_APP_DIR)/$(TSAN_TARGET)
	@printf "\n### Compiling (TSan) $$@ ###\n"
	@mkdir -p $$(@D)
	$$(CXX) $$(TSAN_CXXFLAGS) $$(INCLUDE) -I test/ -o $$@ $$< $$(TSAN_LDFLAGS) \
		$$(TESTFLAGS) $$(TSAN_CUTILLIBRARY)
endef
$(foreach t,$(TSAN_TEST_NAMES),$(eval $(call TSAN_TEST_RULE,$(t))))

test-tsan: ## Make and run the concurrency tests under ThreadSanitizer
test-tsan: $(TSAN_APP_DIR)/$(TSAN_TARGET) $(TSAN_TEST_BINARIES)
ifeq ($(OS_NAME), Linux)
	@printf "\033[0;36m"
	@printf "####################################\n"
	@printf "### Running tests with TSan      ###\n"
	@printf "####################################\n"
	@printf "\033[0m"
	@for t in $(TSAN_TEST_BINARIES); do \
		printf "\n--- $$t ---\n"; \
		env LD_LIBRARY_PATH="$(TSAN_APP_DIR)" LD_PRELOAD= \
			TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1 \
			$$t --gtest_brief=1 || exit 1; \
	done
	@printf "\033[0;32m\nAll tests passed with TSan.\033[0m\n"
else
	@printf "\033[0;31mSanitizer builds are currently only supported on Linux.\033[0m\n"
	@exit 1
endif


clean: ## Remove all contents of the build directories.
# The sanitizer tree is removed too. It is a sibling of the ordinary build
# directory rather than a child, so a clean that names only the ordinary one
# leaves instrumented objects behind - and they are the ones a stale-binary
# mistake is hardest to notice with, because they still run.
	-@rm -rvf $(OBJ_DIR)/*
	-@rm -rvf $(APP_DIR)/*
	-@rm -rvf $(GEN_DIR)/*
	-@rm -rvf $(ASAN_BUILD_DIR)
	-@rm -rvf $(TSAN_BUILD_DIR)
	-@rm -f include/$(SUITE)/$(PROJECT)/float.h

# Files will be as follows:
# /usr/local/lib/(SUITE)/
#   lib(SUITE)-(PROJECT)(BRANCH).so.(MAJOR).(MINOR)
#   lib(SUITE)-(PROJECT)(BRANCH).so.(MAJOR) link to previous
#   lib(SUITE)-(PROJECT)(BRANCH).so link to previous
# Where the dynamic loader configuration fragment goes. Overridable so a
# staged or user-prefix install has somewhere to write it; the default is the
# system location, which is what an ordinary `sudo make install` uses.
LDCONF_INSTALL_PATH ?= /etc/ld.so.conf.d

# Where this project's own .pc file is installed. Defaults to the directory
# pkg-config is already being told to search, but separate from it so a
# staged install can write somewhere else without also redirecting lookups.
PKGCONFIG_INSTALL_PATH ?= $(PKG_CONFIG_PATH)

# $(LDCONF_INSTALL_PATH)/(SUITE)-(PROJECT)(BRANCH).conf will point to $(LIB_INSTALL_PATH)/(SUITE)
# /usr/local/include/(SUITE)/(PROJECT)(BRANCH)/
#   * copied from ./include/
# /usr/local/share/pkgconfig
#   (SUITE)-(PROJECT)(BRANCH).pc created

install: ## Install the library globally, requires sudo
install: all
	# Installing the shared library.
	@mkdir -p $(LIB_INSTALL_PATH)/$(SUITE)
ifeq ($(OS_NAME), Linux)
# Install the .so file
	@cp $(APP_DIR)/$(TARGET) $(LIB_INSTALL_PATH)/$(SUITE)/
	@ln -f -s $(TARGET) $(LIB_INSTALL_PATH)/$(SUITE)/$(SO_NAME)
	@ln -f -s $(SO_NAME) $(LIB_INSTALL_PATH)/$(SUITE)/$(BASE_NAME)
	@if [ -n "$(LDCONF_INSTALL_PATH)" ]; then mkdir -p $(LDCONF_INSTALL_PATH); fi
	@if [ -n "$(LDCONF_INSTALL_PATH)" ]; then echo "$(LIB_INSTALL_PATH)/$(SUITE)" > $(LDCONF_INSTALL_PATH)/$(SUITE)-$(PROJECT)$(BRANCH).conf; fi
endif
ifeq ($(OS_NAME), Windows)
# The .dll file and the .dll.a file
	@mkdir -p $(BIN_INSTALL_PATH)/$(SUITE)
	@cp $(APP_DIR)/$(TARGET).a $(LIB_INSTALL_PATH)
	@cp $(APP_DIR)/$(TARGET) $(BIN_INSTALL_PATH)
endif
	# Installing the headers.
	# Removed first: this directory is owned entirely by this project and
	# branch, and copying over the top of it would leave headers behind that
	# have since been renamed or deleted.
	@rm -rf $(INCLUDE_INSTALL_PATH)/$(SUITE)/$(PROJECT)$(BRANCH)
	@mkdir -p $(INCLUDE_INSTALL_PATH)/$(SUITE)/$(PROJECT)$(BRANCH)
	@cd include &&	find . -name "*.h" -exec cp --parents '{}' $(INCLUDE_INSTALL_PATH)/$(SUITE)/$(PROJECT)$(BRANCH)/ \;
	@cd $(BUILD_DIR)/include &&	find . -name "*.h" -exec cp --parents '{}' $(INCLUDE_INSTALL_PATH)/$(SUITE)/$(PROJECT)$(BRANCH)/ \;
	# Installing the pkg-config files.
	@mkdir -p $(PKGCONFIG_INSTALL_PATH)
	@cat pkgconfig/$(SUITE)-$(PROJECT).pc | sed 's/(SUITE)/$(SUITE)/g; s/(PROJECT)/$(PROJECT)/g; s/(BRANCH)/$(BRANCH)/g; s/(VERSION)/$(VERSION)/g; s|(PC_LIB_DIR)|$(PC_LIB_DIR)|g; s|(PC_INCLUDE_DIR)|$(PC_INCLUDE_DIR)|g' > $(PKGCONFIG_INSTALL_PATH)/$(SUITE)-$(PROJECT)$(BRANCH).pc
	@echo "  pkg-config name: $(SUITE)-$(PROJECT)$(BRANCH)  (use: pkg-config --cflags --libs $(SUITE)-$(PROJECT)$(BRANCH))"
ifeq ($(OS_NAME), Linux)
	# Running ldconfig.
	@if [ -n "$(LDCONF_INSTALL_PATH)" ]; then ldconfig >> /dev/null 2>&1; fi
endif
	@echo "Ghoti.io $(PROJECT)$(BRANCH) installed"

uninstall: ## Delete the globally-installed files.  Requires sudo.
	# Deleting the shared library.
ifeq ($(OS_NAME), Linux)
	@rm -f $(LIB_INSTALL_PATH)/$(SUITE)/$(BASE_NAME)*
	# Deleting the ld configuration file.
	@rm -f $(LDCONF_INSTALL_PATH)/$(SUITE)-$(PROJECT)$(BRANCH).conf
endif
ifeq ($(OS_NAME), Windows)
	@rm -f $(LIB_INSTALL_PATH)/$(TARGET).a
	@rm -f $(BIN_INSTALL_PATH)/$(TARGET)
endif
	# Deleting the headers.
	@rm -rf $(INCLUDE_INSTALL_PATH)/$(SUITE)/$(PROJECT)$(BRANCH)
	# Deleting the pkg-config files.
	@rm -f $(PKGCONFIG_INSTALL_PATH)/$(SUITE)-$(PROJECT)$(BRANCH).pc
	# Cleaning up (potentially) no longer needed directories.
	@rmdir --ignore-fail-on-non-empty $(INCLUDE_INSTALL_PATH)/$(SUITE)
	@rmdir --ignore-fail-on-non-empty $(LIB_INSTALL_PATH)/$(SUITE)
ifeq ($(OS_NAME), Linux)
	# Running ldconfig.
	@if [ -n "$(LDCONF_INSTALL_PATH)" ]; then ldconfig >> /dev/null 2>&1; fi
endif
	@echo "Ghoti.io $(PROJECT)$(BRANCH) has been uninstalled"

debug: ## Build the shared library in DEBUG mode
	make all BUILD=debug

watch-debug: ## Watch the file directory for changes and compile the target in DEBUG mode
	make watch BUILD=debug

test-watch-debug: ## Watch the file directory for changes and run the unit tests in DEBUG mode
	make test-watch BUILD=debug

test-debug: ## Make and run the Unit tests in DEBUG mode
	make test BUILD=debug

install-debug: ## Install the DEBUG library globally, requires sudo
	make install BUILD=debug

uninstall-debug: ## Delete the DEBUG globally-installed files.  Requires sudo.
	make uninstall BUILD=debug

docs: ## Generate the documentation in the ./docs subdirectory
	doxygen

docs-pdf: docs ## Generate the documentation as a pdf, at ./docs/(SUITE)-(PROJECT)(BRANCH).pdf
	cd ./docs/latex/ && make
	mv -f ./docs/latex/refman.pdf ./docs/$(SUITE)-$(PROJECT)$(BRANCH)-docs.pdf

cloc: ## Count the lines of code used in the project
	cloc src include test Makefile

coverage: ## Build instrumented, run the tests, and report line coverage
# Cleans first because the object files would otherwise be reused without the
# instrumentation, then cleans and rebuilds at the end: leaving the
# instrumented objects behind would have a later `make` silently link them,
# and leaving the tree cleaned would break any sibling project that links
# this one. The cost is one extra build; coverage is not run often.
	@$(MAKE) --no-print-directory clean > /dev/null
# The instrumented build, the report and the restoration of the tree are one
# shell command so that the cleanup runs whatever fails. Letting a failure
# stop the recipe leaves the --coverage objects in build/, and the next
# ordinary `make` links them into a library that needs the gcov runtime; every
# later build then fails with undefined references to __gcov_init until
# somebody works out why.
#
# TEST_GATES is cleared because --coverage links the gcov runtime, which
# exports mangle_path. check-symbols is right to reject that in a shipping
# build and wrong to reject it here, and it made this target fail before it
# ever produced a report.
	@status=0; \
	$(MAKE) --no-print-directory test TEST_GATES= \
		EXTRA_CFLAGS="--coverage -O0" \
		EXTRA_LDFLAGS="--coverage" > /dev/null || status=$$?; \
	if [ $$status -eq 0 ]; then \
		tools/coverage.sh $(OBJ_DIR) || status=$$?; \
	else \
		printf "coverage: the instrumented test run failed; no report\n" >&2; \
	fi; \
	$(MAKE) --no-print-directory clean > /dev/null; \
	$(MAKE) --no-print-directory all > /dev/null; \
	exit $$status

help: ## Display this help
# Scan only this makefile. $(MAKEFILE_LIST) grows to include every generated
# .d file once the project has been built, and grep prefixes each match with
# a filename when given more than one file - so every target name in the
# output became "Makefile".
	@grep -E '^[ a-zA-Z_-]+:.*?## .*$$' $(firstword $(MAKEFILE_LIST)) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "%-15s %s\n", $$1, $$2}' | sed "s/(SUITE)/$(SUITE)/g; s/(PROJECT)/$(PROJECT)/g; s/(BRANCH)/$(BRANCH)/g"

