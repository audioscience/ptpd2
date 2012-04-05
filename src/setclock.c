/**
 * @file setclock.c
 *
 * Simple commandline app to set linux hardware clock
 * 
 * Usage "setclock <adjustment in parts per billion>"
 * -512000 <= adjustment <= 512000
 */

#include <linux/ptp_clock.h>

#include "ptpd.h"

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

#undef NOTIFY
#define NOTIFY printf

#undef ERROR
#define ERROR printf

#undef NOTIFY
#define NOTIFY printf

#if 1
/* When glibc offers the syscall, this will go away. */
#include <sys/syscall.h>
static int clock_adjtime(clockid_t id, struct timex *tx)
{
	return syscall(__NR_clock_adjtime, id, tx);
}
#endif

Boolean adjFreq(clockid_t clkid,  int adj)
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

int fd;

int clock_open(char *device)
{
	int clkid;

	fd = open(device, O_RDWR);

	if (fd < 0) {
		ERROR("Cannot open %s: %s\n", device, strerror(errno));
		exit(1);
	} else {
		NOTIFY("Using Linux PTP Hardware Clock API\n");
		clkid = FD_TO_CLOCKID(fd);
	}

	return clkid;
}

void clock_close(void)
{
	if (fd != -1)
		close(fd);
}

int
main(int argc, char **argv)
{
	int clkid = clock_open("/dev/ptp0");
	long adj = 0;

	adj = strtol(argv[1], NULL, 0);

	adjFreq(clkid, adj);

	clock_close();
	return 0;
}
