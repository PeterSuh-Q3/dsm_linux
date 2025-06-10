#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
// Copyright (c) 2000-2016 Synology Inc. All rights reserved.

#include <linux/kernel.h> /* printk() */
#include <linux/errno.h>  /* error codes */
#include <linux/delay.h>
#include <asm/io.h>
#include "../i2c/i2c-linux.h"
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/syno.h>
#include <linux/module.h>
#include <linux/synobios.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include "../mapping.h"
#include "../i2c/i2c-linux.h"
#include "../rtc/rtc.h"
#include "../mesh/mesh.h"
#include "dakota_common.h"
#include <linux/timer.h>
#include <linux/kthread.h>

extern void (*funcSYNOMaintanceUSBLED) (const SYNO_USB_MAINTANCE_TYPE iType, const char *szDev, const size_t len);
extern int SynoDebugFlag;
extern int (*funcEXTDISPLAY)(struct _SynoMsgPkt *);
extern void (*funcSynoUSBIOStart)(void);
extern int gWifiSched;
extern char gMeshBackhaulIface[128];
extern int gLedDbg;
extern int gButtonResetTimer;
extern int gButtonWifiOnOffTimer;
extern int gWifiOnOffReady;
extern bool gMeshIsRE;
extern int gMeshSingalQuality;
extern SYNO_MESH_BACKHAUL_TYPE gMeshBackhaulType;
static int gWifiSwitch = 1;
static int gInProcessing = 0;
extern unsigned int cpufreq_quick_get_max(unsigned int cpu);

typedef enum {
	LED_STATUS_ON,
	LED_STATUS_OFF,
	LED_STATUS_BLINK,
	LED_STATUS_SLOW_BLINK,
	LED_STATUS_FAST_BLINK,
	LED_STATUS_NONE,
} SYNO_LED_STATUS;


#if defined(CONFIG_SYNO_LEDS_EXTENSION)
extern int (*syno_set_ethernet_led)(int, int, int);
extern int (*syno_set_wifi_led)(int, int);
#endif /* CONFIG_SYNO_LEDS_EXTENSION */

static void send_reset_event(unsigned long data)
{
	struct syno_gpio_data *pData = (struct syno_gpio_data *)(void *)data;
	int status = 0;
	GPIO_PIN pin = {
		.pin = (int)pData->gpio,
		.value = 0
	};

	GetGpioPin(&pin);
	status = pin.value;
	switch (status) {
		case BUTTON_PUSH:
			if (gMeshIsRE) {
				break;
			}
			printk(KERN_INFO "synobios: send reset event\n");
			synobios_event_reset_method(SYNO_RESET_PASSWORD_ONLY);
			break;
		case BUTTON_RELEASE:
			break;
		default:
			break;
	}
}

#if defined(CONFIG_SYNO_AUTO_INSTALL_ABILITY)
struct led_work {
	struct _SynoMsgPkt *MsgPkt;
	struct work_struct work;
};
static struct led_work exdisplay_work;
static struct _SynoMsgPkt MsgPkt;

static void update_exdisplay_work(struct work_struct *w)
{
	struct led_work *ledwork = container_of(w, struct led_work, work);
	router_exdisplay_handler(ledwork->MsgPkt);
}

static void trigger_auto_install(unsigned long data)
{
	struct syno_gpio_data *pData = (struct syno_gpio_data *)(void *)data;
	GPIO_PIN pin = {
		.pin = (int)pData->gpio,
		.value = 0
	};

	GetGpioPin(&pin);
	switch (pin.value) {
		case BUTTON_PUSH:
			printk(KERN_INFO "synobios: reset button pressed for auto install\n");
			memset(&MsgPkt, 0, sizeof(struct _SynoMsgPkt));
			MsgPkt.usNum = SYNO_SYS_WAIT_RESET;

			exdisplay_work.MsgPkt = &MsgPkt;
			INIT_WORK(&(exdisplay_work.work), update_exdisplay_work);
			schedule_work(&(exdisplay_work.work));

			syno_force_auto_install();
			break;
		case BUTTON_RELEASE:
			break;
		default:
			break;
	}
}
#endif /* CONFIG_SYNO_AUTO_INSTALL_ABILITY */
static int router_set_router_status(struct _SynoMsgPkt *pMsgPkt);

