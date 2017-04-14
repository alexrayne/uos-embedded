/*
 * Scaled down version of printf(3).
 * Based on FreeBSD sources, heavily rewritten.
 *
 * Copyright (C) 2000-2002 Serge Vakulenko, <vak@cronyx.ru>
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You can redistribute this file and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software Foundation;
 * either version 2 of the License, or (at your discretion) any later version.
 * See the accompanying file "COPYING.txt" for more details.
 *
 * As a special exception to the GPL, permission is granted for additional
 * uses of the text contained in this file.  See the accompanying file
 * "COPY-UOS.txt" for details.
 */
/*
 * Two additional formats:
 *
 * The format %b is supported to decode error registers.
 * Its usage is:
 *
 *	printf("reg=%b\n", regval, "<base><arg>*");
 *
 * where <base> is the output base expressed as a control character, e.g.
 * \10 gives octal; \20 gives hex.  Each arg is a sequence of characters,
 * the first of which gives the bit number to be inspected (origin 1), and
 * the next characters (up to a control character, i.e. a character <= 32),
 * give the name of the register.  Thus:
 *
 *	kvprintf("reg=%b\n", 3, "\10\2BITTWO\1BITONE\n");
 *
 * would produce output:
 *
 *	reg=3<BITTWO,BITONE>
 *
 * The format %D -- Hexdump, takes a pointer. Sharp flag - use `:' as
 * a separator, instead of a space. For example:
                    precision denotes size of groups delimited by space
 *
 *	("%6D", ptr)       -> XX XX XX XX XX XX
 *  ("%6.3D", len, ptr) -> XX:XX:XX XX:XX:XX ...
 *	("%#*D", len, ptr) -> XX:XX:XX:XX ...
 *	
 * The flag specifier introduced:
 *  @ - denotes padding symbol instead default space or 0

 *  ("%@.6D", ptr)       -> XX.XX.XX.XX.XX.XX
 *  ("%@.6d", 100)       -> ...100
 *
 *  extended dump %@Dxxx introduced:
 *      precision denotes size of groups delimited by space
 *      sharp flag - assign group delimiter \n when groupwidth >0, else delimiter sets ':' (base old behaviour)
 *      xxx - is format specifier of dumped items. supports all simple numeric formats
 *          it recognise size of items at provided dump pointer
 *              by default - 1 byte
 *              h          - short
 *              l          - long
 *  ("%#*.*@D08lx", len, groupsize, ptr)    -> xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx ....\n
 *                                             xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx ....\n
 *                                             ....
 *  ("%4@.@Du"  , ipadress)                   -> d.d.d.d
 *  ("%4@.@D03u", ipadress)                   -> ddd.ddd.ddd.ddd
 *  ("%4@.@Dhu"  , ip6adress)                 -> d.d.d.d
 *
 */
#include <runtime/lib.h>
#include <runtime/math.h>
#include <stream/stream.h>
#include <kernel/internal.h>


int stream_printf(stream_t *u, const void *fmt, ...){
	int err;
    va_list args;
    va_start (args, fmt);
	err = stream_vprintf (u, fmt, args);
    va_end (args);
    return err;
}


/* Max number conversion buffer length: a long in base 2, plus NUL byte. */
#define MAXNBUF	(sizeof(long) * 8 + 1)

static unsigned char *ksprintn (unsigned char *buf, unsigned long v, unsigned char base,
	int width, unsigned char *lp);
static unsigned char mkhex (unsigned char ch);

#ifdef ARCH_HAVE_FPU
static int cvt (double number, int prec, int sharpflag, unsigned char *negp,
	unsigned char fmtch, unsigned char *startp, unsigned char *endp);
#endif

#ifdef MIPS32
#define VPRINTF_FRAME (256+16)
#else
#define VPRINTF_FRAME 100
#endif

#ifndef UOS_STRICTS
#define UOS_STRICTS             0
#define UOS_STRICT_STACK        0
#endif

#if STREAM_HAVE_ACCEESS > 0
static
int stream_vprintf_nomt(stream_t *stream, char const *fmt, va_list ap);

int
stream_vprintf (stream_t *stream, char const *fmt, va_list ap){
    UOS_STRICT(STREAM_MEM, ) assert(uos_valid_memory_address(stream));
    UOS_STRICT(STREAM_MEM, ) assert(uos_valid_memory_address(stream->interface));

#if ((UOS_STRICTS & UOS_STRICT_STACK) != 0) && (NDEBUG <= 0)
#ifdef UOS_EXCEPTION_STACK
    if ( (uos_exception_stack == 0) || ((void*)uos_exception_stack < arch_get_stack_pointer()) )
#endif
    if(__builtin_expect (!(task_stack_enough( VPRINTF_FRAME )), 0) )
    {
        debug_puts("printf asserted stack task ");
        debug_puts(task_current->name);
        debug_putchar(0, '\n');
        return 0;//task_exit(0);
    }
#endif

    if (stream->interface->access_tx != 0)
        (stream->interface->access_tx(stream, 1));
    int res = stream_vprintf_nomt(stream, fmt, ap);
    if (stream->interface->access_tx != 0)
        (stream->interface->access_tx(stream, 0));
    return res;
}

