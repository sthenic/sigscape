CXX = g++

# Project variables
PROJECT = adq-rapid

# Directories
ROOTDIR = ../..
BUILDDIR = build

DEFINES = \
	IMGUI_IMPL_OPENGL_LOADER_GL3W

CFLAGS += \
	-O2 \
	-Wall -Wextra \
	-Wswitch -Wswitch-enum \
	-Werror=overflow \
	$(patsubst %,-D%,$(DEFINES))

LDFLAGS += \
	-lGL \
	-lpthread \
	`pkg-config --static --libs glfw3`

INCLUDE_DIRS = \
	implot \
	imgui \
	simple_fft \
	lib \
	include

CFLAGS += \
	`pkg-config --cflags glfw3` \
	$(patsubst %,-I%,$(INCLUDE_DIRS))

CXXSOURCES = \
	implot/implot.cpp \
	implot/implot_items.cpp \
	implot/implot_demo.cpp \
	imgui/imgui.cpp \
	imgui/imgui_draw.cpp \
	imgui/imgui_tables.cpp \
	imgui/imgui_widgets.cpp \
	imgui/imgui_demo.cpp \
	imgui/backends/imgui_impl_glfw.cpp \
	imgui/backends/imgui_impl_opengl3.cpp \
	src/simulated_data_acquisition.cpp \
	src/processing.cpp \
	src/main.cpp

CSOURCES = \
	lib/GL/gl3w.c

OBJECTS = $(patsubst %,$(BUILDDIR)/%,$(CXXSOURCES:.cpp=.o))
OBJECTS += $(patsubst %,$(BUILDDIR)/%,$(CSOURCES:.c=.o))
BIN = $(BUILDDIR)/$(PROJECT)

.PHONY: all clean

all: $(BUILDDIR) $(BIN)

$(BUILDDIR)/%.o: %.cpp
	@echo -- Compiling $@ --
	@mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) -c $< -o $@
	@echo

$(BUILDDIR)/%.o: %.c
	@echo -- Compiling $@ --
	@mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) -c $< -o $@
	@echo

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

$(BIN): $(OBJECTS)
	@echo -- Linking objects to $@ --
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	@rm -rf $(BUILDDIR)
