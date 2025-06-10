// Copyright (c) 2000-2021 Synology Inc. All rights reserved.

#include <linux/kernel.h> /* printk() */
#include <linux/errno.h>  /* error codes */
#include <linux/delay.h>
#include "synobios.h"
#include "raptorlakes_common.h"

// extern function from raptorlakes_common
extern int I2CSmbusReadPowerStatus(int i2c_bus_no, u16 i2c_addr, SYNO_POWER_STATUS* status);
extern int xsSetBuzzerClear(unsigned char buzzer_cleared);
extern int xsGetBuzzerCleared(unsigned char *buzzer_cleared);
extern int xsCPUFanSpeedMapping(FAN_SPEED speed);
extern int xsFanSpeedMapping(FAN_SPEED speed);
#ifdef CONFIG_SYNO_HWMON_PMBUS
extern int RaptorlakesPmbusGetPowerInfo(POWER_INFO *power_info);
#endif /* CONFIG_SYNO_HWMON_PMBUS */

SYNO_HWMON_SENSOR_TYPE RS4025xsp_thermal_sensor = {
	.type_name = HWMON_SYS_THERMAL_NAME,
	.sensor_num = 3,
	.sensor[0] = {
		.sensor_name = "Remote1",
	},
	.sensor[1] = {
		.sensor_name = "Local",
	},
	.sensor[2] = {
		.sensor_name = "Remote2",
	},
};

SYNO_HWMON_SENSOR_TYPE RS4025xsp_voltage_sensor = {
	.type_name = HWMON_SYS_VOLTAGE_NAME,
	.sensor_num = 5,
	.sensor[0] = {
		.sensor_name = "VCC",
	},
	.sensor[1] = {
		.sensor_name = "VPP",
	},
	.sensor[2] = {
		.sensor_name = "V33",
	},
	.sensor[3] = {
		.sensor_name = "V5",
	},
	.sensor[4] = {
		.sensor_name = "V12",
	},
};

SYNO_HWMON_SENSOR_TYPE RS4025xsp_fan_speed_rpm = {
	.type_name = HWMON_SYS_FAN_RPM_NAME,
	.sensor_num = 4,
	.sensor[0] = {
		.sensor_name = HWMON_SYS_FAN1_RPM,
	},
	.sensor[1] = {
		.sensor_name = HWMON_SYS_FAN2_RPM,
	},
	.sensor[2] = {
		.sensor_name = HWMON_SYS_FAN3_RPM,
	},
	.sensor[3] = {
		.sensor_name = HWMON_SYS_FAN4_RPM,
	},
};

SYNO_HWMON_SENSOR_TYPE RS4025xsp_psu_status[2] = {
	{
		.type_name = HWMON_PSU1_STATUS_NAME,
		.sensor_num = 5,
		.sensor[0] = {
			.sensor_name = HWMON_PSU_SENSOR_PIN,
		},
		.sensor[1] = {
			.sensor_name = HWMON_PSU_SENSOR_POUT,
		},
		.sensor[2] = {
			.sensor_name = HWMON_PSU_SENSOR_TEMP,
		},
		.sensor[3] = {
			.sensor_name = HWMON_PSU_SENSOR_FAN,
		},
		.sensor[4] = {
			.sensor_name = HWMON_PSU_SENSOR_STATUS,
		},
	},
	{
		.type_name = HWMON_PSU2_STATUS_NAME,
		.sensor_num = 5,
		.sensor[0] = {
			.sensor_name = HWMON_PSU_SENSOR_PIN,
		},
		.sensor[1] = {
			.sensor_name = HWMON_PSU_SENSOR_POUT,
		},
		.sensor[2] = {
			.sensor_name = HWMON_PSU_SENSOR_TEMP,
		},
		.sensor[3] = {
			.sensor_name = HWMON_PSU_SENSOR_FAN,
		},
		.sensor[4] = {
			.sensor_name = HWMON_PSU_SENSOR_STATUS,
		},
	},
};

SYNO_HWMON_SENSOR_TYPE RS4025xsp_hdd_backplane_status = {
	.type_name = HWMON_HDD_BP_STATUS_NAME,
	.sensor_num = 2,
	.sensor[0] = {
		.sensor_name = HWMON_HDD_BP_DETECT,
	},
	.sensor[1] = {
		.sensor_name = HWMON_HDD_BP_ENABLE,
	},
};

static SYNO_GPIO_INFO alarm_led = {
	.nr_gpio                = 1,
	.gpio_port              = {57},
	.gpio_polarity  = ACTIVE_HIGH,
};

static
void RS4025xspGpioInit(void)
{
	syno_gpio.alarm_led			= &alarm_led;
}

static
void RS4025xspGpioCleanup(void)
{
	syno_gpio.alarm_led			= NULL;
}

static
int RS4025xspInitModuleType(struct synobios_ops *ops)
{
	module_t type_rs4025xsp = MODULE_T_RS4025xsp;
	module_t *pType = &type_rs4025xsp;
	GPIO_PIN Pin;

	/* If user put "buzzer off" of redundant power then poweron,
	 * It may cause gpio RAPTORLAKES_BUZZER_CTRL_PIN set to low, it will casue unwanted buzzer off event*/
	if (ops && ops->set_gpio_pin) {
		Pin.pin = RAPTORLAKES_BUZZER_CTRL_PIN;
		Pin.value = 1;
		ops->set_gpio_pin(&Pin);
	}

	module_type_set(pType);
	return 0;
}

struct model_ops rs4025xsp_ops = {
	.x86_init_module_type = RS4025xspInitModuleType,
	.x86_fan_speed_mapping = xsFanSpeedMapping,
	.x86_set_esata_led_status = NULL,
	.x86_cpufan_speed_mapping = xsCPUFanSpeedMapping,
	.x86_get_buzzer_cleared = xsGetBuzzerCleared,
#ifdef CONFIG_SYNO_HWMON_PMBUS
	.x86_get_power_status = RaptorlakesPmbusGetPowerInfo,
#endif /* CONFIG_SYNO_HWMON_PMBUS */
	.x86_set_buzzer_clear = xsSetBuzzerClear,
	.x86_gpio_init = RS4025xspGpioInit,
	.x86_gpio_cleanup = RS4025xspGpioCleanup,
};

struct hwmon_sensor_list rs4025xsp_sensor_list = {
	.thermal_sensor = &RS4025xsp_thermal_sensor,
	.voltage_sensor = &RS4025xsp_voltage_sensor,
	.fan_speed_rpm = &RS4025xsp_fan_speed_rpm,
	.psu_status = RS4025xsp_psu_status,
	.hdd_backplane = &RS4025xsp_hdd_backplane_status,
};
