#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include <float.h>

#ifdef _WIN32
#define snprintf _snprintf
#endif

#include "internal.h"
#include "builtins.h"
#include "network.h"
#include "utf8.h"

static int needs_quote(module *m, const char *src)
{
	if (!strcmp(src, ",") || !strcmp(src, ".") || !strcmp(src, "|"))
		return 1;

	if (!*src || isupper(*src) || isdigit(*src))
		return 1;

	if (!strcmp(src, "[]") || !strcmp(src, "!"))
		return 0;

	if (get_op(m, src, NULL, NULL, 0))
		return 0;

	while (*src) {
		int ch = get_char_utf8(&src);

		if ((iscntrl(ch) || isspace(ch) || ispunct(ch)) && (ch != '_'))
			return 1;
	}

	return 0;
}

static size_t _sprint_int(char *dst, size_t size, int_t n, int base)
{
	const char *save_dst = dst;

	if ((n / base) > 0)
		dst += _sprint_int(dst, size, n / base, base);

	int n2 = n % base;

	if (n2 > 9) {
		n2 -= 10;
		n2 += 'A';
	} else
		n2 += '0';

	if (size) *dst++ = (char)n2; else dst++;
	return dst - save_dst;
}

size_t sprint_int(char *dst, size_t size, int_t n, int base)
{
	const char *save_dst = dst;

	if ((n < 0) && (base == 10)) {
		if (size) *dst++ = '-'; else dst++;
		n = llabs(n);
	}

	if (n == 0) {
		if (size) *dst++ = '0'; else dst++;
		if (size) *dst = '\0';
		return dst - save_dst;
	}

	dst += _sprint_int(dst, size, n, base);
	if (size) *dst = '\0';
	return dst - save_dst;
}

static size_t formatted(char *dst, size_t dstlen, const char *src, size_t srclen)
{
	extern const char *g_escapes;
	extern const char *g_anti_escapes;
	size_t len = 0;

	while (*src && srclen--) {
		int ch = *src++;
		const char *ptr = strchr(g_escapes, ch);

		if (ptr) {
			if (dstlen) {
				*dst++ = '\\';
				*dst++ = g_anti_escapes[ptr-g_escapes];
			}

			len += 2;;
		} else {
			if (dstlen)
				*dst++ = ch;

			len++;
		}
	}

	return len;
}

static size_t plain(char *dst, size_t dstlen, const char *src, size_t srclen)
{
	size_t len = 0;

	while (*src && srclen--) {
		int ch = *src++;

		if (dstlen)
			*dst++ = ch;

		len++;
	}

	return len;
}

size_t write_canonical_to_buf(query *q, char *dst, size_t dstlen, cell *c, int running, int depth)
{
	char *save_dst = dst;

	if (depth > 9999) {
		fprintf(stderr, "Error: max depth exceeded\n");
		q->error = 1;
		return dst - save_dst;
	}

	if (is_rational(c)) {
		if (((c->flags & FLAG_HEX) || (c->flags & FLAG_BINARY))) {
			dst += snprintf(dst, dstlen, "%s0x", c->val_num<0?"-":"");
			dst += sprint_int(dst, dstlen, c->val_num, 16);
		} else if ((c->flags & FLAG_OCTAL) && !running) {
			dst += snprintf(dst, dstlen, "%s0o", c->val_num<0?"-":"");
			dst += sprint_int(dst, dstlen, c->val_num, 8);
		} else if (c->val_den != 1) {
			if (q->m->flag.rational_syntax_natural) {
				dst += sprint_int(dst, dstlen, c->val_num, 10);
				dst += snprintf(dst, dstlen, "/");
				dst += sprint_int(dst, dstlen, c->val_den, 10);
			} else {
				dst += sprint_int(dst, dstlen, c->val_num, 10);
				dst += snprintf(dst, dstlen, " rdiv ");
				dst += sprint_int(dst, dstlen, c->val_den, 10);
			}
		} else
			dst += sprint_int(dst, dstlen, c->val_num, 10);

		return dst - save_dst;
	}

	if (is_float(c) && (c->val_flt == M_PI)) {
		dst += snprintf(dst, dstlen, "%s", "3.141592653589793");
		return dst - save_dst;
	}
	else if (is_float(c) && (c->val_flt == M_E)) {
		dst += snprintf(dst, dstlen, "%s", "2.718281828459045");
		return dst - save_dst;
	}
	else if (is_float(c)) {
		char tmpbuf[256];
		sprintf(tmpbuf, "%.*g", 16, c->val_flt);

		if (!strchr(tmpbuf, '.'))
			strcat(tmpbuf, ".0");

		dst += snprintf(dst, dstlen, "%s", tmpbuf);
		return dst - save_dst;
	}

	if (is_variable(c) && ((1ULL << c->slot_nbr) & q->nv_mask)) {
		dst += snprintf(dst, dstlen, "'$VAR'(%u)", q->nv_start + count_bits(q->nv_mask, c->slot_nbr));
		return dst - save_dst;
	}

	const char *src = GET_STR(c);
	int dq = 0, quote = !is_variable(c) && needs_quote(q->m, src);
	if (is_string(c)) dq = quote = 1;
	dst += snprintf(dst, dstlen, "%s", quote?dq?"\"":"'":"");
	dst += formatted(dst, dstlen, src, is_blob(c) ? c->len_str : INT_MAX);
	dst += snprintf(dst, dstlen, "%s", quote?dq?"\"":"'":"");

	if (!is_structure(c))
		return dst - save_dst;

	idx_t save_ctx = q->latest_ctx;
	idx_t arity = c->arity;
	dst += snprintf(dst, dstlen, "(");

	for (c++; arity--; c += c->nbr_cells) {
		cell *p = running ? deref_var(q, c, save_ctx) : c;
		dst += write_canonical_to_buf(q, dst, dstlen, p, running, depth+1);

		if (arity)
			dst += snprintf(dst, dstlen, ",");
	}

	dst += snprintf(dst, dstlen, ")");
	q->latest_ctx = save_ctx;
	return dst - save_dst;
}

