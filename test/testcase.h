#ifndef TESTCASE_H
#define TESTCASE_H

#define NORMALMSG 	"NORMAL"
#define SUSPICIOUSMSG	"SUSPICIOUS"
#define MALICIOUSMSG	"MALICIOUS"

#define SERVERIP	"localhost"
#define SERVERPORT	"12345"

#define CLIENTIP	"localhost"
#define CLIENTPORT	20000

#define MAXBUFLEN	2000

#define MESSAGELEN	100

void clientframe(int number, char *message,
	void (*testfunc)(int sockfd, struct sockaddr *server, struct cudp_buffer *cbuf));
void serverframe(int number, char *message,
	void (*testfunc)(int sockfd, struct sockaddr *server, struct cudp_buffer *cbuf));

int testsend(int sockfd, struct sockaddr *opposite, struct cudp_buffer *cbuf,
        const unsigned int len, const unsigned char amp);
int testrecv(int sockfd, struct sockaddr *opposite, struct cudp_buffer *cbuf);

void cudp_pkg_summary(struct sockaddr *saddr, struct cudp_buffer *buf);
void cudp_conn_summary(struct cudp_conn *cc);

#endif
