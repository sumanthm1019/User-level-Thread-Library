/*
 * threadlibrary.h
 *
 *  Created on: Jun 14, 2016
 *      Author: sumanth
 */

#ifndef THREADLIBRARY_H_
#define THREADLIBRARY_H_


typedef struct thread {
	ucontext_t context;
	int tid;
	int state;
	struct thread *next;

} tcb;

typedef struct queue {
	tcb *front;
	tcb *rear;
} threadQueue;

typedef struct block_node {
	tcb *thread;
	struct block_node *next;
} block_node;

typedef struct {
	block_node *front;
	block_node *rear;
} waitQ;

typedef struct mutex {
	short int lock;
	waitQ *wait_queue;
} mutex_t;

typedef struct conditional {
	waitQ *wait_queue;
} cond_t;


static waitQ *wait_queue_create();
static block_node *block_node_create(tcb *thread);
static void wait_enqueue(waitQ *q, tcb *t);
static block_node *wait_dequeue(waitQ *q);
static threadQueue *createQ();
static void enqueue(tcb *node, threadQueue *tQ);
static tcb *dequeue(threadQueue *tQ);
static void delete_queue(threadQueue *tQ);
static tcb *create_tcb();
static void modify_context(tcb *thread);
static tcb *find_current_thread();
static tcb *find_thread_tid(int tid);
int thread_self();
void thread_cancel(int tid);
void thread_yield();
void thread_join(int tid);
static void thread_exit_handler();
static void setup_exit_handler();
static void thread_scheduler();
void thread_exit(void);
static void setup_alarm();
static void thread_init();
int thread_create(int *tid, void *(*start_routine)(void *), void *arg);
static void sig_handler();
void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);
void thread_cond_init(cond_t *c);
void thread_cond_wait(cond_t *c, mutex_t *m);
void thread_cond_signal(cond_t *c);
void thread_cond_broadcast(cond_t *c);


#endif /* THREADLIBRARY_H_ */
