
# TOP is relpath to get from this directory to the project-root directory
TOP=../..
# THISDIR is the path from $(TOP) to this directory
THISDIR=Shared/Sound
# List of subdirectories (of this dir) that participate in the build
SUBDIRS=

# Include top-level Makefile.vars (include 1st so vars can be overridden)
include $(TOP)/Makefile.vars

# ---- Regular rules and targets go below this line ----

libsharedsound_OBJS=Sound.o SoundWav.o SoundSDL.o

$(eval $(call LIB_TARGET,libsharedsound))

# ---- End Regular rules and targets ----

# Include project rules & compiler-generated deps for each DEPFILES .d file
include $(TOP)/Makefile.rules

