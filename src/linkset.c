/* link set management code */
/*
 * (C) 2010-2013 by Holger Hans Peter Freyther <zecke@selfish.org>
 * (C) 2010-2013 by On-Waves
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <bsc_data.h>
#include <cellmgr_debug.h>
#include <mtp_level3.h>
#include <snmp_mtp.h>

#include <osmocom/core/talloc.h>


struct mtp_link_set *link_set_create(struct bsc_data *bsc)
{
	struct mtp_link_set *set;

	set = mtp_link_set_alloc(bsc);
	set->name = talloc_strdup(set, "MTP");

	set->ni = MTP_NI_NATION_NET;
	set->spare = 0;

	set->supported_ssn[1] = 1;
	set->supported_ssn[7] = 1;
	set->supported_ssn[8] = 1;
	set->supported_ssn[146] = 1;
	set->supported_ssn[254] = 1;

	return set;
}

int link_set_init_links(struct bsc_data *bsc, struct mtp_link_set *set)
{
	int i;
	struct mtp_udp_link *lnk;
	struct mtp_link *blnk;


	if (!bsc->udp_src_port) {
		LOGP(DINP, LOGL_ERROR, "You need to set a UDP address.\n");
		return -1;
	}

	LOGP(DINP, LOGL_NOTICE, "Using UDP MTP mode.\n");

	if (link_global_init(&bsc->udp_data) != 0)
		return -1;

	if (link_global_bind(&bsc->udp_data, bsc->udp_src_port) != 0)
		return -1;

	for (i = 1; i <= bsc->udp_nr_links; ++i) {
		blnk = mtp_link_alloc(set);
		lnk = mtp_udp_link_init(blnk);

		lnk->link_index = i;

		/* now connect to the transport */
		if (snmp_mtp_peer_name(lnk->session, bsc->udp_ip) != 0)
			return -1;

		if (link_udp_init(lnk, bsc->udp_ip, bsc->udp_port) != 0)
			return -1;
	}

	return 0;
}

int link_set_shutdown_links(struct mtp_link_set *set)
{
	struct mtp_link *lnk;

	llist_for_each_entry(lnk, &set->links, entry)
		lnk->shutdown(lnk);
	return 0;
}

int link_set_reset_links(struct mtp_link_set *set)
{
	struct mtp_link *lnk;

	llist_for_each_entry(lnk, &set->links, entry)
		lnk->reset(lnk);
	return 0;
}

int link_set_clear_links(struct mtp_link_set *set)
{
	struct mtp_link *lnk;

	llist_for_each_entry(lnk, &set->links, entry)
		lnk->clear_queue(lnk);
	return 0;
}

int link_set_down(struct mtp_link_set *set)
{
	if (set->on_down)
		set->on_down(set);
	return 0;
}

int link_set_up(struct mtp_link_set *set)
{
	if (set->on_up)
		set->on_up(set);
	return 0;
}