static
int stream_vprintf_nomt(stream_t *stream, char const *fmt, va_list ap)
#else
int
stream_vprintf (stream_t *stream, char const *fmt, va_list ap)
#endif
{
#define PUTC(c) { putchar(stream,(unsigned char)(c)); ++retval; }
	unsigned char nbuf [MAXNBUF], padding, *q;
	const unsigned char *s;
	unsigned char c, base, ladjust, sharpflag, neg, dot, size;
	char lflag;
#define laslong     sizeof(long)
#define lasshort    sizeof(short)
//    const int lasbyte = 1;
	small_int_t n, width, dwidth, retval;
	char uppercase, extrazeros, sign;
	small_int_t dump_width, dump_prec, dump_pos;
	char        dump_pad;
	const unsigned char *dump_s;
	unsigned long ul;

	if (! stream)
		return 0;
	if (! fmt)
		fmt = "(null)\n";

    UOS_STRICT(STREAM_MEM, ) assert(uos_valid_memory_address(stream));
    UOS_STRICT(STREAM_MEM, ) assert(uos_valid_memory_address(stream->interface));
    UOS_STRICT(STREAM_MEM, ) assert(uos_valid_code_address(stream->interface->putc));

#if ((UOS_STRICTS & UOS_STRICT_STACK) != 0) && (NDEBUG <= 0)
#ifdef UOS_EXCEPTION_STACK
    if ( (uos_exception_stack == 0) || ((void*)uos_exception_stack < arch_get_stack_pointer()) )
#endif
    if(__builtin_expect (!(task_stack_enough( VPRINTF_FRAME )), 0) )
    {
        debug_puts("printf asserted stack task ");
        debug_puts(task_name(task_current));
        debug_putchar(0, '\n');
        return 0;//task_exit(0);
    }
#endif

    dump_pos = 0;
    dump_pad = ' ';
    dump_prec = -1;
	retval = 0;
	for (;;) {
		while ((c = FETCH_BYTE (fmt++)) != '%') {
			if (! c)
				return retval;
			PUTC (c);
		}
        base = 10;
		padding = ' ';
		width = 0; extrazeros = 0;
		lflag = 0; ladjust = 0; sharpflag = 0; neg = 0;
		sign = 0; dot = 0; uppercase = 0; dwidth = -1;
		size = 0;
		dump_width = -1;
        dump_s = 0;
reswitch:
        c = FETCH_BYTE (fmt++);
doswitch:
        switch (c) {
		case '.':
			dot = 1;
			padding = ' ';
			dwidth = 0;
			goto reswitch;

		case '#':
			sharpflag = 1;
			goto reswitch;

		case '+':
			sign = -1;
			goto reswitch;

		case '-':
			ladjust = 1;
			goto reswitch;

		case '%':
			PUTC (c);
			break;

		case '*':
			if (! dot) {
				width = va_arg (ap, int);
				if (width < 0) {
					ladjust = !ladjust;
					width = -width;
				}
			} else {
				dwidth = va_arg (ap, int);
			}
			goto reswitch;

        case '@':
            c = FETCH_BYTE (fmt++);
            if (c != 'D'){
                padding = c;
                goto reswitch;
            }
            //here is complex dump
            dump_width = width;
            if (! dump_width)
                dump_width = 16;
            dump_prec = dwidth;
            dump_pad = padding;
            if (sharpflag)
                dump_pad = (dwidth > 0)?'\n' : ':';
            padding = ' ';
            dump_pos = 0;
            dwidth = -1;
            width = 0;
            sharpflag = 0;
            dump_s = va_arg (ap, const unsigned char*);
            goto reswitch;

		case '0':
			if (! dot) {
				padding = '0';
				goto reswitch;
			}
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			for (n=0; ; ++fmt) {
				n = n * 10 + c - '0';
				c = FETCH_BYTE (fmt);
				if (c < '0' || c > '9')
					break;
			}
			if (dot)
				dwidth = n;
			else
				width = n;
			goto reswitch;

		case 'b':
			ul = va_arg (ap, int);
			s = va_arg (ap, const unsigned char*);
            base = *s++;
            if (*s == 0)
                goto number;
			q = ksprintn (nbuf, ul, base, -1, 0);
			while (*q)
				PUTC (*q--);

			if (! ul)
				break;
			size = 0;
			while (*s) {
				n = *s++;
				if ((char) (ul >> (n-1)) & 1) {
					PUTC (size ? ',' : '<');
					for (; (n = *s) > ' '; ++s)
						PUTC (n);
					size = 1;
				} else
					while (*s > ' ')
						++s;
			}
			if (size)
				PUTC ('>');
			break;

		case 'c':
			if (! ladjust && width > 0)
				while (width--)
					PUTC (' ');

			PUTC (va_arg (ap, int));

			if (ladjust && width > 0)
				while (width--)
					PUTC (' ');
			break;

		case 'D':
//dumpcmd:
            dump_s = va_arg (ap, const unsigned char*);
            if (dump_width >= 0){
                dump_pos = 0;
                goto takenumber;
            }
            dump_width = width;
            if (! dump_width)
                dump_width = 16;
            dump_prec = dwidth-1;
            dump_pad = padding;
            if (sharpflag){
                if (dump_prec >= 0)
                    dump_pad = '\n';
                else
                    padding = ':';
            }
			{
			    int prec = dump_prec;
			while (width--) {
				c = *dump_s++;
				PUTC (mkhex (c >> 4));
				PUTC (mkhex (c));
				if (prec != 0) {
				    //precision denotes size of groups delimited by space
	                if (width)
	                    PUTC (padding);
				}
				else {
                    PUTC (dump_pad);
				}
				prec--;
				if (prec < 0)
				    prec = dump_prec;
			}//while (width--)
			}
			break;

        case 'i':
		case 'd':
			if (! sign) sign = 1;
			base = 10;
			goto takenumber;

		case 'l':
			lflag = laslong;
			goto reswitch;

		case 'h':
            lflag = lasshort;
            goto reswitch;

		case 'o':
			base = 8;
			goto nosign;

		case 'p':
			ul = (size_t) va_arg (ap, void*);
			if (! ul) {
				s = (const unsigned char*) "(nil)";
				goto const_string;
			}
			base = 16;
			sign = 0;
			sharpflag = (width == 0);
			goto number;

		case 'n':
			base = 10;
			goto takenumber;

		case 'S':
#ifdef __AVR__
			s = va_arg (ap, const unsigned char*);
			if (! s)
				s = (const unsigned char*) "(null)";
const_string:
			n = 0;
			if (! dot)
				while (FETCH_BYTE (s+n))
					++n;
			else
				while (n<dwidth && FETCH_BYTE (s+n))
					++n;

			width -= n;

			if (! ladjust && width > 0)
				while (width--)
					PUTC (' ');
			while (n--)
				PUTC (FETCH_BYTE (s++));
			if (ladjust && width > 0)
				while (width--)
					PUTC (' ');
			break;
#endif /* LY: AVR, and no break here! */

		case 's':
			s = va_arg (ap, unsigned char*);
#ifdef __AVR__
			if (! s) {
				s = (const unsigned char*) "(null)";
				goto const_string;
			}
#else
			if (! s)
				s = (const unsigned char*) "(null)";
const_string:
#endif
			if (! dot)
				n = strlen (s);
			else
				for (n=0; n<dwidth && s[n]; n++)
					continue;

			width -= n;

			if (! ladjust && width > 0)
				while (width--)
					PUTC (' ');
			while (n--)
				PUTC (*s++);
			if (ladjust && width > 0)
				while (width--)
					PUTC (' ');
			break;

		case 'r':
			/* Saturated counters. */
			base = 10;
			sign = 0;
			if (lflag != laslong) {
				ul = va_arg (ap, unsigned long);
				if (ul == (unsigned long)-1) {
cnt_unknown:				if (ladjust)
						PUTC ('-');
					while (--width > 0)
						PUTC (' ');
					if (! ladjust)
						PUTC ('-');
					break;
				}
				if (ul >= (unsigned long)-2) {
					ul = -3;
					neg = '>';
					goto number;
				}
			} else {
				ul = va_arg (ap, unsigned int);
				if (ul == (unsigned short) -1)
					goto cnt_unknown;
				if (ul >= (unsigned short) -2) {
					ul = (unsigned short) -3;
					neg = '>';
					goto number;
				}
			}
			goto number;

		case 'u':
			base = 10;
			goto nosign;

		case 'x':
		case 'X':
			base = 16;
			uppercase = (c == 'X');
			goto nosign;

		case 'z':
		case 'Z':
			base = 16;
			uppercase = (c == 'Z');
			goto takenumber;

nosign:			sign = 0;

takenumber:
            if (dump_width <= 0 ){
            if (lflag == laslong){
                if (sign)
                    ul = (unsigned long)va_arg (ap, long);
                else
                    ul = va_arg (ap, unsigned long);
            }
            else {
                if (sign)
                    ul = (unsigned long)va_arg (ap, int);
                else
                    ul = va_arg (ap, unsigned int);
            }
            }//if (dump_width <= 0 )
            else{
                switch (lflag){
                    case 0 :
                    case 1 :
                    default :
                        if (sign)
                            ul = (unsigned long)(long)(char)(*dump_s++);
                        else
                            ul = (*dump_s++);
                        break;

                    case lasshort :
                            if (sign)
                                ul = (unsigned long)(long)*((short*)dump_s);
                            else
                                ul = *((unsigned short*)dump_s);
                            dump_s += sizeof(short);
                            break;

                    case laslong :
                            if (sign)
                                ul = *((long*)dump_s);
                            else
                                ul = *((unsigned long*)dump_s);
                            dump_s += sizeof(long);
                            break;
                }//switch (lflag)
                dump_width--;
            }//else //if (dump_width <= 0 )

number:		if (sign && ((long) ul != 0L)) {
				if ((long) ul < 0L) {
					neg = '-';
					ul = -(long) ul;
				} else if (sign < 0)
					neg = '+';
			}
			if (dwidth >= (int) sizeof(nbuf)) {
				extrazeros = dwidth - sizeof(nbuf) + 1;
				dwidth = sizeof(nbuf) - 1;
			}
			s = ksprintn (nbuf, ul, base, dwidth, &size);
			if (sharpflag && ul != 0) {
				if (base == 8)
					size++;
				else if (base == 16)
					size += 2;
			}
			if (neg)
				size++;

			if (! ladjust && width && padding == ' ' &&
			    (width -= size) > 0)
				do {
					PUTC (' ');
				} while (--width > 0);

			if (neg)
				PUTC (neg);

			if (sharpflag && ul != 0) {
				if (base == 8) {
					PUTC ('0');
				} else if (base == 16) {
					PUTC ('0');
					PUTC (uppercase ? 'X' : 'x');
				}
			}

			if (extrazeros)
				do {
					PUTC ('0');
				} while (--extrazeros > 0);

			if (! ladjust && width && (width -= size) > 0)
				do {
					PUTC (padding);
				} while (--width > 0);

			for (; *s; --s) {
			    char tmpc = *s;
                if (uppercase && tmpc>='a' && tmpc<='z') {
                    tmpc += ('A' - 'a');
                }
                PUTC (tmpc);
			}

			if (ladjust && width && (width -= size) > 0)
				do {
					PUTC (' ');
				} while (--width > 0);

            if (dump_width <= 0)
                break;

            dump_pos++;
            if ((dump_pos % dump_prec) != 0){
                PUTC(' ');
            }
            else
                PUTC(dump_pad);
            neg = 0;
            goto doswitch;

#ifdef ARCH_HAVE_FPU
		case 'e':
		case 'E':
		case 'f':
		case 'F':
		case 'g':
		case 'G': {
			double d = va_arg (ap, double);

			/*
			 * don't do unrealistic precision; just pad it with
			 * zeroes later, so buffer size stays rational.
			 */
			if (dwidth > DBL_DIG) {
				if ((c != 'g' && c != 'G') || sharpflag)
					extrazeros = dwidth - DBL_DIG;
				dwidth = DBL_DIG;
			} else if (dwidth == -1) {
				dwidth = (lflag ? DBL_DIG : FLT_DIG);
			}
			/*
			 * softsign avoids negative 0 if d is < 0 and
			 * no significant digits will be shown
			 */
			if (d < 0) {
				neg = 1;
				d = -d;
			}
			/*
			 * cvt may have to round up past the "start" of the
			 * buffer, i.e. ``intf("%.2f", (double)9.999);'';
			 * if the first char isn't NULL, it did.
			 */
			if (isnan (d) || isinf (d)) {
				strcpy_flash (nbuf, isnan (d) ? "NaN" : "Inf");
				size = 3;
				extrazeros = 0;
				s = nbuf;
			} else {
				*nbuf = 0;
				size = cvt (d, dwidth, sharpflag, &neg, c,
					nbuf, nbuf + sizeof(nbuf) - 1);
				if (*nbuf) {
					s = nbuf;
					nbuf [size] = 0;
				} else {
					s = nbuf + 1;
					nbuf [size + 1] = 0;
				}
			}
			if (neg)
				size++;
			if (! ladjust && width && padding == ' ' &&
			    (width -= size) > 0)
				do {
					PUTC (' ');
				} while (--width > 0);

			if (neg)
				PUTC ('-');

			if (! ladjust && width && (width -= size) > 0)
				do {
					PUTC (padding);
				} while (--width > 0);

			for (; *s; ++s) {
				if (extrazeros && (*s == 'e' || *s == 'E'))
					do {
						PUTC ('0');
					} while (--extrazeros > 0);

				PUTC (*s);
			}
			if (extrazeros)
				do {
					PUTC ('0');
				} while (--extrazeros > 0);

			if (ladjust && width && (width -= size) > 0)
				do {
					PUTC (' ');
				} while (--width > 0);
			break;
		}
#endif
		default:
			PUTC ('%');
			if (lflag == laslong)
				PUTC ('l');
			PUTC (c);
			break;
		}
	}
}