static DEFINE_TIMER(btn_reset_timer, send_reset_event, 0, 0);
#if defined(CONFIG_SYNO_AUTO_INSTALL_ABILITY)
static DEFINE_TIMER(btn_reset_auto_install_timer, trigger_auto_install, 0, 0);
#endif /* CONFIG_SYNO_AUTO_INSTALL_ABILITY */

static irqreturn_t reset_button_irq(int irq, void *dev_id)
{
	struct syno_gpio_data *data = (struct syno_gpio_data *)dev_id;
	int status = 0;
	GPIO_PIN pin = {
		.pin = (int)data->gpio,
		.value = 0
	};

	GetGpioPin(&pin);
	status = pin.value;
	if (!(data->old_status ^ status)) {
		goto END;
	}
	btn_reset_timer.data = (unsigned long)(void *)dev_id;
#if defined(CONFIG_SYNO_AUTO_INSTALL_ABILITY)
	btn_reset_auto_install_timer.data = (unsigned long)(void *)dev_id;
#endif /* CONFIG_SYNO_AUTO_INSTALL_ABILITY */
	switch (status) {
		case BUTTON_PUSH:
			mod_timer(&btn_reset_timer, jiffies + msecs_to_jiffies(gButtonResetTimer));
#if defined(CONFIG_SYNO_AUTO_INSTALL_ABILITY)
			mod_timer(&btn_reset_auto_install_timer, jiffies + msecs_to_jiffies(10000));
#endif /* CONFIG_SYNO_AUTO_INSTALL_ABILITY */
			break;
		case BUTTON_RELEASE:
			mod_timer(&btn_reset_timer, 0);
#if defined(CONFIG_SYNO_AUTO_INSTALL_ABILITY)
			mod_timer(&btn_reset_auto_install_timer, 0);
#endif /* CONFIG_SYNO_AUTO_INSTALL_ABILITY */
			break;
		default:
			break;
	}
	data->old_status = status;
END:
	return IRQ_HANDLED;
}

static void syno_set_led(char *name, char *trigger, SYNO_LED_STATUS status)
{
#if defined(CONFIG_SYNO_LEDS_EXTENSION)
	struct led_classdev *led_cdev =  syno_get_led_cdev_by_name(name);
	struct led_classdev *led_cdev2;
	unsigned long delay_on = LED_BLINK_DELAY_TIME_ON;
	unsigned long delay_off = LED_BLINK_DELAY_TIME_OFF;
	int brightness = -1;

	if (!led_cdev) {
		goto END;
	}
	if (trigger) {
		led_cdev2 = syno_get_led_cdev_by_trigger_name(trigger);
		if (!led_cdev2 || led_cdev2->dev != led_cdev->dev) {
			syno_led_extension_ops.syno_led_trigger_store(led_cdev->dev, NULL, trigger, strlen(trigger));
		}
	}
	switch (status) {
		case LED_STATUS_ON:
			delay_off=0;
			brightness = LED_FULL;
			break;
		case LED_STATUS_OFF:
			delay_on=0;
			brightness = LED_OFF;
			break;
		case LED_STATUS_BLINK:
			break;
		case LED_STATUS_SLOW_BLINK:
			delay_on = LED_SLOW_BLINK_DELAY_TIME;
			delay_off = LED_SLOW_BLINK_DELAY_TIME;
			break;
		case LED_STATUS_FAST_BLINK:
			delay_on = LED_FAST_BLINK_DELAY_TIME;
			delay_off = LED_FAST_BLINK_DELAY_TIME;
			break;
		case LED_STATUS_NONE:
			delay_on=0;
			brightness = LED_OFF;
			break;
		default:
			goto END;
	}
	if (-1 != brightness && led_cdev->brightness != brightness) {
		led_set_brightness(led_cdev, brightness);
	}
	if (led_cdev->blink_delay_on != delay_on || led_cdev->blink_delay_off != delay_off) {
		led_blink_set(led_cdev, &delay_on, &delay_off);
	}
END:
	return;
#else
	printk("Synology LED extension not found");
#endif /* CONFIG_SYNO_LEDS_EXTENSION */
}

