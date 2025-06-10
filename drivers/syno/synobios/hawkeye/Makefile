ifeq ($(SYNO_PLATFORM),QUALCOMM_HAWKEYE)
obj-m += rt3000ax-synobios.o
endif

common-obj += \
		../synobios.o \
		../syno_ttyS/syno_ttyS_srm.o \
		./hawkeye_common.o \
		../i2c/i2c-linux.o \
		../mesh/mesh.o \
		../mapping.o

rtc-obj += \
		../rtc/localtime.o \
		../rtc/alarmtime.o \
		../rtc/rtc-ricoh-lib.o \
		../rtc/rtc-seiko-lib.o \
		../rtc/rtc-linux-ricoh.o \
		../rtc/rtc-linux-seiko.o \

#fan-pwm-obj += fan-pwm.o
#fan-resister-obj += fan-resister.o

rt3000ax-synobios-objs = rt3000ax.o $(common-obj)