/*
 * Put a NUL-terminated ASCII number (base <= 16) in a buffer in reverse
 * order; return an optional length and a pointer to the last character
 * written in the buffer (i.e., the first character of the string).
 * The buffer pointed to by `nbuf' must have length >= MAXNBUF.
 */
static unsigned char *
ksprintn (unsigned char *nbuf, unsigned long ul, unsigned char base, int width,
	unsigned char *lenp)
{
	unsigned char *p;

	p = nbuf;
	*p = 0;
	if (base <= 16)
	for (;;) {
		unsigned rest = mkhex (ul % base);
		ul /= base;
		*++p = rest;
		if (--width > 0)
			continue;
		if (! ul)
			break;
	}
	if (lenp)
		*lenp = p - nbuf;
	return (p);
}

static unsigned char
mkhex (unsigned char ch)
{
	ch &= 15;
	if (ch > 9)
		return ch + 'a' - 10;
	return ch + '0';
}

#ifdef ARCH_HAVE_FPU
static unsigned char *
cvtround (double fract, int *exp, unsigned char *start, unsigned char *end, unsigned char ch,
	unsigned char *negp)
{
	double tmp;

	if (fract) {
		modf (fract * 10, &tmp);
	} else {
		tmp = ch - '0';
	}
	if (tmp > 4) {
		for (;; --end) {
			if (*end == '.') {
				--end;
			}
			if (++*end <= '9') {
				break;
			}
			*end = '0';
			if (end == start) {
				if (exp) {	/* e/E; increment exponent */
					*end = '1';
					++*exp;
				} else {	/* f; add extra digit */
					*--end = '1';
					--start;
				}
				break;
			}
		}
	} else if (*negp) {
		/*
		 * ``"%.3f", (double)-0.0004'' gives you a negative 0.
		 */
		for (;; --end) {
			if (*end == '.') {
				--end;
			}
			if (*end != '0') {
				break;
			}
			if (end == start) {
				*negp = 0;
			}
		}
	}
	return start;
}

