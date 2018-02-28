#include "cudp.h"

#include <memory.h>
#include <malloc.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

static u8 *cudp_privkey = NULL;

struct cudp_conn *connections = NULL;
struct cudp_conn_stat *statistic = NULL;

pthread_rwlock_t *lock = NULL;

struct cudphdr cfmhdr;

// CUDP connection basic operation
struct cudp_conn* cudp_conn_init(u32 ip, int type);
void cudp_conn_destroy(struct cudp_conn *cc);

struct cudp_conn* cudp_conn_search(u32 ip);
void cudp_conn_add(struct cudp_conn *cc);
void cudp_conn_del(struct cudp_conn *cc);
void cudp_conn_iter(void (*func)(struct cudp_conn *cc));

// CUDP protocol basic operation
void cudp_conn_fillhdr(struct cudp_conn *cc, struct cudp_buffer *buf);
void cudp_conn_proxy(struct cudp_conn *cc, struct cudp_buffer *buf);
void cudp_stat_update(struct cudp_conn *cc, struct cudp_buffer *buf, int i);
void cudp_conn_confirm(int sockfd, int flags, const struct sockaddr *dst_addr,
			socklen_t addrlen, struct cudp_conn *cc);

// Miscellaneous
int cudp_cookie_compare(struct cudp_conn *cc, struct cudp_buffer *buf);
void cudp_conn_cookie(struct cudphdr *hdr, struct cudp_conn *cc);
void cudp_cookie_gen(struct cudp_conn *cc);
void cudp_conn_clear();

// void cudp_conn_summary(struct cudp_conn *cc);
// void cudp_pkg_summary(struct sockaddr *saddr, struct cudp_buffer *buf);

u8* cudp_random()
{
	u8 *rn = (u8*)malloc(CUDP_PRIVKEYLEN * sizeof(u8));

	int rf = open("/dev/random", O_RDONLY);
	int len = 0, res;

	while (len < CUDP_PRIVKEYLEN)
	{
		res = read(rf, rn + len, CUDP_PRIVKEYLEN - len);
		len += res;
	}

	return rn;
}

ssize_t cudp_sendto(int sockfd, struct cudp_buffer *cbuf, int flags,
                        const struct sockaddr *dst_addr, socklen_t addrlen)
{
	int res;
	struct sockaddr_in *sin = (struct sockaddr_in*) dst_addr;

	u32 ip = sin->sin_addr.s_addr;
	struct cudp_conn *cc = cudp_conn_search(ip);
	if (!cc)
	{
		cc = cudp_conn_init(ip, CUDP_CONN_TYPE_CLIENT);
		cudp_conn_add(cc);
		// add expiration time event
	}

	// update statistic
	if (cc->type == CUDP_CONN_TYPE_SERVER)
		cudp_stat_update(cc, cbuf, CUDP_SENDMSG);

	// error: msglen = 0
	cudp_conn_fillhdr(cc, cbuf);

	res = sendto(sockfd, CUDP_HEADER(cbuf), cbuf->msglen, flags, dst_addr, addrlen);

if(res < 0)
perror("sendto");

	if (res < sizeof(struct cudphdr))
		return res;
	return res - sizeof(struct cudphdr);
}

ssize_t cudp_recvfrom(int sockfd, struct cudp_buffer *cbuf, int flags,
                        struct sockaddr *src_addr, socklen_t *addrlen)
{
	int res;
	struct sockaddr_in *sin;
	u32 ip;
	struct cudp_conn *cc;
	
repeat:
	res = recvfrom(sockfd, CUDP_HEADER(cbuf), cbuf->buflen, flags, src_addr, addrlen);
	if (res < sizeof(struct cudphdr))
		return res;
	cbuf->msglen = res;

	sin = (struct sockaddr_in*) src_addr;
	ip = sin->sin_addr.s_addr;

	cc = cudp_conn_search(ip);
	if (!cc)
	{
		cc = cudp_conn_init(ip, CUDP_CONN_TYPE_SERVER);
		cudp_conn_add(cc);
		// add expiration time event
	}

