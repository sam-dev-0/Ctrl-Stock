#include "str16.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static int str16_is_space(char16_t c)
{
    return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r';
}

static int str16_is_surrogate(uint16_t c)
{
    return c >= 0xD800u && c <= 0xDFFFu;
}

static int str16_utf8_cont(unsigned char c)
{
    return (c & 0xC0u) == 0x80u;
}

static int str16_prefix_icase(const char16_t *str, const char *ascii)
{
    size_t i = 0;

    if (!str || !ascii)
        return 0;

    while (ascii[i])
    {
        char16_t a = str[i];
        char b = ascii[i];

        if (a >= u'A' && a <= u'Z')
            a = (char16_t)(a - u'A' + u'a');
        if (b >= 'A' && b <= 'Z')
            b = (char)(b - 'A' + 'a');

        if (a != (char16_t)(unsigned char)b)
            return 0;
        i++;
    }

    return 1;
}

size_t str16_length(const char16_t *string)
{
    size_t n = 0;

    if (!string)
        return 0;

    while (string[n])
        n++;

    return n;
}

char16_t *str16_copy(const char16_t *in, char16_t *out)
{
    size_t n = 0;

    if (!in || !out)
        return NULL;

    while ((out[n] = in[n]) != 0)
        n++;

    return out;
}

int str16_copy_n(const char16_t *in, char16_t *out, size_t capacity)
{
    size_t len;

    if (!in || !out || capacity == 0)
        return 0;

    len = str16_length(in);
    if (len >= capacity)
        return 0;

    for (size_t i = 0; i <= len; i++)
        out[i] = in[i];

    return 1;
}

char16_t *str16_concat(const char16_t *in, char16_t *out)
{
    size_t used;
    size_t i = 0;

    if (!in || !out)
        return NULL;

    used = str16_length(out);
    while ((out[used + i] = in[i]) != 0)
        i++;

    return out;
}

int str16_concat_n(const char16_t *in, char16_t *out, size_t capacity)
{
    size_t used;
    size_t add;

    if (!in || !out || capacity == 0)
        return 0;

    used = str16_length(out);
    add = str16_length(in);

    if (used >= capacity || add > capacity - used - 1)
        return 0;

    for (size_t i = 0; i <= add; i++)
        out[used + i] = in[i];

    return 1;
}

int str16_equal(const char16_t *str1, const char16_t *str2)
{
    size_t n = 0;

    if (!str1 || !str2)
        return str1 == str2;

    while (str1[n] == str2[n])
    {
        if (str1[n] == 0)
            return 1;
        n++;
    }

    return 0;
}

int str16_contains(const char16_t *string, const char16_t *sub)
{
    size_t str1;
    size_t str2;
    size_t *lps;
    size_t len = 0;
    size_t i = 1;
    size_t j = 0;

    if (!string || !sub)
        return 0;

    str1 = str16_length(string);
    str2 = str16_length(sub);

    if (str2 == 0)
        return 1;
    if (str2 > str1)
        return 0;

    lps = malloc(str2 * sizeof(*lps));
    if (!lps)
        return 0;

    lps[0] = 0;
    while (i < str2)
    {
        if (sub[i] == sub[len])
        {
            lps[i++] = ++len;
        }
        else if (len != 0)
        {
            len = lps[len - 1];
        }
        else
        {
            lps[i++] = 0;
        }
    }

    i = 0;
    while ((str1 - i) >= (str2 - j))
    {
        if (sub[j] == string[i])
        {
            i++;
            j++;
        }

        if (j == str2)
        {
            free(lps);
            return 1;
        }

        if (i < str1 && sub[j] != string[i])
        {
            if (j != 0)
                j = lps[j - 1];
            else
                i++;
        }
    }

    free(lps);
    return 0;
}

int str16_fixsize(const char16_t *in, char16_t *out, size_t fixsize)
{
    size_t n;

    if (!in || !out || fixsize == 0)
        return 0;

    n = str16_length(in);
    if (n > fixsize - 1)
        n = fixsize - 1;

    for (size_t i = 0; i < fixsize - 1; i++)
        out[i] = i < n ? in[i] : u' ';

    out[fixsize - 1] = 0;
    return 1;
}

int str16_pack(const char16_t *in, char16_t *out, size_t width)
{
    size_t n;

    if (!in || !out)
        return 0;

    n = str16_length(in);
    if (n > width)
        return 0;

    for (size_t i = 0; i < width; i++)
        out[i] = i < n ? in[i] : u' ';

    return 1;
}

