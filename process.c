#include "3140_concur.h"
#include <stdlib.h>
#include <MK64F12.h>
#include "realtime.h"

/* Struct for the process */

typedef struct process_state {
	unsigned int stack_size;  /* the size of the stack */   
	unsigned int * sp;  /* the stack pointer for the process */  
	unsigned int * original_sp; /* the original stack pointer for the process */
	unsigned int pc; /* the PC of the process */ 
	struct process_state * next;   /* the next process */
	int is_realtime; /* whether this is a real time process */
	int is_periodic; /*whether this is a periodic process */
	realtime_t arrival_time; /* the arrival time of the process */
	realtime_t deadline; /* the deadline of the process */
	realtime_t period; /* the period of the process */
} process_t ;

/* Helper functions (implementations at the bottom) */

void add_process_queue(process_t * next_process);

process_t * remove_process_queue(void);

void add_not_ready_queue(process_t * next_process);

process_t * remove_not_ready_queue(void);

void add_ready_queue(process_t * next_process);

process_t * remove_ready_queue(void);

/* Global variables */

process_t * current_process = NULL; /* The currently running process */

process_t * process_queue = NULL; /* The queue for normal (non-real time) processes */

process_t * ready_queue = NULL; /* The queue for all real time processes that are ready (sorted by deadline) */

process_t * not_ready_queue = NULL; /* The queue for all real time processes that are not ready (sorted by arrival time) */

int process_deadline_met; /* The number of processes that have terminated before their deadlines */

int process_deadline_miss; /* The number of processes that have terminated after their deadlines */

realtime_t current_time; /* The current time */

/* Creates a non-real time process */

int process_create(void (* f)(void), int n){
	process_t * state = malloc(sizeof(process_t)); //Allocates memory for process
	if (state == NULL) {
		return -1;
	}
	unsigned int * stateOfProcess = process_stack_init(f, n); //State of process
	if (stateOfProcess == NULL) {
		free(state);
		return -1;
	}
	else {
		state->sp = stateOfProcess;
		state->original_sp = stateOfProcess;
		state->pc = (unsigned int) f;
		state->next = NULL;
		state->stack_size = n;
		state->is_realtime = 0;
		state->is_periodic = 0;
		state->arrival_time.sec = 0;
		state->arrival_time.msec = 0;
		state->deadline.sec = 0;
		state->deadline.msec = 0;
		state->period.sec = 0;
		state->period.msec = 0;
		add_process_queue(state);
		return NULL;
	}
}	

/* Creates a real time process */

int process_rt_create(void (* f) (void), int n, realtime_t * start, realtime_t * deadline) {
	process_t * state = malloc(sizeof(process_t)); //Allocates memory for process
	if (state == NULL) {
		return -1;
	}
	unsigned int * stateOfProcess = process_stack_init(f, n); //State of process
	if (stateOfProcess == NULL) {
		free(state);
		return -1;
	}
	else {
		state->sp = stateOfProcess;
		state->original_sp = stateOfProcess;
		state->pc = (unsigned int) f;
		state->next = NULL;
		state->stack_size = n;
		state->is_realtime = 1;
		state->is_periodic = 0;
		state->arrival_time = (* start); //start is in absolute time
		//Converts deadline to absolute time because deadline is only relative to start
		state->deadline.sec = start->sec + deadline->sec;
		state->deadline.msec = start->msec + deadline->msec;
		//Makes sure that msec is less than 1000 after converting the deadline
		if(state->deadline.msec >= 1000) {
			state->deadline.sec += state->deadline.msec / 1000;
			state->deadline.msec = state->deadline.msec % 1000;
		}	
		state->period.sec = 0; 
		state->period.msec = 0;
		add_not_ready_queue(state);
		return NULL;
	}	
}	

/* Creates a real time periodic process */

int process_rt_periodic(void (* f)(void), int n, realtime_t *start, realtime_t * deadline, realtime_t * period) {
	process_t * state = malloc(sizeof(process_t)); //Allocates memory for process
	if (state == NULL) {
		return -1;
	}
	unsigned int * stateOfProcess = process_stack_init(f, n); //State of process
	if (stateOfProcess == NULL) {
		free(state);
		return -1;
	}
	else {
		state->sp = stateOfProcess;
		state->original_sp = stateOfProcess;
		state->pc = (unsigned int) f;
		state->next = NULL;
		state->stack_size = n;
		state->is_realtime = 1;
		state->is_periodic = 1;
		state->arrival_time = (* start); //start is in absolute time
		//Converts deadline to absolute time because deadline is only relative to start
		state->deadline.msec = start->msec + deadline->msec;
		state->deadline.sec = start->sec + deadline->sec;
		//Makes sure that msec is less than 1000 after converting the deadline
		if(state->deadline.msec >= 1000) {
			state->deadline.sec += state->deadline.msec / 1000;
			state->deadline.msec = state->deadline.msec % 1000;
		}	
		state->period = (* period);
		add_not_ready_queue(state);
		return NULL;
	}	
}	

