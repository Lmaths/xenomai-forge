/*
 * Copyright (C) 2005 Jan Kiszka <jan.kiszka@web.de>.
 * Copyright (C) 2005 Joerg Langenberg <joerg.langenberg@gmx.net>.
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
#ifndef _COBALT_UAPI_RTDM_SYSCALL_H
#define _COBALT_UAPI_RTDM_SYSCALL_H

#define RTDM_BINDING_MAGIC	0x5254444D

#define sc_rtdm_fdcount		0
#define sc_rtdm_open		1
#define sc_rtdm_socket		2
#define sc_rtdm_close		3
#define sc_rtdm_ioctl		4
#define sc_rtdm_read		5
#define sc_rtdm_write		6
#define sc_rtdm_recvmsg		7
#define sc_rtdm_sendmsg		8

#endif /* !_COBALT_UAPI_RTDM_SYSCALL_H */
