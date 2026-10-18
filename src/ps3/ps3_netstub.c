#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <net/poll.h>

#include "ps3_compat.h"

struct hostent *gethostbyname(const char *name) {
	(void)name;
	errno = ENOSYS;
	return NULL;
}

int getpeername(int s, struct sockaddr *name, socklen_t *namelen) {
	return sysNetGetPeerName(s, name, namelen);
}

int getsockopt(int s, int level, int optname, void *optval, socklen_t *optlen) {
	(void)s;
	(void)level;
	(void)optname;
	(void)optval;
	(void)optlen;
	errno = ENOSYS;
	return -1;
}

int setsockopt(int s, int level, int optname, const void *optval, socklen_t optlen) {
	(void)s;
	(void)level;
	(void)optname;
	(void)optval;
	(void)optlen;
	errno = ENOSYS;
	return -1;
}

int getsockname(int s, struct sockaddr *name, socklen_t *namelen) {
	return sysNetGetSockName(s, name, namelen);
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
	(void)fds;
	(void)nfds;
	(void)timeout;
	errno = ENOSYS;
	return -1;
}

char *inet_ntoa(struct in_addr addr) {
	static char buffer[INET_ADDRSTRLEN];
	unsigned int host = addr.s_addr;
	unsigned int octet[4];
	int i;
	int pos = 0;

	octet[0] = (host >> 24) & 0xff;
	octet[1] = (host >> 16) & 0xff;
	octet[2] = (host >> 8) & 0xff;
	octet[3] = host & 0xff;

	for (i = 0; i < 4; i++) {
		if (octet[i] >= 100)
			buffer[pos++] = (char)('0' + octet[i] / 100);
		if (octet[i] >= 10)
			buffer[pos++] = (char)('0' + (octet[i] / 10) % 10);
		buffer[pos++] = (char)('0' + octet[i] % 10);
		if (i < 3)
			buffer[pos++] = '.';
	}
	buffer[pos] = '\0';
	return buffer;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
	const struct in_addr *in = (const struct in_addr *)src;
	const char *result;

	if (af != AF_INET || size < INET_ADDRSTRLEN)
		return NULL;

	result = inet_ntoa(*in);
	memcpy(dst, result, INET_ADDRSTRLEN);
	return dst;
}