void str16_trimspaces_right(char16_t *string)
{
    size_t n;

    if (!string)
        return;

    n = str16_length(string);
    while (n > 0 && string[n - 1] == u' ')
        n--;

    string[n] = 0;
}

void str16_trimspaces_left(char16_t *string)
{
    size_t i = 0;
    size_t j = 0;

    if (!string)
        return;

    while (string[i] == u' ')
        i++;

    while (string[i])
        string[j++] = string[i++];

    string[j] = 0;
}

char16_t *str16_reverse(char16_t *string)
{
    size_t n;

    if (!string)
        return NULL;

    n = str16_length(string);
    for (size_t i = 0; i < n / 2; i++)
    {
        char16_t tmp = string[i];
        string[i] = string[n - i - 1];
        string[n - i - 1] = tmp;
    }

    return string;
}

void str16_split(const char16_t *string, char16_t **out, char16_t delim)
{
    size_t i = 0;
    size_t j = 0;
    size_t k = 0;

    if (!string || !out)
        return;

    while (string[k])
    {
        if (string[k] == delim)
        {
            out[j++][i] = 0;
            k++;
            i = 0;
            continue;
        }
        out[j][i++] = string[k++];
    }
    out[j][i] = 0;
}

int str16_char_contains(char16_t c, const char16_t *string)
{
    if (!string)
        return 0;

    while (*string)
    {
        if (*string++ == c)
            return 1;
    }

    return 0;
}

char16_t *str16_tokens(char16_t *string, const char16_t *delim, char16_t **save)
{
    char16_t *start;
    char16_t *end;

    if (!delim || !save)
        return NULL;

    if (string)
        *save = string;

    if (!*save || !**save)
        return NULL;

    start = *save;
    while (*start && str16_char_contains(*start, delim))
        start++;

    if (!*start)
    {
        *save = start;
        return NULL;
    }

    end = start;
    while (*end && !str16_char_contains(*end, delim))
        end++;

    if (*end)
    {
        *end = 0;
        *save = end + 1;
    }
    else
    {
        *save = end;
    }

    return start;
}

int char16_write(FILE *stream, uint16_t c)
{
    unsigned char byte[2];

    if (!stream)
        return 0;

    byte[0] = (unsigned char)(c & 0xFFu);
    byte[1] = (unsigned char)((c >> 8) & 0xFFu);

    return fwrite(byte, 1, sizeof(byte), stream) == sizeof(byte);
}

int char16_read(FILE *file, uint16_t *c)
{
    unsigned char byte[2];

    if (!file || !c)
        return 0;

    if (fread(byte, 1, sizeof(byte), file) != sizeof(byte))
        return 0;

    *c = (uint16_t)((uint16_t)byte[0] | ((uint16_t)byte[1] << 8));
    return 1;
}

char *str16_to_utf8_alloc(const char16_t *txt)
{
    size_t len;
    char *out;
    size_t j = 0;

    if (!txt)
        return NULL;

    len = str16_length(txt);
    out = malloc(len * 3 + 1);
    if (!out)
        return NULL;

    for (size_t i = 0; i < len; i++)
    {
        uint16_t c = txt[i];

        if (str16_is_surrogate(c))
        {
            out[j++] = '?';
        }
        else if (c < 0x0080u)
        {
            out[j++] = (char)c;
        }
        else if (c < 0x0800u)
        {
            out[j++] = (char)(0xC0u | (c >> 6));
            out[j++] = (char)(0x80u | (c & 0x3Fu));
        }
        else
        {
            out[j++] = (char)(0xE0u | (c >> 12));
            out[j++] = (char)(0x80u | ((c >> 6) & 0x3Fu));
            out[j++] = (char)(0x80u | (c & 0x3Fu));
        }
    }

    out[j] = '\0';
    return out;
}

int str16_print(const char16_t *txt)
{
#ifdef _WIN32
    size_t n;
    HANDLE console;
    DWORD written;

    if (!txt)
        return 0;

    n = str16_length(txt);
    if (n == 0)
        return 1;

    console = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetFileType(console) != FILE_TYPE_CHAR)
        return 0;

    return WriteConsoleW(console, txt, (DWORD)n, &written, NULL) ? 1 : 0;
#else
    char *utf8;
    size_t n;
    int ok;

    utf8 = str16_to_utf8_alloc(txt);
    if (!utf8)
        return 0;

    n = strlen(utf8);
    ok = fwrite(utf8, 1, n, stdout) == n;
    free(utf8);
    return ok;
#endif
}

int str16_println(const char16_t *txt)
{
    int ok = str16_print(txt);
    return str16_print(u"\n") && ok;
}

