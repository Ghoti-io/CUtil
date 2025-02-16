CXX := g++
CXXFLAGS := -pedantic-errors -Wall -Wextra -Werror -Wno-error=unused-function -Wfatal-errors -std=c++20 -O1 -g
CC := cc
CFLAGS := -pedantic-errors -Wall -Wextra -Werror -Wno-error=unused-function -Wfatal-errors -std=c17 -O3 -g
# -DGHOTIIO_CUTIL_ENABLE_MEMORY_DEBUG
LDFLAGS := -L /usr/lib -lstdc++ -lm
BUILD ?= release
BUILD_DIR := ./build/$(BUILD)
OBJ_DIR := $(BUILD_DIR)/objects
GEN_DIR := $(BUILD_DIR)/generated
APP_DIR := $(BUILD_DIR)/apps

SUITE := ghoti.io
PROJECT := cutil

BRANCH := -dev
# If BUILD is debug, append -debug
ifeq ($(BUILD),debug)
    BRANCH := $(BRANCH)-debug
endif

BASE_NAME_PREFIX := lib$(SUITE)-$(PROJECT)$(BRANCH)
BASE_NAME := $(BASE_NAME_PREFIX).so
MAJOR_VERSION := 0
MINOR_VERSION := 0.0
SO_NAME := $(BASE_NAME).$(MAJOR_VERSION)


# Detect OS
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), Linux)
	OS_NAME := Linux
	LIB_EXTENSION := so
	OS_SPECIFIC_CXX_FLAGS := -shared -fPIC
	OS_SPECIFIC_LIBRARY_NAME_FLAG := -Wl,-soname,$(SO_NAME)
	TARGET := $(SO_NAME).$(MINOR_VERSION)
	EXE_EXTENSION :=
	# Additional Linux-specific variables
	PKG_CONFIG_PATH := /usr/local/share/pkgconfig
	INCLUDE_INSTALL_PATH := /usr/local/include
	LIB_INSTALL_PATH := /usr/local/lib

else ifeq ($(UNAME_S), Darwin)
	OS_NAME := Mac
	LIB_EXTENSION := dylib
	OS_SPECIFIC_CXX_FLAGS := -shared
	OS_SPECIFIC_LIBRARY_NAME_FLAG := -Wl,-install_name,$(BASE_NAME_PREFIX).dylib
	TARGET := $(BASE_NAME_PREFIX).dylib
	EXE_EXTENSION :=
	# Additional macOS-specific variables

else ifeq ($(findstring MINGW32_NT,$(UNAME_S)),MINGW32_NT)  # 32-bit Windows
	OS_NAME := Windows
	LIB_EXTENSION := dll
	OS_SPECIFIC_CXX_FLAGS := -shared
	OS_SPECIFIC_LIBRARY_NAME_FLAG := -Wl,--out-implib,$(APP_DIR)/$(BASE_NAME_PREFIX).dll.a
	TARGET := $(BASE_NAME_PREFIX).dll
	EXE_EXTENSION := .exe
	# Additional Windows-specific variables
	# This is the path to the pkg-config files on MSYS2
	PKG_CONFIG_PATH := /mingw32/lib/pkgconfig
	INCLUDE_INSTALL_PATH := /mingw32/include
	LIB_INSTALL_PATH := /mingw32/lib
	BIN_INSTALL_PATH := /mingw32/bin

else ifeq ($(findstring MINGW64_NT,$(UNAME_S)),MINGW64_NT)  # 64-bit Windows
	OS_NAME := Windows
	LIB_EXTENSION := dll
	OS_SPECIFIC_CXX_FLAGS := -shared
	OS_SPECIFIC_LIBRARY_NAME_FLAG := -Wl,--out-implib,$(APP_DIR)/$(BASE_NAME_PREFIX).dll.a
	TARGET := $(BASE_NAME_PREFIX).dll
	EXE_EXTENSION := .exe
	# Additional Windows-specific variables
	# This is the path to the pkg-config files on MSYS2
	PKG_CONFIG_PATH := /mingw64/lib/pkgconfig
	INCLUDE_INSTALL_PATH := /mingw64/include
	LIB_INSTALL_PATH := /mingw64/lib
	BIN_INSTALL_PATH := /mingw64/bin

else
    $(error Unsupported OS: $(UNAME_S))

endif


