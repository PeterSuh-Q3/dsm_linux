// Copyright (c) 2000-2013 Synology Inc. All rights reserved.

#include <linux/kernel.h> /* printk() */
#include <linux/errno.h>  /* error codes */
#include <linux/delay.h>
#include <asm/io.h>
#include "../i2c/i2c-linux.h"
#include "comcerto2k_common.h"
#include <linux/interrupt.h>
#include <linux/gpio.h>

struct sd_softc {
       int     countEvents;
       int     idxPtr;
       SYNOBIOSEVENT   rgEvents[SYNOBIOS_NEVENTS];
       wait_queue_head_t wq_poll;
};
int synobios_record_event(struct sd_softc *sc, u_int event_type);
#define IRQ_MODEL_WIFIWPS 77
static int wps_gpio = -1 ;

int GetModel(void)
{
	return MODEL_DS215air;
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

static void send_wps_event(unsigned long data);
static DEFINE_TIMER(btn_wps_timer, send_wps_event, 0, 0);
static
void send_wps_event(unsigned long data)
{
	int value = -1;
	static int last = 1;

	SYNO_COMCERTO2K_GPIO_PIN(wps_gpio, &value, 0);
	WARN_ON(-1 == value);
	if (last == value) {
		goto END;
	}

	if(1 == value){
		/* button released */
		printk("synobios: wifi/wps button released\n");
		synobios_record_event(NULL,
				SYNO_EVENT_WIFIWPS);
		SYNO_GPIO_SET_FALLING_EDGE(wps_gpio);
	} else {
		/* button pushed */
		printk("synobios: wifi/wps button pressed\n");
		synobios_record_event(NULL,
				SYNO_EVENT_WIFIWPS);
		SYNO_GPIO_SET_RISING_EDGE(wps_gpio);
	}

	last = value;

END:
	return;
}
static irqreturn_t model_wifiwps_handler(int irq, void *dev_id)
{

	switch (irq) {
		case IRQ_MODEL_WIFIWPS:
			mod_timer(&btn_wps_timer, jiffies + msecs_to_jiffies(15));
			break;
		default:
			printk("Unknown irq %d\n", irq);
	}
	return IRQ_HANDLED;
}
static void register_irq_request(void)
{
       if ( request_threaded_irq(IRQ_MODEL_WIFIWPS, NULL,
                               model_wifiwps_handler, IRQF_TRIGGER_RISING,
                               "wifiwps_gpio", NULL) ) {
               printk("Failed to request WIFI/WPS IRQ\n");
       }
}

static void unregister_irq_request(void)
{
       free_irq(IRQ_MODEL_WIFIWPS, NULL);
}

int GetFanStatus(int fanno, FAN_STATUS *pStatus)
{
	int FanStatus;
	int rgcVolt[2] = {0, 0};

	if ( 1 != fanno ) {
		return -EINVAL;
	}

	do {
		SYNO_CTRL_FAN_STATUS_GET(fanno, &FanStatus);
		rgcVolt[(int)FanStatus] ++;
		if (rgcVolt[0] && rgcVolt[1]) {
			break;
		}
		udelay(300);
	} while ( (rgcVolt[0] + rgcVolt[1]) < 200 );

	if ((rgcVolt[0] == 0) || (rgcVolt[1] == 0) ) {
		*pStatus = FAN_STATUS_STOP;
	} else {
		*pStatus = FAN_STATUS_RUNNING;
	}

	return 0;
}

int 
InitModuleType(struct synobios_ops *ops)
{
	PRODUCT_MODEL model = ops->get_model();
	module_t type_215air = MODULE_T_DS215airv10;
	module_t *pType = NULL;

	switch (model) {
		case MODEL_DS215air:
			pType = &type_215air;
			break;
		default:
			break;
	}

	module_type_set(pType);

	return 0;
}

int SetDiskLedStatus(int disknum, SYNO_DISK_LED status)
{
	return SYNO_SOC_HDD_LED_SET(disknum, status);
}

int SetHDDActLed(SYNO_LED ledStatus)
{
	int err = -1;
	switch(ledStatus) {
		case SYNO_LED_OFF:
			SYNO_SOC_HDD_LED_SET(1, DISK_LED_OFF);
			SYNO_SOC_HDD_LED_SET(2, DISK_LED_OFF);
			break;
		case SYNO_LED_ON:
			SYNO_SOC_HDD_LED_SET(1, DISK_LED_GREEN_BLINK);
			SYNO_SOC_HDD_LED_SET(2, DISK_LED_GREEN_BLINK);
			break;
		default:
			goto ERR;
	}
	err = 0;
ERR:
	return err;
}

int SetPowerLedStatus(SYNO_LED status)
{
	return 0;
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

int model_addon_init(struct synobios_ops *ops)
{
	module_t* pSynoModule = NULL;
	pSynoModule = module_type_get();
	wps_gpio = pSynoModule->wifi_wps_type;
	SYNO_GPIO_SET_FALLING_EDGE(wps_gpio);
	init_timer(&btn_wps_timer);
	register_irq_request();
	return 0;
}

int model_addon_cleanup(struct synobios_ops *ops)
{
	unregister_irq_request();
	del_timer_sync(&btn_wps_timer);

	return 0;
}
