#include <console.h>
#include <string.h>
#include <stdarg.h>
#include <task.h>

struct out {
    void (*put)(void *ctx, char c);
    void *ctx;
};

static void put_pad(struct out *o, char c, int count)
{
    while (count-- > 0)
        o->put(o->ctx, c);
}

static void print_number(struct out *o, uint32_t value, int base, bool upper,
                         int width, bool zero, bool left, bool negative)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char reversed[36];
    char text[40];
    int n = 0;
    int i = 0;
    int len;
    int pad;

    if (value == 0)
        reversed[n++] = '0';
    while (value != 0) {
        reversed[n++] = digits[value % (uint32_t)base];
        value /= (uint32_t)base;
    }

    if (negative)
        text[i++] = '-';
    while (n > 0)
        text[i++] = reversed[--n];
    text[i] = '\0';

    len = i;
    pad = width - len;
    if (pad < 0)
        pad = 0;

    if (left) {
        for (i = 0; text[i]; i++)
            o->put(o->ctx, text[i]);
        put_pad(o, ' ', pad);
    } else if (zero) {
        if (negative)
            o->put(o->ctx, '-');
        put_pad(o, '0', pad);
        for (i = negative ? 1 : 0; text[i]; i++)
            o->put(o->ctx, text[i]);
    } else {
        put_pad(o, ' ', pad);
        for (i = 0; text[i]; i++)
            o->put(o->ctx, text[i]);
    }
}

static void format(struct out *o, const char *fmt, va_list ap)
{
    for (; *fmt; fmt++) {
        bool left = false;
        bool zero = false;
        int width = 0;
        int prec = -1;

        if (*fmt != '%') {
            o->put(o->ctx, *fmt);
            continue;
        }
        fmt++;

        while (*fmt == '-' || *fmt == '0' || *fmt == '+' || *fmt == ' ') {
            if (*fmt == '-')
                left = true;
            if (*fmt == '0')
                zero = true;
            fmt++;
        }

        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');

        if (*fmt == '.') {
            fmt++;
            prec = 0;
            while (*fmt >= '0' && *fmt <= '9')
                prec = prec * 10 + (*fmt++ - '0');
        }

        if (*fmt == 'l' || *fmt == 'z' || *fmt == 'h')
            fmt++;

        switch (*fmt) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            int n;
            int pad;

            if (!s)
                s = "(null)";
            n = (int)strlen(s);
            if (prec >= 0 && n > prec)
                n = prec;
            pad = width - n;
            if (pad < 0)
                pad = 0;
            if (!left)
                put_pad(o, ' ', pad);
            for (int i = 0; i < n; i++)
                o->put(o->ctx, s[i]);
            if (left)
                put_pad(o, ' ', pad);
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            int pad = width - 1;

            if (!left && pad > 0)
                put_pad(o, ' ', pad);
            o->put(o->ctx, c);
            if (left && pad > 0)
                put_pad(o, ' ', pad);
            break;
        }
        case 'd':
        case 'i': {
            int32_t v = va_arg(ap, int32_t);
            bool neg = v < 0;
            uint32_t uv = neg ? (uint32_t)0 - (uint32_t)v : (uint32_t)v;

            print_number(o, uv, 10, false, width, zero, left, neg);
            break;
        }
        case 'u':
            print_number(o, va_arg(ap, uint32_t), 10, false, width, zero, left, false);
            break;
        case 'x':
            print_number(o, va_arg(ap, uint32_t), 16, false, width, zero, left, false);
            break;
        case 'X':
            print_number(o, va_arg(ap, uint32_t), 16, true, width, zero, left, false);
            break;
        case 'p': {
            uint32_t v = (uint32_t)(uintptr_t)va_arg(ap, void *);

            o->put(o->ctx, '0');
            o->put(o->ctx, 'x');
            print_number(o, v, 16, false, 8, true, false, false);
            break;
        }
        case '%':
            o->put(o->ctx, '%');
            break;
        case '\0':
            return;
        default:
            o->put(o->ctx, '%');
            o->put(o->ctx, *fmt);
            break;
        }
    }
}

static void kconsole_put(void *ctx, char c)
{
    UNUSED(ctx);
    kputc(c);
}

int kprintf(const char *fmt, ...)
{
    struct out o = { kconsole_put, NULL };
    va_list ap;

    sched_lock();
    va_start(ap, fmt);
    format(&o, fmt, ap);
    va_end(ap);
    sched_unlock();

    return 0;
}

struct bufctx {
    char *buf;
    size_t cap;
    size_t len;
};

static void buffer_put(void *ctx, char c)
{
    struct bufctx *b = (struct bufctx *)ctx;

    if (b->cap && b->len + 1 < b->cap)
        b->buf[b->len] = c;
    b->len++;
}

int ksnprintf(char *buf, size_t cap, const char *fmt, ...)
{
    struct bufctx ctx;
    struct out o;
    va_list ap;
    size_t end;

    ctx.buf = buf;
    ctx.cap = cap;
    ctx.len = 0;

    o.put = buffer_put;
    o.ctx = &ctx;

    va_start(ap, fmt);
    format(&o, fmt, ap);
    va_end(ap);

    if (cap) {
        end = ctx.len < cap - 1 ? ctx.len : cap - 1;
        buf[end] = '\0';
    }

    return (int)ctx.len;
}
