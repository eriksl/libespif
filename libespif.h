#ifndef libespif_h
#define libespif_h

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
	uint8_t verbose;
	int	connto;		// connect timeout per attempt
	int	conntr;		// connection attempts
	int recvto1;	// initial receive timeout
	int	recvto2;	// receive timeout for next segments
	int retrydelay;	// retry delay
	int	sendtr;		// attempts at sending
	bool force_tcp;
	bool force_udp;
	bool use_multicast;
	int	port;
} espif_setup;

int espif(const espif_setup *setup, const char *host,
		const char *cmd, size_t buflen, char *buf);

#endif