	cudp_conn_proxy(cc, cbuf);
	if (cbuf->msglen == 0)
	{
		cudp_conn_confirm(sockfd, flags, src_addr, *addrlen, cc);
		goto repeat;
	}

	if (cc->type == CUDP_CONN_TYPE_SERVER)
		cudp_stat_update(cc, cbuf, CUDP_RECVMSG);
	
	return cbuf->msglen;
}

int cudp_stop(int sockfd, struct cudp_buffer *cbuf, int flags,
		const struct sockaddr *dst_addr, socklen_t addrlen)
{
	int res;

	struct sockaddr_in *sin = (struct sockaddr_in*) dst_addr;
	u32 ip = sin->sin_addr.s_addr;
	u16 port = sin->sin_port;

	struct cudphdr *hdr = (struct cudphdr*)CUDP_HEADER(cbuf);
	struct cudp_conn *cc = cudp_conn_search(ip);
	if (!cc)
	{
                cc = cudp_conn_init(ip, CUDP_CONN_TYPE_CLIENT);
                cudp_conn_add(cc);
                // add expiration time event
        }

	cc->state = CUDP_CONN_STATE_STOP;
	hdr->state = CUDP_CONN_STATE_STOP;
	if (!cc->cookie)
		cudp_conn_cookie(hdr, NULL);
	else
		cudp_conn_cookie(hdr, cc);

	res = sendto(sockfd, CUDP_HEADER(cbuf), sizeof(struct cudphdr), flags, dst_addr, addrlen);
	if (res < sizeof(struct cudphdr))
		return res;
	return 1;
}

int cudp_wait(int sockfd, struct cudp_buffer *cbuf, int flags,
		struct sockaddr *src_addr, socklen_t *addrlen)
{
	int res;
        struct sockaddr_in *sin;
        u32 ip;
        struct cudp_conn *cc;

repeat:
        res = recvfrom(sockfd, CUDP_HEADER(cbuf), cbuf->buflen, flags, src_addr, addrlen);
        if (res < sizeof(struct cudphdr))
	{
		cudp_stop(sockfd, cbuf, flags, src_addr, *addrlen);
                goto repeat;
	}
        cbuf->msglen = res;

        sin = (struct sockaddr_in*) src_addr;
        ip = sin->sin_addr.s_addr;

        cc = cudp_conn_search(ip);
        if (!cc)
		return -1;

	struct cudphdr *hdr = (struct cudphdr*)CUDP_HEADER(cbuf);
	if (hdr->state == CUDP_CONN_STATE_STOP)
		return 0;

	if (!cc->cookie)
		cc->cookie = (u8*)malloc(3 * sizeof(u8));
	memcpy(cc->cookie, hdr->cookie, 3 * sizeof(u8));
	cudp_stop(sockfd, cbuf, flags, src_addr, *addrlen);
	goto repeat;
}

struct cudp_buffer* cudp_buffer_init(u32 length)
{
	struct cudp_buffer *buf = (struct cudp_buffer*)malloc(sizeof(struct cudp_buffer));

	buf->buflen = length * sizeof(u8) + sizeof(struct cudphdr);
	buf->msglen = 0;

	buf->hdr = (u8*)malloc(buf->buflen);
	buf->buf = buf->hdr + sizeof(struct cudphdr);

	return buf;
}

void cudp_buffer_destroy(struct cudp_buffer *buf)
{
	if (!buf)
	{
		free(buf->hdr);
		free(buf);
	}
}

struct cudp_conn* cudp_conn_init(u32 ip, int type)
{
	struct cudp_conn *cc = (struct cudp_conn*)malloc(sizeof(struct cudp_conn));

	cc->ip		= ip;

	cc->type	= type;
	cc->state	= CUDP_CONN_STATE_NORMAL;

	cc->cookie	= NULL;
	memset(&cc->stat, 0, sizeof(struct cudp_conn_stat));
	cc->timeout	= 0;

	return cc;
}

void cudp_conn_destroy(struct cudp_conn *cc)
{
	free(cc->cookie);
	free(cc);
}

struct cudp_conn* cudp_conn_search(u32 ip)
{
	struct cudp_conn *cc;

	if (!lock)
	{
		lock = (pthread_rwlock_t*)malloc(sizeof(pthread_rwlock_t));
		pthread_rwlock_init(lock, NULL);
	}
	