static char *varformat(unsigned nbr)
{
	static char tmpbuf[80];
	char *dst = tmpbuf;
	dst += sprintf(dst, "%c", 'A'+nbr%26);
	if ((nbr/26) > 0) sprintf(dst, "%u", nbr/26);
	return tmpbuf;
}

size_t write_term_to_buf(query *q, char *dst, size_t dstlen, cell *c, int running, int cons, int max_depth, int depth)
{
	char *save_dst = dst;

	if (depth > 9999) {
		fprintf(stderr, "Error: max depth exceeded\n");
		q->error = 1;
		return dst - save_dst;
	}

	if (is_rational(c)) {
		if (((c->flags & FLAG_HEX) || (c->flags & FLAG_BINARY)) && (running <= 0)) {
			dst += snprintf(dst, dstlen, "%s0x", c->val_num<0?"-":"");
			dst += sprint_int(dst, dstlen, c->val_num, 16);
		} else if ((c->flags & FLAG_OCTAL) && !running) {
			dst += snprintf(dst, dstlen, "%s0o", c->val_num<0?"-":"");
			dst += sprint_int(dst, dstlen, c->val_num, 8);
		} else if (c->val_den != 1) {
			if (q->m->flag.rational_syntax_natural) {
				dst += sprint_int(dst, dstlen, c->val_num, 10);
				dst += snprintf(dst, dstlen, "/");
				dst += sprint_int(dst, dstlen, c->val_den, 10);
			} else {
				dst += sprint_int(dst, dstlen, c->val_num, 10);
				dst += snprintf(dst, dstlen, " rdiv ");
				dst += sprint_int(dst, dstlen, c->val_den, 10);
			}
		} else
			dst += sprint_int(dst, dstlen, c->val_num, 10);

		return dst - save_dst;
	}

	if (is_float(c) && (c->val_flt == M_PI)) {
		dst += snprintf(dst, dstlen, "%s", "3.141592653589793");
		return dst - save_dst;
	}
	else if (is_float(c) && (c->val_flt == M_E)) {
		dst += snprintf(dst, dstlen, "%s", "2.718281828459045");
		return dst - save_dst;
	}
	else if (is_float(c)) {
		char tmpbuf[256];
		sprintf(tmpbuf, "%.*g", 16, c->val_flt);

		if (!strchr(tmpbuf, '.'))
			strcat(tmpbuf, ".0");

		dst += snprintf(dst, dstlen, "%s", tmpbuf);
		return dst - save_dst;
	}

	idx_t save_ctx = q->latest_ctx;
	idx_t save2_ctx = q->latest_ctx;
	const char *src = GET_STR(c);
	int print_list = 0;

	// FIXME make non-recursive

	while (is_real_list(c)) {
		if (max_depth && (depth >= max_depth)) {
			dst += snprintf(dst, dstlen, " |...");
			return dst - save_dst;
		}

		cell *h = LIST_HEAD(c);
		cell *tail = LIST_TAIL(c);

		if (!cons)
			dst += snprintf(dst, dstlen, "%s", "[");

		h = running ? deref_var(q, h, save_ctx) : h;
		dst += write_term_to_buf(q, dst, dstlen, h, running, 0, max_depth, depth+1);

		tail = running ? deref_var(q, tail, save_ctx) : tail;

		if (is_literal(tail) && !is_structure(tail)) {
			src = GET_STR(tail);

			if (strcmp(src, "[]")) {
				dst += snprintf(dst, dstlen, "%s", "|");
				dst += write_term_to_buf(q, dst, dstlen, tail, running, 1, max_depth, depth+1);
			}
		}
		else if (is_list(tail)) {
			dst += snprintf(dst, dstlen, "%s", ",");
			c = tail;
			save_ctx = q->latest_ctx;
			print_list++;
			cons = 1;
			continue;
		}
		else {
			dst += snprintf(dst, dstlen, "%s", "|");
			dst += write_term_to_buf(q, dst, dstlen, tail, running, 1, max_depth, depth+1);
		}

		if (!cons || print_list)
			dst += snprintf(dst, dstlen, "%s", "]");

		q->latest_ctx = save2_ctx;
		return dst - save_dst;
	}

	int optype = (c->flags & OP_FX) | (c->flags & OP_FY) | (c->flags & OP_XF) |
		(c->flags & OP_YF) | (c->flags & OP_XFX) |
		(c->flags & OP_YFX) | (c->flags & OP_XFY);

	if (q->ignore_ops || !optype || !c->arity) {
		int quote = ((running <= 0) || q->quoted) && !is_variable(c) && needs_quote(q->m, src);
		int dq = 0, braces = 0;
		if (is_string(c) && !is_head(c)) dq = quote = 1;
		if (c->arity && !strcmp(src, "{}")) braces = 1;
		dst += snprintf(dst, dstlen, "%s", !braces&&quote?dq?"\"":"'":"");

		if (running && is_variable(c) && ((1ULL << c->slot_nbr) & q->nv_mask)) {
			dst += snprintf(dst, dstlen, "%s", varformat(q->nv_start + count_bits(q->nv_mask, c->slot_nbr)));
			return dst - save_dst;
		}

		if (running && is_variable(c) && (q->latest_ctx != q->st.curr_frame)) {
			frame *g = GET_FRAME(q->latest_ctx);
			slot *e = GET_SLOT(g, c->slot_nbr);
			idx_t slot_nbr = e - q->slots;
			dst += snprintf(dst, dstlen, "_%u", (unsigned)slot_nbr);
			return dst - save_dst;
		}

		int len_str = !is_blob(c) ? strlen(src) : c->len_str;

		if (braces)
			;
		else if (quote) {
			if ((running < 0) && is_blob(c) && (len_str > 128))
				len_str = 128;

			dst += formatted(dst, dstlen, src, is_blob(c) ? len_str : INT_MAX);

			if ((running < 0) && is_blob(c) && (len_str == 128))
				dst += snprintf(dst, dstlen, "%s", "...");
		} else
			dst += plain(dst, dstlen, src, is_blob(c) ? len_str : INT_MAX);

		dst += snprintf(dst, dstlen, "%s", !braces&&quote?dq?"\"":"'":"");

		if (is_structure(c) && !is_string(c)) {
			idx_t arity = c->arity;
			dst += snprintf(dst, dstlen, braces?"{":"(");

			for (c++; arity--; c += c->nbr_cells) {
				cell *tmp = running ? deref_var(q, c, save_ctx) : c;
				dst += write_term_to_buf(q, dst, dstlen, tmp, running, 0, max_depth, depth+1);

				if (arity)
					dst += snprintf(dst, dstlen, ",");
			}

			dst += snprintf(dst, dstlen, braces?"}":")");
		}
	}
	else if ((c->flags & OP_XF) || (c->flags & OP_YF)) {
		cell *lhs = c + 1;
		lhs = running ? deref_var(q, lhs, save_ctx) : lhs;
		dst += write_term_to_buf(q, dst, dstlen, lhs, running, 0, max_depth, depth+1);
		dst += snprintf(dst, dstlen, "%s", src);
	}
	else if ((c->flags & OP_FX) || (c->flags & OP_FY)) {
		cell *rhs = c + 1;
		rhs = running ? deref_var(q, rhs, save_ctx) : rhs;
		int space = isalpha_utf8(peek_char_utf8(src)) || !strcmp(src, ":-") || !strcmp(src, "\\+");
		int parens = is_structure(rhs) && !strcmp(GET_STR(rhs), ",");
		dst += snprintf(dst, dstlen, "%s", src);
		if (space && !parens) dst += snprintf(dst, dstlen, " ");
		if (parens) dst += snprintf(dst, dstlen, "(");
		dst += write_term_to_buf(q, dst, dstlen, rhs, running, 0, max_depth, depth+1);
		if (parens) dst += snprintf(dst, dstlen, ")");
	}
	else {
		cell *lhs = c + 1;
		cell *rhs = lhs + lhs->nbr_cells;
		int my_prec = get_op(q->m, GET_STR(c), NULL, NULL, 0);
		int lhs_prec1 = is_literal(lhs) ? get_op(q->m, GET_STR(lhs), NULL, NULL, 0) : 0;
		int lhs_prec2 = is_literal(lhs) && !lhs->arity ? get_op(q->m, GET_STR(lhs), NULL, NULL, 0) : 0;
		int rhs_prec1 = is_literal(rhs) ? get_op(q->m, GET_STR(rhs), NULL, NULL, 0) : 0;
		int rhs_prec2 = is_literal(rhs) && !rhs->arity ? get_op(q->m, GET_STR(rhs), NULL, NULL, 0) : 0;
		lhs = running ? deref_var(q, lhs, save_ctx) : lhs;
		int parens = 0;//depth && strcmp(src, ",") && strcmp(src, "is") && strcmp(src, "->");
		int lhs_parens = lhs_prec1 > my_prec;
		lhs_parens |= lhs_prec2;
		if (parens || lhs_parens) dst += snprintf(dst, dstlen, "(");
		dst += write_term_to_buf(q, dst, dstlen, lhs, running, 0, max_depth, depth+1);
		if (lhs_parens) dst += snprintf(dst, dstlen, ")");
		rhs = running ? deref_var(q, rhs, save_ctx) : rhs;
		int space = isalpha_utf8(peek_char_utf8(src)) || !strcmp(src, ":-") || !strcmp(src, "-->") || !*src;
		if (space && !parens) dst += snprintf(dst, dstlen, " ");
		dst += snprintf(dst, dstlen, "%s", src);
		if (!*src) space = 0;
		if (space && !parens) dst += snprintf(dst, dstlen, " ");
		int rhs_parens = rhs_prec1 > my_prec;
		rhs_parens |= rhs_prec2;
		if (rhs_parens) dst += snprintf(dst, dstlen, "(");
		dst += write_term_to_buf(q, dst, dstlen, rhs, running, 0, max_depth, depth+1);
		if (parens || rhs_parens) dst += snprintf(dst, dstlen, ")");
	}

	q->latest_ctx = save_ctx;
	return dst - save_dst;
}

