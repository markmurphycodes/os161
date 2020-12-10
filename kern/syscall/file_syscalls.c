#include <kern/file_syscalls.h>
#include <kern/unistd.h>
#include <kern/fcntl.h>
#include <copyinout.h>
#include <limits.h>
#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <current.h>
#include <vfs.h>
#include <vnode.h>
#include <kern/stattypes.h>
#include <uio.h>
#include <proc.h>

int
sys_open(const_userptr_t filename, int flags, userptr_t retval) {

	struct stat *st = NULL;
	struct fdesc *fd = NULL;
	struct vnode *vn = NULL;
	int result = 0;
	const char *safe_filename = NULL;
	int fd_index = -1;
	size_t len = strlen((const char *)filename);
	userptr_t safe_retval = NULL;


	// Clean return val pointer
	copyin(retval, safe_retval, sizeof(retval));

	// Check flags
	int acc_mode = flags & O_ACCMODE;
	int flag_count = 0;

	switch (acc_mode) {
	    case O_RDONLY:
		flag_count++;
	    case O_WRONLY:
		flag_count++;
	    case O_RDWR:
		flag_count++;
		break;
	    default:
		return EINVAL;
	}
	
	// Check for valid file pointer
	if (filename == NULL) {
		return EFAULT;
	}

	result = copyin(filename, (void *)safe_filename, len);

	if (result != 0) {
		return result;
	}

	// Traverse current thread's filetable and check availability
	for (int i = 2; i <= OPEN_MAX; i++) {
		if (curthread->t_ftable[i] == NULL) {
			fd_index = i;
			break;
		}
	}
	if (fd_index < 0) {
		return EMFILE;
	}
	
	// Open file
	result = vfs_open((char *)safe_filename, flags, 0, &vn);
	if (result != 0) {
		// Do I need to close this file? 
		// At this point, I don't think so as the vfs_open function
		// cleans up any shenanigans. vfs_close(vn);
		curthread->t_ftable[fd_index] = NULL;
		return result;
	}

	// TODO these error return values need to be aligned with  the required
	// values from the MAN page
	result = fdesc_init(fd, (char *)safe_filename, vn, flags);
	
	if (result != 0) {
		return result;
	}

	// TODO fix the locking, will probably involve flocks or something similar
	// Enter critical section
	lock_acquire(fd->lk);

	if (flags & O_APPEND) {
		fd->ofst = VOP_STAT(fd->vn, st);
	}
	else {
		fd->ofst = 0;
	}
	spinlock_acquire(&fd->vn->vn_countlock);

	VOP_INCREF(fd->vn);
	fd->refnum++;

	spinlock_release(&fd->vn->vn_countlock);

	// TODO I need to probably implement flocks, because this
	// locking and unlocking with only a regular lock seems sucky
	lock_release(fd->lk);

	return fd_index;

}

int
sys_close(int close_fd) {

	struct fdesc *fd = NULL;

	if (curthread->t_ftable[close_fd] != NULL) {
		fd = curthread->t_ftable[close_fd];
	}
	else {
		return EBADF;
	}

	lock_acquire(fd->lk);

	spinlock_acquire(&fd->vn->vn_countlock);
	
	VOP_DECREF(fd->vn);
	fd->refnum--;
	
	spinlock_release(&fd->vn->vn_countlock);

	vfs_close(fd->vn);

	fdesc_destroy(fd);

	lock_release(fd->lk);

	return 0;
}


