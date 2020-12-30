#include <types.h>

struct process {
	pid_t pid;
	struct thread *t;
	int exitcode;
	int returned;
	struct semaphore proc_sem;
}

struct process p_create(pid_t);

int p_destroy(pid_t);
