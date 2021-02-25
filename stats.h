/*
 * SMC Tools - Shared Memory Communication Tools
 *
 * Copyright IBM Corp. 2021
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

#ifndef SMC_SYSTEM_H_
#define SMC_SYSTEM_H_

extern struct rtnl_handle rth;

int invoke_stats(int argc, char **argv, int detail_level);


#endif /* SMC_SYSTEM_H_ */
