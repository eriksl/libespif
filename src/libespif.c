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

static int resolve(const char * hostname, int port, struct sockaddr_in *connect_addr)
{
	struct addrinfo hints;
	struct addrinfo *res;
	char service[16];
	int s;

	snprintf(service, sizeof(service), "%u", port);
	memset(&hints, 0, sizeof(hints));

	hints.ai_family		=	AF_INET;
	hints.ai_socktype	=	SOCK_DGRAM;
	hints.ai_flags		=	AI_NUMERICSERV;

	profile_start();

	if((s = getaddrinfo(hostname, service, &hints, &res)))
		return(0);

	profile_end("resolve");

	*connect_addr = *(struct sockaddr_in *)res->ai_addr;
	freeaddrinfo(res);

	return(1);
}

static int espif_error(int rv, size_t buflen, char *buf, const char *hostname, const char *error)
{
	snprintf(buf, buflen, "%s: %s (%m)", hostname, error);
	return(rv);
}

static int espif_socket(bool using_tcp, const espif_setup *setup, const struct sockaddr_in *connect_addr)
{
	int fd, attempt;
	struct pollfd pfd;
	int so_error;
	socklen_t so_size;
	int arg;

	for(attempt = setup->conntr; attempt > 0; attempt--)
	{
		if(setup->verbose)
			fprintf(stderr, "libespif: trying to connect using %s, attempt #%d\n", using_tcp ? "TCP" : "UDP", setup->conntr - attempt);

		if(using_tcp)
		{
			if((fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
			{
				if(setup->verbose)
					fprintf(stderr, "libespif: connect tcp socket failed (%m)\n");

				goto next_try;
			}

			if(connect(fd, (const struct sockaddr *)connect_addr, sizeof(*connect_addr)) && (errno != EINPROGRESS))
			{
				if(setup->verbose)
					fprintf(stderr, "libespif: connect tcp failed (%m)\n");

				close(fd);
				goto next_try;
			}

			pfd.fd = fd;
			pfd.events = POLLOUT;

			if(poll(&pfd, 1, setup->connto) != 1)
			{
				if(setup->verbose)
					fprintf(stderr, "libespif: connect tcp poll failed (%m)\n");

				close(fd);
				goto next_try;
			}

			so_error = 0;
			so_size = sizeof(so_error);

			if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &so_size) < 0)
			{
				if(setup->verbose)
					fprintf(stderr, "libespif: connect tcp getsockopt error failed (%m)\n");

				close(fd);
				goto next_try;
			}

			if(so_error != 0)
			{
				if(setup->verbose)
					fprintf(stderr, "libespif: connect tcp so_error failed (%m)\n");

				close(fd);
				goto next_try;
			}
		}
		else
		{
			struct sockaddr_in bind_addr;

			bind_addr.sin_family = AF_INET;
			bind_addr.sin_addr.s_addr = INADDR_ANY;
			bind_addr.sin_port = 0;

			if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
			{
				if(setup->verbose)
					fprintf(stderr, "libespif: connect udp socket failed (%m)\n");

				goto next_try;
			}

			if(bind(fd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)))
			{
				if(setup->verbose)
					fprintf(stderr, "libespif: connect udp cannot bind: %m\n");

				close(fd);
				return(-1);
			}

			arg = 1;

			if(setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &arg, sizeof(arg)) && setup->verbose)
				if(setup->verbose)
					fprintf(stderr, "libespif: connect udp cannot enable broadcast: %m\n");

			if(setup->use_multicast)
			{
				struct ip_mreq mreq;

				arg = 3;
				if(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &arg, sizeof(arg)))
					if(setup->verbose)
						fprintf(stderr, "libespif: connect udp cannot set mc ttl: %m\n");

				mreq.imr_multiaddr = connect_addr->sin_addr;
				mreq.imr_interface.s_addr = INADDR_ANY;

				if(setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)))
					if(setup->verbose)
						fprintf(stderr, "libespif: connect udp cannot join mc group: %m\n");
			}
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
	struct sockaddr_in	connect_addr;
	struct sockaddr_in	remote_addr;
	socklen_t addrlen;
	struct pollfd pfd;
	uint8_t first;
	ssize_t bufsent, bufread;
	bool using_tcp;

	if(!resolve(hostname, setup->port, &connect_addr))
		return(espif_error(-1, buflen, buf, hostname, "cannot resolve"));

	fd = -1;

	using_tcp = setup->force_tcp;

	for(attempt = setup->sendtr; attempt > 0; attempt--)
	{
		if((fd = espif_socket(using_tcp, setup, &connect_addr)) < 0)
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: retry connect, attempt #%d\n", setup->sendtr - attempt);

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

		if(using_tcp)
			bufsent = write(fd, cmd, strlen(cmd));
		else
			bufsent = sendto(fd, cmd, strlen(cmd), 0, (struct sockaddr *)&connect_addr, sizeof(connect_addr));

		if(bufsent != strlen(cmd))
		{
			if(setup->verbose)
				fprintf(stderr, "libespif: write error (%m)\n");

			if(using_tcp)
				goto next_try;
			else
			{
				using_tcp = !setup->force_udp;
				goto next_try_now;
			}

			goto next_try;
		}

		pfd.events = POLLIN;
		current = 0;

		for(first = 1;; first = 0)
		{
			if(poll(&pfd, 1, first ? setup->recvto1 : setup->recvto2) != 1)
			{
				if(first)
				{
					if(using_tcp)
						goto next_try;
					else
					{
						using_tcp = !setup->force_udp;
						goto next_try_now;
					}
				}
				else
					break;
			}

			if(using_tcp)
				bufread = read(fd, buf + current, buflen - current);
			else
			{
				addrlen = sizeof(remote_addr);
				bufread = recvfrom(fd, buf + current, buflen - current, 0, (struct sockaddr *)&remote_addr, &addrlen);

				if(setup->verbose)
				{
					char hostname[32];
					char service[16];

					getnameinfo((struct sockaddr *)&remote_addr, sizeof(remote_addr), hostname, sizeof(hostname), service, sizeof(service), NI_DGRAM | NI_NUMERICHOST | NI_NUMERICSERV);

					fprintf(stderr, "received datagram from: %s port %s\n", hostname, service);
				}
			}

			if(bufread <= 0)
			{
				if(setup->verbose)
					fprintf(stderr, "libespif: read error (%m)\n");

				if(first)
				{
					if(using_tcp)
						goto next_try;
					else
					{
						using_tcp = !setup->force_udp;
						goto next_try_now;
					}
				}
				else
					break;
			}

			if(!using_tcp && (bufread == 1) && (buf[current] == '\0'))
				break;

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
			fprintf(stderr, "libespif: retry read/write, attempt #%d\n", setup->sendtr - attempt);
	}

	close(fd);
	return(espif_error(-1, buflen, buf, hostname, "no more write attempts"));
}
