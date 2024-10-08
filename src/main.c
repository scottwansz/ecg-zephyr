/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr.h>
#include <sys/printk.h>
#include <drivers/sensor.h>
#include <logging/log.h>

#include "../fs_fat/fs_fat.h"
#include "../ble_service/ble.h"

LOG_MODULE_REGISTER(main);

#define ADS1298R DT_NODELABEL(ads1298r)
#define ADS1294R DT_NODELABEL(ads1294r)

struct sensor_value value_1298r[9], value_1294r[5];
// const struct device *dev = DEVICE_DT_GET_ANY(ti_ads129xr);
const struct device *ads1294r = DEVICE_DT_GET(ADS1294R);
const struct device *ads1298r = DEVICE_DT_GET(ADS1298R);

// SD card init and open file status, 0 - success
int sd_ok = -1; 


static void ads129xr_get_data(void){

	sensor_channel_get(ads1294r, SENSOR_CHAN_ALL, value_1294r);
	sensor_channel_get(ads1298r, SENSOR_CHAN_ALL, value_1298r);

	static int counter = 1; // conunt the times this func called
	counter++;

	// use decoded data
	int16_t buff[12] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	for (size_t i = 1; i < 9; i++)
	{
		buff[i-1] = (int16_t)(sensor_value_to_double(&value_1298r[i])*1000);
	}

	for (size_t i = 1; i < 5; i++)
	{
		buff[7+i] = (int16_t)(sensor_value_to_double(&value_1294r[i])*1000);
	}

	LOG_INF("%d volt(µV) : %d %d %d %d %d %d %d %d %d %d %d %d", counter, 
		buff[0], buff[1], buff[2], buff[3], buff[4], buff[5], buff[6], buff[7], 
		buff[8], buff[9], buff[10], buff[11]);

	if(sd_ok == 0){
		save_data(buff, sizeof(buff), counter); // save ECG data as integer with µV unit when use decoded data
	};


	if(my_ble_connected) {
		// ble_send( (uint8_t *)buff, sizeof(buff) );
		// ** it seems only 20 bytes can be sent **
		static uint8_t buff2[20] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		};
		ble_send( (uint8_t *)buff2, sizeof(buff2) );
		LOG_INF("size of buff: %d", sizeof(buff2));
	}
}

static void trigger_handler(const struct device *dev, struct sensor_trigger *trig)
{
	ads129xr_get_data();
	LOG_INF("Sensor trigger called.");

	//use raw data
	// uint8_t buff[24];
	// for (size_t i = 1; i < 9; i++)
	// {
	// 	buff[(i-1)*2] = (value_1298r[i].val1) >> 8;
	// 	buff[(i-1)*2 +1 ] = value_1298r[i].val1;
	// }

	// LOG_INF("%d: %02x %02x", counter, buff[0], buff[1]);

	// for (size_t i = 1; i < 5; i++)
	// {
	// 	buff[16 + (i-1)*2] = value_1294r[i].val1 >> 8;
	// 	buff[16 + (i-1)*2 + 1] = value_1294r[i].val1;
	// }
};



void main(void)
{
	LOG_INF("Hello World! %s\n", CONFIG_BOARD);
	sd_ok = fs_init();

	if (!device_is_ready(ads1294r)) {
		LOG_INF("Device %s is not ready", ads1294r->name);
		return;
	}
	if (!device_is_ready(ads1298r)) {
		LOG_INF("Device %s is not ready", ads1298r->name);
		return;
	}

	
	static struct sensor_trigger drdy_trigger = {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ALL,
	};

	// two ads129xr share the same interupter pin, so only set on one of them
	int rc = sensor_trigger_set(ads1298r, &drdy_trigger, trigger_handler);

	if (rc != 0) {
		LOG_INF("Trigger set failed: %d", rc);
	}

	LOG_INF("Trigger set success.");

	ble_init();

	// k_cpu_idle();

	while (1) {
		// printk("Hello World! %s\n", CONFIG_BOARD);
		k_msleep(1000);

		ads129xr_get_data();
		LOG_INF("Get data in wile loop.");

		// static uint16_t buff[5] = {0x00, 0x11, 0x12, 0x13, 0x14};
		// ble_send((uint8_t *)buff, sizeof(buff));

		// static uint8_t counter = 0;
		// counter++;
		// buff[0] = counter;

		// LOG_INF("Counter in while: %d, sizeof buff: %d", counter, sizeof(buff));
	}
}
