// SPDX-License-Identifier: GPL-2.0
// STMicroelectronics FTS1BA90A driver
//
// Driver based off of stmfts
// Copyright (c) 2024 Goldenkrew3000 <hungrymike@live.com>

// Version 1a

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/printk.h>

// I2C Registers
#define FTS1BA90A_REG_SENSE_ON                  0x10
#define FTS1BA90A_REG_SENSE_OFF                 0x11
#define FTS1BA90A_REG_FORCE_CALIBRATION         0x13
#define FTS1BA90A_REG_READ_GPIO_STATUS          0x20
#define FTS1BA90A_REG_READ_FIRMWARE_INTEGRITY   0x21
#define FTS1BA90A_REG_READ_DEVICE_ID            0x22
#define FTS1BA90A_REG_READ_PANEL_INFO           0x23
#define FTS1BA90A_REG_READ_FIRMWARE_VERSION     0x24
#define FTS1BA90A_REG_SET_TOUCH_TYPE            0x30
#define FTS1BA90A_REG_CLEAR_ALL_EVENTS          0x62
#define FTS1BA90A_REG_SET_SCANMODE              0xA0
#define FTS1BA90A_REG_READ_ALL_EVENTS           0x61

// Touch types
#define FTS1BA90A_TOUCH_TYPE_BIT_TOUCH      (1 << 0)
#define FTS1BA90A_TOUCH_TYPE_BIT_HOVER      (1 << 1)
#define FTS1BA90A_TOUCH_TYPE_BIT_COVER      (1 << 2)
#define FTS1BA90A_TOUCH_TYPE_BIT_GLOVE      (1 << 3)
#define FTS1BA90A_TOUCH_TYPE_BIT_STYLUS     (1 << 4)
#define FTS1BA90A_TOUCH_TYPE_BIT_PALM       (1 << 5)
#define FTS1BA90A_TOUCH_TYPE_BIT_WET        (1 << 6)
#define FTS1BA90A_TOUCH_TYPE_BIT_PROXIMITY  (1 << 7)
#define FTS1BA90A_TOUCH_TYPE_DEFAULT        (FTS1BA90A_TOUCH_TYPE_BIT_TOUCH | FTS1BA90A_TOUCH_TYPE_BIT_PALM | FTS_TOUCHTYPE_BIT_WET)

// Scan Modes
#define FTS1BA90A_SCANMODE_SCAN_OFF         0
#define FTS1BA90A_SCANMODE_MS_SS_SCAN       (1 << 0)
#define FTS1BA90A_SCANMODE_KEY_SCAN         (1 << 1)
#define FTS1BA90A_SCANMODE_HOVER_SCAN       (1 << 2)
#define FTS1BA90A_SCANMODE_FORCE_TOUCH_SCAN (1 << 4)
#define FTS1BA90A_SCANMODE_DEFAULT          FTS1BA90A_SCANMODE_MS_SS_SCAN

// Settings
#define FTS1BA90A_MAX_FINGERS       10
#define FTS1BA90A_EVENT_SIZE        8
#define FTS1BA90A_STACK_DEPTH       32
#define FTS1BA90A_MAX_DATA_SIZE     (FTS1BA90A_EVENT_SIZE * FTS1BA90A_STACK_DEPTH)
#define FTS1BA90A_DRIVER_NAME       "stm_fts1ba90a"
#define FTS1BA90A_SUPPORT_MULTITOUCH 0 // Multitouch is currently disabled

enum fts1ba90a_regulators {
    FTS1BA90A_REGULATOR_VDD,
    FTS1BA90A_REGULATOR_AVDD,
};

struct fts1ba90a_data {
    struct i2c_client *client;
    struct input_dev *input;
    struct mutex mutex;
    struct touchscreen_properties prop;
    struct regulator_bulk_data regulators[2];
    uint16_t chip_id;
    u8 chip_ver; // TODO
    u16 fw_ver;
    u8 config_id;
    u8 config_ver;
    uint8_t data[FTS1BA90A_MAX_DATA_SIZE];
    struct completion cmd_done;
    bool use_key;
    bool led_status;
    bool hover_enabled;
    bool running;
};

struct fts1ba90a_event_coordinate {
    uint8_t eid:2;
    uint8_t tid:4;
    uint8_t tchsta:2;
    uint8_t x_11_4;
    uint8_t y_11_4;
    uint8_t y_3_0:4;
    uint8_t x_3_0:4;
    uint8_t major;
    uint8_t minor;
    uint8_t z:6;
    uint8_t ttype_3_2:2;
    uint8_t left_event:6;
    uint8_t ttype_1_0:2;
} __packed;
struct fts1ba90a_event_coordinate *event_coordinate;