INCLUDE := -I include/ -I $(BUILD_DIR)/include/
LIBOBJECTS := \
  $(OBJ_DIR)/debug.o \
	$(OBJ_DIR)/hash.o \
	$(OBJ_DIR)/memory.o \
	$(OBJ_DIR)/random.o \
	$(OBJ_DIR)/semaphore.o \
	$(OBJ_DIR)/string.o \
	$(OBJ_DIR)/thread.o \
	$(OBJ_DIR)/type.o \
	$(OBJ_DIR)/vector.o

TESTFLAGS := `PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs --cflags gtest`


CUTILLIBRARY := -L $(APP_DIR) -l$(SUITE)-$(PROJECT)$(BRANCH)


all: $(APP_DIR)/$(TARGET) ## Build the shared library

####################################################################
# Dependency Variables
####################################################################
DEP_LIBVER = \
  include/$(PROJECT)/libver.h
DEP_MUTEX = \
	include/$(PROJECT)/mutex.h
DEP_SEMAPHORE = \
	$(DEP_LIBVER) \
	include/$(PROJECT)/semaphore.h
DEP_DEBUG = \
	$(DEP_LIBVER) \
	include/$(PROJECT)/debug.h
DEP_MEMORY = \
	$(DEP_LIBVER) \
	include/$(PROJECT)/memory.h
DEP_FLOAT = \
	$(DEP_LIBVER) \
	$(BUILD_DIR)/include/$(PROJECT)/float.h
DEP_TYPE = \
	$(DEP_FLOAT) \
	include/$(PROJECT)/type.h
DEP_HASH= \
	$(DEP_TYPE) \
	$(DEP_MEMORY) \
	$(DEP_MUTEX) \
	include/$(PROJECT)/hash.h
DEP_RANDOM = \
	$(DEP_LIBVER) \
	include/$(PROJECT)/random.h
DEP_THREAD = \
	$(DEP_LIBVER) \
	$(DEP_HASH) \
	include/$(PROJECT)/thread.h
DEP_VECTOR= \
	$(DEP_TYPE) \
	$(DEP_MEMORY) \
	$(DEP_MUTEX) \
	include/$(PROJECT)/vector.h
DEP_STRING = \
	$(DEP_LIBVER) \
	include/$(PROJECT)/string.h

####################################################################
# Floating Point Type Identification
####################################################################
FLOAT_IDENTIFIER := $(APP_DIR)/float_identifier$(EXE_EXTENSION)
$(FLOAT_IDENTIFIER): \
		src/float_identifier.c \
		src/float.h.template
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/include/$(PROJECT):
	@mkdir -p $@

$(BUILD_DIR)/include/$(PROJECT)/float.h: \
		src/float.h.template \
		$(FLOAT_IDENTIFIER) \
		$(BUILD_DIR)/include/$(PROJECT)
	cat src/float.h.template | sed "s/FLOAT32/$(shell $(FLOAT_IDENTIFIER) 32)/; s/FLOAT64/$(shell $(FLOAT_IDENTIFIER) 64)/" > $@

####################################################################
# Object Files
####################################################################

$(LIBOBJECTS) :
	@printf "\n### Compiling $@ ###\n"
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -MMD -o $@ $(OS_SPECIFIC_CXX_FLAGS)

$(OBJ_DIR)/debug.o: \
	src/debug.c \
	$(DEP_DEBUG)

$(OBJ_DIR)/hash.o: \
	src/hash.c \
	src/hash.template.c \
	$(DEP_HASH)

$(OBJ_DIR)/memory.o: \
	src/memory.c \
	$(DEP_MEMORY)

$(OBJ_DIR)/random.o: \
	src/random.c \
	$(DEP_RANDOM)

$(OBJ_DIR)/semaphore.o: \
	src/semaphore.c \
	$(DEP_SEMAPHORE)

$(OBJ_DIR)/string.o: \
	src/string.c \
	$(DEP_STRING)

$(OBJ_DIR)/thread.o: \
	src/thread.c \
	$(DEP_THREAD)

$(OBJ_DIR)/type.o: \
	src/type.c \
	$(DEP_TYPE)

$(OBJ_DIR)/vector.o: \
	src/vector.c \
	src/vector.template.c \
	$(DEP_VECTOR)

####################################################################
# Shared Library
####################################################################

