/*
 * sys_arch.c
 *
 *  Created on: Dec 19, 2012
 *      Author: kaige
 */

/*sys_arch interface for lwIP 0.6++

 Author: Adam Dunkels

 The operating system emulation layer provides a common interface
 between the lwIP code and the underlying operating system kernel. The
 general idea is that porting lwIP to new architectures requires only
 small changes to a few header files and a new sys_arch
 implementation. It is also possible to do a sys_arch implementation
 that does not rely on any underlying operating system.

 The sys_arch provides semaphores and mailboxes to lwIP. For the full
 lwIP functionality, multiple threads support can be implemented in the
 sys_arch, but this is not required for the basic lwIP
 functionality. Previous versions of lwIP required the sys_arch to
 implement timer scheduling as well but as of lwIP 0.5 this is
 implemented in a higher layer.

 In addition to the source file providing the functionality of sys_arch,
 the OS emulation layer must provide several header files defining
 macros used throughout lwip.  The files required and the macros they
 must define are listed below the sys_arch description.

 Semaphores can be either counting or binary - lwIP works with both
 kinds. Mailboxes are used for message passing and can be implemented
 either as a queue which allows multiple messages to be posted to a
 mailbox, or as a rendez-vous point where only one message can be
 posted at a time. lwIP works with both kinds, but the former type will
 be more efficient. A message in a mailbox is just a pointer, nothing
 more.

 Semaphores are represented by the type "sys_sem_t" which is typedef'd
 in the sys_arch.h file. Mailboxes are equivalently represented by the
 type "sys_mbox_t". lwIP does not place any restrictions on how
 sys_sem_t or sys_mbox_t are represented internally.

 */

#include "arch/sys_arch.h"
#include "lwipopts.h"
#include "ucos_ii.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/stats.h"
#include <string.h>

#include "arch/cc.h"

struct sys_sem{
	OS_EVENT *ossem;
	void* next;
};

struct sys_mbox{
	OS_EVENT *osmbox;
	uint32_t mboxBuffer[LWIP_SYS_ARCH_MBOX_SIZE];
	void* next;
};

struct sys_thread{
	uint8_t prio;
};

struct sys_mbox sys_arch_mbox_space[LWIP_SYS_ARCH_NUMBER_OF_MBOXES];
struct sys_mbox *sys_arch_mbox_list;

OS_STK sys_arch_stack_space[LWIP_SYS_ARCH_TOTAL_STACK_SIZE];
uint32_t sys_arch_stack_index = 0;

struct sys_sem sys_arch_sem_space[LWIP_SYS_ARCH_NUMBER_OF_SEMAPHORES];
struct sys_sem *sys_arch_sem_list;


/*******************************************************************************************************/
/* System																											    */
/*******************************************************************************************************/
/*
 * --void sys_init(void) --
 *
 * lwIP system initialization. This function is called before the any other sys_arch-function is called
 * and is meant to be used to initialize anything that has to be up and running for the rest of the functions to work.
 * for example to set up a pool of semaphores.
 */
void sys_init(void)
{
	uint8_t err = OS_ERR_NONE;
	int i = 0;

	sys_arch_mbox_list = sys_arch_mbox_space;
	for(i = 0; i < LWIP_SYS_ARCH_NUMBER_OF_MBOXES-1; i++){
		sys_arch_mbox_space[i].next = &sys_arch_mbox_space[i+1];
		memset(sys_arch_mbox_space[i].mboxBuffer,0,sizeof(uint32_t)*LWIP_SYS_ARCH_MBOX_SIZE);
	}
	sys_arch_mbox_space[LWIP_SYS_ARCH_NUMBER_OF_MBOXES - 1].next = NULL;

	memset(sys_arch_stack_space,0,sizeof(sys_arch_stack_space));
	sys_arch_stack_index = 0;

	sys_arch_sem_list = sys_arch_sem_space;
	for(i = 0; i < LWIP_SYS_ARCH_NUMBER_OF_SEMAPHORES-1; i++){
		sys_arch_sem_space[i].next = &sys_arch_sem_space[i+1];
	}
	sys_arch_sem_space[LWIP_SYS_ARCH_NUMBER_OF_SEMAPHORES - 1].next = NULL;

#if LWIP_STATS > 0 && SYS_STATS > 0
	memset(&lwip_stats.sys,0,sizeof(struct stats_sys));
#endif

	LWIP_DEBUGF(SYS_DEBUG, ("sys_init done\n"));
}
/*
 * -- uint32_t sys_now(void) --
 *
 * Returns the System-Time in Milliseconds
 */
