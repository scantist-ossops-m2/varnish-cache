/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006-2007 Linpro AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 *
 * This file contains the heritage passed when mgt forks cache
 */

#include "queue.h"

struct listen_sock {
	TAILQ_ENTRY(listen_sock)	list;
	int				sock;
	struct vss_addr			*addr;
};

TAILQ_HEAD(listen_sock_head, listen_sock);

struct heritage {

	/*
	 * Two pipe(2)'s for CLI connection between cache and mgt.
	 * cache reads [2] and writes [1].  Mgt reads [0] and writes [3].
	 */
	int				fds[4];

	/* Sockets from which to accept connections */
	struct listen_sock_head		socks;
	int				nsocks;

	/* Share memory log fd and size (incl header) */
	int				vsl_fd;
	unsigned			vsl_size;

	/* Storage method */
	struct stevedore		*stevedore;

	/* Hash method */
	struct hash_slinger		*hash;

	char				name[1024];
};

struct params {

	/* Unprivileged user / group */
	char			*user;
	uid_t			uid;
	char			*group;
	gid_t			gid;

	/* TTL used for lack of anything better */
	unsigned		default_ttl;

	/* Worker threads */
	unsigned		wthread_min;
	unsigned		wthread_max;
	unsigned		wthread_timeout;
	unsigned		wthread_pools;

	unsigned		overflow_max;

	/* Memory allocation hints */
	unsigned		mem_workspace;

	/* Acceptor hints */
	unsigned		sess_timeout;
	unsigned		pipe_timeout;
	unsigned		send_timeout;

	/* Management hints */
	unsigned		auto_restart;

	/* Fetcher hints */
	unsigned		fetch_chunksize;

	/* Sendfile object minimum size */
	unsigned		sendfile_threshold;

	/* VCL traces */
	unsigned		vcl_trace;

	/* Listen address */
	char			*listen_address;

	/* Listen depth */
	unsigned		listen_depth;

	/* Srcaddr hash */
	unsigned		srcaddr_hash;
	unsigned		srcaddr_ttl;

	/* HTTP proto behaviour */
	unsigned		backend_http11;
	unsigned		client_http11;

	/* Ping interval */
	unsigned		ping_interval;

};

extern volatile struct params *params;
extern struct heritage heritage;

void child_main(void);

int varnish_instance(const char *n_arg, char *name, size_t namelen, char *dir, size_t dirlen);