int str16_scanline(char16_t *buffer, size_t max_len)
{
    if (!buffer || max_len == 0)
        return 0;

#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_INPUT_HANDLE);
    DWORD read;

    if (GetFileType(console) != FILE_TYPE_CHAR)
        return 0;

    if (!ReadConsoleW(console, buffer, (DWORD)max_len - 1, &read, NULL))
        return 0;

    while (read > 0 && (buffer[read - 1] == u'\n' || buffer[read - 1] == u'\r'))
        read--;

    buffer[read] = 0;
    return 1;
#else
    char utf8[2048];
    size_t len;
    size_t i = 0;
    size_t j = 0;

    if (!fgets(utf8, sizeof(utf8), stdin))
        return 0;

    len = strlen(utf8);
    while (i < len && utf8[i] != '\n' && utf8[i] != '\r' && j < max_len - 1)
    {
        unsigned char c = (unsigned char)utf8[i];

        if (c < 0x80u)
        {
            buffer[j++] = (char16_t)c;
            i++;
        }
        else if ((c & 0xE0u) == 0xC0u && i + 1 < len &&
                 str16_utf8_cont((unsigned char)utf8[i + 1]))
        {
            uint16_t cp = (uint16_t)(((c & 0x1Fu) << 6) |
                                    ((unsigned char)utf8[i + 1] & 0x3Fu));
            if (cp >= 0x80u)
                buffer[j++] = (char16_t)cp;
            i += 2;
        }
        else if ((c & 0xF0u) == 0xE0u && i + 2 < len &&
                 str16_utf8_cont((unsigned char)utf8[i + 1]) &&
                 str16_utf8_cont((unsigned char)utf8[i + 2]))
        {
            uint16_t cp = (uint16_t)(((c & 0x0Fu) << 12) |
                                    (((unsigned char)utf8[i + 1] & 0x3Fu) << 6) |
                                    ((unsigned char)utf8[i + 2] & 0x3Fu));
            if (cp >= 0x0800u && !str16_is_surrogate(cp))
                buffer[j++] = (char16_t)cp;
            i += 3;
        }
        else if ((c & 0xF8u) == 0xF0u && i + 3 < len &&
                 str16_utf8_cont((unsigned char)utf8[i + 1]) &&
                 str16_utf8_cont((unsigned char)utf8[i + 2]) &&
                 str16_utf8_cont((unsigned char)utf8[i + 3]))
        {
            /* Fora do BMP por decisão de projeto. */
            i += 4;
        }
        else
        {
            i++;
        }
    }

    buffer[j] = 0;
    return 1;
#endif
}

long int str16_forLong(const char16_t *string, char16_t **end, int base)
{
    const char16_t *str = string;
    const char16_t *digits_start;
    unsigned long acc = 0;
    unsigned long cutoff;
    unsigned long cutlim;
    unsigned long ubase;
    int sign = 1;
    int overflow = 0;

    if (!string)
    {
        if (end)
            *end = NULL;
        return 0;
    }

    while (str16_is_space(*str))
        str++;

    if (*str == u'-')
    {
        sign = -1;
        str++;
    }
    else if (*str == u'+')
    {
        str++;
    }

    if (base < 0 || base == 1 || base > 36)
    {
        if (end)
            *end = (char16_t *)string;
        return 0;
    }

    if ((base == 0 || base == 16) && *str == u'0' && (str[1] == u'x' || str[1] == u'X'))
    {
        base = 16;
        str += 2;
    }

    if (base == 0)
        base = *str == u'0' ? 8 : 10;

    ubase = (unsigned long)base;
    digits_start = str;
    cutoff = sign > 0 ? (unsigned long)LONG_MAX : (unsigned long)LONG_MAX + 1ul;
    cutlim = cutoff % ubase;
    cutoff /= ubase;

    while (*str)
    {
        int val;
        char16_t c = *str;

        if (c >= u'0' && c <= u'9')
            val = (int)(c - u'0');
        else if (c >= u'a' && c <= u'z')
            val = (int)(c - u'a') + 10;
        else if (c >= u'A' && c <= u'Z')
            val = (int)(c - u'A') + 10;
        else
            break;

        if ((unsigned int)val >= (unsigned int)base)
            break;

        if (overflow || acc > cutoff || (acc == cutoff && (unsigned long)val > cutlim))
            overflow = 1;
        else
            acc = acc * ubase + (unsigned long)val;

        str++;
    }

    if (end)
        *end = (char16_t *)(str == digits_start ? string : str);

    if (overflow)
        return sign > 0 ? LONG_MAX : LONG_MIN;

    if (sign > 0)
        return (long)acc;
    if (acc == (unsigned long)LONG_MAX + 1ul)
        return LONG_MIN;
    return -(long)acc;
}

