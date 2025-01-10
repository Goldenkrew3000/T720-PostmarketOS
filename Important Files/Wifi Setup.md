# SM-T720 Wifi Setup

## PLEASE Readme
I have included a copy of all the required firmware in the ```wifi-firmware``` folder. <br>
Inside the ```wifi-firmware``` folder, there is a folder called ```gts4lvwifi```. These files would go into ```/lib/firmware/qcom/sdm670/gts4lvwifi```. <br>
I am not sure whether this will work on all devices as there is a bunch of secure-boot like signing happening here, but generating some of these files is a REAL pain, so I have included them just in case. <br>
Also, most of these files seem to be signature checked. <br>

## Step 1 - Install required programs
Both ```tqftpserv``` and ```rmtfs``` have to be running for them modem to power on and pull in the required firmware. I have them both set to run at boot by running ```sudo rc-update add <service> boot```. <br>
Basically, they have to be loaded after the qrtr kernel modules, but before the modem actually powers up. Worst case scenario, the modem crashes, reboots, and everything proceeds fine. <br>

## Step 2 - Pull the required firmware from partitions
All of the firmware goes into the ```/lib/firmware/qcom/sdm670/gts4lvwifi``` folder. <br>
The 4 files required for the modem to function are ```ipa_fws.mbn```, ```mba.mbn```, ```modem.mbn``` and ```wlanmdsp.mbn```. <br>

The ```ipa_fws.mbn``` file was generated using pil-squasher (https://github.com/linux-msm/pil-squasher) using the ```ipa_fws.b--``` and ```ipa_fws.mdt``` files located on the ```apnhlos``` partition ```(/dev/mmcblk0p44)```. <br>
The ```mba.mbn``` file is from the ```modem``` partition ```(/dev/mmcblk0p46)``` at ```image/modem.mbn```. <br>
The ```modem.mbn``` file was generated using pil-squasher using the ```modem.b--``` and ```modem.mdt``` files located on the ```modem``` partition ```(/dev/mmcblk046)```.<br>
The ```wlanmdsp.mbn``` file is located in the ```apnhlos``` partiton  ```(/dev/mmcblk0p44)``` at ```image/wlanmdsp.mbn```. <br>

## Step 3 - Pull the required filesystems for the modem from partitions
NOTE: As of now, I don't think this is required anymore. There were 4 files pulled in through ```tqftpserv``` for the modem filesystem. <br>

