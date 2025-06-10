// Copyright (c) 2000-2016 Synology Inc. All rights reserved.
#ifndef _DAKOTA_COMMON_H
#define _DAKOTA_COMMON_H
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/syno.h>
#include <linux/module.h>
#include "synobios.h"
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/leds.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#if defined(CONFIG_SYNO_AUTO_INSTALL_ABILITY)
#include <linux/workqueue.h>
#endif /* CONFIG_SYNO_AUTO_INSTALL_ABILITY */
#if defined(CONFIG_SYNO_LEDS_EXTENSION)
#include <linux/syno-led-extension.h>
#endif
#define SZ_WIFIONOFF_NAME "syno_wifionoff"
#define SZ_CPU_TEMPERATURE_NAME "syno_cpu_temperature"
#define SZ_LED_MANUFACTORY_NAME "syno_led_manufactory"
#define SZ_WIFI_SCHED_NAME "syno_wifi_sched_on_off"
#define SZ_LED_DEBUG_NAME "syno_led_debug"
#define SZ_BUTTON_RESET_TIMER_NAME "syno_button_reset_timer"
#define SZ_BUTTON_WIFIONOFF_TIMER_NAME "syno_button_wifionoff_timer"
#define SZ_BUTTON_WIFIONOFF_READY_NAME "syno_button_wifionoff_ready"
#define MAGIC_NUM "1856E96" //25521814
#define LED_BLINK_DELAY_TIME_ON 840
#define LED_BLINK_DELAY_TIME_OFF 360
#define LED_SLOW_BLINK_DELAY_TIME 1000
#define LED_FAST_BLINK_DELAY_TIME 100

struct sd_softc {
	int	countEvents;
	int	idxPtr;
	SYNOBIOSEVENT	rgEvents[SYNOBIOS_NEVENTS];
	wait_queue_head_t wq_poll;
};

enum {
	BUTTON_PUSH = 0,
	BUTTON_RELEASE = 1,
};

struct syno_gpio_data {
	unsigned int irq;
	unsigned int gpio;
	unsigned long irq_flags;
	int is_active_low;
	irq_handler_t handler;
	int old_status;
	char *name;
};

struct msm_gpio_dev {
	struct gpio_chip gpio_chip;
#if 0
	DECLARE_BITMAP(enabled_irqs, NR_MSM_GPIOS);
	DECLARE_BITMAP(wake_irqs, NR_MSM_GPIOS);
	DECLARE_BITMAP(dual_edge_irqs, NR_MSM_GPIOS);
#endif
	struct irq_domain *domain;
};

extern struct gpio_chip *syno_msm_gpio_chip;
int SetGpioPin(GPIO_PIN *pPin);
int GetGpioPin(GPIO_PIN *pPin);
extern int syno_get_uboot_env_variable(const char *name, char *value, size_t len);

int router_exdisplay_handler(struct _SynoMsgPkt *pMsgPkt);
int synobios_record_event(struct sd_softc *sc, u_int event_type);
extern void syno_force_auto_install(void);
int synobios_event_reset_method(const int method);
void send_sdcard_change_event(void);
int syno_irq_register(struct syno_gpio_data *data);
int syno_irq_unregister(struct syno_gpio_data *data);
int start_syno_wifi_proc(void);
int start_syno_proc(void);
int start_syno_button_proc(void);
int remove_syno_wifi_proc(void);
int remove_syno_proc(void);
int remove_syno_button_proc(void);
int syno_wifionoff_read_command(struct seq_file *m, void *v);
int syno_wifi_sched_write_command(struct file *filp,const char *buf,size_t count,loff_t *offp);
int syno_cpu_temperature_read_command(char *buffer, char **buffer_location, off_t offset,
				int buffer_length, int *zero, void *ptr);
int syno_led_manufactory_write(struct file *file, const char *buffer,
				unsigned long count, void *data);
extern int syno_cpu_temperature(struct _SynoCpuTemp *pCpuTemp);
int GetCpuTemperature(struct _SynoCpuTemp *pCpuTemp);

#endif /* _DAKOTA_COMMON_H */
