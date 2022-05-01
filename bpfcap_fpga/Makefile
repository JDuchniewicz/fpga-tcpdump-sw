KDIR ?= /home/jduchniewicz/Projects/Linux/linux-socfpga
CROSS_COMPILE = arm-none-linux-gnueabihf-

default:
	$(MAKE) -C $(KDIR) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=arm M=$(CURDIR)

clean:
	$(MAKE) -C $(KDIR) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=arm M=$(CURDIR) clean

help:
	$(MAKE) -C $(KDIR) CROSS_COMPILE=$(CROSS_COMPILE) ARCH=arm M=$(CURDIR) help
