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
#include <unistd.h>

int s2mm005_read_byte_flash(int i2c_fd, uint16_t reg, int size, uint8_t* read_buf);
int s2mm005_write_byte(int i2c_fd, uint16_t i2c_reg, uint8_t value, int size);
int s2mm005_read_byte(int i2c_fd, uint16_t reg, int size, uint8_t* read_buf);

#define FLAG_WRITE 0
#define REG_RX_CAPABILITY 0x0260
#define REG_TX_REQUEST 0x0240 // Not sure what this is for yet

char* i2c_host = "/dev/i2c-7"; // Change this to the i2c-gpio that contains 0x33
uint8_t i2c_addr = 0x33;

// Temporarially extracted struct from SEC driver to make sense of this no comment code
typedef struct
{
	uint16_t	Message_Type:4;
	uint16_t	Rsvd2_msg_header:1;
	uint16_t	Port_Data_Role:1;
	uint16_t	Specification_Revision:2;
	uint16_t	Port_Power_Role:1;
	uint16_t	Message_ID:3;
	uint16_t	Number_of_obj:3;
	uint16_t	Rsvd_msg_header:1;
} MSG_HEADER_Type;

int main() {
    printf("S2MM005 Userspace Driver\n");
    printf("Goldenkrew3000 2024-12\n");

    // Open the I2C Bus
    int i2c_fd = open(i2c_host, O_RDWR);
    if (i2c_fd < 0) {
        printf("Error: Could not open I2C Bus.\n");
        exit(1);
    } else {
        printf("Opened I2C Bus.\n");
    }

    // Open I2C Device
    if (ioctl(i2c_fd, I2C_SLAVE, i2c_addr) < 0) {
        printf("Could not open I2C Device.\n");
        exit(1);
    } else {
        printf("Opened I2C Device.\n");
    }

    // Get hardware and software version
    uint8_t hwversion_boot[1], hwversion_main[3], hwversion_ver2[4];
    uint8_t swversion_boot[1], swversion_main[3], swversion_ver2[4];
    s2mm005_read_byte_flash(i2c_fd, 0x0, 1, hwversion_boot);
    s2mm005_read_byte_flash(i2c_fd, 0x1, 3, hwversion_main);
    s2mm005_read_byte_flash(i2c_fd, 0x4, 4, hwversion_ver2);
    s2mm005_read_byte_flash(i2c_fd, 0x8, 1, swversion_boot);
    s2mm005_read_byte_flash(i2c_fd, 0x9, 3, swversion_main);
    s2mm005_read_byte_flash(i2c_fd, 0xc, 4, swversion_ver2);
    printf("S2MM005 Hardware version: %2x %2x %2x %2x\n",
           hwversion_main[2], hwversion_main[1], hwversion_main[0], hwversion_boot[0]);
    printf("S2MM005 Hardware version2: %2x %2x %2x %2x\n",
           hwversion_ver2[3], hwversion_ver2[2], hwversion_ver2[1], hwversion_ver2[0]);
    printf("S2MM005 Software version: %2x %2x %2x %2x\n",
           swversion_main[2], swversion_main[1], swversion_main[0], swversion_boot[0]);
    printf("S2MM005 Software version2: %2x %2x %2x %2x\n",
           swversion_ver2[3], swversion_ver2[2], swversion_ver2[1], swversion_ver2[0]);

    // Original driver (/drivers/ccic/s2mm005.c) fetches firmware version here
    
    // Attempting to get USB PD specs of charger
    uint8_t readmsg[32];
    s2mm005_read_byte(i2c_fd, REG_RX_CAPABILITY, 32, readmsg);
    printf("msg:\n");
    for (int i = 0; i < 32; i++) {
        printf("%2x ", readmsg[i]);
    }
    printf("\n");

    MSG_HEADER_Type *MSG_HDR;
    MSG_HDR = (MSG_HEADER_Type *)&readmsg[0];
    printf("Rsvd: %2x\n", MSG_HDR->Rsvd_msg_header);
    printf("NOO: %2x\n", MSG_HDR->Number_of_obj);
    printf("MID: %2x\n", MSG_HDR->Message_ID);
    printf("PPR: %2x\n", MSG_HDR->Port_Power_Role);

    printf("SR: %2x\n", MSG_HDR->Specification_Revision);
    printf("PDR: %2x\n", MSG_HDR->Port_Data_Role);
    printf("RSVD2: %2x\n", MSG_HDR->Rsvd2_msg_header);
    printf("MT: %2x\n", MSG_HDR->Message_Type);

    uint8_t readmsg2[32];
    s2mm005_read_byte(i2c_fd, REG_TX_REQUEST, 32, readmsg2);
    printf("msg:\n");
    for (int i = 0; i < 32; i++) {
        printf("%2x ", readmsg2[i]);
    }
    printf("\n");
}

