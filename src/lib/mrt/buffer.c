/*
 * $Id: buffer.c,v 1.1.1.1 2000/08/14 18:46:11 labovit Exp $
 */

#include "mrt.h"
#include "buffer.h"

#ifndef NT
#include <sys/utsname.h>
#endif /* NT */

static int buffer_aspath_toa (aspath_t * aspath, buffer_t *buffer);

/* these functions may be called from trace() so that we have to 
   avoid from calling high-level functions that may call trace() */

buffer_t *
New_Buffer_Stream (FILE *stream)
{
   buffer_t *buffer = New (buffer_t);

   buffer->type = BUFFER_TYPE_STREAM;
   buffer->stream = stream;
   return (buffer);
}


buffer_t *
New_Buffer (int len)
{
   buffer_t *buffer = New (buffer_t);

#define INITIAL_BUFFER_ALLOC 255
   if (len == 0)
	len = INITIAL_BUFFER_ALLOC;
   buffer->type = BUFFER_TYPE_MEMORY;
   buffer->data = malloc (len + 1);
   buffer->len = len + 1;
   buffer->data_len = 0;
   buffer->data[0] = '\0';
   return (buffer);
}

buffer_t *
Copy_Buffer (buffer_t *origbuff)
{
   buffer_t *buffer = New (buffer_t);
   /* copy the space really occupied */
   buffer->type = origbuff->type;
   buffer->data_len = origbuff->data_len;
   buffer->len = buffer->data_len + 1;
   buffer->data = malloc (buffer->len);
   memcpy (buffer->data, origbuff->data, buffer->len);
   return (buffer);
}


int
buffer_putc (int c, buffer_t *buffer)
{
    if (buffer->stream) {
	return (putc (c, buffer->stream));
    }
    else if (buffer->data) {
	if (buffer->data_len + 1 > buffer->len - 1) {
	    buffer->len = buffer->len * 2;
	    buffer->data = realloc (buffer->data, buffer->len);
	}
	buffer->data[buffer->data_len++] = c;
	buffer->data[buffer->data_len] = '\0';
	return (c);
    }
    return (-1);
}


int
buffer_puts (char *s, buffer_t *buffer)
{
    if (buffer->stream) {
	return (fputs (s, buffer->stream));
    }
    else if (buffer->data) {
	int len = strlen (s);

	if (buffer->data_len + len > buffer->len - 1) {
	    while (buffer->data_len + len > buffer->len - 1)
	        buffer->len = buffer->len * 2;
	    buffer->data = realloc (buffer->data, buffer->len);
	}
	strcpy ((char *) buffer->data + buffer->data_len, s);
	buffer->data_len += len;
	return (len);
    }
    return (-1);
}


int
buffer_gets (buffer_t *buffer)
{
    if (buffer->stream) {
	if (buffer->data == NULL) {
	    int len = INITIAL_BUFFER_ALLOC;
   	    buffer->data = malloc (len);
   	    buffer->len = len;
   	    buffer->data_len = 0;
	}
	else {
   	    buffer->data_len = 0;
	}
	while (fgets ((char *) buffer->data + buffer->data_len, 
		      buffer->len - buffer->data_len, 
		      buffer->stream) != NULL) {
	    buffer->data_len = buffer->data_len + 
			   strlen ((char *) buffer->data + buffer->data_len);
	    if (buffer->data_len < buffer->len - 1)
		break;
	    if (feof (buffer->stream))
		break;
	    buffer->len = buffer->len * 2;
   	    buffer->data = realloc (buffer->data, buffer->len);
	}
	return (buffer->data_len);
    }
    return (-1);
}


int
buffer_peek (buffer_t *buffer, int pos)
{
    if (buffer->data) {
	if (pos > 0 && pos <= buffer->data_len)
	    return (buffer->data[pos]);
    }
    return (-1);
}


int
buffer_poke (int c, buffer_t *buffer, int pos)
{
    if (pos < 0)
	pos = buffer->data_len + pos;
    if (buffer->data) {
	if (pos <= buffer->data_len) {
	    buffer->data[pos] = c;
	    return (c);
	}
	else if (pos < buffer->len - 1) {
	    /* is this OK? */
	    buffer->data[pos] = c;
	    return (c);
	}
    }
    return (-1);
}


int
buffer_insert (int c, buffer_t *buffer, int pos)
{
    if (pos < 0)
	pos = buffer->data_len + pos;
    if (buffer->data) {
	if (pos <= buffer->data_len) {
	    if (buffer->data_len >= buffer->len - 1) {
	        buffer_adjust (buffer, buffer->data_len + 1);
	    }
	    memmove (buffer->data + pos + 1, buffer->data + pos,
                         buffer->data_len - pos);
	    buffer->data_len++;
	    buffer->data[pos] = c;
	    return (c);
	}
    }
    return (-1);
}


int
buffer_delete (buffer_t *buffer, int pos)
{
    if (pos < 0)
	pos = buffer->data_len + pos;
    if (buffer->data) {
	if (pos <= buffer->data_len) {
	    memmove (buffer->data + pos, buffer->data + pos + 1,
                         buffer->data_len - pos + 1);
	    buffer->data_len--;
	    return (buffer->data_len);
	}
    }
    return (-1);
}


