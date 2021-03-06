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
#include <kern/stat.h>
#include <kern/seek.h>


int
sys_open(const_userptr_t filename, int flags, int *retval) {

	struct stat st;
	struct fdesc *fd = kmalloc(sizeof(struct fdesc));
	struct vnode *vn = kmalloc(sizeof(struct vnode));
	const char *safe_filename[NAME_MAX];
	
	int result = 0;
	int fd_index = -1;

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

	result = copyinstr(filename, (void *)safe_filename, sizeof(safe_filename), NULL);

	if (result != 0) {
		return result;
	}

	// Traverse current thread's filetable and check availability
	for (int i = 3; i <= OPEN_MAX; i++) {
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

	result =	VOP_STAT(fd->vn, &st);

	if (result != 0) {
		return result;
	}

	if (flags & O_APPEND) {
		fd->ofst = st.st_size;
	}
	else {
		fd->ofst = 0;
	}

	VOP_INCREF(fd->vn);
	fd->refnum++;

	// TODO I need to probably implement flocks, because this
	// locking and unlocking with only a regular lock seems sucky
	lock_release(fd->lk);

	*retval = fd_index;

	return 0;

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
	
	VOP_DECREF(fd->vn);
	fd->refnum--;

	vfs_close(fd->vn);

	fdesc_destroy(fd);

	lock_release(fd->lk);

	return 0;
}


ssize_t
sys_read(int read_fd, userptr_t buf, size_t buflen, ssize_t *retval) {
	struct fdesc *fd;
	struct iovec iov;
	struct uio u;
	int result;
	struct addrspace *as;
	void *safe_buf = kmalloc(sizeof(size_t));

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

	ssize_t num_bytes = 0;
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

	*retval = num_bytes;

	lock_release(fd->lk);

	kfree(safe_buf);

	return 0;
		
}


ssize_t
sys_write(int write_fd, const_userptr_t buf, size_t buflen, ssize_t *retval) {
	struct fdesc *fd;
	struct iovec iov;
	struct uio u;
	int result;
	struct addrspace *as;
	void *safe_buf = kmalloc(sizeof(size_t));

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

	written_bytes = u.uio_resid - written_bytes;

	*retval = written_bytes;

	lock_release(fd->lk);

	kfree(safe_buf);

	return 0;
}


int
sys_dup2(int oldfid, int newfid) {
	
	struct fdesc *fd;

	fd = curthread->t_ftable[oldfid];

	if (fd == NULL) {
		return EBADF;
	}


	/*
	 * Arg checking
	 */
	if (curthread->t_ftable[oldfid] == NULL) {
		return EBADF;
	}

	if (oldfid > OPEN_MAX || newfid > OPEN_MAX) {
		return EBADF;
	}

	if (oldfid < 0 || newfid < 0) {
		return EBADF;
	}

	if (oldfid == newfid) {
		return newfid;
	}

	lock_acquire(fd->lk);

	if (curthread->t_ftable[newfid] != NULL) {
		sys_close(newfid);
	}

	curthread->t_ftable[newfid] = fd;

	sys_close(oldfid);

	lock_release(fd->lk);

	return newfid;
}


int
sys_lseek(int seek_fd, off_t pos, const_userptr_t whence, off_t *retval) {

	int result = 0;
	struct fdesc *fd;
	struct stat st;
	off_t cursor;
	int safe_whence;

	copyin(whence, &safe_whence, sizeof(int32_t));

	fd = curthread->t_ftable[seek_fd];

	result = VOP_STAT(fd->vn, &st);

	if (result != 0) {
		return result;
	}

	if (fd == NULL) {
		return EBADF;
	}

	if (seek_fd < 0 || seek_fd > OPEN_MAX) {
		return EBADF;
	}

	if (!VOP_ISSEEKABLE(fd->vn)) {
		return ESPIPE;
	}
	
	lock_acquire(fd->lk);

	cursor = fd->ofst;

	switch (safe_whence) {
		case SEEK_SET:
	cursor = pos;
			break;
		case SEEK_CUR:
	cursor = fd->ofst + pos;
			break;
		case SEEK_END:
	cursor = st.st_size + pos;
			break;
		default:
			return EINVAL;
	}

	if (cursor < 0) {
		return EINVAL;
	}
	else {
		*retval = cursor;
	}

	lock_release(fd->lk);

	return 0;
}


int
sys_chdir(const_userptr_t  pathname) {
	
	int result = 0;
	char *safe_pathname = kmalloc(sizeof(const_userptr_t));

	result = copyinstr(pathname, (void *)safe_pathname, sizeof(safe_pathname), NULL);

	if (result != 0) {
		return result;
	}

	result = vfs_chdir(safe_pathname);
	
	if (result != 0) {
		return result;
	}

	kfree(safe_pathname);
	return 0;
}


int
sys_getcwd(userptr_t buf, size_t buflen, int *retval) {
	
	void *safe_buf = kmalloc(sizeof(size_t));
	int result = 0;

	struct iovec iov;
	struct uio u;
	struct addrspace *as;

	as = proc_getas();
	
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
	u.uio_offset = 0;	/* Desired offset into object */
	u.uio_resid = buflen;	/* Remaining amt of data to xfer */
	u.uio_segflg = UIO_USERSPACE;	/* What kind of pointer we have */
	u.uio_rw = UIO_READ;	/* Whether op is a read or write */
	u.uio_space = as;	/* Address space for user pointer */

	result = vfs_getcwd(&u);

	if (result != 0) {
		return result;
	}

	int num_bytes = u.uio_offset;

	if (num_bytes < 0) {
		return EFAULT;
	}

	kfree(safe_buf);
	*retval = num_bytes;
	return 0;
}
