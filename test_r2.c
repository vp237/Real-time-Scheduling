#include "utils.h"
#include "3140_concur.h"
#include "realtime.h"

//Test case 2: This test case has 2 real time processes: process 1 with an earlier arrival time than process 2 but they have the same deadline (relative to arrival time).
//Process 1 makes the red LED blink 4 times and process 2 makes the blue LED blink 3 times.
//Process 2 will miss its deadline.
//Expected behavior: The red LED will blink 4 times first then the blue LED will blink 3 times because process 1 has an earlier arrival time. 
//Finally, the green LED will blink once which indicates that one process misses its deadline.

/*--------------------------*/
/* Parameters for test case */
/*--------------------------*/
 
/* Stack space for processes */
#define RT_STACK  80

/*--------------------------------------*/
/* Time structs for real-time processes */
/*--------------------------------------*/

/* Constants used for 'work' and 'deadline's */
realtime_t t_5sec = {5, 0};

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
	for (i=0; i<4;i++){
	LEDRed_On();
	mediumDelay();
	LEDRed_Toggle();
	mediumDelay();
	}	
}

/*-------------------
 * Real-time process 2
 *-------------------*/

void pRT2(void) {
	int i;
	for (i=0; i<3;i++){
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
    if (process_rt_create(pRT1, RT_STACK, &t_pRT1, &t_5sec) < 0) { return -1; } 
    if (process_rt_create(pRT2, RT_STACK, &t_pRT2, &t_5sec) < 0) { return -1; } 
    /* Launch concurrent execution */
		process_start();

  LED_Off();
  while(process_deadline_miss > 0) {
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