	pthread_rwlock_rdlock(lock);
	HASH_FIND_INT(connections, &ip, cc);
	pthread_rwlock_unlock(lock);

	return cc;
}

int cudp_conn_number()
{
	return HASH_COUNT(connections);
}

void cudp_conn_add(struct cudp_conn *cc)
{
	if (!cc)
		return;

	if (!lock)
	{
		lock = (pthread_rwlock_t*)malloc(sizeof(pthread_rwlock_t));
		pthread_rwlock_init(lock, NULL);
	}
	
	pthread_rwlock_wrlock(lock);
	HASH_ADD_INT(connections, ip, cc);
	pthread_rwlock_unlock(lock);
}

void cudp_conn_del(struct cudp_conn *cc)
{
	if (!cc)
		return;

	if (!lock)
	{
		lock = (pthread_rwlock_t*)malloc(sizeof(pthread_rwlock_t));
		pthread_rwlock_init(lock, NULL);
	}
	
	pthread_rwlock_wrlock(lock);
	HASH_DEL(connections, cc);
	pthread_rwlock_unlock(lock);

	cudp_conn_destroy(cc);
}

void cudp_conn_iter(void (*func)(struct cudp_conn *cc))
{
	struct cudp_conn *cc, *tmp;

	if (!lock)
	{
		lock = (pthread_rwlock_t*)malloc(sizeof(pthread_rwlock_t));
		pthread_rwlock_init(lock, NULL);
	}
	
	pthread_rwlock_wrlock(lock);
	HASH_ITER(hh, connections, cc, tmp)
		(*func)(cc);
	pthread_rwlock_unlock(lock);
}

void cudp_conn_fillhdr(struct cudp_conn *cc, struct cudp_buffer *buf)
{
	struct cudphdr *hdr = (struct cudphdr*)buf->hdr;

	hdr->state = cc->state;
	if (cc->state == CUDP_CONN_STATE_NORMAL)
		cudp_conn_cookie(hdr, NULL);
	else
		cudp_conn_cookie(hdr, cc);
	buf->msglen += sizeof(struct cudphdr);
}

void cudp_conn_proxy(struct cudp_conn *cc, struct cudp_buffer *buf)
{
	struct cudphdr *hdr = (struct cudphdr*)buf->hdr;

	if (cc->type == CUDP_CONN_TYPE_CLIENT)
	{
		if (hdr->state != CUDP_CONN_STATE_NORMAL)
		{
			if (!cc->cookie)
				cc->cookie = (u8*)malloc(3 * sizeof(u8));
			memcpy(cc->cookie, hdr->cookie, 3 * sizeof(u8));
		}
		
		if (hdr->state != CUDP_CONN_STATE_STOP)
			cc->state = hdr->state; // update state

		// Only client actively send a stop, the state of client will become stop.
		//else if (cc->state == CUDP_CONN_STATE_STOP)
		//	goto zero;
	}
	else if (cc->type == CUDP_CONN_TYPE_SERVER)
	{
		// If server received a stop message
		if (hdr->state == CUDP_CONN_STATE_STOP)
		{
			if (!cc->cookie)
				cudp_cookie_gen(cc);
			if (!cudp_cookie_compare(cc, buf))
				cc->state = CUDP_CONN_STATE_STOP;
			else
				goto zero;
		}
		else if (cc->state == CUDP_CONN_STATE_MALICIOUS
			|| cc->state == CUDP_CONN_STATE_STOP)
		{
			if (cudp_cookie_compare(cc, buf))
				goto zero;
			
		}
	}
	else
		goto zero;

	buf->msglen -= sizeof(struct cudphdr);
	return;

zero:
	buf->msglen = 0;
}

int cudp_cookie_compare(struct cudp_conn *cc, struct cudp_buffer *buf)
{
	struct cudphdr *hdr = (struct cudphdr*)buf->hdr;
	return memcmp(cc->cookie, hdr->cookie, 3 * sizeof(u8));
}