$(APP_DIR)/$(TARGET): \
		$(LIBOBJECTS)
	@printf "\n### Compiling Ghoti.io CUtil Shared Library ###\n"
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(OS_SPECIFIC_CXX_FLAGS) -o $@ $^ $(LDFLAGS) $(OS_SPECIFIC_LIBRARY_NAME_FLAG)

ifeq ($(OS_NAME), Linux)
	@ln -f -s $(TARGET) $(APP_DIR)/$(SO_NAME)
	@ln -f -s $(SO_NAME) $(APP_DIR)/$(BASE_NAME)
endif

####################################################################
# Unit Tests
####################################################################

$(APP_DIR)/test-debug$(EXE_EXTENSION): \
		test/test-debug.cpp \
		$(DEP_DEBUG)
	@printf "\n### Compiling Debug Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-type$(EXE_EXTENSION): \
		test/test-type.cpp \
		$(DEP_TYPE)
	@printf "\n### Compiling Types Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-memory$(EXE_EXTENSION): \
		test/test-memory.cpp \
		$(DEP_MEMORY)
	@printf "\n### Compiling Memory Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-hash$(EXE_EXTENSION): \
		test/test-hash.cpp \
		$(DEP_HASH)
	@printf "\n### Compiling Hash Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-random$(EXE_EXTENSION): \
		test/test-random.cpp \
		$(DEP_RANDOM)
	@printf "\n### Compiling Random Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-semaphore$(EXE_EXTENSION): \
		test/test-semaphore.cpp \
		$(DEP_SEMAPHORE)
	@printf "\n### Compiling Semaphore Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-string$(EXE_EXTENSION): \
		test/test-string.cpp \
		$(DEP_STRING)
	@printf "\n### Compiling String Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-thread$(EXE_EXTENSION): \
		test/test-thread.cpp \
		$(DEP_THREAD)
	@printf "\n### Compiling Thread Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-vector$(EXE_EXTENSION): \
		test/test-vector.cpp \
		$(DEP_VECTOR)
	@printf "\n### Compiling Vector Test ###\n"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

####################################################################
# Commands
####################################################################

# General commands
.PHONY: clean cloc docs docs-pdf
# Release build commands
.PHONY: all install test test-watch uninstall watch
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

test: ## Make and run the Unit tests
test: \
		$(APP_DIR)/$(TARGET) \
		$(APP_DIR)/test-debug$(EXE_EXTENSION) \
		$(APP_DIR)/test-memory$(EXE_EXTENSION) \
		$(APP_DIR)/test-type$(EXE_EXTENSION) \
		$(APP_DIR)/test-random$(EXE_EXTENSION) \
		$(APP_DIR)/test-semaphore$(EXE_EXTENSION) \
		$(APP_DIR)/test-string$(EXE_EXTENSION) \
		$(APP_DIR)/test-hash$(EXE_EXTENSION) \
		$(APP_DIR)/test-thread$(EXE_EXTENSION) \
		$(APP_DIR)/test-vector$(EXE_EXTENSION)
	@printf "\033[0;32m"
	@printf "############################\n"
	@printf "### Running normal tests ###\n"
	@printf "############################\n"
	@printf "\033[0m"
	env LD_LIBRARY_PATH="$(APP_DIR)" $(APP_DIR)/test-debug --gtest_brief=1
	env LD_LIBRARY_PATH="$(APP_DIR)" $(APP_DIR)/test-memory --gtest_brief=1
	env LD_LIBRARY_PATH="$(APP_DIR)" $(APP_DIR)/test-semaphore --gtest_brief=1
	env LD_LIBRARY_PATH="$(APP_DIR)" $(APP_DIR)/test-hash --gtest_brief=1
	env LD_LIBRARY_PATH="$(APP_DIR)" $(APP_DIR)/test-thread --gtest_brief=1
	env LD_LIBRARY_PATH="$(APP_DIR)" $(APP_DIR)/test-type --gtest_brief=1
	env LD_LIBRARY_PATH="$(APP_DIR)" $(APP_DIR)/test-random --gtest_brief=1
	env LD_LIBRARY_PATH="$(APP_DIR)" $(APP_DIR)/test-string --gtest_brief=1
	env LD_LIBRARY_PATH="$(APP_DIR)" $(APP_DIR)/test-vector --gtest_brief=1

clean: ## Remove all contents of the build directories.
	-@rm -rvf ./build
	-@rm include/$(PROJECT)/float.h

