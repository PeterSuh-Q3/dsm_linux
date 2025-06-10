// Copyright (c) 2000-2016 Synology Inc. All rights reserved.

#include <linux/kernel.h> /* printk() */
#include <linux/synobios.h>
#include "dakota_common.h"

int gWifiSched = 1;
int gLedDbg = 0;
int gButtonResetTimer = 4000;
int gWifiOnOffReady = 0;
int gButtonWifiOnOffTimer = 3000;

int syno_irq_register(struct syno_gpio_data *data)
{
	int ret = -EINVAL;
	if (!data) {
		goto END;
	}
	data->irq = syno_qualcomm_to_irq(data->gpio);
	ret = request_irq(data->irq, data->handler, data->irq_flags, data->name, data);
	if (ret) {
		printk("Unable to get IRQ %d (%d)\n", data->irq, ret);
		goto END;
	}

	ret = 0;
END:
	return ret;
}

int syno_irq_unregister(struct syno_gpio_data *data)
{
	int ret = -EINVAL;
	if (!data) {
		goto END;
	}
	free_irq(data->irq, data);
	ret = 0;
END:
	return ret;
}

int SetGpioPin(GPIO_PIN *pPin)
{
	int iRet = 0;
	syno_msm_gpio_chip->set(syno_msm_gpio_chip, pPin->pin - syno_msm_gpio_chip->base, pPin->value);
	return iRet;
}

int GetGpioPin(GPIO_PIN *pPin)
{
	int iRet = 0;
	pPin->value = syno_msm_gpio_chip->get(syno_msm_gpio_chip - syno_msm_gpio_chip->base, pPin->pin);
	return iRet;
}

static int synobios_read_proc_wifi_open(struct inode *inode, struct file *file)
{
	return single_open(file, syno_wifionoff_read_command, NULL);
}

