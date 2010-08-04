/* A Media Gateway Control Protocol Media Gateway: RFC 3435 */
/* The protocol implementation */

/*
 * (C) 2009-2010 by Holger Hans Peter Freyther <zecke@selfish.org>
 * (C) 2009-2010 by On-Waves
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

#include <sys/types.h>

#include <cellmgr_debug.h>

#include <mgcp/mgcp.h>
#include <mgcp/mgcp_internal.h>

#include <osmocore/talloc.h>

#include <osmocom/vty/command.h>
#include <osmocom/vty/vty.h>

#include <string.h>
#include <netdb.h>

static struct mgcp_config *g_cfg = NULL;

enum cellmgr_node {
	MGCP_NODE = _LAST_OSMOVTY_NODE + 1,
};

/*
 * vty code for mgcp below
 */
static struct cmd_node mgcp_node = {
	MGCP_NODE,
	"%s(mgcp)#",
	1,
};

static int config_write_mgcp(struct vty *vty)
{
	vty_out(vty, "mgcp%s", VTY_NEWLINE);
	if (g_cfg->local_ip)
		vty_out(vty, "  local ip %s%s", g_cfg->local_ip, VTY_NEWLINE);
	if (g_cfg->bts_ip && strlen(g_cfg->bts_ip) != 0)
		vty_out(vty, "  mgw ip %s%s", g_cfg->bts_ip, VTY_NEWLINE);
	vty_out(vty, "  bind ip %s%s", g_cfg->source_addr, VTY_NEWLINE);
	vty_out(vty, "  bind port %u%s", g_cfg->source_port, VTY_NEWLINE);
	vty_out(vty, "  rtp base %u%s", g_cfg->rtp_base_port, VTY_NEWLINE);
	vty_out(vty, "  rtp ip-dscp %d%s", g_cfg->endp_dscp, VTY_NEWLINE);
	if (g_cfg->audio_payload != -1)
		vty_out(vty, "  sdp audio payload number %d%s", g_cfg->audio_payload, VTY_NEWLINE);
	if (g_cfg->audio_name)
		vty_out(vty, "  sdp audio payload name %s%s", g_cfg->audio_name, VTY_NEWLINE);
	vty_out(vty, "  loop %u%s", !!g_cfg->audio_loop, VTY_NEWLINE);
	vty_out(vty, "  number endpoints %u%s", g_cfg->number_endpoints - 1, VTY_NEWLINE);

	return CMD_SUCCESS;
}

DEFUN(show_mcgp, show_mgcp_cmd, "show mgcp",
      SHOW_STR "Display information about the MGCP Media Gateway")
{
	int i;

	vty_out(vty, "MGCP is up and running with %u endpoints:%s", g_cfg->number_endpoints - 1, VTY_NEWLINE);
	for (i = 1; i < g_cfg->number_endpoints; ++i) {
		struct mgcp_endpoint *endp = &g_cfg->endpoints[i];
		vty_out(vty, " Endpoint 0x%.2x: CI: %d net: %u/%u bts: %u/%u on %s traffic received bts: %u/%u  remote: %u/%u%s",
			i, endp->ci,
			ntohs(endp->net_rtp), ntohs(endp->net_rtcp),
			ntohs(endp->bts_rtp), ntohs(endp->bts_rtcp),
			inet_ntoa(endp->bts), endp->in_bts, endp->bts_state.lost_no,
			endp->in_remote, endp->net_state.lost_no,
			VTY_NEWLINE);
	}

	return CMD_SUCCESS;
}

DEFUN(cfg_mgcp,
      cfg_mgcp_cmd,
      "mgcp",
      "Configure the MGCP")
{
	vty->node = MGCP_NODE;
	return CMD_SUCCESS;
}

