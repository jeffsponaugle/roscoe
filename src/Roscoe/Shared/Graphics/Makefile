
# TOP is relpath to get from this directory to the project-root directory
TOP=../..
# THISDIR is the path from $(TOP) to this directory
THISDIR=Shared/Graphics
# List of subdirectories (of this dir) that participate in the build
SUBDIRS=

# Include top-level Makefile.vars (include 1st so vars can be overridden)
include $(TOP)/Makefile.vars

# ---- Regular rules and targets go below this line ----

libsharedgraphics_OBJS=Control.o ControlButton.o ControlComboBox.o ControlHit.o \
	ControlImage.o ControlLineInput.o ControlListBox.o ControlSlider.o \
	ControlText.o Graphics.o Window.o Font.o

libsharedgraphics_INCLUDE_DIRS=$(TOP) ../freetype2/include/freetype/config ../freetype2/include

$(eval $(call LIB_TARGET,libsharedgraphics))

# ---- End Regular rules and targets ----

# Include project rules & compiler-generated deps for each DEPFILES .d file
include $(TOP)/Makefile.rules

