
ccflags-y := -Iarch/$(SRCARCH)/xenomai/include -Iinclude/xenomai \
	-Idrivers/xenomai/analogy

obj-$(CONFIG_XENO_DRIVERS_ANALOGY) += xeno_analogy.o testing/ intel/ national_instruments/ sensoray/

xeno_analogy-y := \
	buffer.o \
	command.o \
	device.o \
	driver.o \
	driver_facilities.o \
	instruction.o \
	os_facilities.o \
	subdevice.o \
	transfer.o \
	rtdm_interface.o
