
# TOP is relpath to get from this directory to the project-root directory
TOP=../../../..
# THISDIR is the path from $(TOP) to this directory
THISDIR=Shared/freetype2/src/sfnt
# List of subdirectories (of this dir) that participate in the build
SUBDIRS=

# Include top-level Makefile.vars (include 1st so vars can be overridden)
include $(TOP)/Makefile.vars

# ---- Regular rules and targets go below this line ----

libsharedfreetype2srcsfnt_OBJS=pngshim.o sfdriver.o sfobjs.o sfwoff.o sfwoff2.o ttbdf.o ttcmap.o ttcolr.o ttcpal.o ttkern.o ttload.o ttmtx.o ttpost.o ttsbit.o woff2tags.o

libsharedfreetype2srcsfnt_INCLUDE_DIRS=$(TOP) ../../include

$(eval $(call LIB_TARGET,libsharedfreetype2srcsfnt))

# ---- End Regular rules and targets ----

# Include project rules & compiler-generated deps for each DEPFILES .d file
include $(TOP)/Makefile.rules

