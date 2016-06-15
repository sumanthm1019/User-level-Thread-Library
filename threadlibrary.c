/*
 * context_threads.c
 *
 *  Created on: Jun 5, 2016
 *      Author: sumanth
 */

#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>
#include <semaphore.h>
#include "threadlibrary.h"
#define THREAD_STACK 32767

#define RUNNING 1
#define RUNNABLE 0
#define COMPLETE 2
#define _EXIT 3
#define BLOCKED -1

threadQueue *tQ;
int global_id = -1;
sem_t sem_mutex;
sem_t sem_queue;
sem_t sem_cond;
tcb *base;
tcb *exit_handler;
static waitQ *wait_queue_create() {
	waitQ *q = (waitQ *) malloc(sizeof(waitQ));
	q->front = NULL;
	q->rear = NULL;
	return q;
}

static block_node *block_node_create(tcb *thread) {

	block_node *b = (block_node *) malloc(sizeof(block_node));
	b->thread = thread;
	b->next = NULL;
	return b;
}

static void wait_enqueue(waitQ *q, tcb *t) {
	block_node *temp = block_node_create(t);
	if (q->rear == NULL) {
		q->front = q->rear = temp;
		return;
	}
	q->rear->next = temp;
	q->rear = temp;

}
static block_node *wait_dequeue(waitQ *q) {
	if (q->front == NULL)
		return NULL;
	block_node *temp = q->front;
	q->front = q->front->next;
	if (q->front == NULL)
		q->rear = NULL;
	return temp;
}

static threadQueue *createQ() {
	tQ = (threadQueue *) malloc(sizeof(threadQueue));
	tQ->front = NULL;
	tQ->rear = NULL;
	return tQ;
}
static void enqueue(tcb *node, threadQueue *tQ) {
	if (tQ->front == NULL && tQ->rear == NULL) {
		tQ->front = tQ->rear = node;
		return;
	}
	tQ->rear->next = node;
	tQ->rear = node;
}

static tcb *dequeue(threadQueue *tQ) {
	tcb *temp;
	if (tQ->front == NULL)
		return NULL;
	if (tQ->front == tQ->rear) {
		temp = tQ->front;
		tQ->front = tQ->rear = NULL;
		return temp;
	}
	temp = tQ->front;
	tQ->front = tQ->front->next;
	return temp;
}
static void delete_queue(threadQueue *tQ) {
	tcb *temp;
	while (temp != NULL) {
		temp = dequeue(tQ);
		free(temp);
	}
}
static tcb *create_tcb() {
	tcb *thread = (tcb *) malloc(sizeof(tcb));
	thread->tid = global_id;
	thread->state = RUNNABLE;
	return thread;

}
static void modify_context(tcb *thread) {
	thread->context.uc_stack.ss_sp = malloc(THREAD_STACK);
	thread->context.uc_stack.ss_size = THREAD_STACK;
	thread->context.uc_stack.ss_flags = 0;
	if (thread->state == _EXIT) {
		thread->context.uc_link = &base->context;
	} else {
		thread->context.uc_link = &exit_handler->context;
	}
}
static tcb *find_current_thread() {

	tcb *temp = tQ->front;
	while (temp != NULL) {
		if (temp->state == RUNNING) {
			return temp;
		} else {
			temp = temp->next;
		}
	}
	printf("returning?\n");
	return NULL;

}
static tcb *find_thread_tid(int tid) {

	tcb *temp = tQ->front;
	while (temp != NULL) {
		if (temp->tid == tid)
			return temp;
		else
			temp = temp->next;
	}
	return NULL;

}
int thread_self() {
	tcb *temp = find_current_thread();
	return temp->tid;
}
void thread_cancel(int tid) {
	tcb *temp = find_thread_tid(tid);
	temp->state = COMPLETE;
	tcb *next = dequeue(tQ);
	while (next->state == COMPLETE) {
		free(next);
		next = dequeue(tQ);
	}
	next->state = RUNNING;
	enqueue(next, tQ);
	setcontext(&next->context);

}

void thread_yield() {
	//current thread -> runnable, enqueue
	//dequeue and check if runnable in while loop
	//swapcontext

	tcb *current = find_current_thread();
	current->state = RUNNABLE;
	tcb *next = dequeue(tQ);
	while (next->state == COMPLETE) {
		next = dequeue(tQ);
	}
	next->state = RUNNING;
	enqueue(next, tQ);
	swapcontext(&current->context, &next->context);

}
void thread_join(int tid) {
	tcb *join = find_thread_tid(tid);
	while (join->state != COMPLETE) {
		thread_yield();
	}
	return;

}

static void thread_exit_handler() {
	tcb *current = find_current_thread();
	printf("exit handler?! wtf\n");
	current->state = COMPLETE;

	current = dequeue(tQ);
	while (current->state == COMPLETE) {
		free(current);
		current = dequeue(tQ);
	}
	current->state = RUNNING;
	enqueue(current, tQ);

	makecontext(&exit_handler->context, (void (*)()) thread_exit_handler, 0);
	setcontext(&current->context);
}

