/**
 * @file   timer.c
 * @date   Wed Jun 23 09:41:26 2010
 * 
 * @brief  The timers which run the state machine.
 * 
 * Timers in the PTP daemon are run off of the signal system.  
 */

#include "../ptpd.h"

/* This allows subsecond timer precision without an overwhelming number of interrupts */
#define TIMERS_PER_SECOND 1     // normally 20
#define TIMER_INTERVAL (1000000.0 / TIMERS_PER_SECOND)  /* Result is in microseconds */

unsigned int elapsed;

void 
catch_alarm(int sig)
{
	elapsed++;
	DBGV("catch_alarm: elapsed %d\n", elapsed);
}

void 
initTimer(void)
{
	struct itimerval itimer;

	DBG("initTimer\n");

	signal(SIGALRM, SIG_IGN);

	elapsed = 0;
	itimer.it_value.tv_sec = itimer.it_interval.tv_sec = floor(TIMER_INTERVAL / 1000000);
	itimer.it_value.tv_usec = itimer.it_interval.tv_usec = fmod(TIMER_INTERVAL, 1000000);

	signal(SIGALRM, catch_alarm);
	setitimer(ITIMER_REAL, &itimer, 0);
}

void 
timerUpdate(IntervalTimer * itimer)
{

	int i, delta;

	DBG("timerUpdate\n");
	delta = elapsed;
	elapsed = 0;

	if (delta <= 0)
		return;

	for (i = 0; i < TIMER_ARRAY_SIZE; ++i) {
		if ((itimer[i].interval) > 0 && ((itimer[i].left) -= delta) <= 0) {
			itimer[i].left = itimer[i].interval;
			itimer[i].expire = TRUE;
			DBGV("timerUpdate: timer %u expired\n", i);
		}
	}

}

void 
timerStop(UInteger16 index, IntervalTimer * itimer)
{
	if (index >= TIMER_ARRAY_SIZE)
		return;

	itimer[index].interval = 0;
}

void 
timerStart(UInteger16 index, float interval, IntervalTimer * itimer)
{
	if (index >= TIMER_ARRAY_SIZE)
		return;

	itimer[index].expire = FALSE;
	itimer[index].left = interval * TIMERS_PER_SECOND;
	itimer[index].interval = itimer[index].left;

	DBGV("timerStart: set timer %d to %.3f\n", index, interval);
}

Boolean 
timerExpired(UInteger16 index, IntervalTimer * itimer)
{
	timerUpdate(itimer);

	if (index >= TIMER_ARRAY_SIZE)
		return FALSE;

	if (!itimer[index].expire)
		return FALSE;

	itimer[index].expire = FALSE;

	return TRUE;
}
