/*
 * Copyright (C) 2005-2007 Jan Kiszka <jan.kiszka@web.de>.
 * Copyright (C) 2005 Joerg Langenberg <joerg.langenberg@gmx.net>.
 *
 * Xenomai is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Xenomai is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Xenomai; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _RTDM_INTERNAL_H
#define _RTDM_INTERNAL_H

#include <linux/list.h>
#include <linux/sem.h>
#include <cobalt/kernel/ppd.h>
#include <rtdm/driver.h>

#define RTDM_FD_MAX			CONFIG_XENO_OPT_RTDM_FILDES

#define DEF_DEVNAME_HASHTAB_SIZE	256	/* entries in name hash table */
#define DEF_PROTO_HASHTAB_SIZE		256	/* entries in protocol hash table */

struct rtdm_fildes {
	struct rtdm_dev_context *context;
};

struct rtdm_process {
#ifdef CONFIG_XENO_OPT_VFILE
	char name[32];
	pid_t pid;
#endif /* CONFIG_XENO_OPT_VFILE */
};

DECLARE_EXTERN_XNLOCK(rt_fildes_lock);
DECLARE_EXTERN_XNLOCK(rt_dev_lock);

extern int __rtdm_muxid;
extern struct rtdm_fildes fildes_table[];
extern int open_fildes;
extern struct semaphore nrt_dev_lock;
extern unsigned int devname_hashtab_size;
extern unsigned int protocol_hashtab_size;
extern struct list_head *rtdm_named_devices;
extern struct list_head *rtdm_protocol_devices;
extern struct xnpersonality rtdm_personality;

extern int rtdm_initialised;

void cleanup_process_files(struct rtdm_process *owner);
int rtdm_no_support(void);
struct rtdm_device *get_named_device(const char *name);
struct rtdm_device *get_protocol_device(int protocol_family, int socket_type);

static inline void rtdm_dereference_device(struct rtdm_device *device)
{
	atomic_dec(&device->reserved.refcount);
}

int __init rtdm_dev_init(void);
void rtdm_dev_cleanup(void);

#ifdef CONFIG_XENO_OPT_VFILE
int rtdm_proc_init(void);
void rtdm_proc_cleanup(void);
int rtdm_proc_register_device(struct rtdm_device *device);
void rtdm_proc_unregister_device(struct rtdm_device *device);
#else
static inline int rtdm_proc_init(void) { return 0; }
static void inline rtdm_proc_cleanup(void) { }
static inline int
rtdm_proc_register_device(struct rtdm_device *device) { return 0; }
static inline void
rtdm_proc_unregister_device(struct rtdm_device *device) { }
#endif

void rtdm_apc_handler(void *cookie);

int rtdm_init(void);

void rtdm_cleanup(void);

#endif /* _RTDM_INTERNAL_H */