static void setup_exit_handler() {
	exit_handler = create_tcb();
	exit_handler->state = _EXIT;
	exit_handler->tid = 999;
	getcontext(&exit_handler->context);
	modify_context(exit_handler);
	makecontext(&exit_handler->context, (void (*)()) thread_exit_handler, 0);
}
static void thread_scheduler() {
	/*find the running thread! enqueue the running thread
	 dequeue and check if thread is in runnable state
	 & the thread is not the same thread that has been enqueued using tid
	 make the deq thread as current*/
	sem_wait(&sem_queue);
	tcb *current = find_current_thread();
	tcb *next = dequeue(tQ);
	printf("next tid and state %d %d\n", next->tid, next->state);
	while (next->state == COMPLETE) {
		//free(next);
		next = dequeue(tQ);
	}
	if (next->tid == current->tid) {
		enqueue(next, tQ);
		return;
	}
	while (next->state == BLOCKED) {
		printf("this one is blocked? %d\n", next->tid);
		enqueue(next, tQ);
		next = dequeue(tQ);
	}

	current->state = RUNNABLE;
	next->state = RUNNING;
	enqueue(next, tQ);
	ualarm(50000, 0);
	sem_post(&sem_queue);
	printf("now will stop %d and start %d\n", current->tid, next->tid);
	swapcontext(&current->context, &next->context);

}

void thread_exit(void) {
	sem_wait(&sem_queue);
	tcb *current = find_current_thread();
	printf("who called exit\n");
	current->state = COMPLETE;
	tcb *next = dequeue(tQ);
	while (next->state == COMPLETE) {
		next = dequeue(tQ);
	}
	next->state = RUNNING;
	enqueue(next, tQ);
	sem_post(&sem_queue);
	setcontext(&next->context);
}

static void setup_alarm() {
	struct sigaction sched;
	sched.sa_flags = SA_NODEFER;
	sigemptyset(&sched.sa_mask);
	sched.sa_handler = (void *) &thread_scheduler;
	sigaction(SIGALRM, &sched, 0);
}
static void thread_init() {
	tcb *newthread = create_tcb();
	base = newthread;
	newthread->state = RUNNING;
	tQ = createQ();
	enqueue(base, tQ);
	setup_exit_handler();
	setup_alarm();
}

int thread_create(int *tid, void *(*start_routine)(void *), void *arg) {

	sem_wait(&sem_queue);
	if (global_id == -1) {
		thread_init();
	} 								//new threads created by parent
	global_id++;
	*tid = global_id;
	tcb *newthread = create_tcb();
	getcontext(&newthread->context);
	modify_context(newthread);
	makecontext(&newthread->context, (void (*)()) start_routine, 1, arg);
	base->state = RUNNABLE;
	newthread->state = RUNNING;
	enqueue(newthread, tQ);

	sem_post(&sem_queue);
	ualarm(50000, 0);
	swapcontext(&base->context, &newthread->context);
	return 0;
}
void sig_handler() {
	delete_queue(tQ);
	exit(0);
}

void mutex_init(mutex_t *m) {
	m->lock = 0;
	m->wait_queue = wait_queue_create();

	sem_init(&sem_mutex, 0, 1);
	sem_init(&sem_queue, 0, 1);
}

void mutex_lock(mutex_t *m) {
	printf("here 1\n");
	sem_wait(&sem_mutex);
	if (m->lock == 0) {
		printf("obtained lock!!\n");
		m->lock = 1;
		sem_post(&sem_mutex);
	} else {
		sem_wait(&sem_queue);
		tcb *current = find_current_thread();
		current->state = BLOCKED;
		wait_enqueue(m->wait_queue, current);
		tcb *next = dequeue(tQ);
		while (next->state != RUNNABLE) {
			enqueue(next, tQ);
			next = dequeue(tQ);
		}
		next->state = RUNNING;
		enqueue(next, tQ);
		sem_post(&sem_queue);
		sem_post(&sem_mutex);
		swapcontext(&current->context, &next->context);
	}
}

void mutex_unlock(mutex_t *m) {
	sem_wait(&sem_mutex);
	m->lock = 0;

	block_node *temp = wait_dequeue(m->wait_queue);
	if (temp != NULL) {
		sem_wait(&sem_queue);
		temp->thread->state = RUNNABLE;
		sem_post(&sem_queue);

	}
	sem_post(&sem_mutex);
}

void thread_cond_init(cond_t *c) {
	c->wait_queue = wait_queue_create();
	sem_init(&sem_cond, 0, 1);
}

void thread_cond_wait(cond_t *c, mutex_t *m) {
	sem_wait(&sem_cond);
	mutex_unlock(m);
	sem_wait(&sem_queue);
	tcb *current = find_current_thread();
	current->state = BLOCKED;
	wait_enqueue(c->wait_queue, current);
	tcb *next = dequeue(tQ);
	while (next->state != RUNNABLE) {
		enqueue(next, tQ);
		next = dequeue(tQ);
	}
	next->state = RUNNING;
	enqueue(next, tQ);
	sem_post(&sem_queue);
	sem_post(&sem_cond);
	swapcontext(&current->context, &next->context);

}

void thread_cond_signal(cond_t *c) {
	sem_wait(&sem_cond);
	block_node *temp = wait_dequeue(c->wait_queue);
	if (temp != NULL) {
		sem_wait(&sem_queue);
		temp->thread->state = RUNNABLE;
		sem_post(&sem_queue);

	}
	sem_post(&sem_cond);

}

void thread_cond_broadcast(cond_t *c){

	sem_wait(&sem_cond);

	block_node *temp = c->wait_queue->front;
	block_node *current;

	while(temp != NULL){
		current = wait_dequeue(c->wait_queue);
		sem_wait(&sem_queue);
		current->thread->state = RUNNABLE;
		sem_post(&sem_queue);
		temp  = temp->next;
	}
	sem_post(&sem_cond);

}


