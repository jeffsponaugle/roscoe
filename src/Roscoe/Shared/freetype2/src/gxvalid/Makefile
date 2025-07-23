
# TOP is relpath to get from this directory to the project-root directory
TOP=../../../..
# THISDIR is the path from $(TOP) to this directory
THISDIR=Shared/freetype2/src/gxvalid
# List of subdirectories (of this dir) that participate in the build
SUBDIRS=

# Include top-level Makefile.vars (include 1st so vars can be overridden)
include $(TOP)/Makefile.vars

# ---- Regular rules and targets go below this line ----

libsharedfreetype2srcgxvalid_OBJS=gxvbsln.o gxvcommn.o gxvfeat.o gxvjust.o \
		gxvkern.o gxvlcar.o gxvmod.o gxvmort.o gxvmort0.o gxvmort1.o \
		gxvmort2.o gxvmort5.o gxvmort5.o gxvmorx.o gxvmorx0.o gxvmorx1.o \
		 gxvmorx2.o gxvmorx4.o gxvmorx5.o gxvopbd.o gxvprop.o gxvtrak.o

libsharedfreetype2srcgxvalid_INCLUDE_DIRS=$(TOP) ../../include

$(eval $(call LIB_TARGET,libsharedfreetype2srcgxvalid))

# ---- End Regular rules and targets ----

# Include project rules & compiler-generated deps for each DEPFILES .d file
include $(TOP)/Makefile.rules

