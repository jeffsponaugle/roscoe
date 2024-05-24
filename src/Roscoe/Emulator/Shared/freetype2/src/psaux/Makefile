
# TOP is relpath to get from this directory to the project-root directory
TOP=../../../..
# THISDIR is the path from $(TOP) to this directory
THISDIR=Shared/freetype2/src/psaux
# List of subdirectories (of this dir) that participate in the build
SUBDIRS=

# Include top-level Makefile.vars (include 1st so vars can be overridden)
include $(TOP)/Makefile.vars

# ---- Regular rules and targets go below this line ----

libsharedfreetype2srcpsaux_OBJS=afmparse.o cffdecode.o psarrst.o psauxmod.o psblues.o \
		 psconv.o pserror.o psfont.o psft.o pshints.o psintrp.o psobjs.o psread.o psstack.o t1cmap.o t1decode.o

libsharedfreetype2srcpsaux_INCLUDE_DIRS=$(TOP) ../../include

$(eval $(call LIB_TARGET,libsharedfreetype2srcpsaux))

# ---- End Regular rules and targets ----

# Include project rules & compiler-generated deps for each DEPFILES .d file
include $(TOP)/Makefile.rules

