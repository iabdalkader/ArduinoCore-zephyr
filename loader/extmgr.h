/*
 * Copyright (C) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __EXTMGR_H__
#define __EXTMGR_H__

int  extmgr_init(void);
void extmgr_deinit(void);

void *extmgr_load(const char *name);
void  extmgr_unload(void *handle);

const void *extmgr_find_sym(void *handle, const char *sym);

#endif /* __EXTMGR_H__ */
