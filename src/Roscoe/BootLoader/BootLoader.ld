OUTPUT_ARCH(m68k)

SEARCH_DIR(.)
__DYNAMIC  =  0;

/*
 * Top 512K of fast SRAM area - minus 4 bytes at the top reserved
 * for bootloader use.
 */

MEMORY
{
	ram (rwx) : ORIGIN = 0x10080000, LENGTH = 0x7fffc
}

/* Top of stack goes at _ram_end */
_ram_end = ORIGIN(ram) + LENGTH(ram) - _end;

/*
 * Initialize some symbols to be zero so we can reference them in the
 * crt0 without core dumping. These functions are all optional, but
 * we do this so we can have our crt0 always use them if they exist. 
 * This is so BSPs work better when using the crt0 installed with gcc.
 * We have to initialize them twice, so we cover a.out (which prepends
 * an underscore) and coff object file formats.
 */
PROVIDE (hardware_init_hook = 0);
PROVIDE (_hardware_init_hook = 0);
PROVIDE (software_init_hook = 0);
PROVIDE (_software_init_hook = 0);
/*
 * stick everything in ram (of course)
 */
SECTIONS
{
  .text :
  {
    CREATE_OBJECT_SYMBOLS
    *(.text .text.*)

    . = ALIGN(0x4);
    /* These are for running static constructors and destructors under ELF.  */
    KEEP (*crtbegin.o(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    KEEP (*crtbegin.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))

    *(.rodata .rodata.*)

    . = ALIGN(0x4);
    *(.gcc_except_table)

    . = ALIGN(0x4);
    *(.eh_frame)

    . = ALIGN(0x4);
    __INIT_SECTION__ = . ;
    LONG (0x4e560000)   /* linkw %fp,#0 */
    *(.init)
    SHORT (0x4e5e)      /* unlk %fp */
    SHORT (0x4e75)      /* rts */

    . = ALIGN(0x4);
    __FINI_SECTION__ = . ;
    LONG (0x4e560000)   /* linkw %fp,#0 */
    *(.fini)
    SHORT (0x4e5e)      /* unlk %fp */
    SHORT (0x4e75)      /* rts */

    . = ALIGN(0x4);
    _etext = .;
    *(.lit)
  } > ram

  .data :
  {
    *(.got.plt) *(.got)
    *(.shdata)
    *(.data .data.*)
    _edata = .;
  } > ram

  .bss :
  {
    . = ALIGN(0x4);
    __bss_start = . ;
    *(.shbss)
    *(.bss .bss.*)
    *(COMMON)
    _end =  ALIGN (0x8);
    __end = _end;
  } > ram

 .stack (NOLOAD) : ALIGN(0x8) 
 {
	_stack = _ram_end;
	__stack = _ram_end;
	KEEP(*(.stack))
 } > ram
}