double str16_forDouble(const char16_t *string, char16_t **end)
{
    const char16_t *str = string;
    double acc = 0.0;
    int sign = 1;
    int has_digits = 0;

    if (!string)
    {
        if (end)
            *end = NULL;
        return 0.0;
    }

    while (str16_is_space(*str))
        str++;

    if (*str == u'-')
    {
        sign = -1;
        str++;
    }
    else if (*str == u'+')
    {
        str++;
    }

    if (str16_prefix_icase(str, "infinity"))
    {
        str += 8;
        if (end)
            *end = (char16_t *)str;
        return sign > 0 ? HUGE_VAL : -HUGE_VAL;
    }

    if (str16_prefix_icase(str, "inf"))
    {
        str += 3;
        if (end)
            *end = (char16_t *)str;
        return sign > 0 ? HUGE_VAL : -HUGE_VAL;
    }

    while (*str >= u'0' && *str <= u'9')
    {
        acc = acc * 10.0 + (double)(*str - u'0');
        has_digits = 1;
        str++;
    }

    if (*str == u'.' || *str == u',')
    {
        double power = 1.0;
        str++;

        while (*str >= u'0' && *str <= u'9')
        {
            acc = acc * 10.0 + (double)(*str - u'0');
            power *= 10.0;
            has_digits = 1;
            str++;
        }

        acc /= power;
    }

    if (has_digits && (*str == u'e' || *str == u'E'))
    {
        const char16_t *save_e = str++;
        int exp_sign = 1;
        int exp_val = 0;
        int has_exp = 0;

        if (*str == u'-')
        {
            exp_sign = -1;
            str++;
        }
        else if (*str == u'+')
        {
            str++;
        }

        while (*str >= u'0' && *str <= u'9')
        {
            if (exp_val < 10000)
                exp_val = exp_val * 10 + (int)(*str - u'0');
            has_exp = 1;
            str++;
        }

        if (has_exp)
        {
            double factor = 1.0;
            double base_mult = 10.0;
            int exp = exp_val;

            while (exp > 0)
            {
                if (exp % 2 != 0)
                    factor *= base_mult;
                base_mult *= base_mult;
                exp /= 2;
            }

            if (exp_sign < 0)
                acc /= factor;
            else
                acc *= factor;
        }
        else
        {
            str = save_e;
        }
    }

    if (end)
        *end = (char16_t *)(has_digits ? str : string);

    return acc * (double)sign;
}

char16_t *str16_fromLong_n(long value, char16_t *buffer, size_t max_len, int base)
{
    char16_t tmp[sizeof(unsigned long) * CHAR_BIT + 2];
    unsigned long n;
    size_t used = 0;
    int negative = value < 0 && base == 10;

    if (!buffer || max_len == 0 || base < 2 || base > 36)
        return NULL;

    if (negative)
        n = (unsigned long)(-(value + 1)) + 1ul;
    else
        n = (unsigned long)value;

    do
    {
        unsigned long rem = n % (unsigned long)base;
        tmp[used++] = rem > 9ul ? (char16_t)(u'a' + rem - 10ul) : (char16_t)(u'0' + rem);
        n /= (unsigned long)base;
    } while (n != 0);

    if (negative)
        tmp[used++] = u'-';

    if (used + 1 > max_len)
    {
        buffer[0] = 0;
        return NULL;
    }

    for (size_t i = 0; i < used; i++)
        buffer[i] = tmp[used - i - 1];

    buffer[used] = 0;
    return buffer;
}

char16_t *str16_fromLong(long value, char16_t *buffer, int base)
{
    return str16_fromLong_n(value, buffer, sizeof(unsigned long) * CHAR_BIT + 2, base);
}

char16_t *str16_fromDouble(double value, int precision, char16_t *buffer, size_t max_len)
{
    char *tmp;
    int len;

    if (!buffer || max_len == 0 || precision < 0)
        return NULL;

    tmp = malloc(max_len);
    if (!tmp)
        return NULL;

    len = snprintf(tmp, max_len, "%.*f", precision, value);
    if (len < 0 || (size_t)len >= max_len)
    {
        buffer[0] = 0;
        free(tmp);
        return NULL;
    }

    for (size_t i = 0; i < (size_t)len; i++)
        buffer[i] = (char16_t)(unsigned char)tmp[i];

    buffer[len] = 0;
    free(tmp);
    return buffer;
}
