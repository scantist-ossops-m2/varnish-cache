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

/*
 * XXX:
 *	generate interface structure
 *
 * XXX:
 *	Better error messages, throughout.
 *	>It also accured to me that we could link the errors to the error
 *	>documentation.
 *	>
 *	>Unreferenced  function 'request_policy', first mention is
 *	>         Line 8 Pos 4
 *	>         sub request_policy {
 *	>         ----##############--
 *	>Read more about this type of error:
 *	>http://varnish/doc/error.html#Unreferenced%20function
 *	>
 *	>
 *	>         Unknown variable 'obj.bandwidth'
 *	>         At: Line 88 Pos 12
 *	>                 if (obj.bandwidth < 1 kb/h) {
 *	>         ------------#############------------
 *	>Read more about this type of error:
 *	>http://varnish/doc/error.html#Unknown%20variable
 *
 * XXX:
 *	Create proper tmp filenames for .h, .c and .o
 *
 * XXX:
 *	and all the rest...
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include "vsb.h"
#include "queue.h"

#include "vcc_priv.h"
#include "vcc_compile.h"

#include "libvcl.h"
#include "libvarnish.h"

struct method method_tab[] = {
#define VCL_RET_MAC(l,U,b,n)
#define VCL_MET_MAC(l,U,m)	{ "vcl_"#l, m, VCL_MET_##U },
#include "vcl_returns.h"
#undef VCL_MET_MAC
#undef VCL_RET_MAC
	{ NULL, 0U, 0}
};

/*--------------------------------------------------------------------*/

static const char *vcc_default_vcl_b, *vcc_default_vcl_e;

/*--------------------------------------------------------------------*/

void
TlFree(struct tokenlist *tl, void *p)
{
	struct membit *mb;

	mb = calloc(sizeof *mb, 1);
	assert(mb != NULL);
	mb->ptr = p;
	TAILQ_INSERT_TAIL(&tl->membits, mb, list);
}


void *
TlAlloc(struct tokenlist *tl, unsigned len)
{
	void *p;

	p = calloc(len, 1);
	assert(p != NULL);
	TlFree(tl, p);
	return (p);
}

/*--------------------------------------------------------------------*/

int
IsMethod(const struct token *t)
{
	struct method *m;

	for(m = method_tab; m->name != NULL; m++) {
		if (vcc_IdIs(t, m->name))
			return (m - method_tab);
	}
	return (-1);
}

/*--------------------------------------------------------------------
 * Printf output to the two vsbs, possibly indented
 */

void
Fh(const struct tokenlist *tl, int indent, const char *fmt, ...)
{
	va_list ap;

	if (indent)
		vsb_printf(tl->fh, "%*.*s", tl->hindent, tl->hindent, "");
	va_start(ap, fmt);
	vsb_vprintf(tl->fh, fmt, ap);
	va_end(ap);
}

void
Fb(const struct tokenlist *tl, int indent, const char *fmt, ...)
{
	va_list ap;

	assert(tl->fb != NULL);
	if (indent)
		vsb_printf(tl->fb, "%*.*s", tl->indent, tl->indent, "");
	va_start(ap, fmt);
	vsb_vprintf(tl->fb, fmt, ap);
	va_end(ap);
}

void
Fc(const struct tokenlist *tl, int indent, const char *fmt, ...)
{
	va_list ap;

	if (indent)
		vsb_printf(tl->fc, "%*.*s", tl->indent, tl->indent, "");
	va_start(ap, fmt);
	vsb_vprintf(tl->fc, fmt, ap);
	va_end(ap);
}

void
Fi(const struct tokenlist *tl, int indent, const char *fmt, ...)
{
	va_list ap;

	if (indent)
		vsb_printf(tl->fi, "%*.*s", tl->iindent, tl->iindent, "");
	va_start(ap, fmt);
	vsb_vprintf(tl->fi, fmt, ap);
	va_end(ap);
}

void
Ff(const struct tokenlist *tl, int indent, const char *fmt, ...)
{
	va_list ap;

	if (indent)
		vsb_printf(tl->ff, "%*.*s", tl->findent, tl->findent, "");
	va_start(ap, fmt);
	vsb_vprintf(tl->ff, fmt, ap);
	va_end(ap);
}

