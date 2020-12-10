/*
 * File description structs
 * File table will be an array of MAX_SIZE file descriptions within struct thread
 *
 *
 *
 */
#include <types.h>
#include <vnode.h>
#include <synch.h>

struct fdesc {
	char *name;
	int openflags;
	off_t ofst;
	int refnum;
	struct vnode *vn;
	struct lock *lk;
};

int fdesc_init(struct fdesc *, char *, struct vnode *, int);

void fdesc_destroy(struct fdesc *);