static unsigned char *
exponent (unsigned char *p, int exp, unsigned char fmtch)
{
	unsigned char expbuf [8], *t;

	*p++ = fmtch;
	if (exp < 0) {
		exp = -exp;
		*p++ = '-';
	} else {
		*p++ = '+';
	}
	t = expbuf + sizeof(expbuf);
	if (exp > 9) {
		do {
			*--t = exp % 10 + '0';
		} while ((exp /= 10) > 9);
		*--t = exp + '0';
		for (; t < expbuf + sizeof(expbuf); *p++ = *t++)
			continue;
	} else {
		*p++ = '0';
		*p++ = exp + '0';
	}
	return p;
}

static int
cvt (double number, int prec, int sharpflag, unsigned char *negp, unsigned char fmtch,
	unsigned char *startp, unsigned char *endp)
{
	unsigned char *p, *t;
	double fract;
	int dotrim, expcnt, gformat;
	double integer, tmp;

	expcnt = 0;
	dotrim = expcnt = gformat = 0;
	fract = modf (number, &integer);

	/*
	 * get an extra slot for rounding
	 */
	t = ++startp;

	/*
	 * get integer portion of number; put into the end of the buffer; the
	 * .01 is added for modf (356.0 / 10, &integer) returning .59999999...
	 */
	for (p = endp - 1; integer; ++expcnt) {
		tmp = modf (integer / 10, &integer);
		*p-- = (int) ((tmp + .01) * 10) + '0';
	}
	switch (fmtch) {
	case 'f':
		/* reverse integer into beginning of buffer */
		if (expcnt) {
			for (; ++p < endp; *t++ = *p);
		} else {
			*t++ = '0';
		}

		/*
		 * if precision required or alternate flag set, add in a
		 * decimal point.
		 */
		if (prec || sharpflag) {
			*t++ = '.';
		}

		/*
		 * if requires more precision and some fraction left
		 */
		if (fract) {
			if (prec) {
				do {
					fract = modf (fract * 10, &tmp);
					*t++ = (int)tmp + '0';
				} while (--prec && fract);
			}
			if (fract) {
				startp = cvtround (fract, 0, startp,
					t - 1, '0', negp);
			}
		}
		for (; prec--; *t++ = '0');
		break;
	case 'e':
	case 'E':
eformat:	if (expcnt) {
			*t++ = *++p;
			if (prec || sharpflag) {
				*t++ = '.';
			}

			/*
			 * if requires more precision and some integer left
			 */
			for (; prec && ++p < endp; --prec) {
				*t++ = *p;
			}

			/*
			 * if done precision and more of the integer component,
			 * round using it; adjust fract so we don't re-round
			 * later.
			 */
			if (! prec && ++p < endp) {
				fract = 0;
				startp = cvtround (0, &expcnt, startp,
					t - 1, *p, negp);
			}
			/*
			 * adjust expcnt for digit in front of decimal
			 */
			--expcnt;
		}
		/*
		 * until first fractional digit, decrement exponent
		 */
		else if (fract) {
			/*
			 * adjust expcnt for digit in front of decimal
			 */
			for (expcnt = -1;; --expcnt) {
				fract = modf (fract * 10, &tmp);
				if (tmp) {
					break;
				}
			}
			*t++ = (int)tmp + '0';
			if (prec || sharpflag) {
				*t++ = '.';
			}
		} else {
			*t++ = '0';
			if (prec || sharpflag) {
				*t++ = '.';
			}
		}
		/*
		 * if requires more precision and some fraction left
		 */
		if (fract) {
			if (prec) {
				do {
					fract = modf (fract * 10, &tmp);
					*t++ = (int)tmp + '0';
				} while (--prec && fract);
			}
			if (fract) {
				startp = cvtround (fract, &expcnt, startp,
					t - 1, '0', negp);
			}
		}
		/*
		 * if requires more precision
		 */
		for (; prec--; *t++ = '0');

		/*
		 * unless alternate flag, trim any g/G format trailing 0's
		 */
		if (gformat && ! sharpflag) {
			while (t > startp && *--t == '0');
			if (*t == '.') {
				--t;
			}
			++t;
		}
		t = exponent (t, expcnt, fmtch);
		break;
	case 'g':
	case 'G':
		/*
		 * a precision of 0 is treated as a precision of 1
		 */
		if (!prec) {
			++prec;
		}

		/*
		 * ``The style used depends on the value converted; style e
		 * will be used only if the exponent resulting from the
		 * conversion is less than -4 or greater than the precision.''
		 *	-- ANSI X3J11
		 */
		if (expcnt > prec || (! expcnt && fract && fract < .0001)) {
			/*
			 * g/G format counts "significant digits, not digits of
			 * precision; for the e/E format, this just causes an
			 * off-by-one problem, i.e. g/G considers the digit
			 * before the decimal point significant and e/E doesn't
			 * count it as precision.
			 */
			--prec;
			fmtch -= 2;		/* G->E, g->e */
			gformat = 1;
			goto eformat;
		}
		/*
		 * reverse integer into beginning of buffer,
		 * note, decrement precision
		 */
		if (expcnt) {
			for (; ++p < endp; *t++ = *p, --prec);
		} else {
			*t++ = '0';
		}
		/*
		 * if precision required or alternate flag set, add in a
		 * decimal point.  If no digits yet, add in leading 0.
		 */
		if (prec || sharpflag) {
			dotrim = 1;
			*t++ = '.';
		} else {
			dotrim = 0;
		}
		/*
		 * if requires more precision and some fraction left
		 */
		while (prec && fract) {
			fract = modf (fract * 10, &tmp);
			*t++ = (int)tmp + '0';
			prec--;
		}
		if (fract) {
			startp = cvtround (fract, 0, startp, t - 1, '0', negp);
		}
		/*
		 * alternate format, adds 0's for precision, else trim 0's
		 */
		if (sharpflag) {
			for (; prec--; *t++ = '0');
		} else if (dotrim) {
			while (t > startp && *--t == '0');
			if (*t != '.') {
				++t;
			}
		}
	}
	return(t - startp);
}
#endif /* ARCH_HAVE_FPU */