void cudp_conn_cookie(struct cudphdr *hdr, struct cudp_conn *cc)
{
	// cc == NULL, copy zero to hdr->cookie
	if (!cc)
		memset(hdr->cookie, 0, 3 * sizeof(u8));
	else if (!cc->cookie)
	{
		if (cc->type == CUDP_CONN_TYPE_SERVER)
		{
			cudp_cookie_gen(cc);
			memcpy(hdr->cookie, cc->cookie, 3 * sizeof(u8));
		}
		else if (cc->type == CUDP_CONN_TYPE_SERVER)
			memset(hdr->cookie, 0, 3 * sizeof(u8));
	}
	else
		memcpy(hdr->cookie, cc->cookie, 3 * sizeof(u8));
}

void cudp_cookie_gen(struct cudp_conn *cc)
{
	u8 cookie[CUDP_PRIVKEYLEN];
	u8 hash[SHA_DIGEST_LENGTH];

	if (!cudp_privkey)
		cudp_privkey = cudp_random();

	size_t padlen = sizeof(cc->ip);
	memcpy(cookie, cudp_privkey, CUDP_PRIVKEYLEN * sizeof(u8));
	memcpy(cookie + CUDP_PRIVKEYLEN - padlen, &cc->ip, padlen);

	SHA1(cookie, CUDP_PRIVKEYLEN, hash);

	cc->cookie = (u8*)malloc(3 * sizeof(u8));
	memcpy(cc->cookie, hash, 3 * sizeof(u8));
}

void cudp_stat_update(struct cudp_conn *cc, struct cudp_buffer *buf, int i)
{
	if (i == CUDP_SENDMSG)
	{
		cc->stat.total_send_bytes += buf->msglen;
		cc->stat.total_responds ++;
	}
	else if (i == CUDP_RECVMSG)
	{
		cc->stat.total_recv_bytes += buf->msglen;
		cc->stat.total_requests ++;
	}
	else
		return;

	if (cc->stat.total_responds >= CUDP_THREASHOLD_ACCLB)
	{
		double amp = ((double)cc->stat.total_send_bytes)/cc->stat.total_recv_bytes;
		if (amp >= CUDP_THREASHOLD_AMPLB)
		{
			double nf = (cc->stat.total_responds-CUDP_THREASHOLD_ACCLB)
					* (amp - CUDP_THREASHOLD_AMPLB);
			if ( nf >= CUDP_THREASHOLD_MALICIOUS)
				cc->state = CUDP_CONN_STATE_MALICIOUS;
			else if (nf >= CUDP_THREASHOLD_SUSPICIOUS)
				cc->state = CUDP_CONN_STATE_SUSPICIOUS;
		}
	}
}

void cudp_conn_confirm(int sockfd, int flags, const struct sockaddr *dst_addr,
			socklen_t addrlen, struct cudp_conn *cc)
{
	int res;

	// update statistic
	if (cc->type == CUDP_CONN_TYPE_SERVER)
	{
		cfmhdr.state = cc->state;
		memcpy(cfmhdr.cookie, cc->cookie, 3 * sizeof(u8));

		res = sendto(sockfd, &cfmhdr, sizeof(struct cudphdr), flags,
			dst_addr, addrlen);
	}
}

void cudp_conn_clear()
{
	cudp_conn_iter(&cudp_conn_del);
}

/*
void cudp_conn_summary(struct cudp_conn *cc)
{
	printf("\t\tCONN:\t%u\t%s\t%s\t%u%u%u\n",
		cc->ip,	cudp_conn_type[cc->type], cudp_conn_state[cc->state],
		cc->cookie[0], cc->cookie[1], cc->cookie[2]);
}

void cudp_pkg_summary(struct sockaddr *saddr, struct cudp_buffer *buf)
{
	struct sockaddr_in *sin = (struct sockaddr_in*) saddr;
        u32 ip = sin->sin_addr.s_addr;
        u16 port = sin->sin_port;

	struct cudphdr *hdr = (struct cudphdr*)CUDP_HEADER(buf);
	u8 state   = hdr->state;
	u8 *cookie = hdr->cookie;

	printf("PKG:\t%u:%u\t%s\t%u%u%u\n", ip, port, cudp_conn_state[state],
		cookie[0], cookie[1], cookie[2]);

}
*/
