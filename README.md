# SM-T720 PostmarketOS Mainlining
## Latest Kernel Tested: 6.13.3 (2025-03-22)

## What is the purpose of this repository?
This repository is basically a place for in-development drivers to be stored for the Samsung Tab S5e under PmOS Mainline

## SM5705-Fuelgauge
This is the driver for the SM5705 PMIC Fuelgauge. For now, I have included a userspace 'driver' that fetches a bunch of important live information from the PMIC, but most importantly, the battery percentage.<br>
As for now, live USB PD negotiation is not handled at all, so to properly set the PMIC into the right mode, plug in a USB PD charger while powering the device up, and then you can freely plug and unplug it under the mainline linux environment.<br>

## FTS1BA90A
This is the STM Touchscreen. This touchscreen driver is very similar to the upstream STMFTS driver, but this touch panel uses different I2C registers, a different initialization sequence, and returns it's data packed differently, hence the need for
a different driver. <br>
Notes: Still a lot ot fix in this driver, have to properly handle multitouch, and implement shutdown/suspend code.<br>
Driver is functional.

## Panel (AMSA05RB01)
This driver allows the Adreno GPU to power up and output a signal at the right resolution, but the panel does nothing. I am 99% sure the device tree is correct at this point (not including the missing power stuff, that also seems to be missing from the downstream device tree). I do not have the skills to get this working, but I am putting my work here regardless. <br>

## USB
As per a block diagram that I am sure I cannot share here, USB2 is muxed through the PMIC system (SM5705 / S2MM005). USB3 is directly connected to the USB C port, but unfortunately as of now, USB3 is not supported by the mainline kernel. <br>

# Function Graph
![Function Graph](https://github.com/Goldenkrew3000/T720-PostmarketOS/blob/main/feature-matrix.png?raw=true)