u32_t sys_now(void)
{
	return OSTimeGet() / (OS_TICKS_PER_SEC / 1000);
}

/*******************************************************************************************************/
/* Mailboxes																											*/
/*
 * Semaphores can be either counting or binary - lwIP works with both kinds.
 * Semaphores are represented by the type sys_sem_t which is typedef'd in the sys_arch.h file.
 * lwIP does not place any restrictions on how sys_sem_t should be defined or represented internally,
 * but typically it is a pointer to an operating system semaphore or a struct wrapper
 * for an operating system semaphore.
 * Warning! Semaphores are used in lwip's memory-management functions (mem_malloc(), mem_free(), etc.)
 * and can thus not use these functions to create and destroy them.
 */
/*******************************************************************************************************/

/*
 * -- err_t sys_sem_new(sys_sem_t *sem, u8_t count) --
 *
 * Creates a new semaphore returns it through the sem pointer provided as argument to the function,
 * in addition the function returns ERR_MEM if a new semaphore could not be created
 * and ERR_OK if the semaphore was created.
 * The count argument specifies the initial state of the semaphore.
 */
err_t sys_sem_new(struct sys_sem **sem, u8_t count)
{
	struct sys_sem *semaphore;
	uint8_t err;
	CPU_SR cpu_sr;

	if(sys_arch_sem_list == NULL){
		SYS_STATS_INC(sem.err);
		LWIP_DEBUGF(SYS_DEBUG, ("SYS_ARCH: sys_mbox_new failed: Out of Semaphores"));
		return ERR_MEM;
	}
	OS_ENTER_CRITICAL();
	semaphore = sys_arch_sem_list;
	sys_arch_sem_list = semaphore->next;
	semaphore->next = NULL;
	OS_EXIT_CRITICAL();
	semaphore->ossem = OSSemCreate(count);
	if (semaphore->ossem == NULL){
		SYS_STATS_INC(sem.err);
		LWIP_DEBUGF(SYS_DEBUG, ("SYS_ARCH: sys_mbox_new failed: Out of Semaphores (UCOS)"));
		return ERR_MEM;
	}

	OSEventNameSet(semaphore->ossem,"lwip sysarch sem",&err);

	SYS_STATS_INC_USED(sem);

	*sem = semaphore;
	LWIP_DEBUGF(SYS_DEBUG, ("SYS_ARCH: sys_sem_new 0x%08x\n", semaphore->ossem));
	return ERR_OK;
}

/*
 * 	-- void sys_sem_free(sys_sem_t *sem) --
 *
 * 	Frees a semaphore created by sys_sem_new.
 * 	Since these two functions provide the entry and exit point for all semaphores used by lwIP,
 * 	you have great flexibility in how these are allocated and deallocated
 * 	(for example, from the heap, a memory pool, a semaphore pool, etc).
 */
void sys_sem_free(struct sys_sem **sem)
{
	uint8_t err;
	CPU_SR cpu_sr;
	struct sys_sem *semaphore = *sem;
	OSSemDel(semaphore->ossem, OS_DEL_ALWAYS, &err);

	OS_ENTER_CRITICAL();
	struct sys_sem *sys_arch_sem_list_tmp = sys_arch_sem_list;
	sys_arch_sem_list = semaphore;
	sys_arch_sem_list->next = sys_arch_sem_list_tmp;
	OS_EXIT_CRITICAL();

	SYS_STATS_DEC(sem.used);

	LWIP_DEBUGF(SYS_DEBUG, ("SYS_ARCH: sys_sem_free 0x%08x\n", semaphore->ossem));
}

