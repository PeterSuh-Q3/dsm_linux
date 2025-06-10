ifeq ($(SYNO_PLATFORM), QUALCOMM_DAKOTA)
obj-m += mr2200ac-synobios.o
endif

common-obj += \
		../synobios.o \
		../syno_ttyS/syno_ttyS_srm.o \
		./dakota_common.o \
		../i2c/i2c-linux.o \
		../mesh/mesh.o \
		../mapping.o

#fan-pwm-obj += fan-pwm.o
#fan-resister-obj += fan-resister.o

mr2200ac-synobios-objs = mr2200ac.o $(common-obj)
