/**
 * @file phc.c
 *
 * Implements the timer functions using the Linux PTP Hardware Clock API.
 */

#include <linux/ptp_clock.h>

#include "../ptpd.h"

#ifndef ADJ_NANO
#define ADJ_NANO 0x2000
#endif

#ifndef ADJ_SETOFFSET
#define ADJ_SETOFFSET 0x0100
#endif

#ifndef CLOCK_INVALID
#define CLOCK_INVALID -1
#endif

#define CLOCKFD 3
#define FD_TO_CLOCKID(fd)	((~(clockid_t) (fd) << 3) | CLOCKFD)

#if 1
/* When glibc offers the syscall, this will go away. */
#include <sys/syscall.h>
static int clock_adjtime(clockid_t id, struct timex *tx)
{
	return syscall(__NR_clock_adjtime, id, tx);
}
#endif

Boolean adjFreq(clockid_t clkid, Integer32 adj)
{
	struct timex tx;
	int err;

	if (adj > ADJ_FREQ_MAX)
		adj = ADJ_FREQ_MAX;
	else if (adj < -ADJ_FREQ_MAX)
		adj = -ADJ_FREQ_MAX;

	memset(&tx, 0, sizeof(tx));
	tx.modes = ADJ_FREQUENCY;
	tx.freq = (long) (adj * 65.536);

	err = clock_adjtime(clkid, &tx);
	if (err < 0) {
		ERROR("failed adjust the PTP clock: %s\n", strerror(errno));
		return FALSE;
	}
	NOTIFY("adjusted system clock by %d\n", adj);
	return TRUE;
}

Boolean adjOffset(clockid_t clkid, TimeInternal *time)
{
	struct timex tx;
	int err;

	/*
	 * TimeInternal always has both fields with the same sign.
	 */
	if ((time->seconds > 0 && time->nanoseconds < 0) ||
	    (time->seconds < 0 && time->nanoseconds > 0)) {
		ERROR("inconsistent time %ds %dns\n",
		      time->seconds, time->nanoseconds);
		exit(1);
	}

	memset(&tx, 0, sizeof(tx));
	tx.modes = ADJ_SETOFFSET | ADJ_NANO;
	tx.time.tv_sec = -time->seconds;
	tx.time.tv_usec = -time->nanoseconds;

	/*
	 * The value of a timeval is the sum of its fields, but the
	 * field tv_usec must always be non-negative.
	 */
	if (tx.time.tv_usec < 0) {
		tx.time.tv_sec  -= 1;
		tx.time.tv_usec += 1000000000;
	}

	err = clock_adjtime(clkid, &tx);
	if (err < 0) {
		ERROR("failed adjust PTP clock time: %s\n", strerror(errno));
		return FALSE;
	}
	NOTIFY("adjusted system clock by %ds %dns\n",
	       time->seconds, time->nanoseconds);

	return TRUE;
}


Boolean initPtpClock(RunTimeOpts *rtOpts, PtpClock *ptpClock)
{
	char *device = rtOpts->ptpClockDevice;
	int fd;

	fd = open(device, O_RDWR);

	if (fd < 0) {
		ERROR("Cannot open %s: %s\n", device, strerror(errno));
		ERROR("Using CLOCK_REALTIME instead.\n");
		ptpClock->clock_device = -1;
		ptpClock->clkid = CLOCK_REALTIME;
	} else {
		NOTIFY("Using Linux PTP Hardware Clock API\n");
		ptpClock->clock_device = fd;
		ptpClock->clkid = FD_TO_CLOCKID(fd);
	}

	return TRUE;
}

void ptpClockShutdown(PtpClock *ptpClock)
{
	if (ptpClock->clock_device != -1)
		close(ptpClock->clock_device);
}