int
buffer_adjust (buffer_t *buffer, int len)
{
    if (len < 0)
	len = buffer->data_len + len;
    if (buffer->data) {
	if (len <= buffer->data_len) {
	    buffer->data_len = len;
	    buffer->data[len] = '\0';
	    return (-1);
	}
	else if (len > buffer->len - 1) {
	    while (len > buffer->len - 1)
	        buffer->len = buffer->len * 2;
	    buffer->data = realloc (buffer->data, buffer->len);
	    return (1);
	}
    }
    return (0);
}


void
Delete_Buffer (buffer_t *buffer)
{
   if (buffer->data != NULL)
       Delete (buffer->data);
   Delete (buffer);
}


#ifndef HAVE_SNPRINTF
int
snprintf (char *str, size_t size, const char *format, ...)
{
    int n;
    va_list ap;

    va_start (ap, format);
    n = vsprintf (str, format, ap);
    va_end (ap);
    /* XXX silly way, though */
    assert (strlen (str) < size);
    return (n);
}
#endif /* HAVE_SNPRINTF */


/*
 * This is MRT version of vprintf(). 
 * Not fully compatible (e.g. floating point)
 * %s may be overflowed
 */
int
buffer_vprintf (buffer_t *buffer, char *fmt, va_list ap)
{
    int count = 0;
    char strbuf[256], *cp;
    int n;
    u_long ulongarg;
    char *strarg;
    prefix_t *prefix;
    gateway_t *gateway;
    aspath_t *aspath;
    buffer_t *temp_buffer = NULL;
    int fieldsize, realsize;
#ifndef NT
    struct utsname utsname;
#endif /* NT */
    int sign;
    int leftadjust;
    int alternate;
    int width;
    int precision;
    int zeropad;
    int argtype;
    int base;
    char *digits = "0123456789abcdef";
    int size;

    for (; *fmt; fmt++) {
	if (*fmt != '%') {
	    buffer_putc (*fmt, buffer);
	    count++;
	    continue;
	}

    	sign = '\0';
    	leftadjust = 0;
    	alternate = 0;
    	width = 0;
    	precision = -1;
    	zeropad = 0;
    	argtype = 0;
    	base = 10;

    again:
	switch (*++fmt) {
	case '%':
	    buffer_putc (*fmt, buffer);
	    count++;
	    continue;

	case ' ':
	    if (sign == 0)
		sign = ' ';
	    goto again;

	case '#':
	    alternate++;
	    goto again;

	case '*':
	    width = va_arg (ap, int);
	    if (width < 0) {
		width = -width;
		leftadjust++;
	    }
	    goto again;

	case '-':
	    leftadjust++;
	    goto again;

	case '+':
	    sign = '+';
	    goto again;

	case '.':
	    if (*++fmt == '*')
		n = va_arg (ap, int);
	    else {
		n = 0;
		while (isdigit (*fmt))
		    n = n * 10 + *fmt++ - '0';
		fmt--;
	    }
	    precision = (n < 0)? -1 : n;
	    goto again;

	case '0':
	    zeropad++;
	    goto again;

	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    n = 0;
	    do {
		n = 10 * n + *fmt - '0';
	    } while (isdigit (*++fmt));
	    fmt--;
	    width = n;
	    goto again;

	case 'h':
	case 'l':
	    argtype = *fmt;
	    goto again;
	    
	case 'c':
	    strbuf[0] = va_arg (ap, int);
	    strbuf[1] = '\0';
	    strarg = strbuf;
	    break;

	case 'd':
	case 'i':
	case 'o':
	case 'u':
	case 'X':
	case 'x':
	case 'b':
	    if (argtype == 'h')
		ulongarg = va_arg (ap, short);
	    else if (argtype == 'l')
		ulongarg = va_arg (ap, long);
	    else
		ulongarg = va_arg (ap, int);
	    digits = "0123456789abcdef";
	    switch (*fmt) {
	    case 'd':
	    case 'i':
		if ((long)ulongarg < 0) {
		    ulongarg = -(long)ulongarg;
		    sign = '-';
		}
		base = 10;
		break;
	    case 'o':
		base = 8;
		break;
	    case 'u':
		sign = '\0';
		base = 10;
		break;
	    case 'X':
		digits = "0123456789ABCDEF";
	    case 'x':
		base = 16;
		break;
	    case 'b':
		if (alternate)
		    digits = "_|";
		base = 2;
		break;
	    }

	    if (precision >= 0)
		zeropad = 0;

	    cp = strbuf + sizeof (strbuf);
	    *--cp = '\0';
	    if (ulongarg != 0 || precision != 0) {
		do {
		    *--cp = digits[ulongarg % base];
		    ulongarg = ulongarg / base;
		} while (ulongarg != 0);
		if (alternate && base == 8 && *cp != digits[0])
		    *--cp = digits[0];
	    }
	    strarg = cp;
	    size = strbuf + sizeof (strbuf) - cp - 1;

	    goto common;

	case 's':
	    strarg = va_arg (ap, char *);
	    if (strarg == NULL)
		continue;
	    if (fmt[-1] == '%') {
		/* short cut, safer */
		count += buffer_puts (strarg, buffer);
		continue;
	    }
	    break;

	case 'p':
	case 'a':
	    prefix = va_arg (ap, prefix_t *);
	    if (prefix == NULL)
		continue;
	    inet_ntop (prefix->family, prefix_tochar (prefix), 
		       strbuf, sizeof (strbuf));
	    if (*fmt == 'p')
	        sprintf (strbuf + strlen (strbuf), "/%d", prefix->bitlen);
	    strarg = strbuf;
	    break;

	case 'g':
	    gateway = va_arg (ap, gateway_t *);
	    if (gateway == NULL)
		continue;
	    inet_ntop (gateway->prefix->family, 
		       prefix_tochar (gateway->prefix), 
		       strbuf, sizeof (strbuf));
	    sprintf (strbuf + strlen (strbuf), " AS%d", gateway->AS);
	    strarg = strbuf;
	    break;

	case 'm':
	    strarg = strerror (errno);
	    break;

	case 'A':
	    aspath = va_arg (ap, aspath_t *);
	    if (fmt[-1] == '%') {
		/* short cut, safer */
		count += buffer_aspath_toa (aspath, buffer);
		continue;
	    }
	    if (temp_buffer == NULL)
	        temp_buffer = New_Buffer (0);
	    buffer_aspath_toa (aspath, temp_buffer);
	    strarg = buffer_data (temp_buffer);
	    break;

	case 'B':
	    strarg = bgptype2string (va_arg (ap, int));
	    break;

	case 'r':
	    strarg = proto2string (va_arg (ap, int));
	    break;

	case 'H':
#ifndef NT
     	    if (uname (&utsname) >= 0)
	        strarg = utsname.nodename;
	    else
#else
				strarg = strdup ("local");
#endif /* NT */
		continue;
	    break;

	default:
	    abort ();
	    break;
	}

    /* string: */
	size = strlen (strarg);
	if (precision >= 0 && size > precision)
	    size = precision;
	sign = '\0';
	base = 0;
	zeropad = 0;

    common:
	fieldsize = size;
    	if (sign)
	    fieldsize++;
	if (alternate && base == 16)
	    fieldsize += 2;
	realsize = (precision > fieldsize)? precision : fieldsize;

	if (!leftadjust && !zeropad && width > 0) {
	    for (n = realsize; n < width; n++) {
		buffer_putc (' ', buffer);
		count++;
	    }
	}
	if (sign) {
	    buffer_putc (sign, buffer);
	    count++;
	}
	if (alternate && base == 16) {
	    buffer_putc (digits[0], buffer);
	    buffer_putc (*fmt, buffer);
	    count += 2;
	}
	if (!leftadjust && zeropad) {
	    for (n = realsize; n < width; n++) {
		buffer_putc (digits[0], buffer);
		count++;
	    }
	}

	for (n = 0; n < size; n++) {
	    buffer_putc (strarg[n], buffer);
	    count++;
	}

	if (leftadjust) {
	    for (n = realsize; n < width; n++) {
		buffer_putc (' ', buffer);
		count++;
	    }
	}
    }
    if (temp_buffer)
	Delete_Buffer (temp_buffer);
    return (count);
}


