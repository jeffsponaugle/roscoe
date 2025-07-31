# ROSCOE - A 68030 based Single Board Computer

Roscoe is a single board computer build in an ATX form factor using the Pin Grid Array Motorola 68030 processor.  It will support CPUs from 16-40Mhz, and has both onboard static RAM as well as Dynamic RAM.  The 8MBs of static ram is 12ns memory that support 0WS 2 Cycle Syncronous transfers for fast cache filling.  The dynamic RAM can support up to 2GB of RAM in both the onboard slots plus the expansion slots.

The PCB is a 4 layer PCBs, although it uses smallish .20mm vias and 0.15mm traces to get a route in only 2 signal layers (the other two layers are power and ground) The system is designed to support running Linux, and support UEFI. 

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


I have PCBs and have started constiuction - Two PCBs are up and running.


![](/images/RoscoeBench.png)

![](/images/ROSCOE_CPLD.png)

![](/images/ROSCOE_CPLD2.png)

![](/images/68030CPU.png)