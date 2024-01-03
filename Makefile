CXX := g++
CXXFLAGS := -pedantic-errors -Wall -Wextra -Werror -Wno-error=unused-function -Wfatal-errors -std=c++20 -O1 -g
CC := cc
CFLAGS := -pedantic-errors -Wall -Wextra -Werror -Wno-error=unused-function -Wfatal-errors -std=c17 -O3 -g
LDFLAGS := -L /usr/lib -lstdc++ -lm
BUILD := ./build
OBJ_DIR := $(BUILD)/objects
GEN_DIR := $(BUILD)/generated
APP_DIR := $(BUILD)/apps

BRANCH := -dev
BASE_NAME := libghoti.io-cutil$(BRANCH).so
MAJOR_VERSION := 0
MINOR_VERSION := 0.0
SO_NAME := $(BASE_NAME).$(MAJOR_VERSION)
TARGET := $(SO_NAME).$(MINOR_VERSION)

INCLUDE := -I include/
LIBOBJECTS := $(OBJ_DIR)/debug.o \
							$(OBJ_DIR)/hash.o \
							$(OBJ_DIR)/memory.o \
							$(OBJ_DIR)/string.o \
							$(OBJ_DIR)/type.o \
							$(OBJ_DIR)/vector.o

TESTFLAGS := `pkg-config --libs --cflags gtest`


CUTILLIBRARY := -L $(APP_DIR) -lghoti.io-cutil$(BRANCH)


all: $(APP_DIR)/$(TARGET) ## Build the shared library

####################################################################
# Dependency Variables
####################################################################
DEP_LIBVER = \
  include/cutil/libver.h
DEP_DEBUG = \
	$(DEP_LIBVER) \
	include/cutil/debug.h
DEP_MEMORY = \
	$(DEP_LIBVER) \
	include/cutil/memory.h
DEP_FLOAT = \
	$(DEP_LIBVER) \
	include/cutil/float.h
DEP_TYPE = \
	$(DEP_FLOAT) \
	include/cutil/type.h
DEP_HASH= \
	$(DEP_TYPE) \
	$(DEP_MEMORY) \
	include/cutil/hash.h
DEP_VECTOR= \
	$(DEP_TYPE) \
	$(DEP_MEMORY) \
	include/cutil/vector.h
DEP_STRING = \
	$(DEP_LIBVER) \
	include/cutil/string.h

####################################################################
# Floating Point Type Identification
####################################################################
$(APP_DIR)/float_identifier: \
				src/float_identifier.c \
				src/float.h.template
	$(CC) $(CFLAGS) $< -o $@

include/cutil/float.h: \
				src/float.h.template \
				$(APP_DIR)/float_identifier
	$(APP_DIR)/float_identifier 16
	cat src/float.h.template | sed "s/FLOAT32/$(shell $(APP_DIR)/float_identifier 32)/; s/FLOAT64/$(shell $(APP_DIR)/float_identifier 64)/" > include/cutil/float.h

####################################################################
# Object Files
####################################################################

$(LIBOBJECTS) :
	@echo "\n### Compiling $@ ###"
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -MMD -o $@ -fPIC

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

$(OBJ_DIR)/string.o: \
				src/string.c \
				$(DEP_STRING)

$(OBJ_DIR)/type.o: \
				src/type.c \
				$(DEP_TYPE)

$(OBJ_DIR)/vector.o: \
				src/vector.c \
				src/vector.template.c \
				$(DEP_TYPE)

####################################################################
# Shared Library
####################################################################

$(APP_DIR)/$(TARGET): \
				$(LIBOBJECTS)
	@echo "\n### Compiling Ghoti.io CUtil Shared Library ###"
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS) -Wl,-soname,$(SO_NAME)
	@ln -f -s $(TARGET) $(APP_DIR)/$(SO_NAME)
	@ln -f -s $(SO_NAME) $(APP_DIR)/$(BASE_NAME)

####################################################################
# Unit Tests
####################################################################

$(APP_DIR)/test-debug: \
				test/test-debug.cpp \
				$(APP_DIR)/$(TARGET)
	@echo "\n### Compiling Debug Test ###"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-type: \
				test/test-type.cpp \
				$(APP_DIR)/$(TARGET)
	@echo "\n### Compiling Types Test ###"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-memory: \
				test/test-memory.cpp \
				$(APP_DIR)/$(TARGET)
	@echo "\n### Compiling Memory Test ###"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-hash: \
				test/test-hash.cpp \
				$(APP_DIR)/$(TARGET)
	@echo "\n### Compiling Hash Test ###"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-string: \
				test/test-string.cpp \
				$(APP_DIR)/$(TARGET)
	@echo "\n### Compiling String Test ###"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

$(APP_DIR)/test-vector: \
				test/test-vector.cpp \
				$(APP_DIR)/$(TARGET)
	@echo "\n### Compiling Vector Test ###"
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $< $(LDFLAGS) $(TESTFLAGS) $(CUTILLIBRARY)

