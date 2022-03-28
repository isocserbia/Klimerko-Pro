# Klimerko Pro PCB

You'll find all the files you need for PCB manufacturing and component sourcing in this folder.  
The board was designed to be as small as possible and includes the following:  

- ESP32 SOC
- USB to Serial converter (CP2102)
- WS2812B RGB LED
- 5 exposed auxillary (3 GPIO, 1x Ground, 1x 3v3, 1x 5v).  
You can use them to add additional hardware and/or interfaces to Klimerko Pro.
- Linear regulator (5V to 3.3V)
- 3 Buttons for interacting with the device.  
Two of them are essential for flashing the ESP32 chip while the third one is used for WiFi Configuration Mode.
- 10 pin connector for PMS7003
- 8 pin headers for DGS-NO2 and DGS-SO2
- USB-C Port

Check the firmware in this repository to see which pins are exposed to which contacts.

![Klimerko Pro PCB Back](/extras/photos/6.jpeg)
![Klimerko Pro PCB Front](/extras/photos/4.jpeg)
![Klimerko Pro Schematic](/extras/photos/10.png)
![Klimerko Pro PCB Layout](/extras/photos/9.png)