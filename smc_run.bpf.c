// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 *  eBPF version of smc_run
 *
 *  Copyright (c) 2024, Alibaba Inc.
 *
 *  Author:  D. Wythe <alibuda@linux.alibaba.com>
 */

#include "smc_run.bpf.skel.h"
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

static const char *mount_path(const char *);

#define SMC_RUN_PROG_NAME		"smc_run"
#define SMC_RUN_PATH			mount_path(SMC_RUN_PROG_NAME)

#define SMC_RUN_PROCESS_EXIT_PROG_NAME	"smc_run_process_exit"
#define SMC_RUN_PROCESS_EXIT_PATH	mount_path(SMC_RUN_PROCESS_EXIT_PROG_NAME)

#define SMC_RUN_PROCESS_FORK_PROG_NAME	"smc_run_process_fork"
#define SMC_RUN_PROCESS_FORK_PATH	mount_path(SMC_RUN_PROCESS_FORK_PROG_NAME)

#define SMC_RUN_NETNS_CLEANUP_PROG_NAME	"smc_run_cleanup_netns"
#define SMC_RUN_NETNS_CLEANUP_PATH	mount_path(SMC_RUN_NETNS_CLEANUP_PROG_NAME)

#define SMC_RUN_PID_MAP_NAME		"smc_run_pid"
#define SMC_RUN_PID_MAP_PATH		mount_path(SMC_RUN_PID_MAP_NAME)

#define SMC_RUN_NETNS_MAP_NAME		"smc_run_netns"
#define SMC_RUN_NETNS_MAP_PATH		mount_path(SMC_RUN_NETNS_MAP_NAME)

struct smc_run_strategy {
	__u8	enable;
	__u8	inherit;
};

enum {
	SMC_RUN_MAP_ADD,
	SMC_RUN_MAP_DEL,
};

enum {
	SMC_RUN_BPF_MAP,
	SMC_RUN_BPF_PROG,
};

static int smc_run_log_level = LIBBPF_WARN;

static int libbpf_print_fn(enum libbpf_print_level level, const char *format,
						   va_list args)
{
	if (level <= smc_run_log_level)
		return vfprintf(stderr, format, args);
	return 0;
}

static int smc_run_log_fn(enum libbpf_print_level level, const char *format, ...)
{
	va_list args;
	int ret;

	va_start(args, format);
	ret = libbpf_print_fn(level, format, args);
	va_end(args);

	return ret;
}

#define SMC_RUN_LOG_ERR(format, ...) smc_run_log_fn(LIBBPF_WARN, format, ##__VA_ARGS__)
#define SMC_RUN_LOG_DEBUG(format, ...) smc_run_log_fn(LIBBPF_DEBUG, format, ##__VA_ARGS__)

#define min(x, y) ((x) < (y) ? (x) : (y))

static int atoi_check(const char *str, int *val)
{
	const char *p = str;

	if (str == NULL || *str == '\0') {
		return -1;
    }

    while (*p) {
        if (!isdigit((unsigned char)*p)) {
            return -1;
        }
        p++;
    }

    *val = atoi(str);
	return 0;
}

static int smc_run_get_current_netns_ino(int *ino)
{
	struct stat statbuf;

	if (stat("/proc/self/ns/net", &statbuf) == -1) {
		SMC_RUN_LOG_ERR("Unable to access current netns info: %s\n", strerror(errno));
		return -1;
	}

	*ino = statbuf.st_ino;
	return 0;
}

static int smc_run_get_pidns_pid(pid_t pid, pid_t *NSPid)
{
	char path[256], line[256];
	int host_pid = -1;
	char *token;
	FILE *f;

	snprintf(path, sizeof(path), "/proc/%d/status", pid);

	f = fopen(path, "r");
	if (f == NULL) {
		SMC_RUN_LOG_ERR("Unable open %s: %s\n", path, strerror(errno));
		return -1;
	}

	while (fgets(line, sizeof(line), f)) {
		if (strncmp(line, "NSpid:", 6) == 0) {
			strtok(line, "\t");
			while ((token = strtok(NULL, "\t")) != NULL)
				host_pid = atoi(token);
			break;
		}
	}

	fclose(f);

	if (host_pid == -1) {
		SMC_RUN_LOG_ERR("Failed to Find NSPid.\n");
		return -1;
	}

	*NSPid = host_pid;
	return 0;
}

/* very slow path */
static int smc_run_get_map_fd_by_name(const char *name)
{
	struct bpf_map_info info;
	__u32 id = 0, info_len;
	int err, fd = -1;

	info_len = sizeof(info);

	while (true) {
		err = bpf_map_get_next_id(id, &id);
		if (err) {
			SMC_RUN_LOG_DEBUG("Can not get map id: %d\n", err);
			break;
		}
		fd = bpf_map_get_fd_by_id(id);
		if (fd < 0) {
			SMC_RUN_LOG_DEBUG("Can not get map fd by id: %d\n", err);
			break;
		}
		err = bpf_obj_get_info_by_fd(fd, &info, &info_len);
		if (err) {
			SMC_RUN_LOG_DEBUG("Can not get map info by fd: %d\n", err);
			close(fd);
			fd = -1;
			break;
		}
		if (strncmp(name, info.name, min(strlen(name), BPF_OBJ_NAME_LEN)) == 0)
			break;
		close(fd);
		fd = -1;
	}
	return fd;
}

