#ifndef _LINUX_KALLSYMS_H
#define _LINUX_KALLSYMS_H

#include <lwk/kallsyms.h>

#define KSYM_SYMBOL_LEN (sizeof("%s+%#lx/%#lx [%s]") + (KSYM_NAME_LEN - 1) + \
                         2*(BITS_PER_LONG*3/10) + 1)

/* Look up a kernel symbol and return it in a text buffer. */
static inline int
sprint_symbol(char *buffer, unsigned long address)
{
	const char *name;
	unsigned long offset, size;
	char namebuf[KSYM_NAME_LEN];

	name = kallsyms_lookup(address, &size, &offset,namebuf);
	if (!name)
		return sprintf(buffer, "0x%lx", address);

	return sprintf(buffer, "%s+%#lx/%#lx", name, offset, size);
}

static inline void
print_symbol(const char *fmt, unsigned long address)
{
	char buffer[KSYM_SYMBOL_LEN];
	sprint_symbol(buffer, address);
	printk(fmt, buffer);
}

/*
 * Pretty-print a function pointer.
 *
 * ia64 and ppc64 function pointers are really function descriptors,
 * which contain a pointer the real address.
 */
static inline void
print_fn_descriptor_symbol(const char *fmt, void *address)
{
	#if defined(CONFIG_IA64) || defined(CONFIG_PPC64)
        address = *(void **)address;
	#endif
	print_symbol(fmt, (unsigned long)address);
}

#endif