static void syno_set_wifi_signal_led(const char *name, const char *interface, const int len, const int activate, const int value)
{
#if defined(CONFIG_SYNO_LEDS_EXTENSION)
	struct led_classdev *led_cdev =  syno_get_led_cdev_by_name(name);
	struct syno_wifi_trig_data *td = NULL;
	char trigger[SYNO_WIFI_TD_STRING_SIZE] = "none";

	if ( !led_cdev ) {
		goto END;
	}

	if (activate) {
		strncpy(trigger, "syno_wifi", sizeof(trigger) -1);
	}

	syno_led_extension_ops.syno_led_trigger_store(led_cdev->dev, NULL, trigger, strlen(trigger));
	if (activate) {
		td = (struct syno_wifi_trig_data *) led_cdev->trigger_data;
		td->activity_interval = msecs_to_jiffies(5000);
		strncpy(td->interface, interface, len);
		td->value_threshold = value;
	} else {
		led_set_brightness(led_cdev, LED_OFF);
	}
END:
	return;
#else
	printk("Synology LED extension not found");
#endif /* CONFIG_SYNO_LEDS_EXTENSION */
}
int syno_wifionoff_read_command(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", gWifiSwitch);
	return 0;
}

int syno_led_manufactory_write(struct file *file, const char *buffer,
				unsigned long count, void *data)
{
	int val = -1;

	sscanf(buffer, MAGIC_NUM",%d", &val);

	if (-1 == val) {
		goto END;
	}

	switch(val) {
		case 0:
			break;
		case 1:
			// sucess
			syno_set_led(SZ_LED_GREEN, "none", LED_STATUS_ON);
			syno_set_led(SZ_LED_BLUE, "none", LED_STATUS_OFF);
			syno_set_led(SZ_LED_RED, "none", LED_STATUS_OFF);
			syno_set_led(SZ_LED_MIDDLE, "none", LED_STATUS_ON);
			syno_set_led(SZ_LED_HIGH, "none", LED_STATUS_ON);
			break;
		case 2:
			// fail
			syno_set_led(SZ_LED_GREEN, "none", LED_STATUS_OFF);
			syno_set_led(SZ_LED_BLUE, "none", LED_STATUS_OFF);
			syno_set_led(SZ_LED_RED, "none", LED_STATUS_ON);
			syno_set_led(SZ_LED_MIDDLE, "none", LED_STATUS_OFF);
			syno_set_led(SZ_LED_HIGH, "none", LED_STATUS_OFF);
			break;
	}
END:
	return count;
}

int syno_cpu_temperature_read_command(char *buffer, char **buffer_location, off_t offset,
				int buffer_length, int *zero, void *ptr)
{
	long temperature = 0;

	printk("Please implement %s function on synobios", __FUNCTION__);
	return snprintf(buffer, buffer_length, "%ld\n", temperature);
}

static void wifionoff_event(unsigned long data)
{
	struct syno_gpio_data *pData = (struct syno_gpio_data *)(void *)data;
	int status = 0;
	GPIO_PIN pin = {
		.pin = (int)pData->gpio,
		.value = 0
	};

	GetGpioPin(&pin);
	status = pin.value;

	switch (status) {
		case BUTTON_PUSH:
			if ( 1 == gWifiSched ) {
				gWifiSwitch = !gWifiSwitch;
			} else {
				printk("always on in wifi schedule off mode\n");
				gWifiSwitch = 1;
			}
			printk(KERN_INFO "synobios: wifi %s status\n", gWifiSwitch ? "on" : "off");
			synobios_record_event(NULL, SYNO_EVENT_WIFI_ON_OFF);
			break;
		case BUTTON_RELEASE:
			break;
		default:
			break;
	}
}