static int fts1ba90a_command(struct fts1ba90a_data* data, uint8_t command) {
    int err;

    reinit_completion(&data->cmd_done);

    err = i2c_smbus_write_byte(data->client, command);
    if (err) {
        return err;
    }

    return 0;
}

static int fts1ba90a_write_register(struct fts1ba90a_data* data, uint8_t* reg, int bytes) {
    int ret;
    struct i2c_msg transfer_msg[2];
    uint8_t* write_buf;
    
    write_buf = kzalloc(bytes, GFP_KERNEL);
    if (!write_buf) {
        return -ENOMEM;
    }
    memcpy(write_buf, reg, bytes);

    transfer_msg[0].addr = data->client->addr;
    transfer_msg[0].len = bytes;
    transfer_msg[0].flags = 0;
    transfer_msg[0].buf = write_buf;
    ret = i2c_transfer(data->client->adapter, transfer_msg, 1);
    kfree(write_buf);

    return ret;
}

static int fts1ba90a_perform_reset(struct fts1ba90a_data* data) {
    uint8_t write_buf[6] = { 0xFA, 0x20, 0x00, 0x00, 0x24, 0x81 };
    fts1ba90a_write_register(data, &write_buf[0], 6);

    return 0;
}

static int fts1ba90a_set_scanmode(struct fts1ba90a_data* data, uint8_t scanmode) {
    uint8_t write_buf[3] = { FTS1BA90A_REG_SET_SCANMODE, 0x00, scanmode };
    fts1ba90a_write_register(data, &write_buf[0], 3);

    return 0;
}

static int fts1ba90a_set_touchtype(struct fts1ba90a_data* data) {
    // TODO This does not actually set a touch type
    uint8_t write_buf[3] = { FTS1BA90A_REG_SET_TOUCH_TYPE, FTS1BA90A_REG_SET_TOUCH_TYPE & 0xFF, FTS1BA90A_REG_SET_TOUCH_TYPE >> 8 };
    fts1ba90a_write_register(data, &write_buf[0], 3);

    return 0;
}

static int fts1ba90a_power_on(struct fts1ba90a_data* data) {
    int err;
    uint8_t reg[8];

    // Enable regulators (Power on the touchscreen)
    err = regulator_bulk_enable(ARRAY_SIZE(data->regulators), data->regulators);
    if (err) {
        return err;
    }
    msleep(20);

    // TODO Read in device info here

    // NOTE: Was performing a device reset here, but it appears to work fine without it
    //       (And honestly I am not sure whether the downstream driver does it or not anyway)

    // Enable IRQ
    enable_irq(data->client->irq);
    msleep(50);

    // Set touch mode
    fts1ba90a_set_touchtype(data);
    msleep(10);

    // Force calibration
    err = fts1ba90a_command(data, FTS1BA90A_REG_FORCE_CALIBRATION);
    if (err) {
        return err;
    }

    // Clear all events
    err = fts1ba90a_command(data, FTS1BA90A_REG_CLEAR_ALL_EVENTS);
    if (err) {
        return err;
    }

    // Set scan mode to default
    fts1ba90a_set_scanmode(data, FTS1BA90A_SCANMODE_DEFAULT);

    return 0;
}

static void fts1ba90a_power_off(void* data) {
    // TODO Handle power off
}