/*--------------------------------------------------------------------*/

static void
EncString(struct vsb *sb, const char *b, const char *e, int mode)
{

	if (e == NULL)
		e = strchr(b, '\0');

	vsb_cat(sb, "\"");
	for (; b < e; b++) {
		switch (*b) {
		case '\\':
		case '"':
			vsb_printf(sb, "\\%c", *b);
			break;
		case '\n':
			vsb_printf(sb, "\\n");
			if (mode) 
				vsb_printf(sb, "\"\n\t\"");
			break;
		case '\t': vsb_printf(sb, "\\t"); break;
		case '\r': vsb_printf(sb, "\\r"); break;
		case ' ': vsb_printf(sb, " "); break;
		default:
			if (isgraph(*b))
				vsb_printf(sb, "%c", *b);
			else
				vsb_printf(sb, "\\%03o", *b);
			break;
		}
	}
	vsb_cat(sb, "\"");
}

void
EncToken(struct vsb *sb, const struct token *t)
{

	assert(t->tok == CSTR);
	EncString(sb, t->dec, NULL, 0);
}

/*--------------------------------------------------------------------
 * Output the location/profiling table.  For each counted token, we
 * record source+line+charpos for the first character in the token.
 */

static void
LocTable(const struct tokenlist *tl)
{
	struct token *t;
	unsigned lin, pos;
	struct source *sp;
	const char *p;

	Fh(tl, 0, "\n#define VGC_NREFS %u\n", tl->cnt + 1);
	Fc(tl, 0, "\nstatic struct vrt_ref VGC_ref[VGC_NREFS] = {\n");
	lin = 1;
	pos = 0;
	sp = 0;
	p = NULL;
	TAILQ_FOREACH(t, &tl->tokens, list) {
		if (t->cnt == 0)
			continue;
		assert(t->src != NULL);
		if (t->src != sp) {
			lin = 1;
			pos = 0;
			sp = t->src;
			p = sp->b;
		}
		assert(sp != NULL);
		assert(p != NULL);
		for (;p < t->b; p++) {
			if (*p == '\n') {
				lin++;
				pos = 0;
			} else if (*p == '\t') {
				pos &= ~7;
				pos += 8;
			} else
				pos++;

		}
		Fc(tl, 0, "  [%3u] = { %d, %8u, %4u, %3u, 0, \"%.*s\" },\n",
		    t->cnt, sp->idx, t->b - sp->b, lin, pos + 1, PF(t));
	}
	Fc(tl, 0, "};\n");
}

/*--------------------------------------------------------------------*/

static void
EmitInitFunc(const struct tokenlist *tl)
{

	Fc(tl, 0, "\nstatic void\nVGC_Init(void)\n{\n\n");
	vsb_finish(tl->fi);
	/* XXX: check vsb_overflowed ? */
	vsb_cat(tl->fc, vsb_data(tl->fi));
	Fc(tl, 0, "}\n");
}

static void
EmitFiniFunc(const struct tokenlist *tl)
{

	Fc(tl, 0, "\nstatic void\nVGC_Fini(void)\n{\n\n");
	vsb_finish(tl->ff);
	/* XXX: check vsb_overflowed ? */
	vsb_cat(tl->fc, vsb_data(tl->ff));
	Fc(tl, 0, "}\n");
}

/*--------------------------------------------------------------------*/

