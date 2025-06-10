ifeq ($(SYNO_PLATFORM),BROADCOM_NORTHSTARPLUS)
obj-m += ds215router-synobios.o
endif

common-obj += \
		../synobios.o \
		../i2c/i2c-linux.o \
		../rtc/localtime.o \
		../rtc/alarmtime.o \
		../rtc/rtc-ricoh-lib.o \
		../rtc/rtc-seiko-lib.o \
		../rtc/rtc-linux-ricoh.o \
		../rtc/rtc-linux-seiko.o \
		../mapping.o

#fan-pwm-obj += fan-pwm.o
#fan-resister-obj += fan-resister.o

ds215router-synobios-objs = ds215router.o $(common-obj)