/*
 * -- void sys_sem_signal(sys_sem_t *sem) --
 *
 * Signals (or releases) a semaphore referenced by * sem.
 */
void sys_sem_signal(struct sys_sem **sem)
{
	struct sys_sem *semaphore = *sem;
	OSSemPost(semaphore->ossem);
}

/*
 * -- u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout) --
 *
 * Blocks the thread while waiting for the semaphore to be signaled.
 * The timeout parameter specifies how many milliseconds the function should block before returning;
 * if the function times out, it should return SYS_ARCH_TIMEOUT.
 * If timeout=0, then the function should block indefinitely.
 * If the function acquires the semaphore, it should return how many milliseconds expired while waiting for the semaphore.
 * The function may return 0 if the semaphore was immediately available.
 */
u32_t sys_arch_sem_wait(struct sys_sem **sem, u32_t timeout)
{
	struct sys_sem *semaphore = *sem;
	uint32_t startTime = sys_now();
	uint8_t err;
	uint32_t ticks = timeout * (OS_TICKS_PER_SEC/1000);
	if(ticks > 0xFFFF)
		ticks = 0xFFFF;
	OSSemPend(semaphore->ossem, (uint16_t)ticks, &err);
	if (err == OS_ERR_NONE)
		return sys_now() - startTime;
	else{
		return SYS_ARCH_TIMEOUT;
	}

}

/*******************************************************************************************************/
/* Mailboxes																											*/
/*
 * Mailboxes are used for message passing and can be implemented either as a queue
 * which allows multiple messages to be posted to a mailbox,
 * or as a rendez-vous point where only one message can be posted at a time.
 * lwIP works with both kinds, but the former type will be more efficient.
 * A message in a mailbox is just a pointer, nothing more.
 * Mailboxes are equivalently represented by the type sys_mbox_t.
 * lwIP does not place any restrictions on how sys_mbox_t is represented internally,
 * but it is typically a structure pointer type to a structure
 * that wraps the OS-native mailbox type and its queue buffer.
 *
 */
/*******************************************************************************************************/

/*
 * -- err_t sys_mbox_new(sys_mbox_t * mbox, int size) --
 *
 * Trys to create a new mailbox and return it via the mbox pointer provided as argument to the function.
 * Returns ERR_OK if a mailbox was created and ERR_MEM if the mailbox on error.
 */
err_t sys_mbox_new(struct sys_mbox **mbox, int size)
{
	void *mem;
	uint8_t err = OS_ERR_NONE;
	struct sys_mbox *messageBox;
	CPU_SR cpu_sr;

	if(size > LWIP_SYS_ARCH_MBOX_SIZE){
		SYS_STATS_INC(mbox.err);
		return ERR_MEM;
	}

	if(sys_arch_mbox_list == NULL){
		SYS_STATS_INC(mbox.err);
		LWIP_DEBUGF(SYS_DEBUG, ("SYS_ARCH: sys_mbox_new failed: Out of Message Boxes"));
		return ERR_MEM;
	}

	OS_ENTER_CRITICAL();
	messageBox = sys_arch_mbox_list;
	sys_arch_mbox_list = messageBox->next;
	messageBox->next = NULL;
	OS_EXIT_CRITICAL();

	messageBox->osmbox = OSQCreate((void*) messageBox->mboxBuffer, size);
	if (messageBox->osmbox == NULL){
		SYS_STATS_INC(mbox.err);
		LWIP_DEBUGF(SYS_DEBUG, ("SYS_ARCH: sys_mbox_new failed: Out of Message Boxes (UCOS)"));
		return ERR_MEM;
	}
	OSEventNameSet(messageBox->osmbox,"lwip sysarch mbox",&err);

	SYS_STATS_INC_USED(mbox);

	*mbox = messageBox;
	LWIP_DEBUGF(SYS_DEBUG, ("SYS_ARCH: sys_mbox_new 0x%08x\n", messageBox->osmbox));
	return ERR_OK;
}

