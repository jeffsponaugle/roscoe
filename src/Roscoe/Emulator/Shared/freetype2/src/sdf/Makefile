
# TOP is relpath to get from this directory to the project-root directory
TOP=../../../..
# THISDIR is the path from $(TOP) to this directory
THISDIR=Shared/freetype2/src/sdf
# List of subdirectories (of this dir) that participate in the build
SUBDIRS=

# Include top-level Makefile.vars (include 1st so vars can be overridden)
include $(TOP)/Makefile.vars

# ---- Regular rules and targets go below this line ----

libsharedfreetype2srcsdf_OBJS=ftbsdf.o ftsdf.o ftsdfcommon.o ftsdfrend.o

libsharedfreetype2srcsdf_INCLUDE_DIRS=$(TOP) ../../include

$(eval $(call LIB_TARGET,libsharedfreetype2srcsdf))

# ---- End Regular rules and targets ----

# Include project rules & compiler-generated deps for each DEPFILES .d file
include $(TOP)/Makefile.rules

