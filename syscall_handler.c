#include <bwio.h>
#include <syscall.h>
#include <syscall_handler.h>
#include <stdlib.h>

int sysCreate(TaskList *tlist, FreeList *flist, int priority, void (*code) (), int *rtn) {
	Task *task = createTask(flist, priority, code);
	if(task == NULL) return -1;

	insertTask(tlist, task);
	*rtn = task->tid;
	return 0;
}

int sysExit(TaskList *tlist, FreeList *flist) {
	removeCurrentTask(tlist, flist);
	return 0;
}

int sysMyPid(TaskList *tlist, int *rtn) {
	*rtn = tlist->curtask->tid;
	return 0;
}

void syscallHandler(void **parameters, KernelGlobal *global, void *user_sp, void *user_resume_point) {
	int callno = *((int*)(parameters[0]));
	int err = 0;
	int rtn = 0;

	DEBUG(DB_SYSCALL, "| SYSCALL:\tCall number: %d\n", callno);
	// DEBUG(DB_SYSCALL, "| SYSCALL:\tglobal: 0x%x\n", global);
	// DEBUG(DB_SYSCALL, "| SYSCALL:\tuser_sp: 0x%x\n", user_sp);
	// DEBUG(DB_SYSCALL, "| SYSCALL:\tuser_resume_point: 0x%x\n", user_resume_point);

	TaskList *tlist = global->tlist;
	FreeList *flist = global->flist;

	global->tlist->curtask->current_sp = user_sp;
	global->tlist->curtask->resume_point = user_resume_point;

	switch(callno) {
		case SYS_exit:
			err = sysExit(tlist, flist);
			break;
		case SYS_create:
			err = sysCreate(tlist, flist, *((int*)(parameters[1])), *((void **)(parameters[2])), &rtn);
			break;
		case SYS_myTid:
			err = sysMyPid(tlist, &rtn);
			break;
		case SYS_myParentTid:
			break;
		case SYS_pass:
		default:
			break;
	}

	*((int *)user_sp) = (err == 0 ? rtn : err);

	return;
}

