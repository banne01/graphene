/* -*- mode:c; c-file-style:"k&r"; c-basic-offset: 4; tab-width:4; indent-tabs-mode:nil; mode:auto-fill; fill-column:78; -*- */
/* vim: set ts=4 sw=4 et tw=78 fo=cqt wm=0: */

/* Copyright (C) 2014 OSCAR lab, Stony Brook University
   This file is part of Graphene Library OS.

   Graphene Library OS is free software: you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   Graphene Library OS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/*
 * db_mutex.c
 *
 * This file contains APIs that provide operations of (futex based) mutexes.
 * Based on "Mutexes and Condition Variables using Futexes"
 * (http://locklessinc.com/articles/mutex_cv_futex)
 */

#include "pal_defs.h"
#include "pal_linux_defs.h"
#include "pal.h"
#include "pal_internal.h"
#include "pal_linux.h"
#include "pal_error.h"
#include "api.h"

#include <linux/futex.h>
#include <limits.h>
#include <atomic.h>
#include <cmpxchg.h>
#include <asm-errno.h>
#include <linux/time.h>
#include <unistd.h>

#if defined(__i386__)
#define rmb()           asm volatile("lock; addl $0,0(%%esp)" ::: "memory")
#define cpu_relax()     asm volatile("rep; nop" ::: "memory");
#endif

#if defined(__x86_64__)
#include <unistd.h>
#define rmb()           asm volatile("lfence" ::: "memory")
#define cpu_relax()     asm volatile("rep; nop" ::: "memory");
#endif

#define MUTEX_SPINLOCK_TIMES    20

int _DkMutexLockTimeout (struct mutex_handle * mut, int timeout)
{
    int i, c = 0;

    if (timeout == -1)
        return -_DkMutexLock(mut);

    struct atomic_int * m = &mut->value;

    /* Spin and try to take lock */
    for (i = 0 ; i < MUTEX_SPINLOCK_TIMES ; i++)
    {
        c = atomic_dec_and_test(m);
        if (c)
            goto success;
        cpu_relax();
    }

    /* The lock is now contended */

    int ret;

    if (timeout == 0) {
        ret = c ? 0 : -PAL_ERROR_TRYAGAIN;
        goto out;
    }

    while (!c) {
        struct timespec waittime;
        long sec = timeout / 1000000;
        long microsec = timeout - (sec * 1000000);
        waittime.tv_sec = sec;
        waittime.tv_nsec = microsec * 1000;

        ret = INLINE_SYSCALL(futex, 6, m, FUTEX_WAIT, 2, &waittime, NULL, 0);

        if (IS_ERR(ret) && ERRNO(ret) != EWOULDBLOCK) {
            ret = unix_to_pal_error(ERRNO(ret));
            goto out;
        }

#ifdef DEBUG_MUTEX
        if (IS_ERR(ret))
            printf("mutex held by thread %d\n", mut->owner);
#endif

        /* Upon wakeup, we still need to check whether mutex is unlocked or
         * someone else took it.
         * If c==0 upon return from xchg (i.e., the older value of m==0), we
         * will exit the loop. Else, we sleep again (through a futex call).
         */
        c = atomic_dec_and_test(m);
    }

success:
#ifdef DEBUG_MUTEX
    mut->owner = INLINE_SYSCALL(gettid, 0);
#endif
    ret = 0;
out:
    return ret;
}

int _DkMutexLock (struct mutex_handle * mut)
{
    int i, c = 0;
    int ret;
    struct atomic_int * m = &mut->value;

    /* Spin and try to take lock */
    for (i = 0; i < MUTEX_SPINLOCK_TIMES; i++) {
        c = atomic_dec_and_test(m);
        if (c)
            goto success;
        cpu_relax();
    }

    /* The lock is now contended */

    while (!c) {
        ret = INLINE_SYSCALL(futex, 6, m, FUTEX_WAIT, 2, NULL, NULL, 0);

        if (IS_ERR(ret) && ERRNO(ret) != EWOULDBLOCK) {
            ret = unix_to_pal_error(ERRNO(ret));
            goto out;
        }

#ifdef DEBUG_MUTEX
        if (IS_ERR(ret))
            printf("mutex held by thread %d\n", mut->owner);
#endif
        /* Upon wakeup, we still need to check whether mutex is unlocked or
         * someone else took it.
         * If c==0 upon return from xchg (i.e., the older value of m==0), we
         * will exit the loop. Else, we sleep again (through a futex call).
         */
        c = atomic_dec_and_test(m);
    }

success:
#ifdef DEBUG_MUTEX
    mut->owner = INLINE_SYSCALL(gettid, 0);
#endif
    ret = 0;
out:
    return ret;
}

int _DkMutexUnlock (struct mutex_handle * mut)
{
    int ret = 0;
    int must_wake = 0;
    struct atomic_int * m = &mut->value;

#ifdef DEBUG_MUTEX
    mut->owner = 0;
#endif

    /* Unlock, and if not contended then exit. */
    if (atomic_read(m) < 0)
        must_wake = 1;

    atomic_set(m, 1);

    if (must_wake) {
        /* We need to wake someone up */
        ret = INLINE_SYSCALL(futex, 6, m, FUTEX_WAKE, 1, NULL, NULL, 0);
    }

    if (IS_ERR(ret)) {
        ret = -PAL_ERROR_TRYAGAIN;
        goto out;
    }

    ret = 0;
out:
    return ret;
}
