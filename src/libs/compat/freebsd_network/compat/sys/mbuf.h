/*
 * Copyright 2007, Hugo Santos. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_MBUF_H_
#define _FBSD_COMPAT_SYS_MBUF_H_


#include <sys/malloc.h>
#include <sys/param.h>


#define MLEN		((int)(MSIZE - sizeof(struct m_hdr)))
#define MHLEN		((int)(MSIZE - sizeof(struct pkthdr)))

#define MINCLSIZE	(MHLEN + 1)

struct m_hdr {
	struct mbuf *	mh_next;
	struct mbuf *	mh_nextpkt;
	caddr_t			mh_data;
	int				mh_len;
	int				mh_flags;
	short			mh_type;
};

struct pkthdr {
	struct ifnet *	rcvif;
	int				len;
	int				csum_flags;
	int				csum_data;
	uint16_t		tso_segsz;
	uint16_t		ether_vtag;
};

struct m_ext {
	caddr_t			ext_buf;
	unsigned int	ext_size;
	int				ext_type;
};

struct mbuf {
	struct m_hdr m_hdr;
	union {
		struct {
			struct pkthdr	MH_pkthdr;
			union {
				struct m_ext	MH_ext;
				char			MH_databuf[MHLEN];
			} MH_dat;
		} MH;
		char M_databuf[MLEN];
	} M_dat;
};

#define m_next      m_hdr.mh_next
#define m_len       m_hdr.mh_len
#define m_data      m_hdr.mh_data
#define m_type      m_hdr.mh_type
#define m_flags     m_hdr.mh_flags
#define m_nextpkt   m_hdr.mh_nextpkt
#define m_act       m_nextpkt
#define m_pkthdr    M_dat.MH.MH_pkthdr
#define m_ext       M_dat.MH.MH_dat.MH_ext
#define m_pktdat    M_dat.MH.MH_dat.MH_databuf
#define m_dat       M_dat.M_databuf


#define M_DONTWAIT		M_NOWAIT
#define M_TRYWAIT		M_WAITOK
#define M_WAIT			M_WAITOK

#define MT_DATA			1

#define M_EXT			0x0001
#define M_PKTHDR		0x0002
#define	M_RDONLY		0x0008

#define EXT_CLUSTER		1
#define EXT_PACKET		3
#define	EXT_NET_DRV		100

#define M_BCAST			0x00000200
#define M_MCAST			0x00000400
#define	M_VLANTAG		0x00010000

#define CSUM_IP			0x0001
#define CSUM_TCP		0x0002
#define CSUM_UDP		0x0004
#define	CSUM_TSO		0x0020
#define CSUM_IP_CHECKED	0x0100
#define CSUM_IP_VALID	0x0200
#define CSUM_DATA_VALID	0x0400
#define CSUM_PSEUDO_HDR	0x0800
#define CSUM_DELAY_DATA	(CSUM_TCP | CSUM_UDP)

#define	M_MOVE_PKTHDR(to, from)	m_move_pkthdr((to), (from))
#define MGET(m, how, type)		((m) = m_get((how), (type)))
#define MGETHDR(m, how, type)	((m) = m_gethdr((how), (type)))
#define MCLGET(m, how)			m_clget((m), (how))
#define	MEXTADD(m, buf, size, free, args, flags, type) \
    m_extadd((m), (caddr_t)(buf), (size), (free), (args), (flags), (type))


struct mbuf *m_getcl(int how, short type, int flags);
void m_freem(struct mbuf *mbuf);
struct mbuf *m_free(struct mbuf *m);
struct mbuf *m_defrag(struct mbuf *m, int);
void m_adj(struct mbuf *m, int);
struct mbuf *m_pullup(struct mbuf *m, int len);
struct mbuf *m_prepend(struct mbuf *m, int, int);
void m_move_pkthdr(struct mbuf *, struct mbuf *);

u_int m_length(struct mbuf *m, struct mbuf **last);
u_int m_fixhdr(struct mbuf *m);
void m_cat(struct mbuf *m, struct mbuf *n);
void m_copydata(const struct mbuf *m, int off, int len, caddr_t cp);

struct ifnet;
struct mbuf *m_devget(char *, int, int, struct ifnet *,
	void (*)(char *, caddr_t, u_int));
void m_copyback(struct mbuf *, int, int, caddr_t);

struct mbuf *m_get(int how, short type);
struct mbuf *m_gethdr(int how, short type);
void m_clget(struct mbuf *m, int how);

void m_extadd(struct mbuf *m, caddr_t buffer, u_int size,
    void (*freeHook)(void *, void *), void *args, int flags, int type);


#define mtod(m, type)	((type)((m)->m_data))

#define m_tag_delete(mb, tag) \
	panic("m_tag_delete unsupported.");

/* Check if the supplied mbuf has a packet header, or else panic. */
#define	M_ASSERTPKTHDR(m)						\
	KASSERT(m != NULL && m->m_flags & M_PKTHDR,			\
	    ("%s: no mbuf packet header!", __func__))

#define MBUF_CHECKSLEEP(how) do { } while (0)
#define MBTOM(how) (how)

extern int max_linkhdr;
extern int max_protohdr;
extern int max_hdr;

#include <sys/mbuf-fbsd.h>

#endif	/* _FBSD_COMPAT_SYS_MBUF_H_ */
