CHIP "C:\USERS\JEFF\SRC\ROSCOE\PLDS\VIDEO\VIDEOMEM"
BEGIN

    DEVICE = "TQFP44";
    "nEXP_DATA_EN"                            : OUTPUT_PIN = 6 ;
    "nEXP_DEV_WAIT"                           : OUTPUT_PIN = 5 ;
    "nVRAM_OE"                                : OUTPUT_PIN = 3 ;
    "nVRAM_B3"                                : OUTPUT_PIN = 2 ;
    "TDI"                                     : INPUT_PIN = 1 ;
    "nVRAM_B2"                                : OUTPUT_PIN = 44 ;
    "VMEM_ACCESS_BUSY_START_RESET"            : NODE_NUM = 612 ;
    "CPU_ACCESS_IN_PROGRESS"                  : NODE_NUM = 613 ;
    "nVRAM_B1"                                : OUTPUT_PIN = 43 ;
    "VMEM_ACCESS_BUSY_START"                  : NODE_NUM = 615 ;
    "nVRAM_B0"                                : OUTPUT_PIN = 42 ;
    "VRAM_READ"                               : INPUT_PIN = 15 ;
    "EXP_RW"                                  : INPUT_PIN = 14 ;
    "EXP_CPU_A1"                              : INPUT_PIN = 13 ;
    "EXP_CPU_SIZ1"                            : INPUT_PIN = 12 ;
    "EXP_CPU_SIZ0"                            : INPUT_PIN = 11 ;
    "nVRAM_WE"                                : OUTPUT_PIN = 10 ;
    "nVRAM_CE"                                : OUTPUT_PIN = 8 ;
    "TMS"                                     : INPUT_PIN = 7 ;
    "EXP_CPU_A0"                              : INPUT_PIN = 18 ;
    "VGA_BUSY"                                : INPUT_PIN = 19 ;
    "nSYS_RESET"                              : INPUT_PIN = 20 ;
    "TCK"                                     : INPUT_PIN = 26 ;
    "TDO"                                     : INPUT_PIN = 32 ;
    "nEXP_VMEM_CS"                            : INPUT_PIN = 37 ;
END;