static DEFINE_TIMER(btn_wifionoff_timer, wifionoff_event, 0, 0);
static irqreturn_t rf_switch_irq(int irq, void *dev_id)
{
	struct syno_gpio_data *data = (struct syno_gpio_data *)dev_id;
	int status = 0;
	GPIO_PIN pin = {
		.pin = (int)data->gpio,
		.value = 0
	};

	if (!gWifiOnOffReady) {
		goto END;
	}

	GetGpioPin(&pin);
	status = pin.value;

	if ( !(data->old_status ^ status) ) {
		goto END;
	}

	btn_wifionoff_timer.data = (unsigned long)(void *)dev_id;
	switch (status) {
		case BUTTON_PUSH:
			mod_timer(&btn_wifionoff_timer, jiffies + msecs_to_jiffies(gButtonWifiOnOffTimer));
			break;
		case BUTTON_RELEASE:
			mod_timer(&btn_wifionoff_timer, 0);
			break;
		default:
			break;
	}
	data->old_status = status;
END:
	return IRQ_HANDLED;
}

static irqreturn_t wps_button_irq(int irq, void *dev_id)
{
	struct syno_gpio_data *data = (struct syno_gpio_data *)dev_id;
	int status = 0;
	GPIO_PIN pin = {
		.pin = (int)data->gpio,
		.value = 0
	};

	/* skip wps event while changing wifi settings */
	if (gInProcessing) {
		printk(KERN_INFO "synobios: In processing, skip WPS irq\n");
		goto END;
	}

	GetGpioPin(&pin);
	status = pin.value;
	if (data->is_active_low ^ status) {
		printk(KERN_INFO "synobios: wifi/wps button pressed\n");
		synobios_record_event(NULL,	SYNO_EVENT_WIFIWPS);
		data->old_status = status;
	}

END:
	return IRQ_HANDLED;
}

static struct syno_gpio_data reset_button = {
	.gpio = 18,
	.irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
	.is_active_low = 1,
	.handler = reset_button_irq,
	.old_status = 1,
	.name = "SYS reset"
};

static struct syno_gpio_data rf_switch = {
	.gpio = 19,
	.irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
	.is_active_low = 1,
	.handler = rf_switch_irq,
	.old_status = 1,
	.name = "RF switch"
};

static struct syno_gpio_data wps_button = {
	.gpio = 44,
	.irq_flags = IRQF_TRIGGER_FALLING,
	.is_active_low = 1,
	.handler = wps_button_irq,
	.old_status = 1,
	.name = "WPS"
};

static int Uninitialize(void)
{
	return 0;
}

int GetModel(void)
{
	return MODEL_MR2200ac;
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
	snprintf(cpu->clock, sizeof(char) * maxLength, "%d", cpufreq_quick_get_max(0) / 1000);
}