#ifdef TEST
/*
 * Testing vprintf(3).
 */
#undef printf
#undef fclose
#undef fflush
#undef vprintf
#undef getchar
#undef putchar
#undef gets
#undef puts
#undef feof
#define snprintf snprintf_tmp
#define vsnprintf vsnprintf_tmp
#include <stdio.h>
#undef snprintf
#undef vsnprintf

#ifndef UCHAR_MAX
#define UCHAR_MAX ((unsigned char) -1)
#endif
#ifndef USHRT_MAX
#define USHRT_MAX ((unsigned short) -1)
#endif

#define fputs(s,f) printstring (s)
#define puts(s) { printstring (s); write (1, "\n", 1); }

unsigned write (int fd, const void *buf, unsigned count);

static void
printchar (stream_t *stream, short ch)
{
	write (1, &ch, 1);
}

static void
printstring (const char *str)
{
	write (1, str, strlen (str));
}

int
printf (const char *fmt, ...)
{
	static stream_interface_t output_interface = {
		(void (*) (stream_t*, short)) printchar,
		0, 0,
	};
	stream_t output = { &output_interface };
	va_list	args;
	int err;

	va_start (args, fmt);
	err = stream_vprintf (&output, fmt, args);
	va_end (args);
	return err;
}

static void rfg1 (void);
static void rfg2 (void);
static void rfg3 (void);