/*
 * -- void sys_mbox_free(sys_mbox_t * mbox) --
 *
 * Deallocates a mailbox.
 * If there are messages still present in the mailbox when the mailbox is deallocated,
 * it is an indication of a programming error in lwIP and the developer should be notified.
 */
void sys_mbox_free(struct sys_mbox ** mbox)
{
	uint8_t err;
	CPU_SR cpu_sr;
	struct sys_mbox *messageBox = * mbox;
	OS_Q * event_p = messageBox->osmbox->OSEventPtr;
	void *mem = (void*) event_p->OSQStart;
	OSQDel(messageBox->osmbox, OS_DEL_ALWAYS, &err);

	OS_ENTER_CRITICAL();
	struct sys_mbox *sys_arch_mbox_list_tmp = sys_arch_mbox_list;
	sys_arch_mbox_list = messageBox;
	sys_arch_mbox_list->next = sys_arch_mbox_list_tmp;
	OS_EXIT_CRITICAL();

	SYS_STATS_DEC(mbox.used);

	LWIP_DEBUGF(SYS_DEBUG, ("SYS_ARCH: sys_mbox_free 0x%08x\n", messageBox->osmbox));
}

/*
 * -- void sys_mbox_post(sys_mbox_t * mbox, void *msg) --
 *
 * Posts the "msg" to the mailbox
 */
void sys_mbox_post(struct sys_mbox ** mbox, void *msg)
{
	struct sys_mbox *messageBox = * mbox;
	OSQPost(messageBox->osmbox, msg);
}

/*
 * -- u32_t sys_arch_mbox_fetch(sys_mbox_t * mbox, void **msg, u32_t timeout) --
 *
 * Blocks the thread until a message arrives in the mailbox,
 * but does not block the thread longer than timeout milliseconds (similar to the sys_arch_sem_wait() function).
 * The msg argument is a pointer to the message in the mailbox and may be NULL to indicate that the message should be dropped.
 * This should return either SYS_ARCH_TIMEOUT or the number of milliseconds elapsed waiting for a message.
 */
u32_t sys_arch_mbox_fetch(struct sys_mbox ** mbox, void **msg, u32_t timeout)
{
	struct sys_mbox *messageBox = * mbox;
	uint32_t startTime = sys_now();
	uint8_t err;
	uint32_t ticks = (timeout * OS_TICKS_PER_SEC)/1000;
	if(ticks > 0xFFFF)
		ticks = 0xFFFF;
	*msg = OSQPend(messageBox->osmbox, (uint16_t)ticks, &err);
	if (err == OS_ERR_NONE)
		return sys_now() - startTime;
	else{
		return SYS_ARCH_TIMEOUT;
	}
}

/*
 * -- u32_t sys_arch_mbox_tryfetch(sys_mbox_t * mbox, void **msg) --
 *
 * This is similar to sys_arch_mbox_fetch, however if a message is not present in the mailbox,
 * it immediately returns with the code SYS_MBOX_EMPTY.
 * On success 0 is returned with msg pointing to the message retrieved from the mailbox.
 * If your implementation cannot support this functionality,
 * a simple workaround (but inefficient) is #define sys_arch_mbox_tryfetch(mbox,msg) sys_arch_mbox_fetch(mbox,msg,1).
 */
u32_t sys_arch_mbox_tryfetch(struct sys_mbox ** mbox, void **msg)
{
	uint8_t err;
	struct sys_mbox *messageBox = * mbox;
	*msg = OSQAccept(messageBox->osmbox, &err);
	if (err == OS_ERR_NONE)
		return 0;
	else
		return SYS_MBOX_EMPTY;
}

/*
 * -- err_t sys_mbox_trypost(sys_mbox_t * mbox, void *msg) --
 *
 * Tries to post a message to mbox by polling (no timeout).
 * The function returns ERR_OK on success and ERR_MEM if it can't post at the moment.
 *
 */
err_t sys_mbox_trypost(struct sys_mbox ** mbox, void *msg)
{
	struct sys_mbox *messageBox = * mbox;
	int err = OSQPost(messageBox->osmbox, msg);
	if(err == OS_ERR_NONE)
		return ERR_OK;
	else {
		return ERR_MEM;
	}
}