int s2mm005_read_byte_flash(int i2c_fd, uint16_t reg, int size, uint8_t *read_buf) {
    // Write 0xAA to register 0x10
    s2mm005_write_byte(i2c_fd, 0x10, 0xaa, 1);

    uint8_t buf[258];
    uint8_t write_buf[2];

    // Assemble I2C Message
    struct i2c_msg msg[2];
    msg[0].addr = i2c_addr;
    msg[0].flags = FLAG_WRITE;
    msg[0].len = 2;
    msg[0].buf = write_buf;
    msg[1].addr = i2c_addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = size;
    msg[1].buf = read_buf;

    // Assemble write buffer
    write_buf[0] = (reg & 0xFF00) >> 8;
    write_buf[1] = (reg & 0xFF);

    // Perform userspace specific stuff, and peform I2C transfer
    struct i2c_rdwr_ioctl_data io_data;
    io_data.msgs = msg;
    io_data.nmsgs = 2;

    if (ioctl(i2c_fd, I2C_RDWR, &io_data) < 0) {
        printf("Could not transfer I2C message.\n");
        exit(1);
    } else {
        printf("Transferred I2C message.\n");
    }
}

int s2mm005_write_byte(int i2c_fd, uint16_t i2c_reg, uint8_t value, int size) {
    if (size > 256) {
        printf("I2C message over maximum allowed size, exiting.\n");
        exit(1);
    }

    // Maximum write size is 256 bytes + 2 bytes
    uint8_t buf[258]; // 256 bytes + 2 bytes
    
    // Assemble I2C Message
    struct i2c_msg msg[1];
    msg[0].addr = i2c_addr;
    msg[0].flags = FLAG_WRITE;
    msg[0].len = size + 2;
    msg[0].buf = buf;

    // Assemble Buffer
    buf[0] = (value & 0xFF00) >> 8;
    buf[1] = (value & 0xFF);
    buf[2] = value;

    // Perform userspace specific stuff to perform an equivalent to i2c_transfer under userspace
    struct i2c_rdwr_ioctl_data io_data;
    io_data.msgs = msg;
    io_data.nmsgs = 1;

    // Perform I2C transfer
    if (ioctl(i2c_fd, I2C_RDWR, &io_data) < 0) {
        printf("Could not transfer I2C message.\n");
        exit(1);
    } else {
        printf("Transferred I2C message.\n");
    }

    // Make GCC happy
    return 0;
}

int s2mm005_read_byte(int i2c_fd, uint16_t reg, int size, uint8_t* read_buf) {
    uint8_t write_buf[2];

    // Assemble I2C message and write buffer
    struct i2c_msg msg[2];
    msg[0].addr = i2c_addr;
    msg[0].flags = FLAG_WRITE;
    msg[0].len = 2;
    msg[0].buf = write_buf;
    msg[1].addr = i2c_addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = size;
    msg[1].buf = read_buf;
    write_buf[0] = (reg & 0xFF00) >> 8;
    write_buf[1] = (reg & 0xFF);

    // Perform userspace stuff and perform I2C transfer
    struct i2c_rdwr_ioctl_data io_data;
    io_data.msgs = msg;
    io_data.nmsgs = 2;

    if (ioctl(i2c_fd, I2C_RDWR, &io_data) < 0) {
        printf("Could not transfer I2C message.\n");
        exit(1);
    } else {
        printf("Transferred I2C message.\n");
    }
}
