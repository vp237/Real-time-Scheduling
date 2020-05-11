#include "utils.h"
#include "3140_concur.h"
#include "realtime.h"

//Test case 1: This test case has 2 real time processes: process 1 with an earlier arrival time but has a later deadline (relative to arrival time)
//compared to process 2 which has a later arrival time but an earlier deadline (relative to arrival time).
//Process 1 makes the red LED blink 20 times and process 2 makes the blue LED blink 10 times. 
//Both processes will miss their deadlines.
//Expected behavior: The red LED will blink 2 times first then the blue LED will blink 10 times because process 2 pre-empts process 1 as it has an earlier deadline. 
//After that, the red LED will blink the rest of the 18 times.
//Finally, the green LED will blink twice which indicates that two processes miss their deadlines.

/*--------------------------*/ 
/* Parameters for test case */
/*--------------------------*/
 
/* Stack space for processes */
#define RT_STACK  80 

/*--------------------------------------*/
/* Time structs for real-time processes */
/*--------------------------------------*/

/* Constants used for 'work' and 'deadline's */
realtime_t t_2sec = {2, 0};
realtime_t t_10sec = {10, 0};

/* Process start time */
realtime_t t_pRT1 = {0, 1};
realtime_t t_pRT2 = {1, 0};
 
/*------------------*/
/* Helper functions */
/*------------------*/
void shortDelay(){delay();}
void mediumDelay() {delay(); delay();}


/*-------------------
 * Real-time process 1
 *-------------------*/
 
void pRT1(void) {
	int i;
	for (i=0; i<20;i++){
	LEDRed_On();
	shortDelay();
	LEDRed_Toggle();
	shortDelay();
	}	
}

/*-------------------
 * Real-time process 2
 *-------------------*/

void pRT2(void) {
	int i;
	for (i=0; i<10;i++){
	LEDBlue_On();
	mediumDelay();
	LEDBlue_Toggle();
	mediumDelay();
	}
}

/*--------------------------------------------*/
/* Main function - start concurrent execution */
/*--------------------------------------------*/
int main(void) {	
	 
	LED_Initialize();

    /* Create processes */ 
    if (process_rt_create(pRT1, RT_STACK, &t_pRT1, &t_10sec) < 0) { return -1; } 
    if (process_rt_create(pRT2, RT_STACK, &t_pRT2, &t_2sec) < 0) { return -1; } 
    /* Launch concurrent execution */
		process_start();

  LED_Off();
  while(process_deadline_miss>0) {
		LEDGreen_On();
		shortDelay();
		LED_Off();
		shortDelay();
		process_deadline_miss--;
	}
	
	/* Hang out in infinite loop (so we can inspect variables if we want) */ 
	while (1);
	return 0;
}
