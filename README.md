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

## Work still left todo - Hardware
  + Test DRAM implentation, including refresh.
  + Test FPU support
  + Build 3rd PCB revision with fixed

## Videos
    There are a few videos on Youtube about this design and the progress.   (Note the videos are a bit behind the actual project, but there are more videos in editing)

    https://www.youtube.com/watch?v=tBRo3DbZRVQ
    https://www.youtube.com/watch?v=KKh2h3Bjyjc
    https://www.youtube.com/watch?v=ODHoEpH3M6Y
    https://www.youtube.com/watch?v=-ZucQPS6IJU

![](/images/RoscoeBench.png)

![](/images/ROSCOE_CPLD.png)

![](/images/ROSCOE_CPLD2.png)

![](/images/68030CPU.png)