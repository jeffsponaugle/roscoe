
# TOP is relpath to get from this directory to the project-root directory
TOP=..
# THISDIR is the path from $(TOP) to this directory
THISDIR=OS
# List of subdirectories (of this dir) that participate in the build
SUBDIRS=FreeBSD

# Include top-level Makefile.vars (include 1st so vars can be overridden)
include $(TOP)/Makefile.vars

# ---- Regular rules and targets go below this line ----

libos_OBJS=OS.o File.o OSUNIX.o FileUNIX.o

$(eval $(call LIB_TARGET,libos))

# ---- End Regular rules and targets ----

# Include project rules & compiler-generated deps for each DEPFILES .d file
include $(TOP)/Makefile.rules

