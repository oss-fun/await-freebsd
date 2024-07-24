#!/bin/sh

# panic: Assertion *buflen >= 2 failed at ../../../kern/vfs_cache.c:2837
# cpuid = 21
# time = 1598296274
# KDB: stack backtrace:
# db_trace_self_wrapper() at db_trace_self_wrapper+0x2b/frame 0xfffffe0149a9e860
# vpanic() at vpanic+0x182/frame 0xfffffe0149a9e8b0
# panic() at panic+0x43/frame 0xfffffe0149a9e910
# vn_fullpath_any_smr() at vn_fullpath_any_smr+0x4df/frame 0xfffffe0149a9e980
# sys___realpathat() at sys___realpathat+0x20b/frame 0xfffffe0149a9ead0
# amd64_syscall() at amd64_syscall+0x159/frame 0xfffffe0149a9ebf0
# fast_syscall_common() at fast_syscall_common+0xf8/frame 0xfffffe0149a9ebf0
# --- syscall (0, FreeBSD ELF64, nosys), rip = 0x8004294ea, rsp = 0x7fffffffe548, rbp = 0x7fffffffe580 ---
# KDB: enter: panic
# [ thread pid 2831 tid 100306 ]
# Stopped at      kdb_enter+0x37: movq    $0,0x10b4246(%rip)
# db> x/s version
# version:        FreeBSD 13.0-CURRENT #0 r364722: Mon Aug 24 21:04:09 CEST 2020
# pho@t2.osted.lan:/usr/src/sys/amd64/compile/PHO\012
# db>

[ `uname -p` != "amd64" ] && exit 0

# Obtained from markj (syzkaller).
# Fixed by r364723.

. ../default.cfg
kldstat -v | grep -q sctp || kldload sctp.ko
cat > /tmp/syzkaller22.c <<EOF
// autogenerated by syzkaller (https://github.com/google/syzkaller)

#define _GNU_SOURCE

#include <pwd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/endian.h>
#include <sys/syscall.h>
#include <unistd.h>

uint64_t r[1] = {0xffffffffffffffff};

int main(void)
{
	syscall(SYS_mmap, 0x20000000ul, 0x1000000ul, 7ul, 0x1012ul, -1, 0ul);
	intptr_t res = 0;
	memcpy((void*)0x20000000, "./file0\000", 8);
	syscall(SYS_open, 0x20000000ul, 0x200645ul, 0ul);
	memcpy((void*)0x20000000, ".\000", 2);
	res = syscall(SYS_open, 0x20000000ul, 0ul, 0ul);
	if (res != -1)
		r[0] = res;
	memcpy((void*)0x20000240, "./file0\000", 8);
	syscall(SYS___realpathat, r[0], 0x20000240ul, 0x20000280ul, 8ul, 0ul);
	return 0;
}
EOF
mycc -o /tmp/syzkaller22 -Wall -Wextra -O0 /tmp/syzkaller22.c ||
    exit 1

(cd /tmp; timeout 1m ./syzkaller22)

rm -f /tmp/syzkaller22 /tmp/syzkaller22.c /tmp/syzkaller22.core /tmp/file0
exit 0