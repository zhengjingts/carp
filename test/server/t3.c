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

void testcase3(int sockfd, struct sockaddr *server, struct cudp_buffer *cbuf)
{
        u8 *buf = CUDP_MESSAGE(cbuf);
        int num, i;
        unsigned char amp = 0;

        struct sockaddr client;
        struct cudp_conn *cc;
        u32 ip;

	for (i = 0; i < 5; i ++)
	{
		num = testrecv(sockfd, &client, cbuf);

                ip = ((struct sockaddr_in*)&client)->sin_addr.s_addr;
                printf("\tSERVER: RECV:\tsrc=%08x\tlen=%8d\tamp=%8u\n",
                        ip, num, buf[0]);
                cc = cudp_conn_search(ip);
                if (!cc)
                        printf("\tSERVER: Error: Connection does not exist\n");
                else
                        cudp_conn_summary(cc);

                amp = buf[0];
                num *= amp;

                cbuf->msglen = num;
                num = testsend(sockfd, &client, cbuf, num, amp);

                ip = ((struct sockaddr_in*)&client)->sin_addr.s_addr;
                printf("\tSERVER: SEND:\tsrc=%08x\tlen=%8d\tamp=%8u\n",
                        ip, num, buf[0]);
                cc = cudp_conn_search(ip);
                if (!cc)
                        printf("\tSERVER: Error: Connection does not exist\n");
                else
                        cudp_conn_summary(cc);
	}

	num = testrecv(sockfd, &client, cbuf);

        ip = ((struct sockaddr_in*)&client)->sin_addr.s_addr;
        printf("\tSERVER: RECV:\tsrc=%08x\tlen=%8d\tamp=%8u\n",
		ip, num, buf[0]);

        cc = cudp_conn_search(ip);
        if (!cc)
 	       printf("\tSERVER: Error: Connection does not exist\n");
        else
               cudp_conn_summary(cc);
}

int main(void)
{
	serverframe(3, "Malicious Communication", &testcase3);
	return 0;
}


