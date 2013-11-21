/*
 * Server management functions.
 *
 * Copyright 2000-2006 Willy Tarreau <w@1wt.eu>
 * Copyright 2007-2008 Krzysztof Piotr Oledzki <ole@ans.pl>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 */

#include <common/config.h>
#include <common/time.h>

#include <proto/server.h>

int srv_downtime(struct server *s) {

	if ((s->state & SRV_RUNNING) && s->last_change < now.tv_sec)		// ignore negative time
		return s->down_time;

	return now.tv_sec - s->last_change + s->down_time;
}

int srv_getinter(struct server *s) {

	if ((s->state & SRV_CHECKED) && (s->health == s->rise + s->fall - 1))
		return s->inter;

	if (!(s->state & SRV_RUNNING) && s->health==0)
		return (s->downinter)?(s->downinter):(s->inter);

	return (s->fastinter)?(s->fastinter):(s->inter);
}

/* Recomputes the server's eweight based on its state, uweight, the current time,
 * and the proxy's algorihtm. To be used after updating sv->uweight. The warmup
 * state is automatically disabled if the time is elapsed.
 */
void server_recalc_eweight(struct server *sv)
{
	struct proxy *px = sv->proxy;
	unsigned w;

	if (now.tv_sec < sv->last_change || now.tv_sec >= sv->last_change + sv->slowstart) {
		/* go to full throttle if the slowstart interval is reached */
		sv->state &= ~SRV_WARMINGUP;
	}

	/* We must take care of not pushing the server to full throttle during slow starts.
	 * It must also start immediately, at least at the minimal step when leaving maintenance.
	 */
	if ((sv->state & SRV_WARMINGUP) && (px->lbprm.algo & BE_LB_PROP_DYN))
		w = (px->lbprm.wdiv * (now.tv_sec - sv->last_change) + sv->slowstart) / sv->slowstart;
	else
		w = px->lbprm.wdiv;

	sv->eweight = (sv->uweight * w + px->lbprm.wmult - 1) / px->lbprm.wmult;

	/* now propagate the status change to any LB algorithms */
	if (px->lbprm.update_server_eweight)
		px->lbprm.update_server_eweight(sv);
	else if (sv->eweight) {
		if (px->lbprm.set_server_status_up)
			px->lbprm.set_server_status_up(sv);
	}
	else {
		if (px->lbprm.set_server_status_down)
			px->lbprm.set_server_status_down(sv);
	}
}

/*
 * Local variables:
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 */
