#include <kern/unistd.h>
#include <kern/fcntl.h>
#include <types.h>

int sys_open(const_userptr_t, int, userptr_t);

int sys_close(int);

ssize_t sys_read(int, userptr_t, size_t, userptr_t);

ssize_t sys_write(int, const_userptr_t, size_t, userptr_t);

int sys_dup2(int, int);

off_t sys_lseek(int, off_t, int);

int sys_chdir(const char *);

int sys_getcwd(char *, size_t);
