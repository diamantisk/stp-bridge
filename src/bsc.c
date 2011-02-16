/* Everything related to the global BSC */
/*
 * (C) 2010-2011 by Holger Hans Peter Freyther <zecke@selfish.org>
 * (C) 2010-2011 by On-Waves
 * All Rights Reserved
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <bsc_data.h>
#include <cellmgr_debug.h>
#include <mtp_level3.h>
#include <mtp_pcap.h>

#include <osmocore/talloc.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <getopt.h>

#undef PACKAGE_NAME
#undef PACKAGE_VERSION
#undef PACKAGE_BUGREPORT
#undef PACKAGE_TARNAME
#undef PACKAGE_STRING
#include <cellmgr_config.h>

extern struct bsc_data *bsc;
extern char *config;

struct bsc_data *bsc_data_create()
{
	struct bsc_data *bsc;

	bsc = talloc_zero(NULL, struct bsc_data);
	if (!bsc) {
		LOGP(DINP, LOGL_ERROR, "Failed to create the BSC.\n");
		return NULL;
	}

	INIT_LLIST_HEAD(&bsc->linksets);
	INIT_LLIST_HEAD(&bsc->mscs);
	INIT_LLIST_HEAD(&bsc->apps);

	bsc->dpc = 1;
	bsc->opc = 0;
	bsc->sccp_opc = -1;
	bsc->isup_opc = -1;
	bsc->udp_port = 3456;
	bsc->udp_ip = NULL;
	bsc->udp_nr_links = 1;
	bsc->src_port = 1313;
	bsc->ni_ni = MTP_NI_NATION_NET;
	bsc->ni_spare = 0;
	bsc->pcap_fd = -1;
	bsc->udp_reset_timeout = 180;

	return bsc;
}

static void print_usage(const char *arg)
{
	printf("Usage: %s\n", arg);
}

static void print_help()
{
	printf("  Some useful help...\n");
	printf("  -h --help this text\n");
	printf("  -c --config=CFG The config file to use.\n");
	printf("  -p --pcap=FILE. Write MSUs to the PCAP file.\n");
	printf("  -c --once. Send the SLTM msg only once.\n");
	printf("  -v --version. Print the version number\n");
}

void handle_options(int argc, char **argv)
{
	while (1) {
		int option_index = 0, c;
		static struct option long_options[] = {
			{"help", 0, 0, 'h'},
			{"config", 1, 0, 'c'},
			{"pcap", 1, 0, 'p'},
			{"version", 0, 0, 0},
			{0, 0, 0, 0},
		};

		c = getopt_long(argc, argv, "hc:p:v",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_usage(argv[0]);
			print_help();
			exit(0);
		case 'p':
			if (bsc->pcap_fd >= 0)
				close(bsc->pcap_fd);
			bsc->pcap_fd = open(optarg, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP| S_IROTH);
			if (bsc->pcap_fd < 0) {
				fprintf(stderr, "Failed to open PCAP file.\n");
				exit(0);
			}
			mtp_pcap_write_header(bsc->pcap_fd);
			break;
		case 'c':
			config = optarg;
			break;
		case 'v':
			printf("This is %s version %s.\n", PACKAGE, VERSION);
			exit(0);
			break;
		default:
			fprintf(stderr, "Unknown option.\n");
			break;
		}
	}
}
