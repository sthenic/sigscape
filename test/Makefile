CXX = g++

# Project variables
PROJECT = test

# Directories
BUILDDIR = build
SOURCEDIR = ../src

DEFINES = \
	NO_ADQAPI

CXXFLAGS += \
	-std=c++17 \
	-g3 \
	-MMD \
	-DLINUX \
	-Wall -Wextra \
	-Wswitch -Wswitch-default -Wswitch-enum \
	-Werror=overflow \
	$(patsubst %,-D%,$(DEFINES))

LDFLAGS += \
	-g3 \
	-L../../cJSON/build \
	-Wl,-rpath,../../cJSON/build \
	-lpthread \
	-lcjson \
	-lCppUTest

INCLUDE_DIRS = \
	../simple_fft \
	../include \
	../../cJSON

CXXFLAGS += \
	$(patsubst %,-I%,$(INCLUDE_DIRS))

SOURCES = \
	src/generator.cpp \
	src/data_processing.cpp \
 	src/digitizer.cpp \
	src/mock_adqapi.cpp \
	src/mock_digitizer.cpp \
 	src/file_watcher.cpp \
 	src/window.cpp \
	tgenerator.cpp \
	tdata_processing.cpp \
 	tthread_safe_queue.cpp \
 	tmessage_thread.cpp \
 	tfile_watcher.cpp \
 	tdigitizer.cpp \
	main.cpp

OBJECTS = $(patsubst %,$(BUILDDIR)/%,$(SOURCES:.cpp=.o))
DEPENDS = $(OBJECTS:.o=.d)
BIN = $(BUILDDIR)/$(PROJECT)

.PHONY: all clean

all: $(BUILDDIR) $(BIN)
test: all

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

# Rule to create object files from .cpp files relative to the project root.
$(BUILDDIR)/%.o: %.cpp
	@echo -- Compiling $@ --
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
	@echo

# Rule to create object files from .cpp files relative to SOURCEDIR.
$(BUILDDIR)/src/%.o: $(SOURCEDIR)/%.cpp
	@echo -- Compiling foo $@ --
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
	@echo

$(BIN): $(OBJECTS)
	@echo -- Linking objects to $@ --
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@
	@echo

test:
	@echo -- Running test suite --
	@$(BIN) -c -v
	@echo

clean:
	@rm -rf $(BUILDDIR)

-include $(DEPENDS)
