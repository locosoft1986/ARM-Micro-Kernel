/*
 * Kernel Library
 */
#ifndef __KLIB_H__
#define __KLIB_H__

#include <task.h>

typedef struct kernel_global {
	TaskList 	*task_list;
	FreeList 	*free_list;
	BlockedList	*blocked_list;
	MsgBuffer	*msg_array;
	Task	 	*task_array;
} KernelGlobal;

/* String functions */
int strcmp(const char *src, const char *dst);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, int n);
int strlen(const char *s);

/* Memory manipulations */
void copyBytes(char *dst, const char *src);

/* Print */
#include <intern/bwio.h>
#include <intern/debug.h>


#endif // __KLIB_H__
