#ifndef __SYSCALL_HANDLER_H__
#define __SYSCALL_HANDLER_H__

#define SYS_exit		0
#define SYS_create		1
#define SYS_pass		2
#define SYS_myTid		3
#define SYS_myParentTid	4

void syscallHandler();


#endif // __SYSCALL_HANDLER_H__
