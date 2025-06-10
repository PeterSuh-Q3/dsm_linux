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
#include "ipq806x_common.h"
#include <linux/timer.h>
#include <linux/kthread.h>

extern bool gMeshIsRE;
extern bool gMeshHeartBeatAlive;

extern void (*funcSYNOMaintanceUSBLED) (const SYNO_USB_MAINTANCE_TYPE iType, const char *szDev, const size_t len);
extern int SynoDebugFlag;
extern int (*funcEXTDISPLAY)(struct _SynoMsgPkt *);
extern void (*funcSynoUSBIOStart)(void);

static int gWifiSwitch = 1;
static unsigned int mask_all = 0;
static EXTERNAL_STORAGE_MOUNT_STATUS mount_status = EXTERNAL_STORAGE_MOUNT_STATUS_NO_VOLUME;
extern int gWifiSched;
extern int gButtonResetTimer;
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
static const char usbdev_name_list[] = "2-1,3-1,1-1";
extern ssize_t syno_external_dev_trig_usbdev_name_list_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size);

extern int (*syno_set_ethernet_led)(int, int, int);
extern int (*syno_set_wifi_led)(int, int);
#endif /* CONFIG_SYNO_LEDS_EXTENSION */

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
		if (!strcmp(trigger, "syno_external_dev")) {
			syno_external_dev_trig_usbdev_name_list_store(led_cdev->dev, NULL, usbdev_name_list, strlen(usbdev_name_list));
			goto END;
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

static void set_led_external_storage_persist(void)
{
	if (mask_all || EXTERNAL_STORAGE_MOUNT_STATUS_NO_VOLUME == mount_status) {
		syno_set_led(SZ_LED_GREEN_EXTERNAL_STORAGE, "none", LED_STATUS_OFF);
		syno_set_led(SZ_LED_ORANGE_EXTERNAL_STORAGE, "none", LED_STATUS_OFF);
	} else if (EXTERNAL_STORAGE_MOUNT_STATUS_ALL_MOUNTED == mount_status) {
		syno_set_led(SZ_LED_GREEN_EXTERNAL_STORAGE, "syno_external_dev", LED_STATUS_ON);
		syno_set_led(SZ_LED_ORANGE_EXTERNAL_STORAGE, "none", LED_STATUS_OFF);
	} else if (EXTERNAL_STORAGE_MOUNT_STATUS_FORMATTING == mount_status) {
		syno_set_led(SZ_LED_GREEN_EXTERNAL_STORAGE, "timer", LED_STATUS_FAST_BLINK);
		syno_set_led(SZ_LED_ORANGE_EXTERNAL_STORAGE, "none", LED_STATUS_OFF);
	} else {
		syno_set_led(SZ_LED_GREEN_EXTERNAL_STORAGE, "none", LED_STATUS_OFF);
		syno_set_led(SZ_LED_ORANGE_EXTERNAL_STORAGE, "none", LED_STATUS_ON);
	}
}

static void set_led_external_storage_blink(void)
{
	if (mask_all) {
		syno_set_led(SZ_LED_GREEN_EXTERNAL_STORAGE, "none", LED_STATUS_OFF);
		syno_set_led(SZ_LED_ORANGE_EXTERNAL_STORAGE, "none", LED_STATUS_OFF);
	} else if(EXTERNAL_STORAGE_MOUNT_STATUS_ALL_MOUNTED == mount_status) {
		syno_set_led(SZ_LED_GREEN_EXTERNAL_STORAGE, "none", LED_STATUS_OFF);
		syno_set_led(SZ_LED_ORANGE_EXTERNAL_STORAGE, "timer", LED_STATUS_FAST_BLINK);
	}
}

int syno_wifionoff_read_command(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", gWifiSwitch);
	return 0;
}

int syno_led_manufactory_write(struct file *filp,const char *buf,size_t count,loff_t *offp)
{
	int val = -1;

	sscanf(buf, MAGIC_NUM",%d", &val);

	if (-1 == val) {
		goto END;
	}

	switch(val) {
		case 0:
			break;
		case 1:
			syno_set_led(SZ_LED_DUAL_WAN_CTL, "none", LED_STATUS_OFF);
			if (syno_set_ethernet_led) {
				syno_set_ethernet_led(0, 0, 2); // turn on green led
				syno_set_ethernet_led(0, 1, 0); // turn off orange led
				syno_set_ethernet_led(1, 1, 0); // turn off orange led
			}
			break;
		case 2:
			syno_set_led(SZ_LED_DUAL_WAN_CTL, "none", LED_STATUS_OFF);
			if (syno_set_ethernet_led) {
				syno_set_ethernet_led(0, 1, 2); // tunr on lan orange led
				syno_set_ethernet_led(0, 0, 0); // turn off lan green led
			}
			break;
	}
END:
	return count;
}

int syno_cpu_temperature_read_command(struct seq_file *m, void *v)
{
	struct _SynoCpuTemp pCpuTemp;
	long temperature = 0;
	GetCpuTemperature(&pCpuTemp);
	temperature = (pCpuTemp.cpu_temp[0] + pCpuTemp.cpu_temp[1]) / 2;

	seq_printf(m, "%ld\n", temperature);
	return 0;
}


static irqreturn_t rf_switch_irq(int irq, void *dev_id)
{
	struct syno_gpio_data *data = (struct syno_gpio_data *)dev_id;
	int status = 0;
	GPIO_PIN pin = {
		.pin = (int)data->gpio,
		.value = 0
	};

	GetGpioPin(&pin);
	status = pin.value;
	if (data->is_active_low ^ status) {
		if (1 == gWifiSched) {
			gWifiSwitch = !gWifiSwitch;
		} else {
			printk("always on in wifi schedule off mode\n");
			gWifiSwitch = 1;
		}
		printk(KERN_INFO "synobios: wifi %s status\n", gWifiSwitch ? "on" : "off");
		synobios_record_event(NULL, SYNO_EVENT_WIFI_ON_OFF);
		data->old_status = status;
	}

	return IRQ_HANDLED;
}
static irqreturn_t usb_eject_irq(int irq, void *dev_id)
{
	struct syno_gpio_data *data = (struct syno_gpio_data *)dev_id;
	int status = 0;
	GPIO_PIN pin = {
		.pin = (int)data->gpio,
		.value = 0
	};

	GetGpioPin(&pin);
	status = pin.value;
	if (data->is_active_low ^ status) {
		printk(KERN_INFO "synobios: eject button pressed\n");
		synobios_record_event(NULL, SYNO_EVENT_USBSTATION_EJECT);
		data->old_status = status;
	}
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

	GetGpioPin(&pin);
	status = pin.value;
	if (data->is_active_low ^ status) {
		printk(KERN_INFO "synobios: wifi/wps button pressed\n");
		synobios_record_event(NULL,	SYNO_EVENT_WIFIWPS);
		data->old_status = status;
	}
	return IRQ_HANDLED;
}

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
			printk(KERN_INFO "synobios: send reset event\n");
			if (gMeshIsRE) {
				break;
			}
			synobios_event_reset_method(SYNO_RESET_PASSWORD_ONLY);
			break;
		case BUTTON_RELEASE:
			break;
		default:
			break;
	}
}

