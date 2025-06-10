obj-m += mango-synobios.o

common-obj += \
		../common/common.o \
		../i2c/i2c-linux.o \
		../rtc/localtime.o \
		../rtc/alarmtime.o \
		../syno_ttyS/syno_ttyS.o \
		../synobios.o \
		../mapping.o \
		../rtc/rtc-seiko-lib.o \
		../rtc/rtc-linux-seiko.o \
		../led/led_trigger_disk.o \
		rtd1619bmango_common.o

fan-pwm-obj += fan-pwm.o

mango-synobios-objs = mango.o $(fan-pwm-obj) $(common-obj)
