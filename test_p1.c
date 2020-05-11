#include "utils.h"
#include "3140_concur.h"
#include "realtime.h"

//Test case 1: This test case has 2 periodic real time processes with the same period of 10 seconds: process 1 makes the blue LED blink 3 times for every period
//and has an earlier start time and process 2 makes the red LED blink 3 times for every period.
//Expected behavior: The expected behavior was that the blue and red LEDs would alternate blinking, each with a period of 10 seconds, and this pattern would repeat forever.

/*--------------------------*/
/* Parameters for test case */
/*--------------------------*/

/* Stack space for processes */
#define RT_STACK  80



/*--------------------------------------*/
/* Time structs for real-time processes */
/*--------------------------------------*/

/* Constants used for 'work' and 'deadline's */
realtime_t t_10sec = {10, 0}; //Deadline of the periodic processes
realtime_t t_period = {10, 0}; //Time period of the periodic processes
realtime_t t_pRT1 = {0, 1}; //Initial start time of the first Periodic Process
realtime_t t_pRT2 = {1, 0}; //Initial start time of the second Periodic Process

/*------------------*/
/* Helper functions */
/*------------------*/
void shortDelay(){delay();}
void mediumDelay() {delay(); delay();}


void pRT1(void) {
	int i;
	for (i=0; i<3;i++){
	LEDBlue_On();
	mediumDelay();
	LEDBlue_Toggle();
	mediumDelay();
	}
}

void pRT2(void) {
	int i;
	for (i=0; i<3;i++){
	LEDRed_On();
	mediumDelay();
	LEDRed_Toggle();
	mediumDelay();
	}
}

/* Main function */
int main(void) {

	LED_Initialize();

	/* Create process (real_time and periodic) */
	if(process_rt_periodic(pRT1, RT_STACK, &t_pRT1, &t_10sec, &t_period) < 0){ 
		return -1; 
	}
	if(process_rt_periodic(pRT2, RT_STACK, &t_pRT2, &t_10sec, &t_period) < 0){ 
		return -1; 
	}
	
	/* Launch concurrent execution */
	process_start();
	
	LED_Off();
	mediumDelay();
	LEDGreen_On();
  
	/* Hang out in infinite loop (so we can inspect variables if we want) */
	while (1);
	return 0;
}
