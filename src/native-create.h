/* SPDX-License-Identifier: MIT */
#ifndef QGA9_NATIVE_CREATE_H
#define QGA9_NATIVE_CREATE_H
/* Exclusive create in a pinned mounted directory; final-parent unions fail.
 * Caller owns the returned close-on-exec descriptor. No cwd/namespace changes
 * escape the private helper, and no inherited management descriptor is kept. */
int nativefilecreate(const char*, int);
#endif
