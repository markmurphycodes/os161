#include <types.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <mips/trapframe.h>

pid_t sys_getpid(void);

pid_t sys_fork(struct trapframe *, int *);

int sys_execv(const_userptr_t, userptr_t *);

pid_t sys_waitpid(pid_t, userptr_t, int);

void sys_exit(int);