static void
EmitStruct(const struct tokenlist *tl)
{
	struct source *sp;

	Fc(tl, 0, "\nconst char *srcname[%u] = {\n", tl->nsources);
	TAILQ_FOREACH(sp, &tl->sources, list) {
		Fc(tl, 0, "\t");
		EncString(tl->fc, sp->name, NULL, 0);
		Fc(tl, 0, ",\n");
	}
	Fc(tl, 0, "};\n");
	
	Fc(tl, 0, "\nconst char *srcbody[%u] = {\n", tl->nsources);
	TAILQ_FOREACH(sp, &tl->sources, list) {
		Fc(tl, 0, "    /* ");
		EncString(tl->fc, sp->name, NULL, 0);
		Fc(tl, 0, "*/\n");
		Fc(tl, 0, "\t");
		EncString(tl->fc, sp->b, sp->e, 1);
		Fc(tl, 0, ",\n");
	}
	Fc(tl, 0, "};\n");

	Fc(tl, 0, "\nstruct VCL_conf VCL_conf = {\n");
	Fc(tl, 0, "\t.magic = VCL_CONF_MAGIC,\n");
	Fc(tl, 0, "\t.init_func = VGC_Init,\n");
	Fc(tl, 0, "\t.fini_func = VGC_Fini,\n");
	Fc(tl, 0, "\t.nbackend = %d,\n", tl->nbackend);
	Fc(tl, 0, "\t.ref = VGC_ref,\n");
	Fc(tl, 0, "\t.nref = VGC_NREFS,\n");
	Fc(tl, 0, "\t.nsrc = %u,\n", tl->nsources);
	Fc(tl, 0, "\t.srcname = srcname,\n");
	Fc(tl, 0, "\t.srcbody = srcbody,\n");
	Fc(tl, 0, "\t.nhashcount = %u,\n", tl->nhashcount);
#define VCL_RET_MAC(l,u,b,n)
#define VCL_MET_MAC(l,u,b) \
	Fc(tl, 0, "\t." #l "_func = VGC_function_vcl_" #l ",\n"); 
#include "vcl_returns.h"
#undef VCL_MET_MAC
#undef VCL_RET_MAC
	Fc(tl, 0, "};\n");
}

/*--------------------------------------------------------------------*/

static struct source *
vcc_new_source(const char *b, const char *e, const char *name)
{
	struct source *sp;

	if (e == NULL)
		e = strchr(b, '\0');
	sp = calloc(sizeof *sp, 1);
	assert(sp != NULL);
	sp->name = strdup(name);
	sp->b = b;
	sp->e = e;
	return (sp);
}

static void
vcc_destroy_source(struct source *sp)
{

	if (sp->freeit != NULL)
		free(sp->freeit);
	free(sp->name);	
	free(sp);
}

/*--------------------------------------------------------------------*/

static struct source *
vcc_file_source(struct vsb *sb, const char *fn, int fd)
{
	char *f;
	int i;
	struct stat st;
	struct source *sp;

	if (fd < 0) {
		fd = open(fn, O_RDONLY);
		if (fd < 0) {
			vsb_printf(sb, "Cannot open file '%s': %s\n",
			    fn, strerror(errno));
			return (NULL);
		}
	}
	assert(0 == fstat(fd, &st));
	f = malloc(st.st_size + 1);
	assert(f != NULL);
	i = read(fd, f, st.st_size);
	assert(i == st.st_size);
	close(fd);
	f[i] = '\0';
	sp = vcc_new_source(f, f + i, fn);
	sp->freeit = f;
	return (sp);
}

/*--------------------------------------------------------------------*/

static void
vcc_resolve_includes(struct tokenlist *tl)
{
	struct token *t, *t1, *t2;
	struct source *sp;

	TAILQ_FOREACH(t, &tl->tokens, list) {
		if (t->tok != T_INCLUDE)
			continue;

		t1 = TAILQ_NEXT(t, list);
		assert(t1 != NULL);	/* There's always an EOI */
		if (t1->tok != CSTR) {
			vsb_printf(tl->sb,
			    "include not followed by string constant.\n");
			vcc_ErrWhere(tl, t1);
			return;
		}
		t2 = TAILQ_NEXT(t1, list);
		assert(t2 != NULL);	/* There's always an EOI */
		if (t2->tok != ';') {
			vsb_printf(tl->sb,
			    "include <string> not followed by semicolon.\n");
			vcc_ErrWhere(tl, t1);
			return;
		}
		assert(t2 != NULL);

		sp = vcc_file_source(tl->sb, t1->dec, -1);
		if (sp == NULL) {
			vcc_ErrWhere(tl, t1);
			return;
		}
		TAILQ_INSERT_TAIL(&tl->sources, sp, list);
		sp->idx = tl->nsources++;
		tl->t = t2;
		vcc_Lexer(tl, sp);

		TAILQ_REMOVE(&tl->tokens, t, list);
		TAILQ_REMOVE(&tl->tokens, t1, list);
		TAILQ_REMOVE(&tl->tokens, t2, list);
		vcc_FreeToken(t);
		vcc_FreeToken(t1);
		vcc_FreeToken(t2);
		if (!tl->err)
			vcc_resolve_includes(tl);
		return;
	}
}

/*--------------------------------------------------------------------*/

static struct tokenlist *
vcc_NewTokenList(void)
{
	struct tokenlist *tl;
	int i;

	tl = calloc(sizeof *tl, 1);
	assert(tl != NULL);
	TAILQ_INIT(&tl->membits);
	TAILQ_INIT(&tl->tokens);
	TAILQ_INIT(&tl->refs);
	TAILQ_INIT(&tl->procs);
	TAILQ_INIT(&tl->sources);

	tl->nsources = 0;

	/* General C code */
	tl->fc = vsb_new(NULL, NULL, 0, VSB_AUTOEXTEND);
	assert(tl->fc != NULL);

	/* Forward decls (.h like) */
	tl->fh = vsb_new(NULL, NULL, 0, VSB_AUTOEXTEND);
	assert(tl->fh != NULL);

	/* Init C code */
	tl->fi = vsb_new(NULL, NULL, 0, VSB_AUTOEXTEND);
	assert(tl->fi != NULL);

	/* Finish C code */
	tl->ff = vsb_new(NULL, NULL, 0, VSB_AUTOEXTEND);
	assert(tl->ff != NULL);

	/* body code of methods */
	for (i = 0; i < N_METHODS; i++) {
		tl->fm[i] = vsb_new(NULL, NULL, 0, VSB_AUTOEXTEND); \
		assert(tl->fm[i] != NULL);
	}
	return (tl);
}

/*--------------------------------------------------------------------*/

static char *
vcc_DestroyTokenList(struct tokenlist *tl, char *ret)
{
	struct membit *mb;
	struct source *sp;
	int i;

	while (!TAILQ_EMPTY(&tl->membits)) {
		mb = TAILQ_FIRST(&tl->membits);
		TAILQ_REMOVE(&tl->membits, mb, list);
		free(mb->ptr);
		free(mb);
	}
	while (!TAILQ_EMPTY(&tl->sources)) {
		sp = TAILQ_FIRST(&tl->sources);
		TAILQ_REMOVE(&tl->sources, sp, list);
		vcc_destroy_source(sp);
	}
		
	vsb_delete(tl->fh);
	vsb_delete(tl->fc);
	vsb_delete(tl->fi);
	vsb_delete(tl->ff);
	for (i = 0; i < N_METHODS; i++)
		vsb_delete(tl->fm[i]);

	free(tl);
	return (ret);
}

/*--------------------------------------------------------------------
 * Compile the VCL code from the given source and return the C-source
 */

static char *
vcc_CompileSource(struct vsb *sb, struct source *sp)
{
	struct tokenlist *tl;
	char *of;
	int i;

	tl = vcc_NewTokenList();
	tl->sb = sb;

	vcl_output_lang_h(tl->fh);
	Fh(tl, 0, "\n/* ---===### VCC generated below here ###===---*/\n");
	Fh(tl, 0, "\nextern struct VCL_conf VCL_conf;\n");

	Fi(tl, 0, "\tVRT_alloc_backends(&VCL_conf);\n");

	/* Register and lex the main source */
	TAILQ_INSERT_TAIL(&tl->sources, sp, list);
	sp->idx = tl->nsources++;
	vcc_Lexer(tl, sp);
	if (tl->err)
		return (vcc_DestroyTokenList(tl, NULL));

	/* Register and lex the default VCL */
	sp = vcc_new_source(vcc_default_vcl_b, vcc_default_vcl_e, "Default");
	assert(sp != NULL);
	TAILQ_INSERT_TAIL(&tl->sources, sp, list);
	sp->idx = tl->nsources++;
	vcc_Lexer(tl, sp);
	if (tl->err)
		return (vcc_DestroyTokenList(tl, NULL));

	/* Add "END OF INPUT" token */
	vcc_AddToken(tl, EOI, sp->e, sp->e);
	if (tl->err)
		return (vcc_DestroyTokenList(tl, NULL));

	/* Expand and lex any includes in the token string */
	vcc_resolve_includes(tl);
	if (tl->err)
		return (vcc_DestroyTokenList(tl, NULL));

	/* Parse the token string */
	tl->t = TAILQ_FIRST(&tl->tokens);
	vcc_Parse(tl);
	if (tl->err)
		return (vcc_DestroyTokenList(tl, NULL));

	/* Check if we have any backends at all */
	if (tl->nbackend == 0) {
		vsb_printf(tl->sb,
		    "No backends in VCL program, at least one is necessary.\n");
		tl->err = 1;
		return (vcc_DestroyTokenList(tl, NULL));
	}

	/* Check for orphans */
	if (vcc_CheckReferences(tl))
		return (vcc_DestroyTokenList(tl, NULL));

	/* Check that all action returns are legal */
	vcc_CheckAction(tl);
	if (tl->err)
		return (vcc_DestroyTokenList(tl, NULL));

	/* Check that all variable uses are legal */
	vcc_CheckUses(tl);
	if (tl->err)
		return (vcc_DestroyTokenList(tl, NULL));

	Ff(tl, 0, "\tVRT_free_backends(&VCL_conf);\n");

	/* Emit method functions */
	for (i = 0; i < N_METHODS; i++) {
		Fc(tl, 1, "\nstatic int\n");
		Fc(tl, 1, "VGC_function_%s (struct sess *sp)\n",
		    method_tab[i].name);
		vsb_finish(tl->fm[i]);
		/* XXX: check vsb_overflowed ? */
		Fc(tl, 1, "{\n");
		Fc(tl, 1, "%s", vsb_data(tl->fm[i]));
		Fc(tl, 1, "}\n");
	}

	LocTable(tl);

	EmitInitFunc(tl);

	EmitFiniFunc(tl);

	EmitStruct(tl);

	/* Combine it all in the fh vsb */
	vsb_finish(tl->fc);
	/* XXX: check vsb_overflowed ? */
	vsb_cat(tl->fh, vsb_data(tl->fc));
	vsb_finish(tl->fh);

	of = strdup(vsb_data(tl->fh));
	AN(of);

	/* done */
	return (vcc_DestroyTokenList(tl, of));
}

/*--------------------------------------------------------------------
 * Compile the VCL code in the argument.  Error messages, if any are
 * formatted into the vsb.
 */

char *
VCC_Compile(struct vsb *sb, const char *b, const char *e)
{
	struct source *sp;
	char *r;

	sp = vcc_new_source(b, e, "input");
	if (sp == NULL)
		return (NULL);
	r = vcc_CompileSource(sb, sp);
	return (r);
}

/*--------------------------------------------------------------------
 * Compile the VCL code from the file named.  Error messages, if any
 * are formatted into the vsb.
 */

char *
VCC_CompileFile(struct vsb *sb, const char *fn, int fd)
{
	struct source *sp;
	char *r;

	sp = vcc_file_source(sb, fn, fd);
	if (sp == NULL)
		return (NULL);
	r = vcc_CompileSource(sb, sp);
	return (r);
}

/*--------------------------------------------------------------------
 * Initialize the compiler and register the default VCL code for later
 * compilation runs.
 */

void
VCC_InitCompile(const char *default_vcl)
{
	struct var *v;

	vcc_default_vcl_b = default_vcl;
	vcc_default_vcl_e = strchr(default_vcl, '\0');
	assert(vcc_default_vcl_e != NULL);

	vcl_init_tnames();
	for (v = vcc_vars; v->name != NULL; v++)
		v->len = strlen(v->name);
	for (v = vcc_be_vars; v->name != NULL; v++)
		v->len = strlen(v->name);
}