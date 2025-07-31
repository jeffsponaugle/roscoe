# ROSCOE - A 68030 based Single Board Computer

Roscoe is a single board computer build in an ATX form factor using the Pin Grid Array Motorola 68030 processor.  It will support CPUs from 16-40Mhz, and has both onboard static RAM as well as Dynamic RAM.  The 8MBs of static ram is 12ns memory that support 0WS 2 Cycle Syncronous transfers for fast cache filling.  The dynamic RAM can support up to 2GB of RAM in both the onboard slots plus the expansion slots.

The PCB is a 4 layer PCBs, although it uses smallish .20mm vias and 0.15mm traces to get a route in only 2 signal layers (the other two layers are power and ground) The system is designed to support running Linux, and support UEFI. 

This system is designed for the enthusiast to build!

## PCB 
- Features:
  + 68030 PGA Processor, 16-40Mhz
  + 68882 Math Coprocessor, 16-40Mhz Syncronous
  + 4x 72 pin SIMM DRAM sockets, 60ns, 1MB - 128MB Single or Dual Rank Supported
  + 8MB on board 10ns Static Ram, 0WS 2 cycle sync transfer
  + 2 UARTS
  + 2 IDE interfeaces, 4 drives total
  + Programmable Timer Interface
  + Real Time Clock
  + ATX Power and Power Control with Soft power off.

## External Addin Card
  + Microchip 9218 Ethernet controller
  + FT USB controller (2 USB Ports)
  + VGA Video Card ( 640x480, 1bpp, 2bpp,4bpp,16bpp,24bpp )


We have PCBs and have started construction, with two PCBs up and running.  PCB construction is relativly straightforward. The most difficult part to solder are the three CPLDS which are 0.50mm QFP packages.  They are still large enough to do with a normal solder iron with the right technique.  Most of other parts are either through-hole DIP or SOIC.

If you are interested in this project, feel free to reach out to me at jeff@sponaugle.com.  Once we get DRAM and the FPU working this should be a good project entry point as you will be able to build a PCB and get the software stack up and running.

Neil has developed an excellent monitor, boot loader, and BIOS that takes advantage of the system features and can server as a great starting point for OS development.  He also has an excellent build environment using GCC.

From a parts point of view most of the parts needed can be purchased new from Digikey/Mouser, with the key exception being:

  + 68030 CPU, very available on eBay
  + 68030 CPU Socket, available on eBay
  + 68881/68882 FPU, available on eBay
  + 72 pin DRAM SIMM - 8MB/16MB/32MB, 70ns EDO or FPM, available on ebay

The other key parts including the CPLDs can be ordered directly from a major supplier.   We will post up a complete BOM with the next PCB revision.

## Hardware Design Notes

The core system design is relativly simple with a 68030 processor and a 6888x FPU connected to a PCB wide data/address bus that includes an 8-bit wide boot/BIOS flash memory (512k), 4MB of 10/12ns static RAM, a dual UART, RTC, PTC, Dual IDE interfaces, and DRAM interface.  DRAM support is for 72 pin SIMM EDO or FPM DRAM, 70ns or faster.  The 4 SIMM slots can support up to 128MB double-sided SIMMs for a total of 512MBs of DRAM.  Additional DRAM can be added via the expansion interface.

### Logic

The system logic is implemented in 3 ATF1508 CPLDS.  The ATF1508 CPLD is a current production CPLD that comes in a hand solderable form factor and supports 5V operation.  The CPLDs we are using come in a 100pin 0.5mm QFP package and have 80 usable I/O pins, JTAG programming, and 7-12ns propegation delay. The three CPLDs are partitioned into the following logical segments:

#### CPLD 1 - Bus Controller

The first CPLD serves as a bus controller, handling all of the core interactions with the CPU bus including the wait state generation, SRAM and DRAM interfaces, DeviceIO partitioning, and FPU interface.  This implements the broad memory map as well as interfaces needed for 2 and 3 cycle memory access.

#### CPLD 2 - Device Interface

The second CPLD serves as the device interface logic, generating all of the signal needed for the onboard devices as well as device decoding for the expansion interface.  This includes the logic needed for the RTC, IDE interface, as well as the system POST code display.   This CPLD has some additional unused logic space for future sytem expansion and bug fixes.

#### CPLD 3 - Interrupt Controller

The third CPLD implements a 16 input priority vector generating interrupt controller. It has configurable edge/level triggering as well as indiviual interrupt source enable/disable. It interfaces with the 68030 IPL0-2 interrupt lines to provide 7 levels of interrupt priorty as well as unique vector generation for each interrupt source.

### Expansion

The current PCB design has three expansion ports implemented using 98 pin edge card connectors that are the same as used in the original IBM PC AT. (ISA). This connector allows the expansion cards to use gold edge fingers instead of a dedicated connector, making expansion a bit easier. The alignment of these connectors on the PCB is the same as an original ATX style PC motherboard, so slot alignment will work with existing ATX cases.  

The pin/signal format is however not ISA compatible. Due to wanting to have a complete 32 bit address and data bus available for expansion it was not possible to use the existing ISA layout. The expansion bus currently includes 32 data and address lines as well as device-bit-width specific chip selects, 7 interrupt sources, wait state requrts, and a few other bus related signals. It should be realtivly straighforward to build an expansion card to add capabilities. I have a VGA card, network controller, and keyboard controller desing in progress.  

### Power

The PCB power input is provided via an ATX power supply connector, and any ATX power supply of approximatly 50w or greater should work. There is an ATX power controller management implemented in an ATTiny to allow soft power-on and soft power-off, as well as commanded and asked power management at the OS level. Reset control is also done in this controller.

## Work still left todo - Hardware
  - [ ] Test DRAM implentation, including refresh.
  - [ ] Test FPU support
  - [ ] Build 3rd PCB revision with fixed

## Videos
There are a few videos on Youtube about this design and the progress.   (Note the videos are a bit behind the actual project, but there are more videos in editing)

 + https://www.youtube.com/watch?v=tBRo3DbZRVQ
 + https://www.youtube.com/watch?v=KKh2h3Bjyjc
 + https://www.youtube.com/watch?v=ODHoEpH3M6Y
 + https://www.youtube.com/watch?v=-ZucQPS6IJU

![](/images/RoscoeBench.png)

![](/images/ROSCOE_CPLD.png)

![](/images/ROSCOE_CPLD2.png)

![](/images/68030CPU.png)