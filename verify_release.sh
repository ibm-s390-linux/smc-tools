#!/bin/bash

# Consistency check: git branch
sref=`git symbolic-ref HEAD 2>/dev/null`;
if [ "${sref}" != "refs/heads/master" ]; then
	echo;
	echo "  WARNING: Verifying release on non-master branch.";
	echo;
fi

# We assume that at least the Makefile carries the correct version info
VERSION=`grep -w SMC_TOOLS_RELEASE Makefile | head -1 | awk '{print($3)}'`;
echo "Detected version: v$VERSION";


rc=0
# Build to make sure the final files are up to date
make >/dev/null 2>&1
if [ $? -ne 0 ]; then
        echo "Error: Build failed";
        exit 7;
fi

echo "Checking sources..."
# Consistency check: Do we have an entry in the README's "History" section...?
if [ `grep -A 100 "Release History:" README.md | grep "$VERSION" | wc -l` -ne 1 ]; then
	echo "  Error: 'README.md' is missing an entry for version '$VERSION' in 'Release History'";
	let rc=rc+1;
fi

# Check .spec file
if [ -e smc-tools.spec ]; then
	vspec="`grep -e "^Version: " smc-tools.spec | awk '{print($2)}'`";
	if [ "$vspec" != "$VERSION" ]; then
		echo "  Error: .spec file has version $vspec instead of $VERSION";
		let rc=rc+1;
	fi
fi

# Check shell scripts
for i in smc_rnics smc_dbg; do
	if [ `grep "VERSION=" $i | grep "$VERSION" | wc -l` -ne 1 ]; then
		echo "  Error: $i has wrong version info";
		let rc=rc+1;
	fi
done

# Check C files
if [ `grep "RELEASE_STRING" smctools_common.h | grep "$VERSION" | wc -l` -ne 1 ]; then
	echo "  Error: smctools_common.h has wrong version in RELEASE_STRING";
	let rc=rc+1;
fi


# Run programs to verify actual output
echo "Checking output...";
for i in smc_dbg smc_pnet smc_rnics smcd smcr smcss smc_chk; do
	if [ `./$i -v 2>&1 | grep "$VERSION" | wc -l` -ne 1 ]; then
		echo "  Error: './$i -v' has wrong version info: `./$i -v`";
		let rc=rc+1;
	fi
done


# Final verdict
echo;
if [ $rc -ne 0 ]; then
	echo "Verification failed, $rc errors detected";
else
	echo "Verification successful";
fi
echo;

exit $rc;
