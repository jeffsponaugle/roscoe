#
# Utilities
#
CC=/opt/cross/bin/m68k-elf-gcc
AS=/opt/cross/bin/m68k-elf-as
LINK=/opt/cross/bin/m68k-elf-ld
OBJCOPY=/opt/cross/bin/m68k-elf-objcopy
SPLIT=../Utils/split/split
STAMP=../Utils/stamp/stamp

#
# Compiler flags
#
CFLAGS=-m68030 -O3 -fno-delete-null-pointer-checks -D_BOOTLOADER -D__BYTE_ORDER__=__ORDER_BIG_ENDIAN__ 
INCLUDES=-I ../newlib/m68k-elf/include -I .. 

#
# Target name
#
BASENAME=BootLoader
OBJS=$(BASENAME)Startup.o $(BASENAME).o ../Shared/sbrk.o ../Shared/zlib/crc32.o ../Shared/16550.o ../Shared/Version.o $(BASENAME)Version.o  \
	../Utils/newlib/newlib/libc/machine/w65/mulsi3.o ../Utils/newlib/newlib/libc/machine/w65/divsi3.o ../Shared/Flash.o ../Shared/dis68k.o \
	../Shared/LineInput.o ../Shared/Monitor.o ../Shared/lex.o ../Shared/Interrupt.o ../Shared/elf.o \
	../Shared/arith64.o ../Shared/rtc.o ../Shared/muldi3.o ../Shared/IDE.o ../Shared/MemTest.o AsmUtils.o \
	../Shared/Shared.o ../Shared/ptc.o ../Shared/Stream.o ../Shared/YModem.o \
	../Shared/FaultHandler.o FaultHandlerAsm.o ../Shared/FatFS/source/diskio.o ../Shared/FatFS/source/ff.o \
	../Shared/FatFS/source/ffsystem.o ../Shared/FatFS/source/ffunicode.o ../Shared/DOS.o

OUTPUT=$(BASENAME).a
OUTPUTBIN=$(BASENAME).bin
MAPFILE=$(BASENAME).map
LINKFILE=$(BASENAME).ld

BootLoader: $(OBJS)
	$(LINK) $(notdir $(OBJS)) ../newlib/m68k-elf/lib/crt0.o -T$(LINKFILE) -o $(OUTPUT) -Map=$(MAPFILE) -L ../newlib/m68k-elf/lib -lc
	$(OBJCOPY) -O binary BootLoader.a $(OUTPUTBIN)
	/opt/cross/bin/m68k-elf-objdump -m68030 -d BootLoader.a >BootLoader.asm
	$(STAMP) $(OUTPUTBIN)

#
# Special case for BootLoaderStartup. It uses the C preprocessor in order to
# turn #defines into assembly-friendly equates
#

BootLoaderStartup.o: BootLoaderStartup.s ../Shared/AsmUtils.s ../Hardware/Roscoe.h ../Shared/16550.h
	cpp -I include -x assembler-with-cpp $(BASENAME)Startup.s -o $(BASENAME)StartupProc.s
	$(AS) $(BASENAME)StartupProc.s -o $(BASENAME)Startup.o
	@echo $(OBJS)

AsmUtils.o: ../Shared/AsmUtils.s ../Hardware/Roscoe.h ../Shared/16550.h
	cpp -I include -x assembler-with-cpp ../Shared/AsmUtils.s -o AsmUtilsProc.s
	$(AS) AsmUtilsProc.s -o AsmUtils.o
	@echo $(OBJS)

FaultHandlerAsm.o: ../Shared/FaultHandlerAsm.s ../Hardware/Roscoe.h ../Shared/16550.h
	cpp -I include -x assembler-with-cpp ../Shared/FaultHandlerAsm.s -o FaultHandlerAsmProc.s
	$(AS) FaultHandlerAsmProc.s -o FaultHandlerAsm.o
	@echo $(OBJS)

$(BASENAME)Version.c:	FORCE
	../Scripts/subwcrev $(BASENAME)Version.tmpl >$(BASENAME)Version.c

FORCE:

$(BASENAME)Version.o:	$(BASENAME)Version.c $(BASENAME)Version.tmpl
	$(COMPILE.c) $(INCLUDES) $< -o $@

%.o: %.c $(DEFS)
	$(CC) $(INCLUDES) -c -o $(notdir $@) $< $(CFLAGS)

clean:
	rm -f $(notdir $(OBJS))
	rm -f *.map
	rm -f $(MAPFILE)
	rm -f $(OUTPUT)
	rm -f $(OUTPUTBIN)
	rm -f $(BASENAME)StartupProc.s
	rm -f $(BASENAME)Version.c