int SynoFlowingLedThread(void* pObject)
{
	syno_set_led(SZ_LED_GREEN, "none", LED_STATUS_OFF);
	syno_set_led(SZ_LED_BLUE, "none", LED_STATUS_OFF);
	syno_set_led(SZ_LED_RED, "none", LED_STATUS_OFF);
	syno_set_led(SZ_LED_MIDDLE, "none", LED_STATUS_OFF);
	syno_set_led(SZ_LED_HIGH, "none", LED_STATUS_OFF);

	if (gLedDbg) {
		printk("LED_DBG, %s start\n", __FUNCTION__);
	}

	while (!kthread_should_stop()) {
		syno_set_led(SZ_LED_GREEN, "none", LED_STATUS_ON);
		msleep(160);
		syno_set_led(SZ_LED_MIDDLE, "none", LED_STATUS_ON);
		msleep(160);
		syno_set_led(SZ_LED_HIGH, "none", LED_STATUS_ON);
		msleep(160);
		syno_set_led(SZ_LED_GREEN, "none", LED_STATUS_OFF);
		syno_set_led(SZ_LED_MIDDLE, "none", LED_STATUS_OFF);
		syno_set_led(SZ_LED_HIGH, "none", LED_STATUS_OFF);
		msleep(165);
	}

	syno_set_led(SZ_LED_GREEN, "none", LED_STATUS_OFF);
	syno_set_led(SZ_LED_BLUE, "none", LED_STATUS_OFF);
	syno_set_led(SZ_LED_RED, "none", LED_STATUS_OFF);
	syno_set_led(SZ_LED_MIDDLE, "none", LED_STATUS_OFF);
	syno_set_led(SZ_LED_HIGH, "none", LED_STATUS_OFF);

	memset(&MsgPkt, 0, sizeof(struct _SynoMsgPkt));
	MsgPkt.usNum = SYNO_LED_NETWORK_SETTING_END;
	MsgPkt.usLen = 0;
	exdisplay_work.MsgPkt = &MsgPkt;
	INIT_WORK(&(exdisplay_work.work), update_exdisplay_work);
	schedule_work(&(exdisplay_work.work));

	if (gLedDbg) {
		printk("LED_DBG, %s finish\n", __FUNCTION__);
	}
	return 0;
}
static DEFINE_MUTEX(ledLock);
int router_exdisplay_handler(struct _SynoMsgPkt *pMsgPkt)
{
	const int middle_threshold = 60;
	const int high_threshold = 80;
	int ret = -1, sub_msg_num = 0;
	int green = LED_STATUS_OFF;
	int red = LED_STATUS_OFF;
	int blue = LED_STATUS_OFF;
	int middle = LED_STATUS_OFF;
	int high = LED_STATUS_OFF;
	int pre_mask = 0;
	int pre_connected = 0;
	int is_reset_signal = 0;
	static int in_wps = 0;
	static int in_findme = 0;
	static int in_network_setting = 0;
	static int mask = 0;
	static int connected = 0;
	static int pre_event = -1;
	static struct task_struct *pProcessingThread = NULL;
	struct task_struct *pProcessingTempThread = NULL;

	if (!pMsgPkt) {
		goto END;
	}
	pre_mask = mask;
	pre_connected = connected;
	if (gLedDbg) {
		printk("LED_DBG, msg num %04lx\n", pMsgPkt->usNum);
	}
	switch(pMsgPkt->usNum) {
		case SYNO_LED_USBSTATION_MEMTEST_LED:
			break;
		case SYNO_SYS_BOOT:
			blue = LED_STATUS_ON;
			break;
		case SYNO_SYS_RUN:
			/* the model init status control by synoledcontrol, libhwcontrol*/
			break;
		case SYNO_LED_NOMASK_ALL:
			mask = 0;
			break;
		case SYNO_LED_CONNECT:
			green = LED_STATUS_ON;
			connected = 1;
			break;
		case SYNO_LED_READY_TO_SETUP:
			/* Scemd send this event contiguously while first time installation is no finished,
			 * so the blink cycle of blue led will be reset, and bright time of blue led will be inconsistent.
			 * Use pre_event to prevent this situation/
			 * */
			if (SYNO_LED_READY_TO_SETUP == pre_event) {
				goto END;
			}

			if (!gMeshIsRE) {
				connected = 0;
			}

			blue = LED_STATUS_BLINK;
			break;
		case SYNO_LED_NETWORK_SETTING:
			if (0 == pMsgPkt->usLen || !gWifiSwitch) {
				goto END;
			}

			// wps's priority > findme's priority
			if (0 == strncmp(pMsgPkt->szMsg, "wps", pMsgPkt->usLen)) {
				in_wps = 1;
			} else if (0 == strncmp(pMsgPkt->szMsg, "findme", pMsgPkt->usLen)) {
				/*
				 * If RE is in_findme, then changing any wifi-settings will enter "in_processing",
				 * in_findme will be cleared by RE.
				 * After "in_processing" is finished and RE reconnects to CAP,
				 * CAP will resend findme event to RE if findme is not timeout/cancelled.
				 */
				in_findme = 1;
			} else if (0 == strncmp(pMsgPkt->szMsg, "processing", pMsgPkt->usLen)) {
				gInProcessing = 1;
				in_wps = 0;
				in_findme = 0;
			} else {
				printk("unsupport network_setting parameter: %s\n", pMsgPkt->szMsg);
				goto END;
			}

			in_network_setting = 0;

			break;
		case SYNO_LED_DISCONNECT:
			in_network_setting = 0;
			connected = 0;

			if (!gMeshIsRE) {
				green = LED_STATUS_ON;
				break;
			}

			in_wps = 0;
			in_findme = 0;

			if (gWifiSwitch) {
				if (gInProcessing) {
					break;
				} else {
					is_reset_signal = 1;
					red = LED_STATUS_ON;
				}
			} else {
				gInProcessing = 0;
				green = LED_STATUS_SLOW_BLINK;
			}
			break;
		case SYNO_LED_MASK_ALL:
			mask = 1;
			break;
		case SYNO_SYS_SHUTDOWN:
			break;
		case SYNO_SYS_NO_SYSTEM: /* SYNO_SYS_NODISK */
			green = LED_STATUS_OFF;
			red = LED_STATUS_BLINK;
			blue = LED_STATUS_OFF;
			break;
		case SYNO_BEEP_ON:
			break;
		case SYNO_LED_HDD_GS:
			break;
		case SYNO_LED_USB_VOL_MOUNT:
			break;
		case SYNO_LED_NETWORK_SETTING_END:
			in_network_setting = 0;

			if (0 < pMsgPkt->usLen) {
				// wps's priority > findme's priority
				if (0 == strncmp(pMsgPkt->szMsg, "wps", pMsgPkt->usLen)) {
					in_wps = 0;
				} else if (0 == strncmp(pMsgPkt->szMsg, "findme", pMsgPkt->usLen)) {
					in_findme = 0;
				} else if (0 == strncmp(pMsgPkt->szMsg, "processing", pMsgPkt->usLen)) {
					gInProcessing = 0;
				} else {
					printk("unsupport network_setting parameter: %s\n", pMsgPkt->szMsg);
					goto END;
				}
			}

			/* consider factory mode,
			 * before first time installation is finished,
			 * the role of DUT is neithor CAP nor RE,
			 * it should turn on the green LED;
			 * */
			if (!connected && gMeshIsRE) {
				if (MESH_BACKHAUL_TYPE_WIRELESS == gMeshBackhaulType && 0 != strncmp(pMsgPkt->szMsg, "processing", pMsgPkt->usLen)) {
					goto END;
				}

				if (gWifiSwitch) {
					red = LED_STATUS_ON;
				} else {
					green = LED_STATUS_SLOW_BLINK;
				}
			} else {
				green = LED_STATUS_ON;
			}
			break;
		case SYNO_MESH_BACKHAUL_IFACE:
			if (!gMeshIsRE) {
				break;
			}

			in_wps = 0;
			in_findme = 0;

			goto END;
		case SYNO_MESH_SIGNAL_QUALITY:
			if (gInProcessing) {
				goto END;
			}
			is_reset_signal = 1;
			break;
		default:
			printk("%s, Unhandled msg num %04lx\n", __FUNCTION__, pMsgPkt->usNum);
			break;
	}

	if (0 == in_network_setting && (SYNO_SYS_BOOT == pMsgPkt->usNum || 
		SYNO_LED_CONNECT == pMsgPkt->usNum|| SYNO_LED_NETWORK_SETTING == pMsgPkt->usNum || SYNO_LED_NETWORK_SETTING_END == pMsgPkt->usNum||
		SYNO_LED_READY_TO_SETUP == pMsgPkt->usNum || SYNO_LED_DISCONNECT == pMsgPkt->usNum || SYNO_SYS_NO_SYSTEM == pMsgPkt->usNum)) {

		mutex_lock(&ledLock);
		if (gInProcessing) {
			if (NULL == pProcessingThread) {
				pProcessingThread = kthread_run(SynoFlowingLedThread, NULL, "flowing_led");
			}
			mutex_unlock(&ledLock);
			goto END;
		}

		if (pProcessingThread) {
			pProcessingTempThread = pProcessingThread;
			pProcessingThread = NULL;
			kthread_stop(pProcessingTempThread);
			// set event-end led status in kthread
			mutex_unlock(&ledLock);
			goto END;
		}
		mutex_unlock(&ledLock);

		if (in_wps) {
			green = LED_STATUS_FAST_BLINK;
			is_reset_signal = 2;
			middle = LED_STATUS_OFF;
			high = LED_STATUS_OFF;
		} else if (in_findme) {
			// set LED off first to sync their timer
			syno_set_led(SZ_LED_GREEN, "none", LED_STATUS_OFF);
			syno_set_led(SZ_LED_MIDDLE, "none", LED_STATUS_OFF);
			syno_set_led(SZ_LED_HIGH, "none", LED_STATUS_OFF);
			msleep(2000);

			green = LED_STATUS_FAST_BLINK;
			blue = LED_STATUS_OFF;
			red = LED_STATUS_OFF;
			middle = LED_STATUS_FAST_BLINK;
			high = LED_STATUS_FAST_BLINK;
		} else {
			// normal state
			is_reset_signal = 1;
			middle = LED_STATUS_OFF;
			high = LED_STATUS_OFF;
		}

		syno_set_led(SZ_LED_GREEN, "none", green);
		syno_set_led(SZ_LED_BLUE, "none", blue);
		syno_set_led(SZ_LED_RED, "none", red);
		syno_set_led(SZ_LED_MIDDLE, "none", middle);
		syno_set_led(SZ_LED_HIGH, "none", high);

		if (gLedDbg) {
			printk("LED_DBG, in_wps: %d, in_findme: %d gInProcessing: %d, green: %d, blue: %d, red: %d, middle: %d, high: %d\n",
				in_wps, in_findme, gInProcessing, green, blue, red, middle, high);
		}
	}

	if (pre_connected != connected || pre_mask != mask || is_reset_signal) {
		if (2 != is_reset_signal) {
			 if (gMeshIsRE && MESH_BACKHAUL_TYPE_WIRELESS == gMeshBackhaulType) {
				 if (!connected) {
					 middle = LED_STATUS_OFF;
					 high = LED_STATUS_OFF;
				 } else {
					 if (!mask && gMeshSingalQuality >= middle_threshold) {
						 middle = LED_STATUS_ON;
					 }
					 if (!mask && gMeshSingalQuality >= high_threshold) {
						 high = LED_STATUS_ON;
					 }
				 }
			} else {
				if (!mask && connected) {
					middle = LED_STATUS_ON;
					high = LED_STATUS_ON;
				} else {
					middle = LED_STATUS_OFF;
					high = LED_STATUS_OFF;
				}
			}
		}

		syno_set_led(SZ_LED_MIDDLE, "none", middle);
		syno_set_led(SZ_LED_HIGH, "none", high);
		if (gLedDbg) {
			printk("LED_DBG, connected = %d, mask = %d, is_reset_signal = %d, gMeshIsRE = %d, middle: %d[%d], high: %d[%d]\n",
				connected, mask, is_reset_signal, gMeshIsRE, middle, middle_threshold, high, high_threshold);
		}
	}

	ret = 0;
END:
	if (SYNO_LED_NETWORK_SETTING == pMsgPkt->usNum || in_findme || in_wps || gInProcessing) {
		in_network_setting = 1;
	}
	pre_event = pMsgPkt->usNum;
	return ret;
}

