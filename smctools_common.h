/*
 * smc-tools/smctools_common.h
 *
 * Copyright IBM Corp. 2017
 *
 * Author(s): Ursula Braun (ubraun@linux.vnet.ibm.com)
 *
 * Copyright (c) IBM Corp. 2017
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef SMCTOOLS_COMMON_H
#define SMCTOOLS_COMMON_H

#define STRINGIFY_1(x)		#x
#define STRINGIFY(x)		STRINGIFY_1(x)

#define RELEASE_STRING	STRINGIFY (SMC_TOOLS_RELEASE)
#define RELEASE_LEVEL   "e0052e3"

#define PF_SMC 43

#endif /* SMCTOOLS_COMMON_H */
