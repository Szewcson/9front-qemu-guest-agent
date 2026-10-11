/* SPDX-License-Identifier: MIT */
#include "clock.h"

int
qgaclockset(const QgaClockIO *io, void *context, int supplied, QgaInt64 time, const char **error)
{
	QgaInt64 seconds, current;
	int result;

	if(error == nil)
		return -1;
	*error = "Native clock operation failed";
	if(io == nil || io->begin == nil || io->end == nil || io->readrtc == nil ||
	   io->setclock == nil || io->now == nil || io->writertc == nil ||
	   (supplied != 0 && supplied != 1))
		return -1;
	if(supplied && (time < 0 || time > QgaRtcMaxTime)){
		*error = "Requested time is outside the native RTC range (1970..2069 UTC)";
		return -1;
	}
	if(io->begin(context) < 0){
		*error = "Cannot acquire system clock and RTC; native eve privileges required";
		return -1;
	}
	result = -1;
	if(!supplied){
		if(io->readrtc(context, &seconds) < 0 || seconds < 0 || seconds > QgaRtcMaxSeconds){
			*error = "Cannot read a representable time from the native RTC";
			goto out;
		}
		time = seconds*1000000000LL;
	}
	if(io->setclock(context, time) < 0){
		*error = "Cannot set native system clock; RTC was not changed";
		goto out;
	}
	/* Use the actual system clock after the update, not a stale request value. */
	if(io->now(context, &current) < 0 || current < 0 || current > QgaRtcMaxTime){
		*error = "System clock changed but current time could not be read for RTC update";
		goto out;
	}
	if(io->writertc(context, current/1000000000LL) < 0){
		*error = "System clock changed but RTC update failed";
		goto out;
	}
	result = 0;
out:
	io->end(context);
	return result;
}
