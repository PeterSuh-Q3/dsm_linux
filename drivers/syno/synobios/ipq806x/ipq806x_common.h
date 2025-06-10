// Copyright (c) 2000-2016 Synology Inc. All rights reserved.
#ifndef _IPQ806X_COMMON_H
#define _IPQ806X_COMMON_H
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
#include <linux/msm_thermal.h>
#include <asm/uaccess.h>
#include <linux/gpio/consumer.h>
#if defined(CONFIG_SYNO_AUTO_INSTALL_ABILITY)
#include <linux/workqueue.h>
#endif /* CONFIG_SYNO_AUTO_INSTALL_ABILITY */
#if defined(CONFIG_SYNO_LEDS_EXTENSION)
#include <linux/syno-led-extension.h>
#endif /* CONFIG_SYNO_LEDS_EXTENSION */

#define SZ_WIFIONOFF_NAME "syno_wifionoff"
#define SZ_CPU_TEMPERATURE_NAME "syno_cpu_temperature"
#define SZ_LED_MANUFACTORY_NAME "syno_led_manufactory"
#define SZ_WIFI_SCHED_NAME "syno_wifi_sched_on_off"
#define SZ_BUTTON_RESET_TIMER_NAME "syno_button_reset_timer"
#define MAGIC_NUM "1856E96" //25521814
#define NR_GPIO_IRQS 173
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
	DECLARE_BITMAP(enabled_irqs, NR_GPIO_IRQS);
	DECLARE_BITMAP(wake_irqs, NR_GPIO_IRQS);
	DECLARE_BITMAP(dual_edge_irqs, NR_GPIO_IRQS);
	struct irq_domain *domain;
};

extern struct gpio_chip *syno_msm_gpio_chip;
int SetGpioPin(GPIO_PIN *pPin);
int GetGpioPin(GPIO_PIN *pPin);
#if defined(CONFIG_SYNO_AUTO_INSTALL_ABILITY)
extern void syno_force_auto_install(void);
extern int syno_get_uboot_env_variable(const char *name, char *value, size_t len);
#endif /* CONFIG_SYNO_AUTO_INSTALL_ABILITY */

int synobios_record_event(struct sd_softc *sc, u_int event_type);
void send_sdcard_change_event(void);
void init_event_handler(void);
void deinit_event_handler(void);
int syno_irq_register(struct syno_gpio_data *data);
int syno_irq_unregister(struct syno_gpio_data *data);
int start_syno_wifi_proc(void);
int start_syno_proc(void);
int start_syno_button_proc(void);
int remove_syno_wifi_proc(void);
int remove_syno_proc(void);
int remove_syno_button_proc(void);
int synobios_event_reset_method(const int method);
void synobios_event_sdcard_change(const int event);
int syno_wifionoff_read_command(struct seq_file *m, void *v);
int syno_wifi_sched_write_command(struct file *filp,const char *buf,size_t count,loff_t *offp);
int syno_cpu_temperature_read_command(struct seq_file *m, void *v);
int syno_led_manufactory_write(struct file *filp,const char *buf,size_t count,loff_t *offp);
extern int syno_cpu_temperature(struct _SynoCpuTemp *pCpuTemp);
int GetCpuTemperature(struct _SynoCpuTemp *pCpuTemp);

#endif /* _IPQ806X_COMMON_H */
