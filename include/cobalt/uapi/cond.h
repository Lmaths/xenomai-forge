/*
 * Written by Gilles Chanteperdrix <gilles.chanteperdrix@xenomai.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */
#ifndef _COBALT_UAPI_COND_H
#define _COBALT_UAPI_COND_H

#include <cobalt/uapi/mutex.h>

#define COBALT_COND_MAGIC 0x86860505

union cobalt_cond_union {
	pthread_cond_t native_cond;
	struct __shadow_cond {
		unsigned int magic;
		struct cobalt_condattr attr;
		xnhandle_t handle;
		union {
			unsigned int pending_signals_offset;
			unsigned long *pending_signals;
		};
		union {
			unsigned int mutex_datp_offset;
			struct mutex_dat *mutex_datp;
		};
	} shadow_cond;
};

#endif /* !_COBALT_UAPI_COND_H */
