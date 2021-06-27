ifneq ($(KERN_DIR),)
KernelDir := $(KERN_DIR)
else
KernelDir := /lib/modules/$(shell uname -r)/build
endif

CFILES = i2cdrv.c

obj-m := i2c_module.o
i2c_module-objs := $(CFILES:.c=.o)

ccflags-y += -std=gnu99 -Wall -Wno-declaration-after-statement


all:
	make -C $(KernelDir) M=$(shell pwd) modules
clean:
	make -C $(KernelDir) M=$(shell pwd) clean
	rm *~
install:
	sudo insmod i2c_module.ko
	sudo chmod 0666 /dev/i2cdrv0
uninstall:
	sudo rmmod i2c_module