/* Reinitializes the stack and all the necessary contents in the stack (or the process will crash) */

void process_stack_reinit(process_t * process) {
	unsigned int * sp = process->original_sp;	//Original pointer to process stack (allocated in heap) 
	sp[17] = 0x01000000; //xPSR
  sp[16] = (unsigned int) process->pc; //PC
	sp[15] = (unsigned int) process_terminated; //LR
	sp[9] = 0xFFFFFFF9; //EXC_RETURN value, returns to thread mode
	sp[0] = 0x3; //Enables scheduling timer and interrupt
	process->sp = process->original_sp; 
}	

/* Starts up the concurrent execution */

void process_start(void) {
	SIM->SCGC6 |= SIM_SCGC6_PIT_MASK;
	PIT_MCR = 00 << 0;
	PIT_LDVAL0 = SystemCoreClock/100;
	PIT_LDVAL1 = SystemCoreClock/1000;
	//Setting up priority for interrupts
	NVIC_SetPriority(SVCall_IRQn, 1);
	NVIC_SetPriority(PIT0_IRQn, 1);
	NVIC_SetPriority(PIT1_IRQn, 0); //Highest priority
	PIT->CHANNEL[1].TCTRL |= 3;
	//Enabling interrupts
	NVIC_EnableIRQ(PIT0_IRQn);
	NVIC_EnableIRQ(PIT1_IRQn);
	process_begin();
}	

/* Selects which process to run next */

unsigned int * process_select(unsigned int * cursp) {
	while ((not_ready_queue != NULL) && (current_time.sec * 1000 + current_time.msec > not_ready_queue->arrival_time.sec * 1000 + not_ready_queue->arrival_time.msec)) { //Check whether unready processes in the queue become ready or not (current time is greater than start time: ready)
		process_t * ready = remove_not_ready_queue();
		add_ready_queue(ready);
	}
	if (cursp == NULL) { 
		if (current_process != NULL) { //If there is a current process and it is done running
			if (current_process->is_realtime) {
				if (current_time.sec * 1000 + current_time.msec <= current_process->deadline.sec * 1000 + current_process->deadline.msec) { //Checks whether current process misses deadline
					process_deadline_met += 1; //Updates number of processes that met the deadline
				}
				else {
					process_deadline_miss += 1; //Updates number of processes that missed the deadline
				}
			}
			if (current_process->is_periodic) { //If the current process is periodic
				process_stack_reinit(current_process);
				//Updates arrival time and deadline with the period
				current_process->arrival_time.sec += current_process->period.sec;
				current_process->arrival_time.msec += current_process->period.msec; 
				current_process->deadline.sec += current_process->deadline.sec;
				current_process->deadline.msec += current_process->deadline.msec;
				//Makes sure that msec is less than 1000 for the current process's arrival time
				if(current_process->arrival_time.msec >= 1000) {
					current_process->arrival_time.sec += current_process->arrival_time.msec / 1000;
					current_process->arrival_time.msec = current_process->arrival_time.msec % 1000;
				}	
				//Makes sure that msec is less than 1000 for the current process's deadline
				if(current_process->deadline.msec >= 1000) {
					current_process->deadline.sec += current_process->deadline.msec / 1000;
					current_process->deadline.msec = current_process->deadline.msec % 1000;
				}	
				if (current_time.sec * 1000 + current_time.msec > current_process->arrival_time.sec * 1000 + current_process->arrival_time.msec) { //Check whether the current process becomes ready or not
					add_ready_queue(current_process);
				}	
				else {
					add_not_ready_queue(current_process);
				}	
			}
			else {
				process_stack_free(current_process->original_sp, current_process->stack_size); //Frees the process
				free(current_process); //Frees the process as it is done running (for non-periodic processes only)
			}
		}
	}
	else { //The current process is not done running
		current_process->sp = cursp;
		if (current_process->is_realtime) {
			add_ready_queue(current_process); //Adds to real time ready queue
		}
		else {
			add_process_queue(current_process); //Adds to normal non-real time queue
		}
	}
	if (ready_queue != NULL) { //If there are processes in the ready queue (real time processes)
		current_process = remove_ready_queue();
	}	
	else if (process_queue != NULL) { //Else if there are processes in the process queue (non-real time processes)
		current_process = remove_process_queue();
	}	
	else if (not_ready_queue != NULL) {//Else if there are processes in the not ready queue (real time processes)
		__enable_irq(); //Enables interrupt or the process will never become ready
		while (not_ready_queue->arrival_time.sec * 1000 + not_ready_queue->arrival_time.msec > current_time.sec * 1000 + current_time.msec) {} //Busy waits until the process becomes ready
		__disable_irq(); //Disables interrupt		
		current_process = remove_not_ready_queue();	
	}
	else {//There are no processes left
		current_process = NULL;
	}
	if (current_process == NULL) { //If there is no current process running
		return NULL;
  }
	else {
		return current_process->sp;
	}
}

