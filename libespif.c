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

static int resolve(const char * hostname, int port, struct sockaddr_in6 *saddr)
{
	struct addrinfo hints;
	struct addrinfo *res;
	char service[16];
	int s;

	snprintf(service, sizeof(service), "%u", port);
	memset(&hints, 0, sizeof(hints));

	hints.ai_family		=	AF_INET6;
	hints.ai_socktype	=	SOCK_STREAM;
	hints.ai_flags		=	AI_NUMERICSERV | AI_V4MAPPED;

	if((s = getaddrinfo(hostname, service, &hints, &res)))
	{
		freeaddrinfo(res);
		return(0);
	}

	*saddr = *(struct sockaddr_in6 *)res->ai_addr;
	freeaddrinfo(res);

	return(1);
}

static int espif_error(int rv, size_t buflen, char *buf, const char *hostname, const char *error)
{
	snprintf(buf, buflen, "%s: %s (%m)", hostname, error);
	return(rv);
}

static int espif_connect(const espif_setup *setup, const struct sockaddr_in6 *saddr)
{
	int fd, attempt;
	struct pollfd pfd;
	int so_error;
	socklen_t so_size;

	for(attempt = setup->conntr; attempt > 0; attempt--)
	{
		if((fd = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: socket failed (%m)\n");

			usleep(1000 * setup->retrydelay);
			continue;
		}

		if(connect(fd, (const struct sockaddr *)saddr, sizeof(*saddr)) && (errno != EINPROGRESS))
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: connect failed (%m)\n");

			close(fd);
			usleep(1000 * setup->retrydelay);
			continue;
		}

		pfd.fd = fd;
		pfd.events = POLLOUT;

		if(poll(&pfd, 1, setup->connto) != 1)
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: poll failed (%m)\n");

			close(fd);
			usleep(1000 * setup->retrydelay);
			continue;
		}

		so_error = 0;
		so_size = sizeof(so_error);

		if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &so_size) < 0)
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: getsockopt failed (%m)\n");

			close(fd);
			usleep(1000 * setup->retrydelay);
			continue;
		}

		if(so_error != 0)
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: so_error failed (%m)\n");

			close(fd);
			usleep(1000 * setup->retrydelay);
			continue;
		}

		break;
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

	if(!resolve(hostname, 23, &saddr))
		return(espif_error(-1, buflen, buf, hostname, "cannot resolve"));

	fd = -1;

	for(attempt = setup->sendtr; attempt > 0; attempt--)
	{
		if((fd = espif_connect(setup, &saddr)) < 0)
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: retry connect, attempt %d\n", attempt);

			usleep(setup->retrydelay * 1000);
			continue;
		}

		pfd.fd = fd;
		pfd.events = POLLOUT;

		if(poll(&pfd, 1, setup->connto) != 1)
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: poll error (%m)\n");

			usleep(setup->retrydelay * 1000);
			continue;
		}

		if(write(fd, cmd, strlen(cmd)) != strlen(cmd))
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: write error (%m)\n");

			usleep(setup->retrydelay * 1000);
			continue;
		}

		pfd.events = POLLIN;
		current = 0;

		for(first = 1;; first = 0)
		{
			if(poll(&pfd, 1, first ? setup->recvto1 : setup->recvto2) != 1)
			{
				if(first)
					goto no_read;
				else
					break;
			}

			if((bufread = read(fd, buf + current, buflen - current)) <= 0)
			{
				if(first)
					goto no_read;
				else
					break;
			}

			current += bufread;
		}

		buf[current] = '\0';

		close(fd);

		return(0);

no_read:
		if(setup->verbose)
			fprintf(stderr, "libespif: retry read/write, attempt %d\n", attempt);
		usleep(1000 * setup->retrydelay);
	}

	close(fd);
	return(espif_error(-1, buflen, buf, hostname, "no more write attempts"));
}
