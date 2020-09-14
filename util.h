/*
 * SMC Tools - Shared Memory Communication Tools
 *
 * Copyright IBM Corp. 2020
 *
 * Author(s): Guvenc Gulce <guvenc@linux.ibm.com>
 *
 * Userspace program for SMC Information display
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 */
#ifndef UTIL_H_
#define UTIL_H_
#include <stdlib.h>
#define SMC_DETAIL_LEVEL_V 1
#define SMC_DETAIL_LEVEL_VV 2
#define SMC_TYPE_STR_MAX 5


#define NEXT_ARG() do { argv++; argc--; } while(0)
#define NEXT_ARG_OK() (argc - 1 > 0)
#define PREV_ARG() do { argv--; argc++; } while(0)

void print_unsup_msg(void);
void print_type_error(void);
char* trim_space(char *str);
int contains(const char *prfx, const char *str);

inline int is_str_empty(char *str)
{
	if (str && str[0] == '\0')
		return 1;
	else
		return 0;
}

#endif /* UTIL_H_ */