#if defined(CONFIG_SYNO_AUTO_INSTALL_ABILITY)
int router_exdisplay_handler(struct _SynoMsgPkt *pMsgPkt);

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

static struct syno_gpio_data rf_switch = {
	.gpio = 22,
	.irq_flags = IRQF_TRIGGER_FALLING,
	.is_active_low = 1,
	.handler = rf_switch_irq,
	.old_status = 1,
	.name = "RF switch"
};

static struct syno_gpio_data usb_eject = {
	.gpio = 23,
	.irq_flags = IRQF_TRIGGER_FALLING,
	.is_active_low = 1,
	.handler = usb_eject_irq,
	.old_status = 1,
	.name = "USB eject"
};

static struct syno_gpio_data wps_button = {
	.gpio = 54,
	.irq_flags = IRQF_TRIGGER_FALLING,
	.is_active_low = 1,
	.handler = wps_button_irq,
	.old_status = 1,
	.name = "WPS"
};

static struct syno_gpio_data reset_button = {
	.gpio = 65,
	.irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
	.is_active_low = 1,
	.handler = reset_button_irq,
	.old_status = 1,
	.name = "SYS reset"
};


static int Uninitialize(void)
{
	return 0;
}

int GetModel(void)
{
	return MODEL_RT2600ac;
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

typedef enum {
	PRESSED,
	RELEASED,
} HW_TRIGGER;

static void phy_led_off(void)
{
	if (syno_set_ethernet_led) {
		syno_set_ethernet_led(0, 0, 0); // disable LAN green LED
		syno_set_ethernet_led(1, 0, 0); // disable WAN green LED
		syno_set_ethernet_led(0, 1, 0); // disable LAN orange LED
		syno_set_ethernet_led(1, 1, 0); // disable WAN orange LED
	}
}

static void phy_led_on(void)
{
	if (syno_set_ethernet_led) {
		syno_set_ethernet_led(0, 0, 3); // enable LAN green LED
		syno_set_ethernet_led(1, 0, 3); // enable WAN green LED
		syno_set_ethernet_led(0, 1, 3); // enable LAN orange LED
		syno_set_ethernet_led(1, 1, 3); // enable WAN orange LED
	}
}

static void SynoFindMeLedThread(void)
{
	while (!kthread_should_stop()) {
		syno_set_led(SZ_LED_GREEN_STATUS, "none", LED_STATUS_OFF);
		syno_set_led(SZ_LED_ORANGE_STATUS, "none", LED_STATUS_ON);
		syno_set_led(SZ_LED_GREEN_STATUS_CTL, "none", LED_STATUS_ON);
		msleep(200);
		syno_set_led(SZ_LED_GREEN_STATUS, "none", LED_STATUS_OFF);
		syno_set_led(SZ_LED_ORANGE_STATUS, "none", LED_STATUS_OFF);
		syno_set_led(SZ_LED_GREEN_STATUS_CTL, "none", LED_STATUS_ON);
		msleep(200);
		syno_set_led(SZ_LED_GREEN_STATUS, "none", LED_STATUS_ON);
		syno_set_led(SZ_LED_ORANGE_STATUS, "none", LED_STATUS_OFF);
		syno_set_led(SZ_LED_GREEN_STATUS_CTL, "none", LED_STATUS_OFF);
		msleep(200);
		syno_set_led(SZ_LED_GREEN_STATUS, "none", LED_STATUS_OFF);
		syno_set_led(SZ_LED_ORANGE_STATUS, "none", LED_STATUS_OFF);
		syno_set_led(SZ_LED_GREEN_STATUS_CTL, "none", LED_STATUS_ON);
		msleep(200);
	}

	memset(&MsgPkt, 0, sizeof(struct _SynoMsgPkt));
	if (gMeshIsRE && !gMeshHeartBeatAlive) {
		MsgPkt.usNum = SYNO_LED_DISCONNECT;
	} else {
		MsgPkt.usNum = SYNO_SYS_RUN;
	}
	MsgPkt.usLen = 0;
	exdisplay_work.MsgPkt = &MsgPkt;
	INIT_WORK(&(exdisplay_work.work), update_exdisplay_work);
	schedule_work(&(exdisplay_work.work));
}

int router_exdisplay_handler(struct _SynoMsgPkt *pMsgPkt)
{
	int ret = -1, sub_msg_num = 0;
	static struct task_struct *pFindMeThread = NULL;
	struct task_struct *pFindMeTempThread = NULL;
	if (!pMsgPkt) {
		goto END;
	}

	switch(pMsgPkt->usNum) {
		case SYNO_LED_USBSTATION_MEMTEST_LED:
			break;
		case SYNO_LED_USBSTATION_DISK_GREEN:
			break;
		case SYNO_LED_USBSTATION_DISK_ORANGE:
			break;
		case SYNO_LED_MASK_ALL:
			mask_all = 1;
			if (pFindMeThread || (gMeshIsRE && !gMeshHeartBeatAlive)) {
				goto END;
			}
			syno_set_led(SZ_LED_GREEN_STATUS, "none", LED_STATUS_ON);
			syno_set_led(SZ_LED_GREEN_STATUS_CTL, "none", LED_STATUS_ON);
			syno_set_led(SZ_LED_ORANGE_STATUS, "none", LED_STATUS_OFF);
			set_led_external_storage_persist();
			phy_led_off();
			break;
		case SYNO_LED_NOMASK_ALL:
			mask_all = 0;
			if (pFindMeThread || (gMeshIsRE && !gMeshHeartBeatAlive)) {
				goto END;
			}
			syno_set_led(SZ_LED_GREEN_STATUS, "none", LED_STATUS_ON);
			syno_set_led(SZ_LED_GREEN_STATUS_CTL, "none", LED_STATUS_OFF);
			syno_set_led(SZ_LED_ORANGE_STATUS, "none", LED_STATUS_OFF);
			set_led_external_storage_persist();
			phy_led_on();
			break;
		case SYNO_LED_READY_TO_SETUP:
			if (pFindMeThread) {
				goto END;
			}
			syno_set_led(SZ_LED_GREEN_STATUS, "none", LED_STATUS_BLINK);
			syno_set_led(SZ_LED_GREEN_STATUS_CTL, "none", LED_STATUS_BLINK);
			syno_set_led(SZ_LED_ORANGE_STATUS, "none", LED_STATUS_OFF);
			break;
		case SYNO_LED_CONNECT:
		case SYNO_SYS_RUN:
			if (pFindMeThread || (gMeshIsRE && !gMeshHeartBeatAlive)) {
				goto END;
			}
			syno_set_led(SZ_LED_GREEN_STATUS, "none", LED_STATUS_ON);
			if (mask_all) {
				syno_set_led(SZ_LED_GREEN_STATUS_CTL, "none", LED_STATUS_ON);
			} else {
				syno_set_led(SZ_LED_GREEN_STATUS_CTL, "none", LED_STATUS_OFF);
				phy_led_on();
			}
			syno_set_led(SZ_LED_ORANGE_STATUS, "none", LED_STATUS_OFF);
			break;
		case SYNO_SYS_SHUTDOWN:
			break;
		case SYNO_SYS_NO_SYSTEM:
			syno_set_led(SZ_LED_GREEN_STATUS, "none", LED_STATUS_OFF);
			syno_set_led(SZ_LED_GREEN_STATUS_CTL, "none", LED_STATUS_ON);
			syno_set_led(SZ_LED_ORANGE_STATUS, "none", LED_STATUS_BLINK);
			break;
		case SYNO_SYS_WAIT_RESET:
			syno_set_led(SZ_LED_GREEN_STATUS, "none", LED_STATUS_OFF);
			syno_set_led(SZ_LED_GREEN_STATUS_CTL, "none", LED_STATUS_ON);
			syno_set_led(SZ_LED_ORANGE_STATUS, "none", LED_STATUS_FAST_BLINK);
			syno_set_led(SZ_LED_GREEN_EXTERNAL_STORAGE, "none", LED_STATUS_OFF);
			syno_set_led(SZ_LED_ORANGE_EXTERNAL_STORAGE, "none", LED_STATUS_OFF);
			phy_led_off();
			if (syno_set_wifi_led) {
				syno_set_wifi_led(0, 0);
				syno_set_wifi_led(1, 0);
			}
			break;
		case SYNO_SYS_FACTORY_DEFAULT:
			break;
		case SYNO_BEEP_ON:
			/* No buzzer. Dont care. */
			break;
		case SYNO_LED_USB_VOL_MOUNT:
			if (pMsgPkt->usLen > 0) {
				if(0 > kstrtoint(pMsgPkt->szMsg, 10, &sub_msg_num)){
					goto END;
				}
				mount_status = (EXTERNAL_STORAGE_MOUNT_STATUS) sub_msg_num;
			}
			set_led_external_storage_persist();
			break;
		case SYNO_LED_USB_VOL_UMOUNT_BEG:
			break;
		case SYNO_LED_USB_VOL_UMOUNT_END:
			break;
		case SYNO_LED_USB_VOL_NOT_MOUNT:
			set_led_external_storage_persist();
			break;
		case SYNO_LED_USB_SVCCTL_BEG:
		case SYNO_LED_USB_SVCCTL_END:
			/* we don't respond to service/pkg controls */
			break;
		case SYNO_LED_USB_EJECT_BEG:
			set_led_external_storage_blink();
			mount_status = EXTERNAL_STORAGE_MOUNT_STATUS_FAIL;
			break;
		case SYNO_LED_USB_EJECT_END:
			set_led_external_storage_persist();
			break;
		case SYNO_LED_HDD_AB:
		case SYNO_LED_HDD_GS:
			/* No harddisk. Dont care. */
			break;
		case SYNO_LED_NETWORK_SETTING:
			if (gMeshIsRE && !gMeshHeartBeatAlive) {
				goto END;
			}
			if (0 == pMsgPkt->usLen || 0 != strncmp(pMsgPkt->szMsg, "findme", pMsgPkt->usLen)) {
				goto END;
			}

			if (NULL == pFindMeThread) {
				pFindMeThread = kthread_run(SynoFindMeLedThread, NULL, "findme_led");
			}
			break;
		case SYNO_LED_NETWORK_SETTING_END:
			if (gMeshIsRE && !gMeshHeartBeatAlive) {
				goto END;
			}
			if (0 == pMsgPkt->usLen || 0 != strncmp(pMsgPkt->szMsg, "findme", pMsgPkt->usLen)) {
				goto END;
			}

			if (pFindMeThread) {
				pFindMeTempThread = pFindMeThread;
				pFindMeThread = NULL;
				kthread_stop(pFindMeTempThread);
				// set event-end led status in kthread
			}
			break;
		case SYNO_MESH_BACKHAUL_IFACE:
		case SYNO_MESH_SIGNAL_QUALITY:
			break;
		case SYNO_LED_DISCONNECT:
			if (pFindMeThread) {
				goto END;
			}
			syno_set_led(SZ_LED_GREEN_STATUS, "none", LED_STATUS_OFF);
			syno_set_led(SZ_LED_GREEN_STATUS_CTL, "none", LED_STATUS_ON);
			syno_set_led(SZ_LED_ORANGE_STATUS, "none", LED_STATUS_SLOW_BLINK);
			break;
		default:
			printk("Unhandled msg num %04lx\n", pMsgPkt->usNum);
			break;
	}
	ret = 0;
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
#if defined(CONFIG_SYNO_AUTO_INSTALL_ABILITY)
	char buf[32];

	if (0 == syno_get_uboot_env_variable("syno_wifionoff", buf, sizeof(buf))) {
		gWifiSwitch = ('0' == buf[0]) ? 0 : 1;
	} else {
		gWifiSwitch = 1;
	}
#endif /* CONFIG_SYNO_AUTO_INSTALL_ABILITY */

	ops->exdisplay_handler = router_exdisplay_handler;
	init_event_handler();

	start_syno_wifi_proc();
	start_syno_mesh_proc();
	start_syno_proc();
	start_syno_button_proc();
	syno_irq_register(&rf_switch);
	syno_irq_register(&usb_eject);
	syno_irq_register(&wps_button);
	resetButtonHelper(true);

	return 0;
}

int model_addon_cleanup(struct synobios_ops *ops)
{
	funcEXTDISPLAY = NULL;
	funcSynoUSBIOStart = NULL;
	funcSYNOMaintanceUSBLED = NULL;
	ops->exdisplay_handler = NULL;
	deinit_event_handler();

	remove_syno_button_proc();
	remove_syno_mesh_proc();
	remove_syno_wifi_proc();
	remove_syno_proc();
	syno_irq_unregister(&rf_switch);
	syno_irq_unregister(&usb_eject);
	syno_irq_unregister(&wps_button);
	resetButtonHelper(false);


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
	module_t type_2600acv10 = MODULE_T_RT2600acv10;
	module_t *pType = NULL;

	switch (model) {
		case MODEL_RT2600ac:
#ifdef MY_DEF_HERE
			if (!strncmp(gszSynoHWVersion, HW_RT2600ac, strlen(HW_RT2600ac))) {
				pType = &type_2600acv10;
			} else {
				WARN_ON("Never happened!!..");
				pType = &type_2600acv10;
			}
#else
			pType = &type_2600acv10;
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
	.check_microp_id	 = NULL,
	.set_microp_id		 = NULL,
	.get_cpu_info		 = GetCPUInfo,
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