DEFUN(cfg_mgcp_local_ip,
      cfg_mgcp_local_ip_cmd,
      "local ip IP",
      "Set the IP to be used in SDP records")
{
	struct hostent *hosts;
	struct in_addr *addr;

	hosts = gethostbyname(argv[0]);
	if (!hosts || hosts->h_length < 1 || hosts->h_addrtype != AF_INET) {
		vty_out(vty, "Failed to resolve '%s'%s", argv[0], VTY_NEWLINE);
		return CMD_WARNING;
	}

	addr = (struct in_addr *) hosts->h_addr_list[0];
	if (g_cfg->local_ip)
		talloc_free(g_cfg->local_ip);
	g_cfg->local_ip = talloc_strdup(g_cfg, inet_ntoa(*addr));
	return CMD_SUCCESS;
}

DEFUN(cfg_mgcp_mgw_ip,
      cfg_mgcp_mgw_ip_cmd,
      "mgw ip IP",
      "Set the IP of the MGW for RTP forwarding")
{
	struct hostent *hosts;
	struct in_addr *addr;

	hosts = gethostbyname(argv[0]);
	if (!hosts || hosts->h_length < 1 || hosts->h_addrtype != AF_INET) {
		vty_out(vty, "Failed to resolve '%s'%s", argv[0], VTY_NEWLINE);
		return CMD_WARNING;
	}

	addr = (struct in_addr *) hosts->h_addr_list[0];
	if (g_cfg->bts_ip)
		talloc_free(g_cfg->bts_ip);
	g_cfg->bts_ip = talloc_strdup(g_cfg, inet_ntoa(*addr));
	inet_aton(g_cfg->bts_ip, &g_cfg->bts_in);
	return CMD_SUCCESS;
}

DEFUN(cfg_mgcp_bind_ip,
      cfg_mgcp_bind_ip_cmd,
      "bind ip A.B.C.D",
      "Bind the MGCP to this local addr")
{
	if (g_cfg->source_addr)
		talloc_free(g_cfg->source_addr);
	g_cfg->source_addr = talloc_strdup(g_cfg, argv[0]);
	return CMD_SUCCESS;
}

DEFUN(cfg_mgcp_bind_port,
      cfg_mgcp_bind_port_cmd,
      "bind port <0-65534>",
      "Bind the MGCP to this port")
{
	unsigned int port = atoi(argv[0]);
	g_cfg->source_port = port;
	return CMD_SUCCESS;
}

DEFUN(cfg_mgcp_rtp_base_port,
      cfg_mgcp_rtp_base_port_cmd,
      "rtp base <0-65534>",
      "Base port to use")
{
	unsigned int port = atoi(argv[0]);
	g_cfg->rtp_base_port = port;
	return CMD_SUCCESS;
}

DEFUN(cfg_mgcp_rtp_ip_dscp,
      cfg_mgcp_rtp_ip_dscp_cmd,
      "rtp ip-dscp <0-255>",
      "Set the IP_TOS socket attribute on the RTP/RTCP sockets.\n" "The DSCP value.")
{
	int dscp = atoi(argv[0]);
	g_cfg->endp_dscp = dscp;
	return CMD_SUCCESS;
}

ALIAS_DEPRECATED(cfg_mgcp_rtp_ip_dscp, cfg_mgcp_rtp_ip_tos_cmd,
      "rtp ip-tos <0-255>",
      "Set the IP_TOS socket attribute on the RTP/RTCP sockets.\n" "The DSCP value.")


DEFUN(cfg_mgcp_sdp_payload_number,
      cfg_mgcp_sdp_payload_number_cmd,
      "sdp audio payload number <1-255>",
      "Set the audio codec to use")
{
	unsigned int payload = atoi(argv[0]);
	g_cfg->audio_payload = payload;
	return CMD_SUCCESS;
}

DEFUN(cfg_mgcp_sdp_payload_name,
      cfg_mgcp_sdp_payload_name_cmd,
      "sdp audio payload name NAME",
      "Set the audio name to use")
{
	if (g_cfg->audio_name)
		talloc_free(g_cfg->audio_name);
	g_cfg->audio_name = talloc_strdup(g_cfg, argv[0]);
	return CMD_SUCCESS;
}

