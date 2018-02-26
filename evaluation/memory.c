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
#include <netinet/ip.h>
#include <netinet/udp.h>
#include "../cudp.h"
#include "evalcase.h"

#define EVALLOG "../log/memory"
#define EVALWAIT 2000000
#define EVALBATCH 1000
#define EVALBUF  1000

static int *sig;
static int *fin;
static int *wat;

static FILE *log;

struct packet
{
	struct ip	ih;
	struct udphdr	uh;
	struct cudphdr	ch;
	u8 data[100];
};

void logger(pid_t pid, int num)
{
	char fn[100];
	sprintf(fn, "/proc/%d/statm", pid);
	
	FILE *fs = fopen(fn, "r");
	if (!fs)
	{
		perror("fopen");
		return;
	}

	unsigned long size, resident, share, text, lib, data, dt;
	fscanf(fs, "%ld %ld %ld %ld %ld %ld %ld", &size, &resident,
		&share, &text, &lib, &data, &dt);

	printf("PID %d: Size = %ld, Conn = %d\n", pid, size, num);
	fclose(fs);

	fprintf(log, "%d %ld %ld %ld %ld %ld %ld %ld\n", num, size,
		resident, share, text, lib, data, dt);	
}

int translate(char* addr)
{
	int ip_value = 0, i, j;
	char part[100];

	i = 0;
	while(1)
	{
		for(j = 0; addr[i] != '.' && addr[i] != '\0'; i ++, j ++)
			part[j] = addr[i];
		part[j] = '\0';

		int v = atoi(part);
		ip_value = ((ip_value >> 8) & 0x00FFFFFF) | (v << 24);

		if(addr[i] == '\0')
			break;
		i ++;
	}

	return ip_value;
}

void sender(struct sockaddr *server)
{
	cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(0, &mask);
        if (sched_setaffinity(0, sizeof(mask), &mask) < 0)
                perror("sched_setaffinity");

        int sockfd, i, len = sizeof(struct sockaddr);
	int ihlen = sizeof(struct ip), uhlen = sizeof(struct udphdr);
	int chlen = sizeof(struct cudphdr), datlen = 1;

	struct packet p;
	memset(&p, 0, sizeof(struct packet));

	p.ih.ip_hl	=	5;
	p.ih.ip_v	=	4;
	p.ih.ip_tos	=	0;
	p.ih.ip_len	=	htons (ihlen + uhlen + chlen + datlen);
	p.ih.ip_id	=	htons (0);
	p.ih.ip_off	=	htons (0);
	p.ih.ip_ttl	=	255;
	p.ih.ip_p	=	IPPROTO_UDP;

	p.ih.ip_src.s_addr = translate("10.0.0.1");
	p.ih.ip_dst.s_addr = ((struct sockaddr_in*)server)->sin_addr.s_addr;

	p.ih.ip_sum	=	0;

	p.uh.source	=	htons(CLIPORTN);
	p.uh.dest	=	htons(SERPORTN);
	p.uh.len	=	htons(uhlen + chlen + datlen);

	if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP)) == -1)
	{
		perror("socket");
		return;
	}

	int on = 1;
	if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0)
	{
		perror("setsockopt");
		return;
	}

	int nc = 0;
        while(*sig)
	{
		while (nc == *sig)
			;
		
		printf("Sender Start: size = %d\n", *sig);

		for (; nc < *sig && *sig != 0; nc ++)
		{
			p.ih.ip_src.s_addr += 256;
                	sendto(sockfd, &p, ihlen + uhlen + chlen + datlen,
				0, server, len);
			while (!*wat)
				;
			*wat = 0;
		}
	}
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

	int nc = 0;
	while (*sig)
	{
		while (nc == *sig)
			;
		
		printf("Recver Start: size = %d\n", *sig);

		for (; nc < *sig && *sig != 0; nc ++, *wat = 1)
			cudp_recvfrom(sockfd, cbuf, 0, client, &len);
		*fin = 1;
		printf("Recver Conn: num = %d\n", cudp_conn_number());
	}
	cudp_buffer_destroy(cbuf);
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
	*sig = EVALBATCH;

	fin = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*fin = 0;

	wat = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*wat = 0;

	log = fopen(EVALLOG, "w");
	if (!log)
	{
		perror("fopen");
		return 1;
	}

	init_addrinfo(&sinfo, SERVERPORT);
	init_addrinfo(&cinfo, CLIENTPORT);

	serverfd = init_socket(sinfo, &s);
	clientfd = init_socket(cinfo, &c);

	init_bind(serverfd, s);
	// init_bind(clientfd, c);
	
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
				sender(s->ai_addr);
				break;
			}
		}
		else
			printf("Error: fork() failed.\n");
	}

	if (res > 0)
	{
		for (i = 1; i <= 1000; i ++)
		{
			*sig = i * EVALBATCH;

			while (!*fin)
				;
			*fin = 0;

			logger(recver_pid, *sig);
		}

		*sig = 0;

		if (recver_pid)
			wait(&recver_pid);
		if (sender_pid)
			wait(&sender_pid);

		freeaddrinfo(cinfo);
		freeaddrinfo(sinfo);
		fclose(log);
	}
}
