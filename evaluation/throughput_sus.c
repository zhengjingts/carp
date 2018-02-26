#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/mman.h>
#include "../cudp.h"
#include "evalcase.h"

#define EVALLOG "../log/throughput.sus"
#define EVALTIME 10000000
#define EVALWAIT 2000000
#define EVALBUF  1000

static int *sig;

void logger(const char *fn, const unsigned long *num, int len)
{
	FILE *log = fopen(fn, "w");
	if (!log)
	{
		perror("fopen");
		return;
	}

	int i;
	for (i = 0; i < len; i ++)	
		fprintf(log, "%lu\n", num[i]);
	
	fclose(log);
}

void sender(int sockfd, struct sockaddr *server)
{
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	if (sched_setaffinity(0, sizeof(mask), &mask) < 0)
		perror("sched_setaffinity");

	int i, len = sizeof(struct sockaddr);
	unsigned long num, size[100];
	struct cudp_buffer *cbuf = cudp_buffer_init(EVALBUF);
	struct sockaddr client;

	// trigger suspicious state
        for (i = 0; i < 4; i ++)
        {
                cbuf->msglen = 1;
                cudp_sendto(sockfd, cbuf, 0, server, len);
                cudp_recvfrom(sockfd, cbuf, 0, &client, &len);
        }

	for (i = 0, num = 0; *sig != 0; i ++)
	{
		while (*sig == -1)
			;

		printf("Sender Start: size = %d\n", *sig);

		int siz = *sig;
		while (*sig == siz)
		{
			cbuf->msglen = siz;
			num += cudp_sendto(sockfd, cbuf, 0, server, len);
		}
		size[i] = num;
	}
	cudp_buffer_destroy(cbuf);

	char fn[100];
	sprintf(fn, "%s.%s.log", EVALLOG, "snd");
	logger(fn, size, i);
}

void recver(int sockfd, struct sockaddr *client)
{
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(1, &mask);
	if (sched_setaffinity(0, sizeof(mask), &mask) < 0)
		perror("sched_setaffinity");

	int i, len = sizeof(struct sockaddr);
	unsigned long num = 0, size[100];
	struct cudp_buffer *cbuf = cudp_buffer_init(EVALBUF);

	// trigger suspicious state
        for (i = 0; i < 4; i ++)
        {
                cudp_recvfrom(sockfd, cbuf, 0, client, &len);
                cbuf->msglen = 10;
                cudp_sendto(sockfd, cbuf, 0, client, len);

        }

	for (i = 0, num = 0; *sig != 0; i ++)
	{
		while (*sig == -1)
			;

		printf("Recver Start: size = %d\n", *sig);

		int siz = *sig;
		while (*sig == siz)
			num += cudp_recvfrom(sockfd, cbuf, 0, client, &len);
		size[i] = num;
	}
	cudp_buffer_destroy(cbuf);

	char fn[100];
	sprintf(fn, "%s.%s.log", EVALLOG, "rcv");
	logger(fn, size, i);
}

int init_addrinfo(struct addrinfo **ais, const char *port)
{
	int res;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	if ((res = getaddrinfo(LOCALHOST, port, &hints, ais)) != 0)
	{
		perror("getaddrinfo");
		return res;
	}

	return 0;
}

int init_socket(struct addrinfo *ais, struct addrinfo **ai)
{
	int sockfd;
	
	for ( *ai = ais; *ai != NULL; *ai = (*ai)->ai_next)
	{
		if ((sockfd = socket((*ai)->ai_family, (*ai)->ai_socktype,
			(*ai)->ai_protocol)) == -1)
		{
			perror("socket");
			continue;
		}

		break;
	}

	return sockfd;
}

int init_bind(int sockfd, struct addrinfo *ai)
{
	int res;

	if ((res = bind(sockfd, ai->ai_addr, ai->ai_addrlen)) == -1)
		perror("bind");

	return res;
}

int main()
{
	int serverfd, clientfd;
	struct addrinfo *sinfo, *cinfo, *s, *c;

	pid_t sender_pid = 0, recver_pid = 0;
	int i, res;

	sig = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*sig = -1;


	init_addrinfo(&sinfo, SERVERPORT);
	init_addrinfo(&cinfo, CLIENTPORT);

	serverfd = init_socket(sinfo, &s);
	clientfd = init_socket(cinfo, &c);

	init_bind(serverfd, s);
	init_bind(clientfd, c);
	
	for (i = 0; i < 2; i ++)
	{
		res = fork();
		if (res > 0)
		{
			if (i == 0)
			{
				recver_pid = res;
				printf("Recver PID:\t%d\n", res);
			}
			else
			{
				sender_pid = res;
				printf("Sender PID:\t%d\n", res);
			}
		}
		else if (res == 0)
		{
			if (i == 0)
			{
				recver(serverfd, c->ai_addr);
				break;
			}
			else
			{
				sender(clientfd, s->ai_addr);
				break;
			}
		}
		else
			printf("Error: fork() failed.\n");
	}

	if (res > 0)
	{
		usleep(EVALWAIT);
		*sig = 1;
		usleep(EVALTIME);
		*sig = -1;
		usleep(EVALWAIT);
		*sig = 10;
		usleep(EVALTIME);
		*sig = -1;
		usleep(EVALWAIT);
		*sig = 100;
		usleep(EVALTIME);
		*sig = -1;
		usleep(EVALWAIT);
		*sig = 1000;
		usleep(EVALTIME);
		*sig = 0;

		if (recver_pid)
			wait(&recver_pid);
		if (sender_pid)
			wait(&sender_pid);

		freeaddrinfo(cinfo);
		freeaddrinfo(sinfo);
	}
}
