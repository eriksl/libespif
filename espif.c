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

static void usage(void)
{
	fprintf(stderr, "usage: espif [options] host string [string...]\n");
	fprintf(stderr, "-C|--conntr    set connect attempts [4]\n");
	fprintf(stderr, "-c|--connto    set connect / send timeout in milliseconds [2000]\n");
	fprintf(stderr, "-r|--recvto1   set initial receive timeout in milliseconds [2000]\n");
	fprintf(stderr, "-R|--recvto2   set subsequent receive timeout in milliseconds [100]\n");
	fprintf(stderr, "-s|--sendtr    set send/receive retries [8]\n");
	fprintf(stderr, "-v|--verbose   enable verbose output on stderr\n");
}

int main(int argc, char ** argv)
{
	static const char *shortopts = "C:c:R:r:s:v";
	static const struct option longopts[] =
	{
		{ "conntr", required_argument, 0, 'C' },
		{ "connto", required_argument, 0, 'c' },
		{ "recvto2", required_argument, 0, 'R' },
		{ "recvto1", required_argument, 0, 'r' },
		{ "sendtr", required_argument, 0, 's' },
		{ "verbose", no_argument, 0, 'v' },
		{ 0, 0, 0, 0 }
	};

	char		buffer[128 * 1024];
	int			arg;
	espif_setup	setup =
	{
		.verbose = 0,
		.connto = 2000,
		.conntr = 4,
		.recvto1 = 2000,
		.recvto2 = 100,
		.sendtr = 8,
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
				usage();
				exit(1);
			}
		}
	}

	if((argc - optind) <= 0)
	{
		usage();
		exit(1);
	}

	for(arg = optind + 1; argv[arg]; arg++)
	{
		snprintf(buffer, sizeof(buffer), "%s\r\n", argv[arg]);

		if(espif(&setup, argv[optind], buffer, sizeof(buffer), buffer))
			printf("ERROR: %s\n", buffer);
		else
			printf("%s\n", buffer);
	}

	return(0);
}