####################################################################
# Commands
####################################################################

.PHONY: all clean cloc docs docs-pdf install test test-watch watch

watch: ## Watch the file directory for changes and compile the target
	@while true; do \
					make all; \
					echo "\033[0;32m"; \
					echo "#########################"; \
					echo "# Waiting for changes.. #"; \
					echo "#########################"; \
					echo "\033[0m"; \
					inotifywait -qr -e modify -e create -e delete -e move src include test Makefile --exclude '/\.'; \
					done

test-watch: ## Watch the file directory for changes and run the unit tests
	@while true; do \
					make test; \
					echo "\033[0;32m"; \
					echo "#########################"; \
					echo "# Waiting for changes.. #"; \
					echo "#########################"; \
					echo "\033[0m"; \
					inotifywait -qr -e modify -e create -e delete -e move src include test Makefile --exclude '/\.'; \
					done

test: ## Make and run the Unit tests
test: \
				$(APP_DIR)/test-debug \
				$(APP_DIR)/test-memory \
				$(APP_DIR)/test-type \
				$(APP_DIR)/test-string \
				$(APP_DIR)/test-hash \
				$(APP_DIR)/test-vector \
				$(APP_DIR/$(TARGET)
	@echo "\033[0;32m"
	@echo "############################"
	@echo "### Running normal tests ###"
	@echo "############################"
	@echo "\033[0m"
	env LD_LIBRARY_PATH="$(APP_DIR)" $(APP_DIR)/test-debug --gtest_brief=1
	env LD_LIBRARY_PATH="$(APP_DIR)" $(APP_DIR)/test-memory --gtest_brief=1
	env LD_LIBRARY_PATH="$(APP_DIR)" $(APP_DIR)/test-type --gtest_brief=1
	env LD_LIBRARY_PATH="$(APP_DIR)" $(APP_DIR)/test-string --gtest_brief=1
	env LD_LIBRARY_PATH="$(APP_DIR)" $(APP_DIR)/test-hash --gtest_brief=1
	env LD_LIBRARY_PATH="$(APP_DIR)" $(APP_DIR)/test-vector --gtest_brief=1

clean: ## Remove all contents of the build directories.
	-@rm -rvf $(OBJ_DIR)/*
	-@rm -rvf $(APP_DIR)/*
	-@rm -rvf $(GEN_DIR)/*
	-@rm include/cutil/float.h

# Files will be as follows:
# /usr/local/lib/ghoti.io/
#   libghoti.io-cutil(BRANCH).so.(MAJOR).(MINOR)
#   libghoti.io-cutil(BRANCH).so.(MAJOR) link to previous
#   libghoti.io-cutil(BRANCH).so link to previous
# /etc/ld.so.conf.d/ghoti.io-cutil(BRANCH).conf will point to /usr/local/lib/ghoti.io
# /usr/local/include/ghoti.io/cutil(BRANCH)
#   *.h copied from ./include/cutil
# /usr/local/share/pkgconfig
#   ghoti.io-cutil(BRANCH).pc created

install: ## Install the library globally, requires sudo
install: all
	# Installing the Shared Library
	@mkdir -p /usr/local/lib/ghoti.io
	@cp $(APP_DIR)/$(TARGET) /usr/local/lib/ghoti.io/
	@ln -f -s $(TARGET) /usr/local/lib/ghoti.io/$(SO_NAME)
	@ln -f -s $(SO_NAME) /usr/local/lib/ghoti.io/$(BASE_NAME)
	@echo "/usr/local/lib/ghoti.io" > /etc/ld.so.conf.d/ghoti.io-cutil$(BRANCH).conf
	# Installing the headers
	@mkdir -p /usr/local/include/ghoti.io/cutil$(BRANCH)
	@cp include/cutil/*.h /usr/local/include/ghoti.io/cutil$(BRANCH)
	# Installing the pkgconfig files
	@mkdir -p /usr/local/share/pkgconfig
	@ cat pkgconfig/ghoti.io-cutil.pc | sed 's/(BRANCH)/$(BRANCH)/g; s/(VERSION)/$(VERSION)/g' > /usr/local/share/pkgconfig/ghoti.io-cutil$(BRANCH).pc
	# Running ldconfig
	@ldconfig >> /dev/null 2>&1
	@echo "Ghoti.io CUtil$(BRANCH) installed"

docs: ## Generate the documentation in the ./docs subdirectory
	doxygen

docs-pdf: docs ## Generate the documentation as a pdf, in ./docs/cutil(BRANCH)-docs.pdf
	cd ./docs/latex/ && make
	mv -f ./docs/latex/refman.pdf ./docs/cutil$(BRANCH)-docs.pdf

cloc: ## Count the lines of code used in the project
	cloc src include test Makefile

help: ## Display this help
	@grep -E '^[ a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "%-15s %s\n", $$1, $$2}' | sed 's/(BRANCH)/$(BRANCH)/g'

