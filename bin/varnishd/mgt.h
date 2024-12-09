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
 */

#include "common.h"
#include "miniobj.h"

#include "libvarnish.h"

struct cli;

extern struct evbase	*mgt_evb;

/* mgt_child.c */
void mgt_run(int dflag, const char *T_arg);
extern pid_t mgt_pid, child_pid;

/* mgt_cli.c */

void mgt_cli_init(void);
void mgt_cli_setup(int fdi, int fdo, int verbose);
int mgt_cli_askchild(unsigned *status, char **resp, const char *fmt, ...);
void mgt_cli_start_child(int fdi, int fdo);
void mgt_cli_stop_child(void);
int mgt_cli_telnet(const char *T_arg);

/* mgt_param.c */
void MCF_ParamSync(void);
void MCF_ParamInit(struct cli *);
void MCF_ParamSet(struct cli *, const char *param, const char *val);

/* mgt_vcc.c */
void mgt_vcc_init(void);
int mgt_vcc_default(const char *bflag, const char *fflag, int f_fd, int Cflag);
int mgt_push_vcls_and_start(unsigned *status, char **p);

#include "stevedore.h"

extern struct stevedore sma_stevedore;
extern struct stevedore smf_stevedore;

#include "hash_slinger.h"

extern struct hash_slinger hsl_slinger;
extern struct hash_slinger hcl_slinger;
