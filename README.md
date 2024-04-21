# ROSCOE - A 68030 based Single Board Computer

Roscoe is a single board computer build in an ATX form factor using the PGA Motorola 68030 processor.  It will support CPUs from 16-40Mhz, and has both onboard static RAM as well as Dynamic RAM.  The 8MBs of static ram is 10ns memory that support 0WS 2 Cycle Syncronous transfers for fast cache filling.  The dynamic RAM can support up to 2GB of RAM in both the onboard slots plus the expansion slots.

The PCB is a 4 layer PCBs, although it uses smallish .20mm vias and 0.15mm traces to get a route in only 4 layers.  The system is designed to run Linux, and support UEFI.   It has as dedicated 64MB Flash for UEFI storage plus 512K of boot flash. 

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
  + Microchip 9218 Ethernet controller
  + FT USB controller (2 USB Ports)
  + ATX Power and Power Control with Soft power off.


PCBs are on order now, so construction should start in about a week.

![](/images/3DPCB.png)

![](/images/MainBoardPCBRouted.png)