static int smc_run_load_bpf_prog()
{
	struct smc_run_bpf_bpf *skel;
	int err, map_fd;
	struct stat st;

	/* fast path to check if bpf prog is already loaded */
	if (stat(SMC_RUN_PID_MAP_PATH, &st) == 0)
		return 0;

	/* slow path */
	map_fd = smc_run_get_map_fd_by_name(SMC_RUN_PID_MAP_NAME);
	if (map_fd > 0) {
		close(map_fd);
		return 0;
	}

	/* Open load and verify BPF application */
	skel = smc_run_bpf_bpf__open_and_load();
	if (!skel) {
		SMC_RUN_LOG_ERR("Failed to open BPF skeleton.\n");
		return -1;
	}

	/* Attach tracepoint handler */
	err = smc_run_bpf_bpf__attach(skel);
	if (err) {
		SMC_RUN_LOG_ERR("Failed to attach prog: %d\n", err);
		goto err_out;
	}

	/* Pin smc_run prog */
	err = bpf_link__pin(skel->links.smc_run, SMC_RUN_PATH);
	if (err < 0) {
		SMC_RUN_LOG_ERR("Failed to pin smc_run prog: %d\n", err);
		goto err_out;
	}

	/* Pin smc_run_process_exit prog */
	err = bpf_link__pin(skel->links.smc_run_on_process_exit,
						SMC_RUN_PROCESS_EXIT_PATH);
	if (err < 0) {
		SMC_RUN_LOG_ERR("Failed to pin smc_run_process_exit prog: %d\n", err);
		goto err_pin_pexit;
	}

	/* Pin smc_run_process_fork prog */
	err = bpf_link__pin(skel->links.smc_run_on_process_fork,
						SMC_RUN_PROCESS_FORK_PATH);
	if (err < 0) {
		SMC_RUN_LOG_ERR("Failed to pin smc_run_process_fork prog: %d\n", err);
		goto err_pin_pfork;
	}

	/* Pin scm_run_on_net_cleanup prog */
	err = bpf_link__pin(skel->links.smc_run_on_net_cleanup,
				SMC_RUN_NETNS_CLEANUP_PATH);
	if (err < 0) {
		SMC_RUN_LOG_ERR("Failed to pin smc_run_on_net_cleanup prog: %d\n", err);
		goto err_pin_netns_cleanup;	
	}

	/* Pin pid map */
	err = bpf_map__pin(skel->maps.smc_run_pid, SMC_RUN_PID_MAP_PATH);
	if (err < 0) {
		SMC_RUN_LOG_ERR("Failed to pin smc_run_pid map: %d\n", err);
		goto err_pin_pid;
	}

	/* Pin netns map */
	err = bpf_map__pin(skel->maps.smc_run_netns, SMC_RUN_NETNS_MAP_PATH);
	if (err < 0) {
		SMC_RUN_LOG_ERR("Failed to pin smc_run_netns map: %d\n", err);
		goto err_pin_netns;
	}

	smc_run_bpf_bpf__destroy(skel);
	return 0;
err_pin_netns:
	bpf_map__unpin(skel->maps.smc_run_pid, NULL);
err_pin_pid:
	bpf_link__unpin(skel->links.smc_run_on_net_cleanup);	
err_pin_netns_cleanup:
	bpf_link__unpin(skel->links.smc_run_on_process_fork);
err_pin_pfork:
	bpf_link__unpin(skel->links.smc_run_on_process_exit);
err_pin_pexit:
	bpf_link__unpin(skel->links.smc_run);
err_out:
	smc_run_bpf_bpf__destroy(skel);
	return -1;
}

static int smc_run_update_map__int(const char *name, int key, struct smc_run_strategy *value, int op)
{
	bool optimistic = true;
	int map_fd;

again:
	map_fd = bpf_obj_get(mount_path(name));
	if (map_fd < 0) {
		/* try slow path */
		map_fd = smc_run_get_map_fd_by_name(name);
		if (map_fd < 0) {
			if (optimistic) {
				if (smc_run_load_bpf_prog() < 0)
					return -1;
				optimistic = false;
				goto again;
			}
			SMC_RUN_LOG_ERR("Can't open BPF map(%s): %s\n", name, strerror(errno));
			return -1;
		}
	}

	switch (op) {
	case SMC_RUN_MAP_ADD:
		if (bpf_map_update_elem(map_fd, &key, value, BPF_ANY) != 0) {
			SMC_RUN_LOG_ERR("Failed to update BPF map(%s): %s\n", name, strerror(errno));
			return -1;
		}
		break;
	case SMC_RUN_MAP_DEL:
		if (bpf_map_delete_elem(map_fd, &key) != 0) {
			SMC_RUN_LOG_ERR("Failed to delete BPF map(%s): %s\n", name, strerror(errno));
			return -1;
		}
		break;
	default:
		return -1;
	}	

	close(map_fd);
	return 0;
}