/*******************************************************************************************************/
/* Threads																											*/
/*
 * Threads are not required for lwIP, although lwIP is written to use them efficiently.
 * The following function will be used to instantiate a thread for lwIP
 *
 */
/*******************************************************************************************************/

/*
 * -- sys_thread_t sys_thread_new(char *name, void (* thread)(void *arg), void *arg, int stacksize, int prio) --
 *
 * name is the thread name. thread(arg) is the call made as the thread's entry point.
 * stacksize is the recommanded stack size for this thread. -> stacksize is in sizeof(OS_STCK) * Bytes
 * prio is the priority that lwIP asks for.
 * Stack size(s) and priority(ies) have to be are defined in lwipopts.h,
 * and so are completely customizable for your system.
 */
struct sys_thread *sys_thread_new(const char *name, void(* thread)(void *arg), void *arg, int stacksize, int prio)
{
	OS_STK *taskStack;
	uint8_t err;
	struct sys_thread *sysThread;

	if (prio == 0 || prio >= OS_TASK_IDLE_PRIO){
		LWIP_DEBUGF(SYS_DEBUG, ("SYS_ARCH: sys_thread_new failed: Invalid Prio 0x%08x\n", prio));
		return NULL;
	}

	if(LWIP_SYS_ARCH_TOTAL_STACK_SIZE - sys_arch_stack_index <= stacksize){
		LWIP_DEBUGF(SYS_DEBUG, ("SYS_ARCH: sys_thread_new failed: Not enough Memory 0x%08x <= 0x%08x\n", LWIP_SYS_ARCH_TOTAL_STACK_SIZE - sys_arch_stack_index, stacksize));
		return NULL;
	}

	taskStack = &sys_arch_stack_space[sys_arch_stack_index];
	sys_arch_stack_index += stacksize;

	//Create Task
	err = OSTaskCreateExt(thread, arg, &taskStack[stacksize - 1], prio, prio, &taskStack[0], stacksize, (void *) 0, OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
	if(err != OS_ERR_NONE){
		LWIP_DEBUGF(SYS_DEBUG, ("SYS_ARCH: sys_thread_new failed: Task create failed with error 0x%08x\n", err));
		return NULL;
	}

#if (OS_TASK_NAME_EN > 0)
    OSTaskNameSet(prio,(INT8U *)name,&err);
#endif
    sysThread->prio = prio;
	if (err == OS_ERR_NONE) {
		LWIP_DEBUGF(SYS_DEBUG, ("SYS_ARCH: sys_thread_new: Created Task %s Prio:0x%08x\n", name,prio));
		return sysThread;
	} else {
		LWIP_DEBUGF(SYS_DEBUG, ("SYS_ARCH: sys_thread_new failed: Task name set failed with error 0x%08x\n", err));
		return NULL;
	}
}

/*
 * This optional function does a "fast" critical region protection and returns
 the previous protection level. This function is only called during very short
 critical regions. An embedded system which supports ISR-based drivers might
 want to implement this function by disabling interrupts. Task-based systems
 might want to implement this by using a mutex or disabling tasking. This
 function should support recursive calls from the same task or interrupt. In
 other words, sys_arch_protect() could be called while already protected. In
 that case the return value indicates that it is already protected.

 sys_arch_protect() is only required if your port is supporting an operating
 system.
 */

sys_prot_t sys_arch_protect(void) {
#if OS_CRITICAL_METHOD == 3                            /* Allocate storage for CPU status register     */
    OS_CPU_SR  cpu_sr = 0;
#endif
	OS_ENTER_CRITICAL();
	return cpu_sr;
}

/*
 * This optional function does a "fast" set of critical region protection to the
 value specified by pval. See the documentation for sys_arch_protect() for
 more information. This function is only required if your port is supporting
 an operating system.
 */
void sys_arch_unprotect(sys_prot_t cpu_sr) {
	OS_EXIT_CRITICAL();
}