/* Interrupt handler for PIT1 to generate interrupts every millisecond */

void PIT1_IRQHandler (void) {
	NVIC_DisableIRQ(PIT1_IRQn);
	current_time.msec += 1;
	if (current_time.msec >= 1000) {
		current_time.sec += 1;
		current_time.msec = 0;
	}	
	PIT_TFLG1 = 1 << 0; //Resets the flag	
	NVIC_EnableIRQ(PIT1_IRQn);
}
			
/* Helper functions */

/* Adds process to the end of the process queue */

void add_process_queue(process_t * next_process) {
	if (process_queue == NULL) {
		process_queue = next_process;
		next_process->next = NULL;
	}
	else {
		process_t * tmp = process_queue;
		while (tmp->next != NULL) {
			tmp = tmp->next;
		}
		tmp->next = next_process;
		next_process->next = NULL;
	}	
}	

/* Removes the first process in the process queue and return the first process of the queue after the removal */

process_t * remove_process_queue(void) {
	if (process_queue == NULL) {
		return NULL;
	}
	else {
		process_t * temp = process_queue;
		process_queue = process_queue->next;
		temp->next = NULL;
		return temp;
  }
}	

/* Adds process to the not ready queue (sorted by arrival time) */

void add_not_ready_queue(process_t * next_process) {
	if (not_ready_queue == NULL) {
		not_ready_queue = next_process;
		next_process->next = NULL;
	}
	else {
		process_t * before = NULL;
		process_t * after = not_ready_queue;
		while ((after != NULL) && (next_process->arrival_time.sec * 1000 + next_process->arrival_time.msec >= after->arrival_time.sec * 1000 + after->arrival_time.msec)) {
			before = after;
			after = after->next;
		}
		next_process->next = after;
		if (before != NULL) {
			before->next = next_process;
		}
		else {
			process_t * tmp = not_ready_queue;
			not_ready_queue = next_process;
			not_ready_queue->next = tmp;
		}	
	}	
}	

/* Removes the first process in the not ready queue and return the first process of the queue after the removal */

process_t * remove_not_ready_queue(void) {
	if (not_ready_queue == NULL) {
		return NULL;
	}
	else {
		process_t * temp = not_ready_queue;
		not_ready_queue = not_ready_queue->next;
		temp->next = NULL;
		return temp;
  }
}

/* Adds process to the ready queue (sorted by deadline) */

void add_ready_queue(process_t * next_process) {
	if (ready_queue == NULL) {
		ready_queue = next_process;
		next_process->next = NULL;
	}
	else {
		process_t * before = NULL;
		process_t * after = ready_queue;
		while ((after != NULL) && (next_process->deadline.sec * 1000 + next_process->deadline.msec >= after->deadline.sec * 1000 + after->deadline.msec)) {
			before = after;
			after = after->next;
		}
		next_process->next = after;
		if (before != NULL) {
			before->next = next_process;
		}
		else {
			process_t * tmp = ready_queue;
			ready_queue = next_process;
			ready_queue->next = tmp;
		}	
	}	
}	

/* Removes the first process in the ready queue and return the first process of the queue after the removal */

process_t * remove_ready_queue(void) {
	if (ready_queue == NULL) {
		return NULL;
	}
	else {
		process_t * temp = ready_queue;
		ready_queue = ready_queue->next;
		temp->next = NULL;
		return temp;
  }
}

