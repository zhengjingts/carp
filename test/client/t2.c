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
void testcase2(int sockfd, struct sockaddr *server, struct cudp_buffer* cbuf)
{
	int i;
	u8 *buf = CUDP_MESSAGE(cbuf);
	unsigned int  num, len = 10;
	unsigned char amp = 50;

	for (i = 0; i < 6; i ++)
		num = testsend(sockfd, server, cbuf, len, amp);

	for (i = 0; i < 6; i ++)
	{
		num = testrecv(sockfd, server, cbuf);
		cudp_pkg_summary(server, cbuf);
	}
}


int main(int argc, char *argv[])
{
	clientframe(2, "Suspicious Communication", &testcase2);
	return 0;
}
