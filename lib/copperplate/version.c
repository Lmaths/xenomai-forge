/*
 * Copyright (C) 2013 Philippe Gerum <rpm@xenomai.org>.
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

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */
#include <xeno_config.h>
#include "git-stamp.h"

#ifndef GIT_STAMP
#define GIT_STAMP  ""
#endif

#ifdef CONFIG_XENO_COBALT
#define core_suffix "/cobalt v"
#else /* CONFIG_XENO_MERCURY */
#define core_suffix "/mercury v"
#endif

const char *xenomai_version_string = PACKAGE_NAME \
	core_suffix PACKAGE_VERSION " -- " GIT_STAMP;