static void fmtchk (const char *fmt)
{
	(void)fputs (fmt, stdout);
	(void)printf (":\t`");
	(void)printf (fmt, 0x12);
	(void)printf ("'\n");
}

static void fmtst1chk (const char *fmt)
{
	(void)fputs (fmt, stdout);
	(void)printf (":\t`");
	(void)printf (fmt, 4, 0x12);
	(void)printf ("'\n");
}

static void fmtst2chk (const char *fmt)
{
	(void)fputs (fmt, stdout);
	(void)printf (":\t`");
	(void)printf (fmt, 4, 4, 0x12);
	(void)printf ("'\n");
}

/* This page is covered by the following copyright: */

/* (C) Copyright C E Chew
 *
 * Feel free to copy, use and distribute this software provided:
 *
 *	1. you do not pretend that you wrote it
 *	2. you leave this copyright notice intact.
 */

/*
 * Extracted from exercise.c for glibc-1.05 bug report by Bruce Evans.
 */

#define DEC -123
#define INT 255
#define UNS (~0)

/* Formatted Output Test
 *
 * This exercises the output formatting code.
 */

static void fp_test (void)
{
	int i, j, k, l;
	char buf[7];
	char *prefix = buf;
	char tp[20];

	puts ("\nFormatted output test");
	printf ("prefix  6d      6o      6x      6X      6u\n");
	strcpy (prefix, "%");
	for (i = 0; i < 2; i++) {
		for (j = 0; j < 2; j++) {
			for (k = 0; k < 2; k++) {
				for (l = 0; l < 2; l++) {
					strcpy (prefix, "%");
					if (i == 0)
						strcat (prefix, "-");
					if (j == 0)
						strcat (prefix, "+");
					if (k == 0)
						strcat (prefix, "#");
					if (l == 0)
						strcat (prefix, "0");
					printf ("%5s |", prefix);
					strcpy (tp, prefix);
					strcat (tp, "6d |");
					printf (tp, DEC);
					strcpy (tp, prefix);
					strcat (tp, "6o |");
					printf (tp, INT);
					strcpy (tp, prefix);
					strcat (tp, "6x |");
					printf (tp, INT);
					strcpy (tp, prefix);
					strcat (tp, "6X |");
					printf (tp, INT);
					strcpy (tp, prefix);
					strcat (tp, "6u |");
					printf (tp, UNS);
					printf ("\n");
				}
			}
		}
	}
	printf ("%10s\n", (char *)0);
	printf ("%-10s\n", (char *)0);
}

