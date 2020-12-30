#include <types.h>
#include <kern/unistd.h>
#include <kern/fcntl.h>

int sys_open(const_userptr_t, int, int *);

int sys_close(int);

ssize_t sys_read(int, userptr_t, size_t, int *);

ssize_t sys_write(int, const_userptr_t, size_t, int *);

int sys_dup2(int, int);

int sys_lseek(int, off_t, const_userptr_t, off_t *);

int sys_chdir(const_userptr_t);

int sys_getcwd(userptr_t, size_t, int *);