static int set_router_status(struct _SynoMsgPkt *pMsgPkt)
{
	int ret = 0;
	if (!pMsgPkt) {
		ret = -1;
		goto END;
	}

	switch (pMsgPkt->usNum) {
		case SYNO_EVENT_WIFI_ON_OFF_READY:
			gWifiOnOffReady = 1;
			break;
		default:
			ret = -1;
			printk("%s, Unhandled set ready msg num %04lx\n", __FUNCTION__, pMsgPkt->usNum);
			break;
	}

END:
	return ret;
}

void resetButtonHelper(bool blEnable) {
	if (blEnable) {
		init_timer(&btn_reset_timer);
#if defined(CONFIG_SYNO_AUTO_INSTALL_ABILITY)
		init_timer(&btn_reset_auto_install_timer);
#endif /* CONFIG_SYNO_AUTO_INSTALL_ABILITY */
		syno_irq_register(&reset_button);
	} else {
		syno_irq_unregister(&reset_button);
		del_timer_sync(&btn_reset_timer);
#if defined(CONFIG_SYNO_AUTO_INSTALL_ABILITY)
		del_timer_sync(&btn_reset_auto_install_timer);
#endif /* CONFIG_SYNO_AUTO_INSTALL_ABILITY */
	}
}
int model_addon_init(struct synobios_ops *ops)
{
	char buf[32];

	if ( 0 == syno_get_uboot_env_variable("syno_wifionoff", buf, sizeof(buf)) ) {
		gWifiSwitch = ('0' == buf[0]) ? 0 : 1;
	} else {
		gWifiSwitch = 1;
	}

	ops->exdisplay_handler = router_exdisplay_handler;
	ops->set_router_status = set_router_status;
	syno_irq_register(&wps_button);

	start_syno_proc();
	start_syno_wifi_proc();
	start_syno_mesh_proc();
	start_syno_button_proc();

	syno_irq_register(&rf_switch);
	resetButtonHelper(true);
	syno_set_led(SZ_LED_BLUE, "none", LED_STATUS_ON);
	return 0;
}

