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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include "smctools_common.h"
#include "libnetlink.h"
#include "util.h"
#include "linkgroup.h"
#include "dev.h"
#include "info.h"
#include "stats.h"

static int option_detail = 0;
#if defined(SMCD)
char *myname = "smcd";
#elif defined(SMCR)
char *myname = "smcr";
#else
char *myname = "smc";
#endif

static void version(void)
{
	fprintf(stderr,
		"%s utility, smc-tools-%s\n", myname, RELEASE_STRING);
	exit(-1);
}
static void usage(void)
{
	fprintf(stderr,
		"Usage: %s  [ OPTIONS ] OBJECT {COMMAND | help}\n"
		"where  OBJECT := {info | linkgroup | device | stats}\n"
#if defined(SMCD)
		"       OPTIONS := {-v[ersion] | -d[etails] | -a[bsolute]}\n", myname);
#else
		"       OPTIONS := {-v[ersion] | -d[etails] | -dd[details] | -a[bsolute]}\n", myname);
#endif
}

static int invoke_help(int argc, char **argv, int k)
{
	usage();
	return 0;
}

static const struct cmd {
	const char *cmd;
	int (*func)(int argc, char **argv, int option_detail);
} cmds[] = {
	{ "device",	invoke_devs },
	{ "linkgroup",	invoke_lgs },
	{ "info",	invoke_info },
	{ "stats",	invoke_stats },
	{ "help",	invoke_help },
	{ 0 }
};

static int run_cmd(const char *argv0, int argc, char **argv)
{
	const struct cmd *c;

	for (c = cmds; c->cmd; ++c) {
		if (contains(argv0, c->cmd) == 0)
			return -(c->func(argc-1, argv+1, option_detail));
	}

	fprintf(stderr, "Error: Object \"%s\" is unknown, try \"%s help\".\n", argv0, myname);
	return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
	int rc = 0;

	while (argc > 1) {
		char *opt = argv[1];

		if (strcmp(opt, "--") == 0) {
			argc--; argv++;
			break;
		}
		if (opt[0] != '-')
			break;
		if (opt[1] == '-')
			opt++;

		if ((strncmp(opt, "-ad", 3) == 0) || (strncmp(opt, "-da", 3) == 0)) {
			option_detail = SMC_OPTION_DETAIL_ABS;
		} else if (contains(opt, "-absolute") == 0) {
			option_detail = SMC_OPTION_ABS;
		} else if (contains(opt, "-version") == 0) {
			version();
		} else if (contains(opt, "-detail") == 0) {
			option_detail = SMC_DETAIL_LEVEL_V;
		} else if (contains(opt, "-ddetail") == 0) {
			option_detail = SMC_DETAIL_LEVEL_VV;
		} else if (contains(opt, "-help") == 0) {
			usage();
			goto out;
		} else {
			fprintf(stderr,
				"Error: Option \"%s\" is unknown, try \"%s help\".\n",
				opt, myname);
			exit(-1);
		}
		argc--;	argv++;
	}

	if (gen_nl_open(myname))
		exit(1);
	if (argc > 1) {
		rc = run_cmd(argv[1], argc-1, argv+1);
		goto out;
	}
	usage();
out:
	gen_nl_close();
	return rc;
}