static const struct file_operations synobios_read_proc_wifi_fops = {
	.owner		= THIS_MODULE,
	.open		= synobios_read_proc_wifi_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int syno_wifi_sched_read_command(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", gWifiSched);
	return 0;
}

int syno_wifi_sched_write_command(struct file *filp,const char *buf,size_t count,loff_t *offp)
{

	sscanf(buf, "%d", &gWifiSched);

	return count;
}
static int synobios_read_proc_wifi_sched_open(struct inode *inode, struct file *file)
{
	return single_open(file, syno_wifi_sched_read_command, NULL);
}


static const struct file_operations synobios_read_proc_wifi_sched_fops = {
	.owner		= THIS_MODULE,
	.open		= synobios_read_proc_wifi_sched_open,
	.write      = syno_wifi_sched_write_command,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
int start_syno_wifi_proc(void)
{
	static struct proc_dir_entry *syno_wifionoff;
	static struct proc_dir_entry *syno_wifisched;
   syno_wifionoff =  proc_create_data(SZ_WIFIONOFF_NAME, 0400, proc_synobios_root,
   					&synobios_read_proc_wifi_fops, NULL);
	syno_wifisched =  proc_create_data(SZ_WIFI_SCHED_NAME, 0600, proc_synobios_root,
						&synobios_read_proc_wifi_sched_fops, NULL);
	return 0;
}

int remove_syno_wifi_proc(void)
{
	remove_proc_entry(SZ_WIFI_SCHED_NAME, proc_synobios_root);
	remove_proc_entry(SZ_WIFIONOFF_NAME, proc_synobios_root);
	return 0;
}


void resetButtonHelper(bool);
int syno_button_reset_timer_write_command(struct file *filp,const char *buf,size_t count,loff_t *offp)
{
	int val = -1;

	sscanf(buf, MAGIC_NUM",%d", &val);

	if (-1 == val) {
		resetButtonHelper(false);
	} else if (0 > val) {
		goto END;
	}

	if (-1 == gButtonResetTimer && 0 <= val) {
		resetButtonHelper(true);

	}

	gButtonResetTimer = val;

END:
	return count;
}

int syno_button_reset_timer_read_command(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", gButtonResetTimer);
	return 0;
}

static int synobios_read_proc_button_reset_timer_open(struct inode *inode, struct file *file)
{
	return single_open(file, syno_button_reset_timer_read_command, NULL);
}

static const struct file_operations synobios_read_proc_button_reset_timer_fops = {
	.owner		= THIS_MODULE,
	.open		= synobios_read_proc_button_reset_timer_open,
	.write		= syno_button_reset_timer_write_command,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int syno_button_wifionoff_timer_write_command(struct file *filp,const char *buf,size_t count,loff_t *offp)
{
	int val = -1;

	sscanf(buf, MAGIC_NUM",%d", &val);

	if (0 > val) {
		goto END;
	}

	gButtonWifiOnOffTimer = val;
END:
	return count;
}

int syno_button_wifionoff_timer_read_command(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", gButtonWifiOnOffTimer);
	return 0;
}

static int synobios_read_proc_button_wifionoff_timer_open(struct inode *inode, struct file *file)
{
	return single_open(file, syno_button_wifionoff_timer_read_command, NULL);
}

static const struct file_operations synobios_read_proc_button_wifionoff_timer_fops = {
	.owner		= THIS_MODULE,
	.open		= synobios_read_proc_button_wifionoff_timer_open,
	.write		= syno_button_wifionoff_timer_write_command,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int syno_button_wifionoff_ready_write_command(struct file *filp,const char *buf,size_t count,loff_t *offp)
{
	int val = -1;

	sscanf(buf, MAGIC_NUM",%d", &val);

	if (0 > val) {
		goto END;
	}

	gWifiOnOffReady = val;
END:
	return count;
}

int syno_button_wifionoff_ready_read_command(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", gWifiOnOffReady);
	return 0;
}

static int synobios_read_proc_button_wifionoff_ready_open(struct inode *inode, struct file *file)
{
	return single_open(file, syno_button_wifionoff_ready_read_command, NULL);
}

static const struct file_operations synobios_read_proc_button_wifionoff_ready_fops = {
	.owner		= THIS_MODULE,
	.open		= synobios_read_proc_button_wifionoff_ready_open,
	.write		= syno_button_wifionoff_ready_write_command,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int start_syno_button_proc(void)
{
	static struct proc_dir_entry *syno_button_reset_timer;
	static struct proc_dir_entry *syno_button_wifionoff_timer;
	static struct proc_dir_entry *syno_button_wifionoff_ready;
	syno_button_reset_timer = proc_create_data(SZ_BUTTON_RESET_TIMER_NAME, 0600, proc_synobios_root,
							&synobios_read_proc_button_reset_timer_fops, NULL);
	syno_button_wifionoff_timer = proc_create_data(SZ_BUTTON_WIFIONOFF_TIMER_NAME, 0600, proc_synobios_root,
							&synobios_read_proc_button_wifionoff_timer_fops, NULL);
	syno_button_wifionoff_ready = proc_create_data(SZ_BUTTON_WIFIONOFF_READY_NAME, 0600, proc_synobios_root,
							&synobios_read_proc_button_wifionoff_ready_fops, NULL);
	return 0;
}

int remove_syno_button_proc(void)
{
	remove_proc_entry(SZ_BUTTON_WIFIONOFF_READY_NAME, proc_synobios_root);
	remove_proc_entry(SZ_BUTTON_WIFIONOFF_TIMER_NAME, proc_synobios_root);
	remove_proc_entry(SZ_BUTTON_RESET_TIMER_NAME, proc_synobios_root);
	return 0;
}

int syno_led_dbg_read_command(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", gLedDbg);
	return 0;
}

int syno_led_dbg_write_command(struct file *filp,const char *buf,size_t count,loff_t *offp)
{
	sscanf(buf, "%d", &gLedDbg);

	return count;
}

static int synobios_read_proc_led_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, syno_led_dbg_read_command, NULL);
}

static const struct file_operations synobios_read_proc_led_dbg_fops = {
	.owner		= THIS_MODULE,
	.open		= synobios_read_proc_led_dbg_open,
	.write		= syno_led_dbg_write_command,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations synobios_write_proc_manufactory_fops = {
	.owner		= THIS_MODULE,
	.write      = syno_led_manufactory_write,
	.llseek     = noop_llseek,
};

int start_syno_proc(void)
{
	static struct proc_dir_entry *syno_led_manufactory;
	static struct proc_dir_entry *syno_led_dbg;
	syno_led_manufactory =  proc_create_data(SZ_LED_MANUFACTORY_NAME , 0200 , proc_synobios_root,
						&synobios_write_proc_manufactory_fops, NULL);
	syno_led_dbg = proc_create_data(SZ_LED_DEBUG_NAME, 0600, proc_synobios_root,
						&synobios_read_proc_led_dbg_fops, NULL);

	return 0;
}

int remove_syno_proc(void)
{
	remove_proc_entry(SZ_LED_DEBUG_NAME, proc_synobios_root);
	remove_proc_entry(SZ_LED_MANUFACTORY_NAME, proc_synobios_root);
	return 0;
}

int GetCpuTemperature(struct _SynoCpuTemp *pCpuTemp)
{
	int iRet = -1;
	printk("Please implement %s function on synobios", __FUNCTION__);
	return iRet;
}
