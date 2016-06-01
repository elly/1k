/* fmt.c
 * Flags 0
 * Widths l, ll
 * Conversions d, o, u, x, c, s, p, %
 */

#include <stdarg.h>
#include <stdio.h>

struct fmtctx {
	const char *str;
	unsigned int state;
	int num_written;
	void (*out)(void *priv, char c);
	void *priv;
};

enum {
	ST_ZEROPAD  = 0x00000001,
	ST_LONG     = 0x00000002,
	ST_LONGER   = 0x00000004,
	ST_WIDTH    = 0x00000008,
	ST_NEGATIVE = 0x00000010,
	ST_PREC     = 0x00000020,
};

#define PACKWID(c,w)	((c)->state |= (w << 20))
#define WID(c)		(((c)->state >> 20) & 0xFFF)
#define PACKPREC(c,w)	((c)->state |= (w << 12))
#define PREC(c)		(((c)->state >> 12) & 0xFF)

static void _out(struct fmtctx *ctx, char c) {
	ctx->num_written++;
	ctx->out(ctx->priv, c);
}

static int _isdigit(char c) {
	return c >= '0' && c <= '9';
}

unsigned int _atou(const char **p) {
	unsigned int v = 0;
	while (_isdigit(**p)) {
		v *= 10;
		v += (**p - '0');
		(*p)++;
	}
	return v;
}

void _utoa(struct fmtctx *ctx, unsigned int base, unsigned long long arg) {
	static const char digits[] = "0123456789abcdef";
	char buf[23];	/* 64 bits = 22 octal digits, leading '-' */
	char *n = buf + sizeof(buf) - 1;
	char *o = n;

	while (arg) {
		*n = digits[arg % base];
		n--;
		arg /= base;
	}
	if (ctx->state & ST_ZEROPAD)
		while ((o - n) < WID(ctx) && n > buf)
			*n-- = '0';
	if (ctx->state & ST_NEGATIVE)
		*n-- = '-';
	n++;
	while (n < buf + sizeof(buf))
		_out(ctx, *n++);
}

static void _fmts(struct fmtctx *ctx, char c, va_list va) {
	const char *s = va_arg(va, const char *);
	unsigned int n = PREC(ctx);
	(void)c;
	while ((!(ctx->state & ST_PREC) || n) && *s) {
		_out(ctx, *s++);
		n--;
	}
}

static void _fmtp(struct fmtctx *ctx, char c, va_list va) {
	void *p = va_arg(va, void *);
	(void)c;
	ctx->state |= ST_ZEROPAD;
	PACKWID(ctx, sizeof(void*) * 2);
	_utoa(ctx, 16, (unsigned long long)p);
}

static void _fmtd(struct fmtctx *ctx, char c, va_list va) {
	int n = va_arg(va, int);
	(void)c;
	if (n < 0) {
		ctx->state |= ST_NEGATIVE;
		n = -n;
	}
	_utoa(ctx, 10, n);
}

static void _fmtu(struct fmtctx *ctx, char c, va_list va) {
	unsigned int n = va_arg(va, unsigned int);
	_utoa(ctx, c == 'u' ? 10 : (c == 'o' ? 8 : 16), n);
}

static void _fmtc(struct fmtctx *ctx, char c, va_list va) {
	char n = va_arg(va, int);
	(void)c;
	_out(ctx, n);
}

static void _fmtb(struct fmtctx *ctx, char c, va_list va) {
	unsigned int len = va_arg(va, unsigned int);
	const unsigned char *p = va_arg(va, const unsigned char *);
	unsigned int i;
	(void)c;
	ctx->state |= ST_ZEROPAD;
	PACKWID(ctx, 2);
	for (i = 0; i < len; i++)
		_utoa(ctx, 16, p[i]);
}

static void _fmtpct(struct fmtctx *ctx, char c, va_list va) {
	(void)c;
	(void)va;
	_out(ctx, '%');
}

struct {
	char f;
	void (*func)(struct fmtctx *ctx, char c, va_list va);
} fmts[] = {
	{ 's', _fmts },
	{ 'p', _fmtp },
	{ 'd', _fmtd },
	{ 'u', _fmtu },
	{ 'o', _fmtu },
	{ 'x', _fmtu },
	{ 'c', _fmtc },
	{ 'b', _fmtb },
	{ '%', _fmtpct },
	{ '\0', 0 }
};

int _fmt(struct fmtctx *ctx, va_list args) {
	int i;
	ctx->num_written = ctx->state = 0;
	while (*ctx->str) {
		if (*ctx->str != '%') {
			_out(ctx, *ctx->str++);
			continue;
		}

		ctx->str++;
		ctx->state = 0;
		if (*ctx->str == '0') {
			ctx->state |= ST_ZEROPAD;
			ctx->str++;
		}

		if (_isdigit(*ctx->str)) {
			PACKWID(ctx, _atou(&ctx->str));
			ctx->state |= ST_WIDTH;
		}

		if (*ctx->str == '.') {
			ctx->str++;
			if (_isdigit(*ctx->str))
				PACKPREC(ctx, _atou(&ctx->str));
			else if (*ctx->str == '*') {
				ctx->str++;
				PACKPREC(ctx, va_arg(args, unsigned int));
			}
			ctx->state |= ST_PREC;
		}

		if (*ctx->str == 'l') {
			ctx->str++;
			ctx->state |= (*ctx->str == 'l' ? ST_LONGER : ST_LONG);
		}

		for (i = 0; fmts[i].f; i++)
			if (fmts[i].f == *ctx->str)
				fmts[i].func(ctx, *ctx->str, args);

		ctx->str++;
	}
	return ctx->num_written;
}

struct sfmtctx {
	char *buf;
	unsigned int idx;
	unsigned int len;
};

void _sfmtout(void *p, char c) {
	struct sfmtctx *ctx = p;
	if (ctx->idx >= ctx->len)
		return;
	ctx->buf[ctx->idx++] = c;
}

int sfmt(char *buf, unsigned int len, const char *fmt, ...) {
	struct fmtctx ctx;
	struct sfmtctx sctx;
	va_list ap;
	int num_written;

	ctx.str = fmt;
	ctx.out = _sfmtout;
	ctx.priv = &sctx;

	sctx.buf = buf;
	sctx.len = len;
	sctx.idx = 0;

	va_start(ap, fmt);
	num_written = _fmt(&ctx, ap);
	va_end(ap);

	if (sctx.idx >= sctx.len) {
		buf[sctx.idx - 1] = '\0';
	} else {
		buf[sctx.idx] = '\0';
	}

	return num_written;
}
