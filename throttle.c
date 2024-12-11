#include <errno.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "ftp.h"

/* Pause for an amount af time so the total transfer speed of this
 * transfer does not exceed the bandwidth cap.
 * Always successful. */
int throttle_pause( ftp_session_t *session )
{
	int diff, idealdiff, transfer_size;
	ftp_xfer_info_t *info;
	info = &session->info;
	struct timeval now, then;
	struct timespec ts[2];
	struct timespec *slp, *rem;

	/* First do some timekeeping and decide whether we need to date
	 * the masterserver up */

	gettimeofday( &now, NULL );
	then = info->xfer_probe;
	transfer_size = info->probe_len;

	diff = msecdiff( &now, &then );

	if( diff < 0 )
	{
		/* This can happen if the user changes his clock midway 
		 * through a download. We reset the clock, so the user
		 * gets one block for free */
		log_warn("Clock skew detected, throttling failed\n");
		gettimeofday( &info->xfer_probe, NULL );
		return FTP_SUCCESS;
	}

	if( diff > 500 )
	{
		send_state( session, T_XFER );
		gettimeofday( &info->xfer_probe, NULL );
		info->probe_len = 0;
	}

	/* Now that is done, do the actual throttling */

	if( config.throttle_rate < 1 )
		return FTP_SUCCESS;

	/* Since the throttle rate is in kbps, we're dividing bytes by 
	 * kilobytes per second, giving the difference in milliseconds */
	idealdiff = transfer_size / config.throttle_rate;

	/* Don't bother */
	if( idealdiff < diff  )
		return FTP_SUCCESS;

	/* Calculate how long we need to sleep */
	ts[0].tv_sec  =  ( idealdiff - diff ) / 1000;
	ts[0].tv_nsec = (( idealdiff - diff ) % 1000) * 1000000;
	slp = &ts[0];
	rem = &ts[1];

	while( nanosleep( slp, rem ) )
	{
		void *tmp;
		tmp = slp;
		slp = rem;
		rem = tmp;
	}
	
	return FTP_SUCCESS;

}
	

