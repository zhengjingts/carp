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
void testcase3(int sockfd, struct sockaddr *server, struct cudp_buffer* cbuf)
{
	int i;
	u8 *buf = CUDP_MESSAGE(cbuf);
	unsigned int  num, len = 10;
	unsigned char amp = 100;

	for (i = 0; i < 8; i ++)
		num = testsend(sockfd, server, cbuf, len, amp);

	for (i = 0; i < 5; i ++)
	{
		num = testrecv(sockfd, server, cbuf);
		cudp_pkg_summary(server, cbuf);
	}

	struct cudphdr *hdr = (struct cudphdr*)CUDP_HEADER(cbuf);
	memset(hdr, 0, sizeof(struct cudphdr));
	CUDP_MESSAGE(cbuf)[0] = amp;
	unsigned int addrlen = sizeof(struct sockaddr);

	for (i = 0; i < 3; i ++)
	{
		num = recvfrom(sockfd, CUDP_HEADER(cbuf), cbuf->buflen, 0,
			server, &addrlen);
		cudp_pkg_summary(server, cbuf);
	}

	
	num = testsend(sockfd, server, cbuf, 1, 1);
}

int main(int argc, char *argv[])
{
	clientframe(3, "Malicious Communication", &testcase3);
	return 0;
}
