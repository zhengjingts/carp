#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "../cudp.h"
#include "evalcase.h"

#define EVALLOG "../log/latency.udp"
#define EVALNUM 1000
#define EVALBUF 1000

void logger(const char *fn, const struct timeval *tv, int len)
{
	FILE *log = fopen(fn, "w");
	if (!log)
	{
		perror("fopen");
		return;
	}
	
	int i;
	for (i = 0; i < len; i ++)
		fprintf(log, "%lu%lu\n", tv[i].tv_sec, tv[i].tv_usec);
	
	fclose(log);
}

void sender(int sockfd, struct sockaddr *server)
{
	printf("Sender Start:\n");

	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	if (sched_setaffinity(0, sizeof(mask), &mask) < 0)
		perror("sched_setaffinity");

	int i, num, len = sizeof(struct sockaddr);
	char buf[EVALBUF];
	struct timeval start[EVALNUM], end[EVALNUM];
	for (i = 0; i < EVALNUM; i ++)
	{
		gettimeofday(start + i, NULL);
		num = sendto(sockfd, buf, 1, 0, server, len);
	}

	char fn[100];
	sprintf(fn, "%s.%s.log", EVALLOG, "beg");
	logger(fn, start, EVALNUM);
}

void recver(int sockfd, struct sockaddr *client)
{
	printf("Recver Start:\n");
	
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(1, &mask);
	if (sched_setaffinity(0, sizeof(mask), &mask) < 0)
		perror("sched_setaffinity");

	int i, num, len = sizeof(struct sockaddr);
	char buf[EVALBUF];
	struct timeval start[EVALNUM], end[EVALNUM];
	for (i = 0; i < EVALNUM; i ++)
	{
		num = recvfrom(sockfd, buf, 1, 0, client, &len);
		gettimeofday(end, NULL);
	}

	char fn[100];
	sprintf(fn, "%s.%s.log", EVALLOG, "end");
	logger(fn, end, EVALNUM);
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
		if (recver_pid)
			wait(&recver_pid);
		if (sender_pid)
			wait(&sender_pid);

		freeaddrinfo(cinfo);
		freeaddrinfo(sinfo);
	}
}
