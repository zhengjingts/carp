#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "../../cudp.h"
#include "../testcase.h"

// Test throughput
void testcase4(int sockfd, struct sockaddr *server, struct cudp_buffer* cbuf)
{
	int i;
	u8 *buf = CUDP_MESSAGE(cbuf);
	unsigned int  num, len;
	unsigned char amp = 10;

	//struct cudphdr *hdr = (struct cudphdr*)CUDP_HEADER(cbuf);
	//memset(hdr, 0, sizeof(struct cudphdr));
	unsigned int addrlen = sizeof(struct sockaddr);

	num = cudp_stop(sockfd, cbuf, 0, server, addrlen);
	num = recvfrom(sockfd, CUDP_HEADER(cbuf), cbuf->buflen, 0,
		server, &addrlen);
	cudp_pkg_summary(server, cbuf);

	num = cudp_stop(sockfd, cbuf, 0, server, addrlen);
	num = cudp_wait(sockfd, cbuf, 0, server, &addrlen);

	len = sizeof(struct cudphdr) + 1;
	memset(CUDP_HEADER(cbuf), 0, len);
	CUDP_MESSAGE(cbuf)[0] = amp;
	num = sendto(sockfd, CUDP_HEADER(cbuf), len, 0, server, addrlen);
	num = recvfrom(sockfd, CUDP_HEADER(cbuf), cbuf->buflen, 0,
		server, &addrlen);
	cudp_pkg_summary(server, cbuf);

	num = testsend(sockfd, server, cbuf, 1, amp);
	num = testrecv(sockfd, server, cbuf);
	cudp_pkg_summary(server, cbuf);
}

int main(int argc, char *argv[])
{
	clientframe(4, "Stop Communication", &testcase4);
	return 0;
}
