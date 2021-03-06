/*
 * Functionality for file decriptors
 *
 */
#include <kern/stattypes.h>
#include <kern/fdesc.h>
#include <synch.h>
#include <vnode.h>
#include <types.h>
#include <lib.h>

int
fdesc_init(struct fdesc *fd, char *name, struct vnode *vn, int flags) {
	
	fd->name = kstrdup(name);
	if (fd->name == NULL) {
		return -1;
	}

	fd->openflags = flags;

	fd->vn = vn;
	if (fd->vn == NULL) {
		return -1;
	}

	fd->ofst = 0;

	fd->refnum = 0;

	fd->lk = lock_create(fd->name);
	if (fd->lk == NULL) {
		return -1;
	}

	return 0;
}

void
fdesc_destroy(struct fdesc *fd) {
	
	kfree(fd->name);

	lock_destroy(fd->lk);

	kfree(fd);
}