int main ()
{
	static char shortstr[] = "Hi, Z.";
	static char longstr[] = "Good morning, Doctor Chandra.  This is Hal.  \
I am ready for my first lesson today.";
	int result = 0;

	fmtchk ("%.4x");
	fmtchk ("%04x");
	fmtchk ("%4.4x");
	fmtchk ("%04.4x");
	fmtchk ("%4.3x");
	fmtchk ("%04.3x");

	fmtst1chk ("%.*x");
	fmtst1chk ("%0*x");
	fmtst2chk ("%*.*x");
	fmtst2chk ("%0*.*x");

	printf ("binary format:\t\"%b\"\n", 55, "\20\7b7\6b6\5b5\4b4\3b3\2b2\1b1");
	printf ("nil pointer (padded):\t\"%10p\"\n", (void *)0);

	printf ("decimal negative:\t\"%d\"\n", -2345);
	printf ("octal negative:\t\"%o\"\n", -2345);
	printf ("hex negative:\t\"%x\"\n", -2345);
	printf ("long decimal number:\t\"%ld\"\n", -123456L);
	printf ("long octal negative:\t\"%lo\"\n", -2345L);
	printf ("long unsigned decimal number:\t\"%lu\"\n", -123456L);
	printf ("zero-padded LDN:\t\"%010ld\"\n", -123456L);
	printf ("left-adjusted ZLDN:\t\"%-010ld\"\n", -123456L);
	printf ("space-padded LDN:\t\"%10ld\"\n", -123456L);
	printf ("left-adjusted SLDN:\t\"%-10ld\"\n", -123456L);

	printf ("zero-padded string:\t\"%010s\"\n", shortstr);
	printf ("left-adjusted Z string:\t\"%-010s\"\n", shortstr);
	printf ("space-padded string:\t\"%10s\"\n", shortstr);
	printf ("left-adjusted S string:\t\"%-10s\"\n", shortstr);
	printf ("null string:\t\"%s\"\n", (char *)0);
	printf ("limited string:\t\"%.22s\"\n", longstr);

	printf ("e-style >= 1:\t\"%e\"\n", 12.34);
	printf ("e-style >= .1:\t\"%e\"\n", 0.1234);
	printf ("e-style < .1:\t\"%e\"\n", 0.001234);
	printf ("e-style big:\t\"%.60e\"\n", 1e20);
	printf ("e-style == .1:\t\"%e\"\n", 0.1);
	printf ("f-style >= 1:\t\"%f\"\n", 12.34);
	printf ("f-style >= .1:\t\"%f\"\n", 0.1234);
	printf ("f-style < .1:\t\"%f\"\n", 0.001234);
	printf ("g-style >= 1:\t\"%g\"\n", 12.34);
	printf ("g-style >= .1:\t\"%g\"\n", 0.1234);
	printf ("g-style < .1:\t\"%g\"\n", 0.001234);
	printf ("g-style big:\t\"%.60g\"\n", 1e20);

	printf (" %6.5f\n", .099999999860301614);
	printf (" %6.5f\n", .1);
	printf ("x%5.4fx\n", .5);

	printf ("%#03x\n", 1);

	printf ("something really insane: %.10000f\n", 1.0);

	{
		double d = FLT_MIN;
		int niter = 17;

		while (niter-- != 0)
			printf ("%.17e\n", d / 2);
		fflush (stdout);
	}

	printf ("%15.5e\n", 4.9406564584124654e-324);

#define FORMAT "|%12.4f|%12.4e|%12.4g|\n"
	printf (FORMAT, 0.0, 0.0, 0.0);
	printf (FORMAT, 1.0, 1.0, 1.0);
	printf (FORMAT, -1.0, -1.0, -1.0);
	printf (FORMAT, 100.0, 100.0, 100.0);
	printf (FORMAT, 1000.0, 1000.0, 1000.0);
	printf (FORMAT, 10000.0, 10000.0, 10000.0);
	printf (FORMAT, 12345.0, 12345.0, 12345.0);
	printf (FORMAT, 100000.0, 100000.0, 100000.0);
	printf (FORMAT, 123456.0, 123456.0, 123456.0);
#undef	FORMAT

	{
		char buf[20];
		char buf2[512];

		printf ("snprintf (\"%%30s\", \"foo\") == %d, \"%.*s\"\n",
		    snprintf (buf, sizeof (buf), "%30s", "foo"), (int)sizeof (buf),
		    buf);
		printf ("snprintf (\"%%.999999u\", 10) == %d\n",
		    snprintf (buf2, sizeof (buf2), "%.999999u", 10));
	}

	fp_test ();

	printf ("%e should be 1.234568e+06\n", 1234567.8);
	printf ("%f should be 1234567.800000\n", 1234567.8);
	printf ("%g should be 1.23457e+06\n", 1234567.8);
	printf ("%g should be 123.456\n", 123.456);
	printf ("%g should be 1e+06\n", 1000000.0);
	printf ("%g should be 10\n", 10.0);
	printf ("%g should be 0.02\n", 0.02);

	{
		char buf[200];

		snprintf (buf, sizeof(buf), "%*s%*s%*s", -1, "one", -20, "two", -30, "three");

		result |= strcmp (buf,
		    "onetwo                 three                         ");

		puts (result != 0 ? "Test failed!" : "Test ok.");
	}
#if 0
	{
		char buf[200];

		snprintf (buf, sizeof(buf), "%07Lo", 040000000000ll);
		printf ("sprintf (buf, \"%%07Lo\", 040000000000ll) = %s", buf);

		if (strcmp (buf, "40000000000") != 0) {
			result = 1;
			fputs ("\tFAILED", stdout);
		}
		puts ("");
	}

	printf ("printf (\"%%hhu\", %u) = %hhu\n", UCHAR_MAX + 2, UCHAR_MAX + 2);
	printf ("printf (\"%%hu\", %u) = %hu\n", USHRT_MAX + 2, USHRT_MAX + 2);
#endif
	puts ("--- Should be no further output. ---");
	rfg1 ();
	rfg2 ();
#if 0
	rfg3 ();
	{
		char bytes[7];
		char buf[20];

		memset (bytes, '\xff', sizeof bytes);
		snprintf (buf, sizeof(buf), "foo%hhn\n", &bytes[3]);
		if (bytes[0] != '\xff' || bytes[1] != '\xff' || bytes[2] != '\xff'
		    || bytes[4] != '\xff' || bytes[5] != '\xff' || bytes[6] != '\xff') {
			puts ("%hhn overwrite more bytes");
			result = 1;
		}
		if (bytes[3] != 3) {
			puts ("%hhn wrote incorrect value");
			result = 1;
		}
	}
#endif
	return result != 0;
}