# Files will be as follows:
# /usr/local/lib/(SUITE)/
#   lib(SUITE)-(PROJECT)(BRANCH).so.(MAJOR).(MINOR)
#   lib(SUITE)-(PROJECT)(BRANCH).so.(MAJOR) link to previous
#   lib(SUITE)-(PROJECT)(BRANCH).so link to previous
# /etc/ld.so.conf.d/(SUITE)-(PROJECT)(BRANCH).conf will point to /usr/local/lib/(SUITE)
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
	@echo "/usr/local/lib/$(SUITE)" > /etc/ld.so.conf.d/$(SUITE)-$(PROJECT)$(BRANCH).conf
endif
ifeq ($(OS_NAME), Windows)
# The .dll file and the .dll.a file
	@mkdir -p $(BIN_INSTALL_PATH)/$(SUITE)
	@cp $(APP_DIR)/$(TARGET).a $(LIB_INSTALL_PATH)
	@cp $(APP_DIR)/$(TARGET) $(BIN_INSTALL_PATH)
endif
	# Installing the headers.
	@mkdir -p $(INCLUDE_INSTALL_PATH)/$(SUITE)/$(PROJECT)$(BRANCH)
	@cd include &&	find . -name "*.h" -exec cp --parents '{}' $(INCLUDE_INSTALL_PATH)/$(SUITE)/$(PROJECT)$(BRANCH)/ \;
	@cd $(BUILD_DIR)/include &&	find . -name "*.h" -exec cp --parents '{}' $(INCLUDE_INSTALL_PATH)/$(SUITE)/$(PROJECT)$(BRANCH)/ \;
	# Installing the pkg-config files.
	@mkdir -p $(PKG_CONFIG_PATH)
	@cat pkgconfig/$(SUITE)-$(PROJECT).pc | sed 's/(SUITE)/$(SUITE)/g; s/(PROJECT)/$(PROJECT)/g; s/(BRANCH)/$(BRANCH)/g; s/(VERSION)/$(VERSION)/g; s|(LIB)|$(LIB_INSTALL_PATH)|g; s|(INCLUDE)|$(INCLUDE_INSTALL_PATH)|g' > $(PKG_CONFIG_PATH)/$(SUITE)-$(PROJECT)$(BRANCH).pc
ifeq ($(OS_NAME), Linux)
	# Running ldconfig.
	@ldconfig >> /dev/null 2>&1
endif
	@echo "Ghoti.io $(PROJECT)$(BRANCH) installed"

uninstall: ## Delete the globally-installed files.  Requires sudo.
	# Deleting the shared library.
ifeq ($(OS_NAME), Linux)
	@rm -f $(LIB_INSTALL_PATH)/$(SUITE)/$(BASE_NAME)*
	# Deleting the ld configuration file.
	@rm -f /etc/ld.so.conf.d/$(SUITE)-$(PROJECT)$(BRANCH).conf
endif
ifeq ($(OS_NAME), Windows)
	@rm -f $(LIB_INSTALL_PATH)/$(TARGET).a
	@rm -f $(BIN_INSTALL_PATH)/$(TARGET)
endif
	# Deleting the headers.
	@rm -rf $(INCLUDE_INSTALL_PATH)/$(SUITE)/$(PROJECT)$(BRANCH)
	# Deleting the pkg-config files.
	@rm -f $(PKG_CONFIG_PATH)/$(SUITE)-$(PROJECT)$(BRANCH).pc
	# Cleaning up (potentially) no longer needed directories.
	@rmdir --ignore-fail-on-non-empty $(INCLUDE_INSTALL_PATH)/$(SUITE)
	@rmdir --ignore-fail-on-non-empty $(LIB_INSTALL_PATH)/$(SUITE)
ifeq ($(OS_NAME), Linux)
	# Running ldconfig.
	@ldconfig >> /dev/null 2>&1
endif
	@echo "Ghoti.io $(PROJECT)$(BRANCH) has been uninstalled"

all-debug: ## Build the shared library in DEBUG mode
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

help: ## Display this help
	@grep -E '^[ a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "%-15s %s\n", $$1, $$2}' | sed "s/(SUITE)/$(SUITE)/g; s/(PROJECT)/$(PROJECT)/g; s/(BRANCH)/$(BRANCH)/g"

