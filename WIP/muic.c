#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>

char* i2c_host = "/dev/i2c-5";
uint8_t i2c_addr = 0x25;

// Extracted from driver
#define REG_ITEM(addr, bitp, mask) ((bitp<<16) | (mask<<8) | addr)

int main() {
    printf("SM5705 MUIC Userspace Driver\n");
    printf("Goldenkrew3000 2024-12\n");

    // Open the i2c host
    int i2c_file = open(i2c_host, O_RDWR);
    if (i2c_file < 0) {
        printf("Could not open i2c host.\n");
        exit(1);
    } else {
        printf("Opened i2c host.\n");
    }

    // ---
    if (ioctl(i2c_file, I2C_SLAVE, i2c_addr) < 0) {
        printf("Could not contact i2c address.\n");
        exit(1);
    } else {
        printf("Contacted i2c address.\n");
    }


    // Test: Attempt to get the switching mode
    int Ctrl_ManualSW = REG_ITEM(0x02, 2, 0x01); // REG_CTRL, _BIT2, _MASK1
    printf("Unholy output of Ctrl_ManualSW: %d\n", Ctrl_ManualSW);

    int ret = i2c_smbus_read_byte_data(i2c_file, 1);
    if (ret < 0) {
        printf("fuck\n");
        exit(1);
    }

   printf("Data: %d\n", ret); 
}
