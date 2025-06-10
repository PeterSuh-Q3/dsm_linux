// Copyright (c) 2000-2015 Synology Inc. All rights reserved.

#include <linux/cpumask.h>
#include "rtd1619bmango_common.h"
#include "syno_ttyS.h"
#if defined(CONFIG_SYNO_PWM_CONTROL_LED)
#include <linux/kthread.h>
#include <linux/timer.h>
#endif /* CONFIG_SYNO_PWM_CONTROL_LED */

#ifdef CONFIG_SYNO_LEDS_TRIGGER_DISK
#include "../led/led_trigger_disk.h"
#endif /* CONFIG_SYNO_LEDS_TRIGGER_DISK */

#if defined(CONFIG_SYNO_PWM_CONTROL_LED)
extern int SynoRTKPWMSet(const int id, const int enable, const int clkout_div, const int clksrc_div, const int duty_rate);
#endif /* CONFIG_SYNO_PWM_CONTROL_LED */

int GetModel(void)
{
	return MODEL_MANGO;
}

int InitModuleType(struct synobios_ops *ops)
{
	PRODUCT_MODEL model = GetModel();
	module_t type_mango = MODULE_T_MANGO;
	module_t *pType = NULL;

	switch (model) {
	case MODEL_MANGO:
		pType = &type_mango;

#ifdef CONFIG_SYNO_LEDS_TRIGGER_DISK
		SetupLedTrigDiskMap();
#endif /* CONFIG_SYNO_LEDS_TRIGGER_DISK */
		break;
	default:
		break;
	}

	module_type_set(pType);
	return 0;
}


/*
 *  Mango GPIO config table
 *
 *  Pin     In/Out    Function
 *
 *  3       Out       HDD Power Enable 1
 *  68      Out       Fan control voltage
 *  60      Out       USB3 Power Enable 1
 *  61      Out       USB3 Power Enable 2
 *  73       In       HDD Detect 1
 *
 */

/*
 *  Mango other control
 *
 *  LAN LED			Realtek driver (r8169soc_1619.c, rtd-1619b-synology-mango.dts)
 *
 */

static SYNO_GPIO_INFO hdd_detect = {
	.nr_gpio		= 1,
	.gpio_port		= {73},
	.gpio_polarity	= ACTIVE_LOW,
};
static SYNO_GPIO_INFO hdd_enable = {
	.nr_gpio		= 1,
	.gpio_port		= {3},
	.gpio_polarity	= ACTIVE_HIGH,
};

void syno_gpio_init(void)
{
	if (!syno_gpio.hdd_detect) {
		syno_gpio.hdd_detect = &hdd_detect;
	} else {
		check_gpio_consistency(syno_gpio.hdd_detect, &hdd_detect);
	}

	if (!syno_gpio.hdd_enable) {
		syno_gpio.hdd_enable = &hdd_enable;
	} else {
		check_gpio_consistency(syno_gpio.hdd_enable, &hdd_enable);
	}


#if defined(CONFIG_SYNO_AHCI_SOFTWARE_ACITIVITY) || defined(CONFIG_SYNO_AHCI_GPIO_SOFTWARE_PRESENT_BLINK)
	syno_ahci_disk_led_enable_by_port(1, 1);
#endif /* CONFIG_SYNO_AHCI_SOFTWARE_ACITIVITY || CONFIG_SYNO_AHCI_GPIO_SOFTWARE_PRESENT_BLINK */
}

void syno_gpio_cleanup(void)
{
	if (&hdd_detect == syno_gpio.hdd_detect) {
		syno_gpio.hdd_detect = NULL;
	}

	if (&hdd_enable == syno_gpio.hdd_enable) {
		syno_gpio.hdd_enable = NULL;
	}

}

#if defined(CONFIG_SYNO_PWM_CONTROL_LED)
#define BREATH_INTERVAL 20
#define UART2_CMD_LED_HD_OFF                0x37
#define UART2_CMD_LED_HD_GS                 0x38
#define UART2_CMD_LED_HD_GB                 0x39
#define UART2_CMD_LED_HD_AS                 0x3A
#define UART2_CMD_LED_HD_AB                 0x3B
#define UART2_CMD_LED_HD_BREATH             0x3D
#define PWM_GREEN 1
#define PWM_ORANGE 2
#define PWM_ENABLE 1 // always enable, even led off
#define PWM_CLKOUT_DIV 255
#define PWM_FULL_LIGHT 0
#define PWM_FULL_OFF 100
#define PWM_STATIC_RATE 10
#define PWM_BLINK_RATE 15
static struct task_struct *pProcessingThread = NULL;
int SynoBreathingLedThread(void* pObject)
{
	int i = 0;
	int dir = 1;
	int id = (int) pObject;

	while (!kthread_should_stop()) {
		if (1 == dir) {
			i++;
			if (PWM_FULL_OFF == i) {
				dir = 2;
			}
		} else {
			i--;
			if (PWM_FULL_LIGHT == i) {
				dir = 1;
			}

		}
		msleep(BREATH_INTERVAL);
		SynoRTKPWMSet(id, PWM_ENABLE, PWM_CLKOUT_DIV, PWM_STATIC_RATE, i);
	}
	SynoRTKPWMSet(id, PWM_ENABLE, PWM_CLKOUT_DIV, PWM_STATIC_RATE, PWM_FULL_OFF);
	return 0;
}