int
buffer_printf (buffer_t *buffer, char *fmt, ...)
{
    va_list ap;
    int n;

    va_start (ap, fmt);
    n = buffer_vprintf (buffer, fmt, ap);
    va_end (ap);
    return (n);
}


static int
buffer_short_toa (u_short *as, int len, buffer_t *buffer)
{
    int i;
    int count = 0;

    for (i = 0; i < len; i++) {
        char numbuf[16];

	if (count > 0) {
	    buffer_putc (' ', buffer);
	    count++;
	}
	count += sprintf (numbuf, "%d", as[i]);
	buffer_puts (numbuf, buffer);
    }
    return (count);
}


int
buffer_aspath_toa (aspath_t * aspath, buffer_t *buffer)
{
    aspath_segment_t *as_seg;
    int count = 0;

    if (aspath == NULL) {
	return (count);
    }

    LL_Iterate (aspath, as_seg) {

	if (count > 0)
	    buffer_putc (' ', buffer);

	if (as_seg->type == PA_PATH_SET) {
	    buffer_putc ('[', buffer);
	}
	else if (as_seg->type == PA_PATH_SEQ) {
	    /* buffer_putc ('|', buffer); */
	}
	else {
	    buffer_putc ('?', buffer);
	}

        count += buffer_short_toa (as_seg->as, as_seg->len, buffer);

	if (as_seg->type == PA_PATH_SET) {
	    buffer_putc (']', buffer);
	}
	else if (as_seg->type == PA_PATH_SEQ) {
	    /* buffer_putc ('|', buffer); */
	}
	else {
	    buffer_putc ('?', buffer);
	}
    }
    return (count);
}

