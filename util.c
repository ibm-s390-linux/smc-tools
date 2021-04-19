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
#include <math.h>

#include "util.h"

void print_unsup_msg(void)
{
	fprintf(stderr, "Error: Kernel does not support this parameter !\n");
	exit(-1);
}

void print_type_error(void) {
	fprintf(stderr, "Error: You entered an invalid type. Possible values are smcd and smcr !\n");
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

static void determine_mag_factor(int leading_places, char *magnitude,
			double *factor)
{
	if (leading_places < 7) {
		*magnitude = 'K';
		*factor = 1000;
	}
	else if (leading_places < 10) {
		*magnitude = 'M';
		*factor = 1000000;
	}
	else if (leading_places < 13) {
		*magnitude = 'G';
		*factor = 1000000000;
	}
	else {
		// this is quite expensive, hence we avoid if possible
		*factor = pow(1000, leading_places/3);
		if (leading_places < 16)
			*magnitude = 'T';
		else if (leading_places < 19)
			*magnitude = 'P';
		else
			*magnitude = '?';
	}
}


static void determine_digs(int leading_places, int max_digs,
		int *num_full_digs, int *num_places)
{
	*num_full_digs = leading_places % 3;
	if (*num_full_digs == 0)
		*num_full_digs = 3;
	*num_places = max_digs - *num_full_digs - 2;
	if (*num_places <= 0) {
		*num_places = 0;
		*num_full_digs = max_digs - 1;
	}
}


int get_abbreviated(uint64_t num, int max_digs, char *res)
{
	int num_full_digs, leading_places;
	char magnitude;
	int num_places;
	double factor;
	char tmp[128];

	if (num == 0) {
		snprintf(res, max_digs + 1, "0");
		return 1;
	}

	leading_places = sprintf(tmp, "%lld", (long long int)num);
	if (leading_places < 4) {
		snprintf(res, max_digs + 1, "%lu", num);
		return leading_places;
	}

	determine_digs(leading_places, max_digs, &num_full_digs, &num_places);
	determine_mag_factor(leading_places, &magnitude, &factor);

	double tmpnum = num / factor;
	if (tmpnum + 5 * pow(10, -1 - num_places) >= pow(10, num_full_digs)) {
		if (num_places > 0)
			// just strip down one decimal place,
			// e.g. 9.96... with 1.1 format would result in
			// 10.0 otherwise
			num_places--;
		else {
			// indicate that we need one more leading place
			// e.g. 999.872 with 3.0 format digits would result
			// in 1.000K otherwise
			leading_places++;
			determine_digs(leading_places, max_digs, &num_full_digs, &num_places);
			determine_mag_factor(leading_places, &magnitude, &factor);
			tmpnum = num / factor;
		}
	}

	snprintf(res, max_digs + 1, "%*.*lf%c", max_digs - 1, num_places, num / factor, magnitude);
	return 0;
}
