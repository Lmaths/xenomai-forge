/*
 * Copyright (C) 2005 Philippe Gerum <rpm@xenomai.org>.
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
 * along with Xenomai; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#ifndef _COBALT_POWERPC_ASM_FEATURES_H
#define _COBALT_POWERPC_ASM_FEATURES_H

#ifndef __KERNEL__
#error "Pure kernel header included from user-space!"
#endif

struct xnfeatinfo;
static inline void collect_arch_features(struct xnfeatinfo *p) { }

#include <asm/xenomai/uapi/features.h>

#endif /* !_COBALT_POWERPC_ASM_FEATURES_H */