static void rfg1 (void)
{
	char buf[100];

	snprintf (buf, sizeof(buf), "%5.s", "xyz");
	if (strcmp (buf, "     ") != 0)
		printf ("got: '%s', expected: '%s'\n", buf, "     ");
	snprintf (buf, sizeof(buf), "%5.f", 33.3);
	if (strcmp (buf, "   33") != 0)
		printf ("got: '%s', expected: '%s'\n", buf, "   33");
	snprintf (buf, sizeof(buf), "%8.e", 33.3e7);
	if (strcmp (buf, "   3e+08") != 0)
		printf ("got: '%s', expected: '%s'\n", buf, "   3e+08");
	snprintf (buf, sizeof(buf), "%8.E", 33.3e7);
	if (strcmp (buf, "   3E+08") != 0)
		printf ("got: '%s', expected: '%s'\n", buf, "   3E+08");
	snprintf (buf, sizeof(buf), "%.g", 33.3);
	if (strcmp (buf, "3e+01") != 0)
		printf ("got: '%s', expected: '%s'\n", buf, "3e+01");
	snprintf (buf, sizeof(buf), "%.G", 33.3);
	if (strcmp (buf, "3E+01") != 0)
		printf ("got: '%s', expected: '%s'\n", buf, "3E+01");
}

static void rfg2 (void)
{
	int prec;
	char buf[100];

	prec = 0;
	snprintf (buf, sizeof(buf), "%.*g", prec, 3.3);
	if (strcmp (buf, "3") != 0)
		printf ("got: '%s', expected: '%s'\n", buf, "3");
	prec = 0;
	snprintf (buf, sizeof(buf), "%.*G", prec, 3.3);
	if (strcmp (buf, "3") != 0)
		printf ("got: '%s', expected: '%s'\n", buf, "3");
	prec = 0;
	snprintf (buf, sizeof(buf), "%7.*G", prec, 3.33);
	if (strcmp (buf, "      3") != 0)
		printf ("got: '%s', expected: '%s'\n", buf, "      3");
	prec = 3;
	snprintf (buf, sizeof(buf), "%04.*o", prec, 33);
	if (strcmp (buf, " 041") != 0)
		printf ("got: '%s', expected: '%s'\n", buf, " 041");
	prec = 7;
	snprintf (buf, sizeof(buf), "%09.*u", prec, 33);
	if (strcmp (buf, "  0000033") != 0)
		printf ("got: '%s', expected: '%s'\n", buf, "  0000033");
	prec = 3;
	snprintf (buf, sizeof(buf), "%04.*x", prec, 33);
	if (strcmp (buf, " 021") != 0)
		printf ("got: '%s', expected: '%s'\n", buf, " 021");
	prec = 3;
	snprintf (buf, sizeof(buf), "%04.*X", prec, 33);
	if (strcmp (buf, " 021") != 0)
		printf ("got: '%s', expected: '%s'\n", buf, " 021");
}

static void rfg3 (void)
{
	char buf[100];
	double g = 5.0000001;
	unsigned long l = 1234567890;
	double d = 321.7654321;
	const char s[] = "test-string";
	int i = 12345;
	int h = 1234;

	snprintf (buf, sizeof(buf),
	    "%1$*5$d %2$*6$hi %3$*7$lo %4$*8$f %9$*12$e %10$*13$g %11$*14$s",
	    i, h, l, d, 8, 5, 14, 14, d, g, s, 14, 3, 14);
	if (strcmp (buf,
		"   12345  1234    11145401322     321.765432   3.217654e+02   5    test-string") != 0)
		printf ("got: '%s', expected: '%s'\n", buf,
		    "   12345  1234    11145401322     321.765432   3.217654e+02   5    test-string");
}
#endif
