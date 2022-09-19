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

#ifndef DEV_H_
#define DEV_H_

extern struct rtnl_handle rth;

int invoke_devs(int argc, char **argv, int detail_level);
int dev_count_ism_devices(int *ism_count);
int dev_count_roce_devices(int *rocev1_count, int *rocev2_count, int *rocev3_count);

#endif /* DEV_H_ */