int status_led_exdisplay_handler(struct _SynoMsgPkt *pMsgPkt)
{
	int ret = -1;
	struct task_struct *pProcessingTempThread = NULL;

	if (NULL == pMsgPkt || SYNO_PURE_MESSAGE != pMsgPkt->usNum) {
		goto END;
	}
	if (UART2_CMD_LED_HD_BREATH != pMsgPkt->szMsg[0] && pProcessingThread) {
		pProcessingTempThread = pProcessingThread;
		pProcessingThread = NULL;
		kthread_stop(pProcessingTempThread);
	}
	switch (pMsgPkt->szMsg[0]) {
		case UART2_CMD_LED_HD_OFF:
			SynoRTKPWMSet(PWM_GREEN, PWM_ENABLE, PWM_CLKOUT_DIV, PWM_STATIC_RATE, PWM_FULL_OFF);
			SynoRTKPWMSet(PWM_ORANGE, PWM_ENABLE, PWM_CLKOUT_DIV, PWM_STATIC_RATE, PWM_FULL_OFF);
			break;
		case UART2_CMD_LED_HD_GS:
			SynoRTKPWMSet(PWM_GREEN, PWM_ENABLE, PWM_CLKOUT_DIV, PWM_STATIC_RATE, PWM_FULL_LIGHT);
			SynoRTKPWMSet(PWM_ORANGE, PWM_ENABLE, PWM_CLKOUT_DIV, PWM_STATIC_RATE, PWM_FULL_OFF);
			break;
		case UART2_CMD_LED_HD_GB:
			SynoRTKPWMSet(PWM_GREEN, PWM_ENABLE, PWM_CLKOUT_DIV, PWM_BLINK_RATE, PWM_FULL_LIGHT);
			SynoRTKPWMSet(PWM_ORANGE, PWM_ENABLE, PWM_CLKOUT_DIV, PWM_STATIC_RATE, PWM_FULL_OFF);
			break;
		case UART2_CMD_LED_HD_AS:
			SynoRTKPWMSet(PWM_GREEN, PWM_ENABLE, PWM_CLKOUT_DIV, PWM_STATIC_RATE, PWM_FULL_OFF);
			SynoRTKPWMSet(PWM_ORANGE, PWM_ENABLE, PWM_CLKOUT_DIV, PWM_STATIC_RATE, PWM_FULL_LIGHT);
			break;
		case UART2_CMD_LED_HD_AB:
			SynoRTKPWMSet(PWM_GREEN, PWM_ENABLE, PWM_CLKOUT_DIV, PWM_STATIC_RATE, PWM_FULL_OFF);
			SynoRTKPWMSet(PWM_ORANGE, PWM_ENABLE, PWM_CLKOUT_DIV, PWM_BLINK_RATE, PWM_FULL_LIGHT);
			break;
		case UART2_CMD_LED_HD_BREATH:
			if (NULL == pProcessingThread) {
				pProcessingThread = kthread_run(SynoBreathingLedThread, (int) PWM_GREEN, "breathing_led");
			}
			break;
		default:
			goto END;
	}
	ret = 0;
END:
	return ret;
}
#endif /* CONFIG_SYNO_PWM_CONTROL_LED */
int model_addon_init(struct synobios_ops *ops)
{


#if defined(CONFIG_SYNO_PWM_CONTROL_LED)
	ops->exdisplay_handler = status_led_exdisplay_handler;
#endif /* CONFIG_SYNO_PWM_CONTROL_LED */
	return 0;
}

int model_addon_cleanup(struct synobios_ops *ops)
{
#if defined(CONFIG_SYNO_PWM_CONTROL_LED)
	ops->exdisplay_handler = NULL;
#endif /* CONFIG_SYNO_PWM_CONTROL_LED */
	return 0;
}
