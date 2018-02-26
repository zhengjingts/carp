#ifndef _CUDP_COMMON_H
#define _CUDP_COMMON_H

#include <sys/types.h>
#include <sys/socket.h>
#include <uthash.h>

#define CUDP_PRIVKEYLEN 16

#define CUDP_MAX_AMPLIFICATION_FACTOR 100
#define CUDP_COUNTER_BUCKET_NUM 64

#define CUDP_CONN_TYPE_CLIENT 0
#define CUDP_CONN_TYPE_SERVER 1

#define CUDP_CONN_STATE_NORMAL		0x00
#define CUDP_CONN_STATE_SUSPICIOUS	0x01
#define CUDP_CONN_STATE_MALICIOUS	0x02
#define CUDP_CONN_STATE_STOP		0x03

#define CUDP_SENDMSG 0
#define CUDP_RECVMSG 1

#define CUDP_THREASHOLD_AMPLB		3
#define CUDP_THREASHOLD_ACCLB		3
#define CUDP_THREASHOLD_SUSPICIOUS	1
#define CUDP_THREASHOLD_MALICIOUS	100

#define u8  unsigned char
#define u16 unsigned short
#define u32 unsigned int

struct cudphdr
{
	u8	state;
	u8	cookie[3];
};

/*
struct cudp_conn_opposite
{
	u32 ip;
	u16 port;
};
*/

struct cudp_counter_bucket
{
	unsigned long recv_bytes;
	unsigned long send_bytes;
};

struct cudp_conn_stat
{
	unsigned long total_recv_bytes;
	unsigned long total_send_bytes;
	unsigned long total_requests;
	unsigned long total_responds;

	unsigned int bucket_num;
	struct cudp_counter_bucket buckets[CUDP_COUNTER_BUCKET_NUM];
};

struct cudp_conn
{
	u32 ip;

	unsigned int	type;
	unsigned int	state;
	u8		*cookie;

	struct cudp_conn_stat	stat;
	unsigned long		timeout;

	UT_hash_handle hh;
};

struct cudp_buffer
{
	// pointer to head of buffer
	u8 *hdr;

	// pointer to head of user message buffer
	u8 *buf;

	// buffer length
	u32 buflen;

	// message length
	u32 msglen;
};

ssize_t cudp_recvfrom(int sockfd, struct cudp_buffer *cbuf, int flags,
			struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t cudp_sendto(int sockfd, struct cudp_buffer *cbuf, int flags,
			const struct sockaddr *dst_addr, socklen_t addrlen);
// return 1 for success
int cudp_stop(int sockfd, struct cudp_buffer *cbuf, int flags,
			const struct sockaddr *dst_addr, socklen_t addrlen);
int cudp_wait(int sockfd, struct cudp_buffer *cbuf, int flags,
			struct sockaddr *src_addr, socklen_t *addrlen);
struct cudp_conn* cudp_conn_search(u32 ip);
int cudp_conn_number();
void cudp_conn_clear();
// void cudp_conn_summary(struct cudp_conn *cc);
// void cudp_pkg_summary(struct sockaddr *saddr, struct cudp_buffer *buf);

struct cudp_buffer* cudp_buffer_init(u32 length);
void cudp_buffer_destroy(struct cudp_buffer *buf);

static char cudp_conn_state[4][100] = {"NOR", "SUS", "MAL", "STP"};
static char cudp_conn_type[2][100] = {"CLI", "SER"};

#define CUDP_MESSAGE(buffer) (buffer)->buf
#define CUDP_HEADER(buffer) (buffer)->hdr

#endif


