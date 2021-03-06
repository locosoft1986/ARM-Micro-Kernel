#include <task.h>
#include <klib.h>
#include <unistd.h>
#include <kern/md_const.h>
#include <intern/trapframe.h>

void readyQueueInitial(ReadyQueue *ready_queue, Heap *task_heap, HeapNode *task_heap_nodes, TaskList *task_list) {
	ready_queue->curtask = NULL;
	ready_queue->head = NULL;
	ready_queue->heap = task_heap;
	ready_queue->heap_nodes = task_heap_nodes;

	int i;

	// TODO: Add magic number to the end of each stack, so can detect stack overflow

	for (i = 0; i < TASK_PRIORITY_MAX; i++) {
		task_heap_nodes[i].key = i;
		task_heap_nodes[i].datum = &(task_list[i]);
		task_list[i].head = NULL;
		task_list[i].tail = NULL;
	}
}

void taskArrayInitial(Task *task_array, char *stacks) {
	int i;
	for (i = 0; i < TASK_MAX; i++) {
		task_array[i].tid = i - TASK_MAX;
		task_array[i].init_sp = &(stacks[(i+1) * TASK_STACK_SIZE - 4]);
		task_array[i].state = Zombie;
	}
}

void freeListInitial(FreeList *free_list, Task *task_array) {
	int i;
	for (i = 0; i < TASK_MAX - 1; i++) {
		task_array[i].next = &(task_array[i+1]);
	}
	task_array[TASK_MAX - 1].next = NULL;

	free_list->head = &task_array[0];
	free_list->tail = &task_array[TASK_MAX - 1];
}

void blockedListsInitial(BlockedList *blocked_lists, int list_num) {
	int i = -1;
	for (i = 0; i < list_num; i++) {
		blocked_lists[i].head = NULL;
		blocked_lists[i].tail = NULL;
	}
}

void msgArrayInitial(MsgBuffer *msg_array) {
	int i = -1;
	for (i = 0; i < TASK_MAX; i++) {
		msg_array[i].msg = NULL;
		msg_array[i].reply = NULL;
		msg_array[i].receive = NULL;
		msg_array[i].event = NULL;
	}
}

int insertTask(ReadyQueue *ready_queue, Task *new_task) {
	int priority = new_task->priority;

	// Change the task state to Ready
	new_task->state = Ready;
	new_task->next = NULL;

	HeapNode *node = &(ready_queue->heap_nodes[priority]);
	TaskList *task_list = (TaskList *)(node->datum);

	if (task_list->head == NULL) {
		task_list->head = new_task;
		task_list->tail = new_task;
		minHeapInsert(ready_queue->heap, node);
	} else {
		task_list->tail->next = new_task;
		task_list->tail = new_task;
	}

	// Update head task
	ready_queue->head = ((TaskList *)(ready_queue->heap->data[0]->datum))->head;

	DEBUG(DB_TASK, "| TASK:\tInserted\tTid: %d SP: 0x%x\n", new_task->tid, new_task->current_sp);
	return 1;
}

void removeCurrentTask(ReadyQueue *ready_queue, FreeList *free_list) {
	assert(ready_queue->curtask == ready_queue->head, "Trying to remove a curtask that is not a head. ");
	Task *task = ready_queue->curtask;
	int priority = task->priority;

	TaskList *task_list = (TaskList *)(ready_queue->heap_nodes[priority].datum);

	// Last task in its priority queue
	if (task->next == NULL) {
		task_list->head = NULL;
		task_list->tail = NULL;
		minHeapPop(ready_queue->heap);
	}
	// Otherwise
	else {
		task_list->head = task_list->head->next;
	}

	// Update head task and clear curtask
	if (ready_queue->heap->heapsize > 0) {
		ready_queue->head = ((TaskList *)(ready_queue->heap->data[0]->datum))->head;
	} else {
		ready_queue->head = NULL;
	}
	ready_queue->curtask = NULL;

	// Change the task state to Zombie and next to NULL (tail of free_list)
	assert(task->state == Active, "Current task state is not Active");
	task->state = Zombie;
	task->next = NULL;

	// Add the task to free list
	if (free_list->head == NULL) {
		free_list->head = task;
		free_list->tail = task;
	} else {
		free_list->tail->next = task;
		free_list->tail = task;
	}
}

