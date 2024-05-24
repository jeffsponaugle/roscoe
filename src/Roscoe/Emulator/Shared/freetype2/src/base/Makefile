
# TOP is relpath to get from this directory to the project-root directory
TOP=../../../..
# THISDIR is the path from $(TOP) to this directory
THISDIR=Shared/freetype2/src/base
# List of subdirectories (of this dir) that participate in the build
SUBDIRS=

# Include top-level Makefile.vars (include 1st so vars can be overridden)
include $(TOP)/Makefile.vars

# ---- Regular rules and targets go below this line ----

libsharedfreetype2srcbase_OBJS=ftadvanc.o ftbbox.o ftbdf.o ftbitmap.o ftcalc.o ftcid.o \
		ftcolor.o ftdbgmem.o ftdebug.o fterrors.o ftfntfmt.o ftfstype.o ftgasp.o \
		ftgloadr.o ftglyph.o ftgxval.o fthash.o ftinit.o ftlcdfil.o ftmac.o ftmm.o \
		ftobjs.o ftotval.o ftoutln.o ftpatent.o ftpfr.o ftpsprop.o ftrfork.o \
		ftsnames.o ftstream.o ftstroke.o ftsynth.o ftsystem.o fttrigon.o fttype1.o \
		ftutil.o ftwinfnt.o

libsharedfreetype2srcbase_INCLUDE_DIRS=$(TOP) ../../include

$(eval $(call LIB_TARGET,libsharedfreetype2srcbase))

# ---- End Regular rules and targets ----

# Include project rules & compiler-generated deps for each DEPFILES .d file
include $(TOP)/Makefile.rules

