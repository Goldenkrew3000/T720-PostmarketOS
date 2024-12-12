# SM-T720 PostmarketOS Mainlining
## Latest Kernel Tested: 6.12.3 (2024-12-12)

## What is the purpose of this repository?
This repository is basically a place for in-development drivers to be stored for the Samsung Tab S5e under PmOS Mainline

## SM5705-Fuelgauge

## FTS1BA90A
This is the STM Touchscreen. This touchscreen driver is very similar to the upstream STMFTS driver, but this touch panel uses
different I2C registers, a different initialization sequence, and returns it's data packed differently, hence the need for
a different driver. <br>
Initialization Sequence until I figure out the bug: <br>
gpiomon -c gpiochip2 123 <br>
insmod driver.ko <br>
rmmod driver.ko <br>
Ctrl + C the gpiomon <br>
insmod driver.ko <br>
Bam it works! Very poorly for now, but it works!!

