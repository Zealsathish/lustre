/*
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see
 * http://www.gnu.org/licenses/gpl-2.0.html
 *
 * GPL HEADER END
 */
/*
 * Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright (c) 2011, 2014, Intel Corporation.
 */
#ifndef _LNET_NIDSTRINGS_H
#define _LNET_NIDSTRINGS_H
#include <lnet/types.h>

struct list_head;

#define LNET_NIDSTR_COUNT  1024    /* # of nidstrings */
#define LNET_NIDSTR_SIZE   32      /* size of each one (see below for usage) */

/* support decl needed both by kernel and liblustre */
int libcfs_isknown_lnd(__u32 lnd);
char *libcfs_lnd2modname(__u32 lnd);
char *libcfs_lnd2str(__u32 lnd);
char *libcfs_lnd2str_r(__u32 lnd, char *buf, size_t buf_size);
int libcfs_str2lnd(const char *str);
char *libcfs_net2str(__u32 net);
char *libcfs_net2str_r(__u32 net, char *buf, size_t buf_size);
char *libcfs_nid2str(lnet_nid_t nid);
char *libcfs_nid2str_r(lnet_nid_t nid, char *buf, size_t buf_size);
__u32 libcfs_str2net(const char *str);
lnet_nid_t libcfs_str2nid(const char *str);
int libcfs_str2anynid(lnet_nid_t *nid, const char *str);
char *libcfs_id2str(lnet_process_id_t id);
void cfs_free_nidlist(struct list_head *list);
int cfs_parse_nidlist(char *str, int len, struct list_head *list);
int cfs_print_nidlist(char *buffer, int count, struct list_head *list);
int cfs_match_nid(lnet_nid_t nid, struct list_head *list);
bool cfs_nidrange_is_contiguous(struct list_head *nidlist);
void cfs_nidrange_find_min_max(struct list_head *nidlist, char *min_nid,
			       char *max_nid, size_t nidstr_length);
void libcfs_init_nidstrings(void);

#endif /* _LNET_NIDSTRINGS_H */
