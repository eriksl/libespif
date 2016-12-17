#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <poll.h>
#include <getopt.h>

#include "libespif.h"

static void usage(const espif_setup *setup)
{
	fprintf(stderr, "usage: espif [options] host string...\n");
	fprintf(stderr, "-C|--conntr       set connect attempts [%d]\n", setup->conntr);
	fprintf(stderr, "-c|--connto       set connect / send timeout in milliseconds [%d]\n", setup->connto);
	fprintf(stderr, "-d|--retrydelay   set delay in milliseconds before earch retry [%d]\n", setup->retrydelay);
	fprintf(stderr, "-p|--port         set port [%d]\n", setup->port);
	fprintf(stderr, "-r|--recvto1      set initial receive timeout in milliseconds [%d]\n", setup->recvto1);
	fprintf(stderr, "-R|--recvto2      set subsequent receive timeout in milliseconds [%d]\n", setup->recvto2);
	fprintf(stderr, "-s|--sendtr       set send/receive retries [%d]\n", setup->sendtr);
	fprintf(stderr, "-v|--verbose      enable verbose output on stderr\n");
}

int main(int argc, char ** argv)
{
	static const char *shortopts = "C:c:R:r:p:s:v";
	static const struct option longopts[] =
	{
		{ "conntr",		required_argument, 0, 'C' },
		{ "connto",		required_argument, 0, 'c' },
		{ "port",		required_argument, 0, 'p' },
		{ "recvto2",	required_argument, 0, 'R' },
		{ "recvto1",	required_argument, 0, 'r' },
		{ "retrydelay", required_argument, 0, 'd' },
		{ "sendtr",		required_argument, 0, 's' },
		{ "verbose",	no_argument, 0, 'v' },
		{ 0, 0, 0, 0 }
	};

	char		buffer[128 * 1024];
	int			arg;
	espif_setup	setup =
	{
		.verbose = 0,
		.connto = 1000,
		.conntr = 4,
		.port = 24,
		.recvto1 = 1000,
		.recvto2 = 100,
		.retrydelay = 100,
		.sendtr = 4,
	};

	while((arg = getopt_long(argc, argv, shortopts, longopts, 0)) != -1)
	{
		switch(arg)
		{
			case('C'):
			{
				setup.conntr = atoi(optarg);

				break;
			}

			case('c'):
			{
				setup.connto = atoi(optarg);

				break;
			}

			case('d'):
			{
				setup.retrydelay = atoi(optarg);

				break;
			}

			case('p'):
			{
				setup.port = atoi(optarg);

				break;
			}

			case('R'):
			{
				setup.recvto2 = atoi(optarg);

				break;
			}

			case('r'):
			{
				setup.recvto1 = atoi(optarg);

				break;
			}

			case('s'):
			{
				setup.sendtr = atoi(optarg);

				break;
			}

			case('v'):
			{
				setup.verbose = 1;
				break;
			}

			default:
			{
				usage(&setup);
				exit(1);
			}
		}
	}

	if((argc - optind) <= 1)
	{
		usage(&setup);
		exit(1);
	}

	buffer[0] = '\0';

	for(arg = optind + 1; argv[arg]; arg++)
	{
		if(strlen(buffer) > 0)
		{
			strncat(buffer, " ", sizeof(buffer));
			buffer[sizeof(buffer) - 1] = '\0';
		}

		strncat(buffer, argv[arg], sizeof(buffer));
		buffer[sizeof(buffer) - 1] = '\0';
	}

	strncat(buffer, "\r\n", sizeof(buffer));
	buffer[sizeof(buffer) - 1] = '\0';

	if(espif(&setup, argv[optind], buffer, sizeof(buffer), buffer))
		printf("ERROR: %s\n", buffer);
	else
		printf("%s\n", buffer);

	return(0);
}
