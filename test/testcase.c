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
#include "../cudp.h"
#include "testcase.h"

void cudp_pkg_summary(struct sockaddr *saddr, struct cudp_buffer *buf)
{
        struct sockaddr_in *sin = (struct sockaddr_in*) saddr;
        u32 ip = sin->sin_addr.s_addr;
        u16 port = sin->sin_port;

        struct cudphdr *hdr = (struct cudphdr*)CUDP_HEADER(buf);
        u8 state   = hdr->state;
        u8 *cookie = hdr->cookie;

        printf("\tPKG:\t%08x:%04x\t%s\t%02x%02x%02x\n", ip, port, cudp_conn_state[state],
                cookie[0], cookie[1], cookie[2]);

}

void cudp_conn_summary(struct cudp_conn *cc)
{
        double r = 0.0;
        double amp = ((double)cc->stat.total_send_bytes)/cc->stat.total_recv_bytes;

        printf("\t\tCONN:\tsrc=%08x\ttyp=%8s\tstt=%8s\tcki=%02x%02x%02x\n",
                cc->ip, cudp_conn_type[cc->type], cudp_conn_state[cc->state],
                cc->cookie ? cc->cookie[0] : 0,
                cc->cookie ? cc->cookie[1] : 0,
                cc->cookie ? cc->cookie[2] : 0);

        if (cc->stat.total_responds > CUDP_THREASHOLD_ACCLB
                && amp > CUDP_THREASHOLD_AMPLB)
                r = (cc->stat.total_responds - CUDP_THREASHOLD_ACCLB)
                        * (amp - CUDP_THREASHOLD_AMPLB);

        printf("\t\tSTAT:\trcv=%8lu\tsnd=%8lu\treq=%8lu\trsp=%8lu\trat=%8f\n",
                cc->stat.total_recv_bytes, cc->stat.total_send_bytes,
                cc->stat.total_requests, cc->stat.total_responds, r);
}

void clientframe(int number, char *message, void (*testfunc)(int sockfd, struct sockaddr *server, struct cudp_buffer *cbuf))
{
	int sockfd;
	struct addrinfo *serverinfo, *server, *clientinfo, *client;
	struct cudp_buffer *cbuf;

	struct addrinfo hints;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	// Retrieve server info
	if ((rv = getaddrinfo(SERVERIP, SERVERPORT, &hints, &serverinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		goto error;
	}

	// loop through all the results and make a socket
	for( server = serverinfo; server != NULL; server = server->ai_next) {
		if ((sockfd = socket(server->ai_family, server->ai_socktype,
				server->ai_protocol)) == -1) {
			perror("socket");
			continue;
		}
		break;
	}

	if (server == NULL) {
		fprintf(stderr, "failed to create socket\n");
		goto error;
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	// Retrieve client info
	char tmp[10];
	sprintf(tmp, "%u", CLIENTPORT + number);
	if ((rv = getaddrinfo(NULL, tmp, &hints, &clientinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		goto error;
	}

	// loop through all the results and make a socket
	for (client = clientinfo; client != NULL; client = client->ai_next) {
		if (bind(sockfd, client->ai_addr, client->ai_addrlen) == -1) {
			perror("client: bind");
			continue;
		}
		break;
	}

	if (client == NULL) {
		close(sockfd);
		fprintf(stderr, "failed to create socket\n");
		goto error;
	}

	// initialize cudp buffer
	cbuf = cudp_buffer_init(MAXBUFLEN);

	printf("TEST CASE %d: %s\n", number, message);
	// running test
	(*testfunc)(sockfd, server->ai_addr, cbuf);
	printf("====================\n\n");

	goto final;

error:	
	printf("TEST CASE %d: Initialization Error!\n", number);

final:
	// finalization
	freeaddrinfo(serverinfo);
	freeaddrinfo(clientinfo);
        close(sockfd);
        cudp_buffer_destroy(cbuf);
}

void serverframe(int number, char *message, void (*testfunc)(int sockfd, struct sockaddr *server, struct cudp_buffer *cbuf))
{
	int sockfd;
        struct addrinfo *serverinfo, *server;
        struct cudp_buffer *cbuf;

        struct addrinfo hints;
	int rv;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET; // set to AF_INET to force IPv4
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE; // use my IP

        if ((rv = getaddrinfo(NULL, SERVERPORT, &hints, &serverinfo)) != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                goto error;
        }

	// loop through all the results and bind to the first we can
        for(server = serverinfo; server != NULL; server = server->ai_next) {
                if ((sockfd = socket(server->ai_family, server->ai_socktype,
                                server->ai_protocol)) == -1) {
                        perror("server: socket");
                        continue;
                } 

                if (bind(sockfd, server->ai_addr, server->ai_addrlen) == -1) {
                        close(sockfd);
                        perror("server: bind");
                        continue;
                }

                break;
        }

        if (server == NULL) {
                fprintf(stderr, "server: failed to bind socket\n");
                goto error;
        }

	// initialize cudp buffer
        cbuf = cudp_buffer_init(MAXBUFLEN);

        printf("TEST CASE %d: %s\n", number, message);
	// running test
	(*testfunc)(sockfd, server->ai_addr, cbuf);
	printf("====================\n\n");

	goto final;

error:
	printf("TEST CASE %d: Initialization Error!\n", number);

final:
	// finalize
        freeaddrinfo(serverinfo);
	close(sockfd);
        cudp_buffer_destroy(cbuf);
}

int testsend(int sockfd, struct sockaddr *opposite, struct cudp_buffer *cbuf,
		const unsigned int len, const unsigned char amp)
{
	int res;
	u8* buf = CUDP_MESSAGE(cbuf);

	memset(buf, 0, len * sizeof(u8));
	buf[0] = amp;

        cbuf->msglen = len;
        if ((res = cudp_sendto(sockfd, cbuf, 0, opposite,
		sizeof(struct sockaddr))) == -1) {
                perror("sendto");
                return -1;
        }

	return res;
}

int testrecv(int sockfd, struct sockaddr *opposite, struct cudp_buffer *cbuf)
{
	int res, addrlen = sizeof(struct sockaddr);
	u8 *buf = CUDP_MESSAGE(cbuf);

	if ((res = cudp_recvfrom(sockfd, cbuf, 0, opposite,
		&addrlen)) == -1) {
                perror("recvfrom");
                return -1;
        }

	return res;
}