ssize_t
sys_read(int read_fd, userptr_t buf, size_t buflen, userptr_t retval) {
	(void)retval;
	ssize_t num_bytes = 0;
	struct fdesc *fd;
	struct iovec iov;
	struct uio u;
	int result;
	struct addrspace *as;
	void *safe_buf = NULL;

	as = proc_getas();

	// Check for valid filetable entry
	if (curthread->t_ftable[read_fd] != NULL) {
		fd = curthread->t_ftable[read_fd];
	}
	else {
		return EBADF;
	}
	

	// Check read/write flags
	if ((fd->openflags & O_ACCMODE) == O_WRONLY) {
		return EBADF;
	}

	// TODO this is incorrect, copyout should be used to write to buf
	// Clean buffer pointer
	result = copyin((userptr_t)buf, safe_buf, buflen);

	if (result != 0) {
		return result;
	}


	// Initialize iovec and uio for reading
	iov.iov_ubase = safe_buf;
	iov.iov_len = buflen;
	u.uio_iov = &iov;			/* Data blocks */
	u.uio_iovcnt = 1;	/* Number of iovecs */
	u.uio_offset = fd->ofst;	/* Desired offset into object */
	u.uio_resid = buflen;	/* Remaining amt of data to xfer */
	u.uio_segflg = UIO_USERSPACE;	/* What kind of pointer we have */
	u.uio_rw = UIO_READ;	/* Whether op is a read or write */
	u.uio_space = as;	/* Address space for user pointer */


	lock_acquire(fd->lk);

	// Read file
	result = VOP_READ(fd->vn, &u);

	if (result != 0) {
		lock_release(fd->lk);
		return result;
	}

	ssize_t available_bytes = u.uio_offset - fd->ofst;
	fd->ofst = u.uio_offset; 

	if (available_bytes > 0) {
		num_bytes = buflen;
	}
	else if (available_bytes == 0) {
		num_bytes = 0;
	}
	else {
		num_bytes = available_bytes * -1;
	}

	lock_release(fd->lk);

	return num_bytes;
		
}


ssize_t
sys_write(int write_fd, const_userptr_t buf, size_t buflen, userptr_t retval) {
	(void)retval;			
	ssize_t num_bytes = 0;
	struct fdesc *fd;
	struct iovec iov;
	struct uio u;
	int result;
	struct addrspace *as;
	void *safe_buf = NULL;

	as = proc_getas();

	// Check for valid filetable entry
	if (curthread->t_ftable[write_fd] != NULL) {
		fd = curthread->t_ftable[write_fd];
	}
	else {
		return EBADF;
	}
	

	// Check read/write flags
	if ((fd->openflags & O_ACCMODE) == O_RDONLY) {
		return EBADF;
	}

	// TODO this is incorrect, copyout should be used to write to buf
	// Clean buffer pointer
	result = copyin((userptr_t)buf, safe_buf, buflen);

	if (result != 0) {
		return result;
	}


	// Initialize iovec and uio for reading
	iov.iov_ubase = safe_buf;
	iov.iov_len = buflen;
	u.uio_iov = &iov;			/* Data blocks */
	u.uio_iovcnt = 1;	/* Number of iovecs */
	u.uio_offset = fd->ofst;	/* Desired offset into object */
	u.uio_resid = buflen;	/* Remaining amt of data to xfer */
	u.uio_segflg = UIO_USERSPACE;	/* What kind of pointer we have */
	u.uio_rw = UIO_WRITE;	/* Whether op is a read or write */
	u.uio_space = as;	/* Address space for user pointer */

	
	lock_acquire(fd->lk);

	//  Write to file
	result = VOP_WRITE(fd->vn, &u);

	if (result != 0) {
		lock_release(fd->lk);
		return result;
	}

	ssize_t written_bytes = u.uio_offset - fd->ofst;
	fd->ofst = u.uio_offset; 

	num_bytes = u.uio_resid - written_bytes;

	lock_release(fd->lk);

	return num_bytes;
}


int
sys_dup2(int oldfid, int newfid) {
	(void)oldfid;
	(void)newfid;
	return 0;
}


off_t
sys_lseek(int fd, off_t pos, int whence) {
	(void)fd;
	(void)pos;
	(void)whence;
	return pos;
}


int
sys_chdir(const char *pathname) {
	(void)pathname;
	return 0;
}


int
sys_getcwd(char *buf, size_t buflen) {
	(void)buf;
	(void)buflen;
	return 0;
}
