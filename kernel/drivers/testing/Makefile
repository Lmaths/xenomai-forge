
obj-$(CONFIG_XENO_DRIVERS_TIMERBENCH) += xeno_timerbench.o
obj-$(CONFIG_XENO_DRIVERS_SWITCHTEST) += xeno_switchtest.o
obj-$(CONFIG_XENO_DRIVERS_RTDMTEST)   += xeno_rtdmtest.o

xeno_timerbench-y := timerbench.o

xeno_switchtest-y := switchtest.o

xeno_rtdmtest-y := rtdmtest.o

ccflags-y := -Iarch/$(SRCARCH)/xenomai/include -Iinclude/xenomai