int model_addon_cleanup(struct synobios_ops *ops)
{
	syno_irq_unregister(&wps_button);
	ops->exdisplay_handler = NULL;

	remove_syno_button_proc();
	remove_syno_mesh_proc();
	remove_syno_wifi_proc();
	remove_syno_proc();

	resetButtonHelper(false);
	syno_irq_unregister(&rf_switch);
	return 0;
}

int GetBrand(void)
{
	return BRAND_SYNOLOGY;
}

static int 
InitModuleType(struct synobios_ops *ops)
{
	PRODUCT_MODEL model = ops->get_model();
	module_t type_mr2200acv10 = MODULE_T_MR2200acv10;
	module_t *pType = NULL;

	switch (model) {
		case MODEL_MR2200ac:
#ifdef MY_DEF_HERE
			if (!strncmp(gszSynoHWVersion, HW_MR2200ac, strlen(HW_MR2200ac))) {
				pType = &type_mr2200acv10;
			} else {
				WARN_ON("Never happened!!..");
				pType = &type_mr2200acv10;
			}
#else
			pType = &type_mr2200acv10;
#endif
			break;
		default:
			break;
	}

	module_type_set(pType);

	return 0;
}

static struct synobios_ops synobios_ops = {
	.owner                = THIS_MODULE,
	.get_brand            = GetBrand,
	.get_model            = GetModel,
	.get_rtc_time         = NULL,
	.set_rtc_time         = NULL,
	.get_fan_status       = NULL,
	.set_fan_status       = NULL,
	.get_gpio_pin         = GetGpioPin,
	.set_gpio_pin         = SetGpioPin,
	.set_power_led        = NULL,
	.set_disk_led         = NULL,
	.get_sys_temperature  = NULL,
	.get_cpu_temperature  = GetCpuTemperature,
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
	.check_microp_id      = NULL,
	.set_microp_id        = NULL,
	.get_cpu_info         = GetCPUInfo,
	.set_router_status    = NULL,
};

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

	*ops = &synobios_ops;
	if (synobios_ops.init_auto_poweron) {
		synobios_ops.init_auto_poweron();
	}

	model_addon_init(*ops);

	return 0;
}