void write_canonical_to_stream(query *q, stream *str, cell *c, int running, int depth)
{
	idx_t save_ctx = q->latest_ctx;
	size_t len = write_canonical_to_buf(q, NULL, 0, c, running, depth);
	char *dst = malloc(len+1);
	write_canonical_to_buf(q, dst, len+1, c, running, depth);
	const char *src = dst;

	while (len) {
		size_t nbytes = net_write(src, len, str);

		if (feof(str->fp)) {
			q->error = 1;
			return;
		}

		len -= nbytes;
		src += nbytes;
	}

	free(dst);
	q->latest_ctx = save_ctx;
}

void write_canonical(query *q, FILE *fp, cell *c, int running, int depth)
{
	idx_t save_ctx = q->latest_ctx;
	size_t len = write_canonical_to_buf(q, NULL, 0, c, running, depth);
	char *dst = malloc(len+1);
	write_canonical_to_buf(q, dst, len+1, c, running, depth);
	const char *src = dst;

	while (len) {
		size_t nbytes = fwrite(src, 1, len, fp);

		if (feof(fp)) {
			q->error = 1;
			return;
		}

		len -= nbytes;
		src += nbytes;
	}

	free(dst);
	q->latest_ctx = save_ctx;
}

void write_term_to_stream(query *q, stream *str, cell *c, int running, int cons, int max_depth, int depth)
{
	idx_t save_ctx = q->latest_ctx;
	size_t len = write_term_to_buf(q, NULL, 0, c, running, cons, max_depth, depth);
	char *dst = malloc(len+1);
	write_term_to_buf(q, dst, len+1, c, running, cons, max_depth, depth);
	const char *src = dst;

	while (len) {
		size_t nbytes = net_write(src, len, str);

		if (feof(str->fp)) {
			q->error = 1;
			return;
		}

		len -= nbytes;
		src += nbytes;
	}

	free(dst);
	q->latest_ctx = save_ctx;
}

void write_term(query *q, FILE *fp, cell *c, int running, int cons, int max_depth, int depth)
{
	idx_t save_ctx = q->latest_ctx;
	size_t len = write_term_to_buf(q, NULL, 0, c, running, cons, max_depth, depth);
	char *dst = malloc(len+1);
	write_term_to_buf(q, dst, len+1, c, running, cons, max_depth, depth);
	const char *src = dst;

	while (len) {
		size_t nbytes = fwrite(src, 1, len, fp);

		if (feof(fp)) {
			q->error = 1;
			return;
		}

		len -= nbytes;
		src += nbytes;
	}

	free(dst);
	q->latest_ctx = save_ctx;
}
