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

void testcase1(int sockfd, struct sockaddr *server, struct cudp_buffer* cbuf)
{
	u8 *buf = CUDP_MESSAGE(cbuf);
	unsigned int  num, len = 100;
	unsigned char amp = 1;

	num = testsend(sockfd, server, cbuf, len, amp);
	if (num != len)
		printf("\tCLIENT: Send Test Fail: num=%d\tlen=%d\tamp=%d\n",
			num, len, amp);
	else
		printf("\tCLIENT: Send Test Pass: num=%d\tlen=%d\tamp=%d\n",
			num, len, amp);

	num = testrecv(sockfd, server, cbuf);
	if (num != len * amp)
		printf("\tCLIENT: Recv Test Fail: num=%d\tlen=%d\tamp=%d\n",
			num, len, amp);
	else
		printf("\tCLIENT: Recv Test Pass: num=%d\tlen=%d\tamp=%d\n",
			num, len, amp);

	len = 10;
	amp = 10;
	num = testsend(sockfd, server, cbuf, len, amp);
	if (num != len)
		printf("\tCLIENT: Send Test Fail: num=%d\tlen=%d\tamp=%d\n",
			num, len, amp);
	else
		printf("\tCLIENT: Send Test Pass: num=%d\tlen=%d\tamp=%d\n",
			num, len, amp);

	num = testrecv(sockfd, server, cbuf);
	if (num != len * amp)
		printf("\tCLIENT: Recv Test Fail: num=%d\tlen=%d\tamp=%d\n",
			num, len, amp);
	else
		printf("\tCLIENT: Recv Test Pass: num=%d\tlen=%d\tamp=%d\n",
			num, len, amp);

	len = 1;
	amp = 100;
	num = testsend(sockfd, server, cbuf, len, amp);
	if (num != len)
		printf("\tCLIENT: Send Test Fail: num=%d\tlen=%d\tamp=%d\n",
			num, len, amp);
	else
		printf("\tCLIENT: Send Test Pass: num=%d\tlen=%d\tamp=%d\n",
			num, len, amp);

	num = testrecv(sockfd, server, cbuf);
	if (num != len * amp)
		printf("\tCLIENT: Recv Test Fail: num=%d\tlen=%d\tamp=%d\n",
			num, len, amp);
	else
		printf("\tCLIENT: Recv Test Pass: num=%d\tlen=%d\tamp=%d\n",
			num, len, amp);
}

int main(int argc, char *argv[])
{
	clientframe(1, "Normal Communication", &testcase1);
	return 0;
}
