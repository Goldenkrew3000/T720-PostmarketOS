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

char* i2c_host = "/dev/i2c-7";
uint8_t i2c_addr = 0x71;

int main() {
    printf("SM5705 Userspace Driver\n");
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

    // Variables for the rest of this thing
    int res;

    // Step 1: Read the device ID (Register 0x00), Reads in word (2 bytes)
    uint8_t buf_device_id[2];
    unsigned int sm5705_device_id;
    res = i2c_smbus_read_i2c_block_data(i2c_file, 0x00, 2, buf_device_id);
    if (res < 0) {
        printf("Could not read SM5705 Device ID Register (0x00).\n");
        exit(1);
    } else {
        sm5705_device_id = buf_device_id[0] | (buf_device_id[1] << 8);
        printf("SM5705 Device ID: 0x%x\n", sm5705_device_id);
    }

    // Step 2: Read the OCV / Open Circuit Voltage (Register 0x06)
    uint8_t buf_ocv[2];
    unsigned int sm5705_ocv;
    res = i2c_smbus_read_i2c_block_data(i2c_file, 0x06, 2, buf_ocv);
    if (res < 0) {
        printf("Could not read SM5705 OCV Register (0x06).\n");
        exit(1);
    } else {
        sm5705_ocv = buf_ocv[0] | (buf_ocv[1] << 8);
        printf("SM5705 OCV: %d\n", sm5705_ocv);
    }

    // Perform some calculations on it (Extracted from sm5705_get_ocv)
    //ocv = ((res2 & 0x7800) >> 11) * 1000;
    //ocv = ocv + (((res2 & 0x07ff) * 1000) / 2048);
    //printf("New OCV: %d\n", ocv);*/
    
    // Step 3: Read the 'CNTL' (Register 0x01), and compare it to 'CNTL_REG_DEFAULT_VALUE (0x2008)'
    // NOTE: No idea what this does yet, it's just stored for later.
    uint8_t buf_cntl[2];
    unsigned int sm5705_cntl;
    res = i2c_smbus_read_i2c_block_data(i2c_file, 0x01, 2, buf_cntl);
    if (res < 0) {
        printf("Could not read SM5705 CNTL Register (0x01).\n");
        exit(1);
    } else {
        sm5705_cntl = buf_cntl[0] | (buf_cntl[1] << 8);
        printf("SM5705 CNTL: %x\n", sm5705_cntl);
    }

    // Step 4: Set SOC Cycle Configuration


    // Test: Read voltage (Register 0x07)
    uint8_t buf_vbat[2];
    unsigned int sm5705_vbat;
    res = i2c_smbus_read_i2c_block_data(i2c_file, 0x07, 2, buf_vbat);
    if (res < 0) {
        printf("Could not read SM5705 VBAT Register (0x07).\n");
        exit(1);
    } else {
        sm5705_vbat = buf_vbat[0] | (buf_vbat[1] << 8);

        unsigned int vbat2 = ((sm5705_vbat & 0x3800) >> 11) * 1000;
        vbat2 = vbat2 + (((sm5705_vbat & 0x07ff) * 1000) / 2048);
        printf("SM5705 VBAT: %d\n", sm5705_vbat);
        printf("VBAT2: %d\n", vbat2);
    }

    // Test: Read current (Register 0x08)
    uint8_t buf_current[2];
    unsigned int sm5705_current_raw;
    unsigned int sm5705_current;
    res = i2c_smbus_read_i2c_block_data(i2c_file, 0x08, 2, buf_current);
    if (res < 0) {
        printf("Could not read SM5705 CURRENT Register (0x08).\n");
        exit(1);
    } else {
        sm5705_current_raw = buf_current[0] | (buf_current[1] << 8);
        sm5705_current = ((sm5705_current_raw & 0x1800) >> 11) * 1000;
        sm5705_current = sm5705_current + (((sm5705_current & 0x07ff) * 1000) / 2048);
        printf("SM5705 Current (Raw / Calculated): %d / %d\n", sm5705_current_raw, sm5705_current);
    }

}
