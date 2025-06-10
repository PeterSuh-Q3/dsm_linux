// Copyright (c) 2000-2013 Synology Inc. All rights reserved.

#include <linux/kernel.h> /* printk() */
#include <linux/errno.h>  /* error codes */
#include <linux/delay.h>
#include <asm/io.h>
#include "../i2c/i2c-linux.h"
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/syno.h>
#include <linux/module.h>
#include "synobios.h"
#include <linux/fs.h>
#include <asm/io.h>
#include "../mapping.h"
#include "../i2c/i2c-linux.h"
#include "../rtc/rtc.h"

static int Uninitialize(void)
{
	return 0;
}

int model_addon_init(struct synobios_ops *ops)
{
	return 0;
}

int model_addon_cleanup(struct synobios_ops *ops)
{
	return 0;
}

int GetModel(void)
{
	return MODEL_DS215router;
}

void GetCPUInfo(SYNO_CPU_INFO *cpu, const unsigned int maxLength)
{
#if defined(CONFIG_SMP)
	int i;

	cpu->core = 0;
	for_each_online_cpu(i) {
		cpu->core++;
	}
#else /* CONFIG_SMP */
	cpu->core = 1;
#endif
	snprintf(cpu->clock, sizeof(char) * maxLength, "%d", 1200);
}

static int 
InitModuleType(struct synobios_ops *ops)
{
	PRODUCT_MODEL model = ops->get_model();
	module_t type_215router = MODULE_T_DS215routerv10;
	module_t *pType = NULL;

	switch (model) {
		case MODEL_DS215router:
			pType = &type_215router;
			break;
		default:
			break;
	}

	module_type_set(pType);

	return 0;
}

static struct synobios_ops synobios_ops = {
	.owner                = THIS_MODULE,
	.get_brand            = NULL,
	.get_model            = GetModel,
	.get_rtc_time         = NULL,
	.set_rtc_time         = NULL,
	.get_fan_status       = NULL,
	.set_fan_status       = NULL,
	.get_gpio_pin         = NULL,
	.set_gpio_pin         = NULL,
	.set_power_led        = NULL,
	.set_disk_led         = NULL,
	.get_sys_temperature  = NULL,
	.get_cpu_temperature  = NULL,
	.get_auto_poweron     = NULL,
	.set_auto_poweron     = NULL,
	.init_auto_poweron    = NULL,
	.uninit_auto_poweron  = NULL,
	.set_alarm_led        = NULL,
	.get_backplane_status = NULL,
	.get_mem_byte         = NULL,
	.get_buzzer_cleared   = NULL,
	.set_phy_led          = NULL,
	.set_hdd_led          = NULL,
	.module_type_init     = InitModuleType,
	.uninitialize         = Uninitialize,
	.check_microp_id	 = NULL,
	.set_microp_id		 = NULL,
	.get_cpu_info		 = GetCPUInfo,
	.set_aha_led          = NULL,
};

int SetDiskLedStatus(int disknum, SYNO_DISK_LED status)
{
	return 0;
}

int SetHDDActLed(SYNO_LED ledStatus)
{
	int err = -1;
	return err;
}

int SetAlarmLed(unsigned char type)
{
	return 0;
}

int GetBackPlaneStatus(BACKPLANE_STATUS *pStatus)
{
	return 0;
}

int SetPhyLed(SYNO_LED ledStatus)
{
	return 0;
}

int synobios_model_cleanup(struct file_operations *fops, struct synobios_ops **ops)
{
	model_addon_cleanup(*ops);

	return 0;
}

int synobios_model_init(struct file_operations *fops, struct synobios_ops **ops)
{
	module_t* pSynoModule = NULL;

	if (synobios_ops.module_type_init) {
		synobios_ops.module_type_init(&synobios_ops);
	}

	pSynoModule = module_type_get();
	if( pSynoModule && RTC_SEIKO == pSynoModule->rtc_type ) {
		synobios_ops.get_rtc_time		 = rtc_seiko_get_time;
		synobios_ops.set_rtc_time		 = rtc_seiko_set_time;
		synobios_ops.get_auto_poweron	 = rtc_get_auto_poweron;
		synobios_ops.set_auto_poweron	 = rtc_seiko_set_auto_poweron;
		synobios_ops.init_auto_poweron	 = rtc_seiko_auto_poweron_init;
		synobios_ops.uninit_auto_poweron = rtc_seiko_auto_poweron_uninit;
	}

	*ops = &synobios_ops;
	if( synobios_ops.init_auto_poweron ) {
		synobios_ops.init_auto_poweron();
	}

	model_addon_init(*ops);

	return 0;
}

