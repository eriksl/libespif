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
#include <stdbool.h>
#include <time.h>

#include "libespif.h"

static bool profile = false;

static struct timespec ts1;

static void profile_start(void)
{
	if(profile)
		clock_gettime(CLOCK_BOOTTIME, &ts1);
}

static void profile_end(const char *tag)
{
	struct timespec ts2, ts3;

	if(!profile)
		return;

	clock_gettime(CLOCK_BOOTTIME, &ts2);

	ts3.tv_nsec = ts2.tv_nsec - ts1.tv_nsec;

	if(ts3.tv_nsec < 0 )
	{
		ts3.tv_nsec += 1000000000;
		ts3.tv_sec = ts2.tv_sec - ts1.tv_sec - 1;
	}
	else
		ts3.tv_sec = ts2.tv_sec - ts1.tv_sec;

	fprintf(stderr, "%s: start: %lu.%10.10lu, stop: %lu.%10.10lu, diff: %lu.%10.10lu, %lu ms\n",
			tag,
			ts1.tv_sec, ts1.tv_nsec,
			ts2.tv_sec, ts2.tv_nsec,
			ts3.tv_sec, ts3.tv_nsec,
			(ts3.tv_sec * 1000000000 + ts3.tv_nsec) / 1000 / 1000);
}

static int resolve(const char * hostname, int port, struct sockaddr_in6 *saddr)
{
	struct addrinfo hints;
	struct addrinfo *res;
	char service[16];
	int s;

	snprintf(service, sizeof(service), "%u", port);
	memset(&hints, 0, sizeof(hints));

	hints.ai_family		=	AF_INET6;
	hints.ai_socktype	=	SOCK_DGRAM;
	hints.ai_flags		=	AI_NUMERICSERV | AI_V4MAPPED;

	profile_start();

	if((s = getaddrinfo(hostname, service, &hints, &res)))
		return(0);

	profile_end("resolve");

	*saddr = *(struct sockaddr_in6 *)res->ai_addr;
	freeaddrinfo(res);

	return(1);
}

static int espif_error(int rv, size_t buflen, char *buf, const char *hostname, const char *error)
{
	snprintf(buf, buflen, "%s: %s (%m)", hostname, error);
	return(rv);
}

static int espif_connect(bool using_tcp, const espif_setup *setup, const struct sockaddr_in6 *saddr)
{
	int fd, attempt;
	struct pollfd pfd;
	int so_error;
	socklen_t so_size;

	for(attempt = setup->conntr; attempt > 0; attempt--)
	{
		if(setup->verbose)
			fprintf(stderr, "libespif: trying to connect using %s, attempt #%d\n", using_tcp ? "TCP" : "UDP", setup->conntr - attempt);

		if((fd = socket(AF_INET6, (using_tcp ? SOCK_STREAM : SOCK_DGRAM) | SOCK_NONBLOCK, 0)) < 0)
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: tcp socket failed (%m)\n");

			goto next_try;
		}

		if(connect(fd, (const struct sockaddr *)saddr, sizeof(*saddr)) && (errno != EINPROGRESS))
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: connect failed (%m)\n");

			close(fd);
			goto next_try;
		}

		pfd.fd = fd;
		pfd.events = POLLOUT;

		if(poll(&pfd, 1, setup->connto) != 1)
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: poll failed (%m)\n");

			close(fd);
			goto next_try;
		}

		so_error = 0;
		so_size = sizeof(so_error);

		if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &so_size) < 0)
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: getsockopt failed (%m)\n");

			close(fd);
			goto next_try;
		}

		if(so_error != 0)
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: so_error failed (%m)\n");

			close(fd);
			goto next_try;
		}

		break;

next_try:
		usleep(1000 * setup->retrydelay);
	}

	if(attempt < 1)
		return(-1);

	return(fd);
}

int espif(const espif_setup *setup, const char *hostname, const char *cmd, size_t buflen, char *buf)
{
	int attempt, fd, current;
	struct sockaddr_in6	saddr;
	struct pollfd pfd;
	uint8_t first;
	ssize_t bufread;
	bool using_tcp;

	if(!resolve(hostname, setup->port, &saddr))
		return(espif_error(-1, buflen, buf, hostname, "cannot resolve"));

	fd = -1;
	using_tcp = false;

	for(attempt = setup->sendtr; attempt > 0; attempt--)
	{
		if((fd = espif_connect(using_tcp, setup, &saddr)) < 0)
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: retry connect, attempt %d\n", attempt);

			goto next_try;
		}

		pfd.fd = fd;
		pfd.events = POLLOUT;

		if(poll(&pfd, 1, setup->connto) != 1)
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: poll error (%m)\n");

			goto next_try;
		}

		profile_start();

		if(write(fd, cmd, strlen(cmd)) != strlen(cmd))
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: write error (%m)\n");

			goto next_try;
		}

		pfd.events = POLLIN;
		current = 0;

		for(first = 1;; first = 0)
		{
			if(poll(&pfd, 1, first ? setup->recvto1 : setup->recvto2) != 1)
			{
				if(first)
					goto next_try;
				else
					break;
			}

			if((bufread = read(fd, buf + current, buflen - current)) <= 0)
			{
				if(setup->verbose)
					fprintf(stderr, "libespif: read error (%m)\n");

				if(first)
				{
					if(using_tcp)
						goto next_try;
					else
					{
						using_tcp = true;
						goto next_try_now;
					}
				}
				else
					break;
			}

			current += bufread;
		}

		profile_end("command");

		buf[current] = '\0';

		close(fd);

		return(0);

next_try:
		usleep(1000 * setup->retrydelay);
next_try_now:
		if(setup->verbose)
			fprintf(stderr, "libespif: retry read/write, attempt %d\n", attempt);
	}

	close(fd);
	return(espif_error(-1, buflen, buf, hostname, "no more write attempts"));
}