static int smc_run_add_pid(pid_t pid)
{
	struct smc_run_strategy value = { .enable = 1, .inherit = 1, };
	return smc_run_update_map__int(SMC_RUN_PID_MAP_NAME, pid, &value, SMC_RUN_MAP_ADD);
}

static int smc_run_add_netns(int ino)
{
	struct smc_run_strategy value = { .enable = 1, };
	return smc_run_update_map__int(SMC_RUN_NETNS_MAP_NAME, ino, &value, SMC_RUN_MAP_ADD);
}

static void smc_run_del_netns(int ino)
{
	smc_run_update_map__int(SMC_RUN_NETNS_MAP_NAME, ino, NULL, SMC_RUN_MAP_DEL);	
}

void smc_run_destroy(void)
{
	unlink(SMC_RUN_PATH);
	unlink(SMC_RUN_PROCESS_EXIT_PATH);
	unlink(SMC_RUN_PROCESS_FORK_PATH);
	unlink(SMC_RUN_PID_MAP_PATH);
	unlink(SMC_RUN_NETNS_MAP_PATH);
	unlink(SMC_RUN_NETNS_CLEANUP_PATH);
}

void smc_run_with_opt(int argc, char **argv)
{
	int opt, netns_ino, on;
	pid_t pid;

	while ((opt = getopt(argc, argv, "hvs:p:n:")) != -1)
	{
		switch (opt)
		{
		case 'h':
print_usage:
			printf("Usage: %s [-h] [-v] [-s [load|unload]] [-p pid] [-n [on(1)|off(1)]]\n", argv[0]);
			printf("Usage: %s COMMAND \n", argv[0]);
			exit(EXIT_SUCCESS);
		case 's':
			/* check for root privileges */
			if (geteuid() != 0) {
				SMC_RUN_LOG_ERR("Failed to perform, requires root privileges.\n");
				exit(EXIT_SUCCESS);
			}
			if (strcmp(optarg, "unload") == 0) {
				smc_run_destroy();
			} else if (strcmp(optarg, "load") == 0) {
				if (smc_run_load_bpf_prog() < 0)
					exit(EXIT_FAILURE);
			}
			break;
		case 'p':
			pid = atoi(optarg);
			/* wrong pid or zero pid is un-accepted */
			if (!pid) {
				SMC_RUN_LOG_ERR("Seen incorrect pid(%s)\n", optarg);
				exit(EXIT_FAILURE);
			}
			/* get NSPid */
			if (smc_run_get_pidns_pid(pid, &pid) < 0) /* Get parent NSPid */
				exit(EXIT_FAILURE);
			/* Add NSPid to pid map */
			if (smc_run_add_pid(pid) < 0)
				exit(EXIT_FAILURE);
			break;
		case 'v':
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
			/* show version */
			printf("smc_run.bpf (ebpf version of smc_run), smc-tools-%s\n", TOSTRING(SMC_TOOLS_RELEASE));
			break;
#undef TOSTRING
#undef STRINGIFY
		case 'n':
			if (atoi_check(optarg, &on) < 0)
				goto print_usage;
			if (smc_run_get_current_netns_ino(&netns_ino) < 0)
				exit(EXIT_FAILURE);
			/* Add netns_ino to netns map */
			if (on && smc_run_add_netns(netns_ino) < 0)
				exit(EXIT_FAILURE);
			else if (!on)
				smc_run_del_netns(netns_ino);	
			break;
		default:
			SMC_RUN_LOG_ERR("unkonw option: %c\n", opt);
			exit(EXIT_FAILURE);
		}
	}
}

static char *default_bpf_mount_path = "/sys/fs/bpf";

static const char *mount_path(const char *name)
{
	static char path_buf[128];
	sprintf(path_buf,"%s/%s", default_bpf_mount_path, name);
	return path_buf;
}

static void parse_env(void)
{
	const char *t;

	if (getenv("SMC_DEBUG"))
		smc_run_log_level = LIBBPF_DEBUG;

	if((t = getenv("SMC_RUN_BPF_MOUNT_FS")))
		default_bpf_mount_path = (char *)t;
}

int main(int argc, char **argv)
{
	if (argc < 2)
		exit(EXIT_SUCCESS);

	/* Set up libbpf errors and debug info callback */
	libbpf_set_print(libbpf_print_fn);

	/* Handle env */
	parse_env();

	/* run with args */
	if (argv[1][0] == '-') {
		smc_run_with_opt(argc, argv);
	} else { /* run with cmd */
		/* add current NSPid to smc_run_pid map */
		pid_t pid;
		if (smc_run_get_pidns_pid(getpid(), &pid) < 0 || smc_run_add_pid(pid) < 0)
			exit(EXIT_FAILURE);
		/* exec */
		execvp(argv[1], argv + 1);
	}
}
