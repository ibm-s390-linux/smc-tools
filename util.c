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
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "util.h"

void print_unsup_msg(void)
{
	printf("Kernel doesn't support this parameter ! \n");
	exit(-1);
}

void print_type_error(void) {
	printf("You entered an invalid type. Possible values are smcd and smcr !\n");
	exit(-1);
}

char* trim_space(char *str)
{
	char *end;

	while (isspace(*str)) {
		str = str + 1;
	}
	/* remove trailing whitespace */
	end = str + strlen((const char*)str) - 1;
	while (end > str && isspace(*end)) {
		end = end - 1;
	}
	*(end+1) = '\0';
	return str;
}

int contains(const char *prfx, const char *str)
{
	if (!*prfx)
		return 1;
	while (*str && *prfx == *str) {
		prfx++;
		str++;
	}

	return !!*prfx;
}
