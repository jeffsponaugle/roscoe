
# TOP is relpath to get from this directory to the project-root directory
TOP=../../../..
# THISDIR is the path from $(TOP) to this directory
THISDIR=Shared/freetype2/src/truetype
# List of subdirectories (of this dir) that participate in the build
SUBDIRS=

# Include top-level Makefile.vars (include 1st so vars can be overridden)
include $(TOP)/Makefile.vars

# ---- Regular rules and targets go below this line ----

libsharedfreetype2srctruetype_OBJS=ttdriver.o ttgload.o ttgxvar.o ttinterp.o ttobjs.o ttpload.o ttsubpix.o

libsharedfreetype2srctruetype_INCLUDE_DIRS=$(TOP) ../../include

$(eval $(call LIB_TARGET,libsharedfreetype2srctruetype))

# ---- End Regular rules and targets ----

# Include project rules & compiler-generated deps for each DEPFILES .d file
include $(TOP)/Makefile.rules