static void fts1ba90a_parse_event(struct fts1ba90a_data* data) {
    int i;
    int event_type;

    // Coordinate event variables
    int touch_id;
    int touch_x;
    int touch_y;
    int touch_z;
    int touch_action;
    int touch_minor;
    int touch_major;

    for (i = 0; i < FTS1BA90A_STACK_DEPTH; i++) {
        uint8_t* event = &data->data[i * FTS1BA90A_EVENT_SIZE];
        event_type = event[0] & 0x3;

        switch (event_type) {
            case 0:
                // Coordinate event
                event_coordinate = (struct fts1ba90a_event_coordinate*)event;
                touch_id = event_coordinate->tid;
                // NOTE: Downstream driver does error checking for over 10 fingers here... why...

                touch_x = (event_coordinate->x_11_4 << 4) | (event_coordinate->x_3_0);
                touch_y = (event_coordinate->y_11_4 << 4) | (event_coordinate->y_3_0);
                touch_z = event_coordinate->z & 0x3F; // Area
                touch_action = event_coordinate->tchsta; // 1 = First press, 2 = Hold, 3 = Release
                touch_minor = event_coordinate->minor;
                touch_major = event_coordinate->major;

                // The X coordinate seems to be reversed to the panel, reverse it
                // NOTE: BAD BAD BAD FOR UPSTREAM, THIS ASSUMES THE PANEL IS 1600Y
                touch_x = 1600 - touch_x;

                if (touch_x != 0 && touch_y != 0) {
                    if (FTS1BA90A_SUPPORT_MULTITOUCH == 0 && touch_id == 0) {
                        // If multitouch is disabled
                        dev_info(&data->client->dev, "Finger is down\n");
                        //dev_err(&data->client->dev, "Touch X/Y/Act: %d %d %d\n", touch_x, touch_y, touch_z);
                        
                        // Send input!!!!
                        if (touch_action == 1) {
                            // First finger press
                            dev_info(&data->client->dev, "Finger has been detected\n");

                            input_mt_slot(data->input, touch_id);
                            input_mt_report_slot_state(data->input, MT_TOOL_FINGER, true);

                            input_report_abs(data->input, ABS_MT_POSITION_Y, touch_x);
                            input_report_abs(data->input, ABS_MT_POSITION_X, touch_y);
                            input_report_abs(data->input, ABS_MT_TOUCH_MAJOR, touch_major);
                            input_report_abs(data->input, ABS_MT_TOUCH_MINOR, touch_minor);
                            input_report_abs(data->input, ABS_MT_PRESSURE, touch_z);

                            input_sync(data->input);
                        } else if (touch_action == 2) {
                            input_mt_slot(data->input, touch_id);

                            input_report_abs(data->input, ABS_MT_POSITION_Y, touch_x);
                            input_report_abs(data->input, ABS_MT_POSITION_X, touch_y);
                            input_report_abs(data->input, ABS_MT_TOUCH_MAJOR, touch_major);
                            input_report_abs(data->input, ABS_MT_TOUCH_MINOR, touch_minor);
                            input_report_abs(data->input, ABS_MT_PRESSURE, touch_z);

                            input_sync(data->input);
                        } else if (touch_action == 3) {
                            // Finger release
                            dev_info(&data->client->dev, "Finger has been released\n");
                            input_mt_slot(data->input, touch_id);
                            input_mt_report_slot_inactive(data->input);

                            input_sync(data->input);
                        }
                    }
                }
                break;

            default:
                break;
        }
    }
}

static int fts1ba90a_read_event(struct fts1ba90a_data* data) {
    int ret;
    uint8_t command = FTS1BA90A_REG_READ_ALL_EVENTS;
    struct i2c_msg transfer_msg[2];
    
    transfer_msg[0].addr = data->client->addr;
    transfer_msg[0].len = 1;
    transfer_msg[0].buf = &command;
    transfer_msg[1].addr = data->client->addr;
    transfer_msg[1].flags = I2C_M_RD;
    transfer_msg[1].len = FTS1BA90A_MAX_DATA_SIZE;
    transfer_msg[1].buf = data->data;

    ret = i2c_transfer(data->client->adapter, transfer_msg, ARRAY_SIZE(transfer_msg));
    if (ret < 0) {
        return ret;
    }

    return ret == ARRAY_SIZE(transfer_msg) ? 0 : -EIO;
}

static irqreturn_t fts1ba90a_irq_handler(int irq, void* dev) {
    int err;
    struct fts1ba90a_data* data = dev;

    mutex_lock(&data->mutex);
    err = fts1ba90a_read_event(data);
    if (unlikely(err)) { // unlikely() is branch prediction
        dev_err(&data->client->dev, "Could not read IRQ event.\n");
    } else {
        fts1ba90a_parse_event(data);
    }
    mutex_unlock(&data->mutex);

    return IRQ_HANDLED;
}

static int fts1ba90a_input_open(struct input_dev* dev) {
    int err;
    struct fts1ba90a_data* data = input_get_drvdata(dev);

    err = pm_runtime_resume_and_get(&data->client->dev);
    if (err) {
        return err;
    }

    // TODO Not sure whether this is required (Upstream driver sends to register 0x93, Downstream seems to use 0x10,
    // Using downstream methodology for now)
    // P.S. Was working with the old value in the test driver
    err = i2c_smbus_write_byte(data->client, 0x93); // FTS1BA90A_REG_SENSE_ON
    if (err) {
        pm_runtime_put_sync(&data->client->dev);
        return err;
    }

    mutex_lock(&data->mutex);
    data->running = true;
    // TODO Here they enable 'hover sense', but I am 99% sure it's not even supported on this device
    mutex_unlock(&data->mutex);
    
    // TODO Here they 'use key', and I do not think it's needed.

    return 0;
}

