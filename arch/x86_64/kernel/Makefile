extra-y		:= head.o head64.o bootstrap.o vmlwk.lds
EXTRA_FLAGS	:= -traditional
obj-y		:= setup.o e820.o cpuinfo.o resource.o \
		   mpparse.o entry.o show.o syscall.o i387.o cpu.o \
		   lapic.o ioapic.o trampoline.o interrupts.o mpboot.o \
		   time.o sys_arch_prctl.o vsyscall.o xcall.o \
		   task.o sched.o maccess.o tlbflush.o signal.o acpi.o \
		   sys_not_implemented.o
obj-$(CONFIG_KGDB) += kgdb.o

obj-$(CONFIG_PALACIOS_INTERNAL) += palacios/

obj-$(CONFIG_PALACIOS_GDB) += gdb.o

obj-$(CONFIG_X86_MCE) += mcheck/

obj-y += pci/
