ifeq ($(SYNO_PLATFORM),QUALCOMM_CYPRESS)
obj-m += rt6600ax-synobios.o
endif

common-obj += \
		../synobios.o \
		../syno_ttyS/syno_ttyS_srm.o \
		./cypress_common.o \
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

rt6600ax-synobios-objs = rt6600ax.o $(common-obj)