static void fts1ba90a_input_close(struct input_dev* dev) {
    // TODO Handle shutdown
}

static int fts1ba90a_probe(struct i2c_client* client) {
    int err;
    struct fts1ba90a_data *data;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C | I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_I2C_BLOCK)) {
        return -ENODEV;
    }

    data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
    if (!data) {
        return -ENOMEM;
    }

    i2c_set_clientdata(client, data);
    data->client = client;
    mutex_init(&data->mutex);
    init_completion(&data->cmd_done);

    dev_info(&data->client->dev, "Probing for STM FTS1BA90A touchscreen...\n");

    // Setup regulators
    data->regulators[FTS1BA90A_REGULATOR_VDD].supply = "vdd";
    data->regulators[FTS1BA90A_REGULATOR_AVDD].supply = "avdd";
    err = devm_regulator_bulk_get(&client->dev, ARRAY_SIZE(data->regulators), data->regulators);
    if (err) {
        return err;
    }

    // Allocate input device
    data->input = devm_input_allocate_device(&client->dev);
    if (!data->input) {
        return -ENOMEM;
    }
    data->input->name = FTS1BA90A_DRIVER_NAME;
    data->input->id.bustype = BUS_I2C;
    data->input->open = fts1ba90a_input_open;
    data->input->close = fts1ba90a_input_close;
    
    // Set input capabilities
    input_set_capability(data->input, EV_ABS, ABS_MT_POSITION_X);
    input_set_capability(data->input, EV_ABS, ABS_MT_POSITION_Y);
    touchscreen_parse_properties(data->input, true, &data->prop);
    input_set_abs_params(data->input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
    input_set_abs_params(data->input, ABS_MT_TOUCH_MINOR, 0, 255, 0, 0);
    input_set_abs_params(data->input, ABS_MT_PRESSURE, 0, 255, 0, 0);
    // TODO Currently no orientation support
    input_set_abs_params(data->input, ABS_DISTANCE, 0, 255, 0, 0);

    err = input_mt_init_slots(data->input, FTS1BA90A_MAX_FINGERS, INPUT_MT_DIRECT);
    if (err) {
        return err;
    }
    input_set_drvdata(data->input, data);

    // Setup IRQ
    err = devm_request_threaded_irq(&client->dev, client->irq, NULL, fts1ba90a_irq_handler, IRQF_ONESHOT | IRQF_NO_AUTOEN, "fts1ba90a_irq", data);
    if (err) {
        return err;
    }

    // Power on the touchscreen
    err = fts1ba90a_power_on(data);
    if (err) {
        return err;
    }

    err = devm_add_action_or_reset(&client->dev, fts1ba90a_power_off, data);
    if (err) {
        return err;
    }

    err = input_register_device(data->input);
    if (err) {
        err;
    }

    pm_runtime_enable(&client->dev);
    device_enable_async_suspend(&client->dev);

    return 0;
}

static void fts1ba90a_remove(struct i2c_client* client) {
    pm_runtime_disable(&client->dev);
}

static void fts1ba90a_shutdown(struct i2c_client* client) {
    // TODO Add shutdown code
}

static const struct i2c_device_id fts1ba90a_device_id[] = {
    { FTS1BA90A_DRIVER_NAME },
    {},
};

static const struct of_device_id fts1ba90a_match_table[] = {
    { .compatible = "stm,fts1ba90a", }, // Change to stm,fts1ba90a
    {},
};

static struct i2c_driver fts1ba90a_driver = {
    .driver = {
        .name = FTS1BA90A_DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = fts1ba90a_match_table, // Requires CONFIG_OF (Device Tree)
        //.pm = --, // Requires CONFIG_PM (Power Management)
    },
    .probe = fts1ba90a_probe,
    .remove = fts1ba90a_remove,
    .shutdown = fts1ba90a_shutdown,
    // Suspend and resume here
    .id_table = fts1ba90a_device_id,
};

static int __init fts1ba90a_init(void) {
    return i2c_add_driver(&fts1ba90a_driver);
}

static void __exit fts1ba90a_exit(void) {
    return i2c_del_driver(&fts1ba90a_driver);
}

module_init(fts1ba90a_init);
module_exit(fts1ba90a_exit);

MODULE_DESCRIPTION("STM FTS1BA90A Touchscreen I2C Driver");
MODULE_AUTHOR("Goldenkrew3000");
MODULE_LICENSE("GPL");