Task *createTask(FreeList *free_list, int priority, void (*code) ()) {
	assert(priority >= 0 && priority <= TASK_PRIORITY_MAX, "Invalid priority value!");
	Task *ret = NULL;
	ret = free_list->head;
	if(ret == NULL) {
		assert(0, "run out of td");
		return ret;
	}

	assert(ret->state == Zombie, "Invalid task descriptor to use!");
	ret->tid = ret->tid + TASK_MAX;
	ret->state = Ready;
	ret->priority = priority;
	assert(ret->tid >= 0, "Tid overflowed!");

	free_list->head = free_list->head->next;

	// Link to next ready task and parent tid should be set when pushing to task list
	ret->next = NULL;
	ret->parent_tid = -1;

	ret->current_sp = initTrap(ret->init_sp, (TEXT_REG_BASE + Exit), (TEXT_REG_BASE + code));
	DEBUG(DB_TASK, "| Task:\tCreated\t\tTid: 0x%x SP: 0x%x, LR: 0x%x\n", ret->tid, ret->current_sp, (TEXT_REG_BASE + code));
	return ret;
}

void moveCurrentTaskToEnd(ReadyQueue *ready_queue) {
	Task *task = ready_queue->curtask;
	int priority = task->priority;

	// Change the task state to Ready
	assert(task->state == Active, "Current task state is not Active");
	task->state = Ready;

	TaskList *task_list = (TaskList *)(ready_queue->heap_nodes[priority].datum);

	if (task_list->head != task_list->tail) {
		task_list->head = task_list->head->next;
		task_list->tail->next = task;
		task_list->tail = task;
		task->next = NULL;
	}

	// Update head task
	ready_queue->head = ((TaskList *)(ready_queue->heap->data[0]->datum))->head;
}

void refreshCurtask(ReadyQueue *ready_queue) {
	ready_queue->curtask = ready_queue->head;
	// Change the task state to Active
	ready_queue->curtask->state = Active;
}

int scheduleNextTask(ReadyQueue *ready_queue) {
	if(ready_queue->curtask != NULL) {
		moveCurrentTaskToEnd(ready_queue);
	}
	refreshCurtask(ready_queue);
	if (ready_queue->curtask == NULL) {
		return 0;
	}
	DEBUG(DB_TASK, "| TASK:\tScheduled\tTid: %d Priority: %d\n", ready_queue->curtask->tid, ready_queue->curtask->priority);
	return 1;
}

void enqueueBlockedList(BlockedList *blocked_lists, int blocked_list_index, Task* task) {
	// If no blocked list to enqueue, return
	if (blocked_lists == NULL) {
		return;
	}

	task->next = NULL;
	if (blocked_lists[blocked_list_index].head == NULL) {
		blocked_lists[blocked_list_index].head = task;
		blocked_lists[blocked_list_index].tail = task;
	} else {
		blocked_lists[blocked_list_index].tail->next = task;
		blocked_lists[blocked_list_index].tail = task;
	}
}

int dequeueBlockedList(BlockedList *blocked_lists, int blocked_list_index) {
	int ret = -1;
	Task *task = NULL;
	if (blocked_lists[blocked_list_index].head != NULL) {
		task = blocked_lists[blocked_list_index].head;
		ret = task->tid;
		blocked_lists[blocked_list_index].head = blocked_lists[blocked_list_index].head->next;
		if (blocked_lists[blocked_list_index].head == NULL) {
			blocked_lists[blocked_list_index].tail = NULL;
		}
	}
	return ret;
}

void blockCurrentTask(ReadyQueue *ready_queue, TaskState blocked_state,
                      BlockedList *blocked_lists, int blocked_list_index) {
	assert(ready_queue->curtask == ready_queue->head, "Blocking syscall invalid");
	Task *task = ready_queue->curtask;
	int priority = task->priority;

	TaskList *task_list = (TaskList *)(ready_queue->heap_nodes[priority].datum);

	// Last task in its priority queue
	if (task->next == NULL) {
		task_list->head = NULL;
		task_list->tail = NULL;
		minHeapPop(ready_queue->heap);
		// Update head task and clear curtask
		if (ready_queue->heap->heapsize > 0) {
			ready_queue->head = ((TaskList *)(ready_queue->heap->data[0]->datum))->head;
		} else {
			ready_queue->head = NULL;
		}
	}
	// Otherwise
	else {
		task_list->head = task_list->head->next;
		ready_queue->head = task_list->head;
	}

	assert(task->state == Active, "Current task state is not Active");
	task->state = blocked_state;
	ready_queue->curtask = NULL;

	enqueueBlockedList(blocked_lists, blocked_list_index, task);
}