DEFUN(cfg_mgcp_loop,
      cfg_mgcp_loop_cmd,
      "loop (0|1)",
      "Loop the audio")
{
	g_cfg->audio_loop = atoi(argv[0]);
	return CMD_SUCCESS;
}

DEFUN(cfg_mgcp_number_endp,
      cfg_mgcp_number_endp_cmd,
      "number endpoints <0-65534>",
      "The number of endpoints to allocate. This is not dynamic.")
{
	/* + 1 as we start counting at one */
	g_cfg->number_endpoints = atoi(argv[0]) + 1;
	return CMD_SUCCESS;
}

DEFUN(loop_endp,
      loop_endp_cmd,
      "loop-endpoint NAME (0|1)",
      "Loop a given endpoint\n"
      "The name in hex of the endpoint\n" "Disable the loop\n" "Enable the loop\n")
{
	struct mgcp_endpoint *endp;

	int endp_no = strtoul(argv[0], NULL, 16);
	if (endp_no < 1 || endp_no >= g_cfg->number_endpoints) {
		vty_out(vty, "Loopback number %s/%d is invalid.%s",
		argv[0], endp_no, VTY_NEWLINE);
		return CMD_WARNING;
	}


	endp = &g_cfg->endpoints[endp_no];
	int loop = atoi(argv[1]);

	if (loop)
		endp->conn_mode = MGCP_CONN_LOOPBACK;
	else
		endp->conn_mode = endp->orig_mode;

	return CMD_SUCCESS;
}

DEFUN(ournode_exit,
      ournode_exit_cmd, "exit", "Exit current mode and down to previous node\n")
{
	switch (vty->node) {
	case MGCP_NODE:
		vty->node = CONFIG_NODE;
		vty->index = NULL;
		break;
	}

	return CMD_SUCCESS;
}

DEFUN(ournode_end,
      ournode_end_cmd, "end", "End current mode and change to enable mode.")
{
	switch (vty->node) {
	case VIEW_NODE:
	case ENABLE_NODE:
		break;
	case MGCP_NODE:
		vty_config_unlock(vty);
		vty->node = ENABLE_NODE;
		vty->index = NULL;
		vty->index_sub = NULL;
		break;
	}

	return CMD_SUCCESS;
}

int mgcp_vty_init(void)
{
	install_element_ve(&show_mgcp_cmd);
	install_element(ENABLE_NODE, &loop_endp_cmd);

	install_element(CONFIG_NODE, &cfg_mgcp_cmd);
	install_node(&mgcp_node, config_write_mgcp);

	install_default(MGCP_NODE);
	install_element(MGCP_NODE, &ournode_exit_cmd);
	install_element(MGCP_NODE, &ournode_end_cmd);
	install_element(MGCP_NODE, &cfg_mgcp_local_ip_cmd);
	install_element(MGCP_NODE, &cfg_mgcp_mgw_ip_cmd);
	install_element(MGCP_NODE, &cfg_mgcp_bind_ip_cmd);
	install_element(MGCP_NODE, &cfg_mgcp_bind_port_cmd);
	install_element(MGCP_NODE, &cfg_mgcp_rtp_base_port_cmd);
	install_element(MGCP_NODE, &cfg_mgcp_rtp_ip_dscp_cmd);
	install_element(MGCP_NODE, &cfg_mgcp_rtp_ip_tos_cmd);
	install_element(MGCP_NODE, &cfg_mgcp_sdp_payload_number_cmd);
	install_element(MGCP_NODE, &cfg_mgcp_sdp_payload_name_cmd);
	install_element(MGCP_NODE, &cfg_mgcp_loop_cmd);
	install_element(MGCP_NODE, &cfg_mgcp_number_endp_cmd);

	return 0;
}

void mgcp_vty_set_config(struct mgcp_config *cfg)
{
	g_cfg = cfg;
}