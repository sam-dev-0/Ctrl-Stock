#include <limits.h>

#include "str16.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

size_t str16_length(const char16_t *string)
{
    size_t n = 0;
    while (string[n])
        n++;

    return n;
}

char16_t* str16_copy(const char16_t *in, char16_t *out)
{
    size_t n = 0;
    while ((out[n] = in[n]))
        n++;

    return out;
}

char16_t* str16_concat(const char16_t *strIN, char16_t *strOUT)
{
    size_t out = str16_length(strOUT);
    size_t i = 0;

    while ((strOUT[out + i] = strIN[i]))
        i++;

    return strOUT;
}

int str16_equal(const char16_t *str1, const char16_t *str2)
{
    size_t n = 0;
    while (str1[n] == str2[n]){
        if (str1[n] == 0)
            return 1;
        n++;
    }

    return 0;
}

int str16_contains(const char16_t *string, const char16_t *sub)
{
    size_t str1 = str16_length(string);
    size_t str2 = str16_length(sub);

    if (str2 == 0)
        return 1;
    if (str2 > str1)
        return 0;

    size_t *LPS = malloc(str2 * sizeof(size_t));
    if (LPS == NULL)
        return 0;

    LPS[0] = 0;
    size_t len = 0;
    size_t i = 1;

    while (i < str2)
    {
        if (sub[i] == sub[len])
        {
            len++;
            LPS[i] = len;
            i++;
        }
        else
        {
            if (len != 0)
            {
                len = LPS[len - 1];
            }
            else
            {
                LPS[i] = 0;
                i++;
            }
        }
    }

    i = 0;
    size_t j = 0;

    while ((str1 - i) >= (str2 - j))
    {
        if (sub[j] == string[i])
        {
            j++;
            i++;
        }

        if (j == str2)
        {
            free(LPS);
            return 1;
        }
        else if (i < str1 && sub[j] != string[i])
        {
            if (j != 0)
                j = LPS[j - 1];
            else
                i++;
        }
    }

    free(LPS);
    return 0;
}

int str16_fixsize(const char16_t *in, char16_t *out, size_t fixsize)
{
    size_t n = str16_length(in);
    if (fixsize < 1)
        return 0;
    if (n > (fixsize - 1))
        n = (fixsize - 1);

    for (size_t i = 0; i < fixsize; i++)
        out[i] = (i < n) ? in[i] : u' ';

    out[fixsize - 1] = 0;

    return 1;
}

void str16_trimspaces_right(char16_t *string)
{
    size_t n = str16_length(string);
    while (n > 0 && string[n - 1] == u' ')
        n--;

    string[n] = 0;
}

void str16_trimspaces_left(char16_t *string)
{
    size_t n = str16_length(string);

    size_t i = 0;
    while (string[i] == u' ')
        i++;

    size_t j = 0;
    while (string[i])
    {
        string[j] = string[i];
        i++;
        j++;
    }
    
    string[j] = 0;
}

char16_t *str16_reverse(char16_t *string)
{
    size_t n = str16_length(string);
    if (n < 2)
        return NULL;

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

    while (string[k])
    {
        if (string[k] == delim)
        {
            out[j][i] = 0;
            j++;
            k++;
            i = 0;
            continue;
        }
        out[j][i] = string[k];
        i++, k++;
    }
    out[j][i] = 0;
}

int str16_char_contains(char16_t c, const char16_t *string)
{
    while (*string)
    {
        if (*string == c)
            return 1;
        string++;
    }

    return 0;
}

char16_t *str16_tokens(char16_t *string, const char16_t *delim, char16_t **save)
{
    char16_t *start;
    char16_t *end;

    if (string != NULL)
        *save = string;

    if (*save == 0 || **save == 0)
        return NULL;

    start = *save;
    while (*start && str16_char_contains(*start, delim))
        start++;

    if (*start == 0)
    {
        *save = start;
        return NULL;
    }

    end = start;
    while (*end && !str16_char_contains(*end, delim))
        end++;
    
    if(*end){
        *end = 0;
        *save = end + 1;
    }
    else{
        *save = end;
    }

    return start;
}

int char16_write(FILE *stream, uint16_t c)
{
    unsigned char byte[sizeof(char16_t)] = {
        (unsigned char)(c & 0xFFu),
        (unsigned char)((c >> 8) & 0xFFu)};

    return fwrite(byte, 1, sizeof(char16_t), stream) == sizeof(char16_t) ? 1 : 0;
}

int char16_read(FILE *file, uint16_t *c)
{
    unsigned char byte[sizeof(char16_t)];
    if (fread(byte, 1, sizeof(char16_t), file) != sizeof(char16_t))
        return 0;

    *c = (uint16_t)byte[0] | ((uint16_t)byte[1] << 8);

    return 1;
}

// int str16_print(const char16_t *txt)
// {
//     size_t n = str16_length(txt);
//     if (n == 0)
//         return 0;

//     unsigned char *buffer = malloc(n * sizeof(char16_t));
//     if (buffer == NULL)
//         return 0;

//     for (size_t i = 0; i < n; i++)
//     {
//         buffer[i * 2] = (unsigned char)(txt[i] & 0xFFu);
//         buffer[i * 2 + 1] = (unsigned char)((txt[i] >> 8) & 0xFFu);
//     }

//     size_t written = fwrite(buffer, 1, n * sizeof(char16_t), stdout);
//     free(buffer);

//     if (written != n * sizeof(char16_t))
//         return 0;

//     return 1;
// }

int str16_print(const char16_t *txt)
{
    size_t n = str16_length(txt);
    if (n == 0)
        return 0;

#ifdef _WIN32
    /* Modo Windows: API Direta do Console */
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetFileType(hConsole) == FILE_TYPE_CHAR)
    {
        DWORD written;
        return WriteConsoleW(hConsole, txt, (DWORD)n, &written, NULL) ? 1 : 0;
    }

    return 0;
#else
    /* Modo POSIX: Conversão Bitwise UTF-16 para UTF-8 On-The-Fly */
    size_t utf8_max_len = (n * 3) + 1;
    unsigned char *utf8_buf = malloc(utf8_max_len);
    if (!utf8_buf)
        return 0;

    size_t j = 0;
    for (size_t i = 0; i < n; i++)
    {
        uint16_t c = txt[i];
        if (c < 0x0080)
        {
            utf8_buf[j++] = (unsigned char)c;
        }
        else if (c < 0x0800)
        {
            utf8_buf[j++] = (unsigned char)(0xC0 | (c >> 6));
            utf8_buf[j++] = (unsigned char)(0x80 | (c & 0x3F));
        }
        else
        {
            utf8_buf[j++] = (unsigned char)(0xE0 | (c >> 12));
            utf8_buf[j++] = (unsigned char)(0x80 | ((c >> 6) & 0x3F));
            utf8_buf[j++] = (unsigned char)(0x80 | (c & 0x3F));
        }
    }

    int res = (fwrite(utf8_buf, 1, j, stdout) == j);
    free(utf8_buf);
    return res;
#endif
}

int str16_println(const char16_t *txt)
{
    int res = str16_print(txt);
    str16_print(u"\n");
    return res;
}

// int str16_scan(char16_t *out, size_t max_len)
// {
//     if (max_len == 0 || out == NULL)
//         return 0;

//     size_t i = 0;
//     uint16_t c;

//     while (i < max_len - 1)
//     {
//         if (!char16_read(stdin, &c))
//             break;

//         if (c == u'\r')
//             continue;

//         if (c == u'\n')
//             break;

//         out[i] = (char16_t)c;
//         i++;
//     }
//     out[i] = 0;

//     return (i > 0 || !feof(stdin)) ? 1 : 0;
// }

int str16_scanline(char16_t *buffer, size_t max_len)
{
    if (!buffer || max_len == 0)
        return 0;

#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
    if (GetFileType(hConsole) == FILE_TYPE_CHAR)
    {
        DWORD read;
        // Lê diretamente do console limitando o tamanho
        if (ReadConsoleW(hConsole, buffer, (DWORD)max_len - 1, &read, NULL))
        {
            // Limpeza de CR LF (\r\n) do Windows
            while (read > 0 && (buffer[read - 1] == u'\n' || buffer[read - 1] == u'\r'))
            {
                read--;
            }
            buffer[read] = u'\0';
            return 1;
        }
    }
    return 0;
#else
    /* Modo POSIX: Lê UTF-8 do stdin e decodifica para char16_t */
    char utf8_buf[2048]; // Buffer temporário para a linha em UTF-8
    if (!fgets(utf8_buf, sizeof(utf8_buf), stdin))
        return 0;

    size_t i = 0, j = 0;
    while (utf8_buf[i] && utf8_buf[i] != '\n' && utf8_buf[i] != '\r' && j < max_len - 1)
    {
        unsigned char c = (unsigned char)utf8_buf[i];

        if (c < 0x80)
        { // 1 Byte (0xxxxxxx)
            buffer[j++] = c;
            i++;
        }
        else if ((c & 0xE0) == 0xC0)
        { // 2 Bytes (110xxxxx 10xxxxxx)
            buffer[j++] = (char16_t)(((c & 0x1F) << 6) | (utf8_buf[i + 1] & 0x3F));
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0)
        { // 3 Bytes (1110xxxx 10xxxxxx 10xxxxxx)
            buffer[j++] = (char16_t)(((c & 0x0F) << 12) |
                                     ((utf8_buf[i + 1] & 0x3F) << 6) |
                                     (utf8_buf[i + 2] & 0x3F));
            i += 3;
        }
        else
        {
            i++; // Ignora caracteres inválidos ou fora do plano multilíngue básico (BMP)
        }
    }
    buffer[j] = u'\0';
    return 1;
#endif
}

/* Converte a sua string UTF-16 para o padrão C char UTF-8.
   O chamador deve dar free() no ponteiro retornado. */
char *str16_to_utf8_alloc(const char16_t *txt)
{
    if (!txt)
        return NULL;
    size_t len = str16_length(txt);

    // Pior cenário: Cada caractere 16-bits vira 3 bytes UTF-8 + terminador nulo
    char *utf8_out = malloc((len * 3) + 1);
    if (!utf8_out)
        return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++)
    {
        uint16_t c = txt[i];
        if (c < 0x0080)
        {
            utf8_out[j++] = (char)c;
        }
        else if (c < 0x0800)
        {
            utf8_out[j++] = (char)(0xC0 | (c >> 6));
            utf8_out[j++] = (char)(0x80 | (c & 0x3F));
        }
        else
        {
            utf8_out[j++] = (char)(0xE0 | (c >> 12));
            utf8_out[j++] = (char)(0x80 | ((c >> 6) & 0x3F));
            utf8_out[j++] = (char)(0x80 | (c & 0x3F));
        }
    }
    utf8_out[j] = '\0';

    return utf8_out;
}

long int str16_forLong(const char16_t *string, char16_t **end, int base)
{
    const char16_t *str = string;
    unsigned long acc = 0;
    int sign = 1;
    int overflow = 0;

    while (*str == u' ' || *str == u'\t' || *str == u'\n' || *str == u'\r')
        str++;

    const char16_t *save = str;

    if (*str == u'-')
    {
        sign = -1;
        str++;
    }
    else if (*str == u'+')
        str++;

    if (base < 0 || base == 1 || base > 36)
    {
        if (end != NULL)
            *end = (char16_t *)string;

        return 0;
    }

    if (base == 0 || base == 16)
    {
        if (*str == u'0' && (*(str + 1) == u'x' || *(str + 1) == u'X'))
        {
            base = 16;
            str += 2;
        }
    }
    if (base == 0)
    {
        if (*str == u'0')
            base = 8;

        else
            base = 10;
    }

    const char16_t *digits_start = str;

    unsigned long cutoff = (sign > 0) ? LONG_MAX : (unsigned long)LONG_MAX + 1;
    unsigned long cutlim = cutoff % base;
    cutoff /= base;

    while (*str)
    {
        int c = *str;
        int val;

        if (c >= u'0' && c <= u'9')
            val = c - u'0';
        else if (c >= u'a' && c <= u'z')
            val = c - u'a' + 10;
        else if (c >= u'A' && c <= u'Z')
            val = c - u'A' + 10;
        else
            break;

        if (val >= base)
            break;

        if (overflow || acc > cutoff || (acc == cutoff && (unsigned long)val > cutlim))
            overflow = 1;

        else
            acc = (acc * base) + val;

        str++;
    }

    if (end != NULL)
        *end = (char16_t *)(str == digits_start ? string : str);

    if (overflow)
        return (sign > 0) ? LONG_MAX : LONG_MIN;

    return (long)(sign > 0 ? acc : -acc);
}

double str16_forDouble(const char16_t *string, char16_t **end)
{
    const char16_t *str = string;
    double acc = 0.0;
    int sign = 1;

    while (*str == u' ' || *str == u'\t' || *str == u'\n' || *str == u'\r')
        str++;

    if (*str == u'-')
    {
        sign = -1;
        str++;
    }
    else if (*str == u'+')
        str++;

    if ((*str == u'I' || *str == u'i') && (*(str + 1) == u'N' || *(str + 1) == u'n') && (*(str + 2) == u'F' || *(str + 2) == u'f'))
    {
        str += 3;

        if ((*str == u'I' || *str == u'i') && (*(str + 1) == u'N' || *(str + 1) == u'n') && (*(str + 4) == u'Y' || *(str + 4) == u'y'))
            while ((*str >= u'a' && *str <= u'z') || (*str >= u'A' && *str <= u'Z'))
                str++;

        if (end != NULL)
            *end = (char16_t *)str;

        return sign > 0 ? (1e300 * 1e300) : -(1e300 * 1e300);
    }

    int has_digits = 0;

    while (*str >= u'0' && *str <= u'9')
    {
        acc = (acc * 10.0) + (*str - u'0');
        has_digits = 1;
        str++;
    }

    if (*str == u'.' || *str == u',')
    {
        str++;
        double power = 1.0;

        while (*str >= u'0' && *str <= u'9')
        {
            acc = (acc * 10.0) + (*str - u'0');
            power *= 10.0;
            has_digits = 1;
            str++;
        }

        acc /= power;
    }

    if (has_digits && (*str == u'e' || *str == u'E'))
    {
        const char16_t *save_e = str;
        str++;

        int exp_sign = 1;
        if (*str == u'-')
        {
            exp_sign = -1;
            str++;
        }
        else if (*str == u'+')
            str++;

        int exp_val = 0;
        int has_exp_digits = 0;

        while (*str >= u'0' && *str <= u'9')
        {
            if (exp_val < 10000)
                exp_val = (exp_val * 10) + (*str - u'0');

            has_exp_digits = 1;
            str++;
        }

        if (has_exp_digits)
        {
            double exp_mult = 1.0;
            double base_mult = 10.0;
            int e = exp_val;

            while (e > 0)
            {
                if (e % 2 == 1)
                    exp_mult *= base_mult;

                base_mult *= base_mult;
                e /= 2;
            }

            if (exp_sign < 0)
                acc /= exp_mult;

            else
                acc *= exp_mult;
        }
        else
            str = save_e;
    }

    if (end != NULL)
        *end = (char16_t *)(has_digits ? str : string);

    return acc * sign;
}

char16_t *str16_fromLong(long value, char16_t *buffer, int base)
{
    if (base < 2 || base > 36)
    {
        buffer[0] = 0;
        return NULL;
    }

    size_t i = 0;
    int isNegative = 0;
    unsigned long n;

    if (value < 0 && base == 10)
    {
        isNegative = 1;
        n = (unsigned long)(-(value + 1)) + 1;
    }
    else
        n = (unsigned long)value;

    do
    {
        int rem = n % base;
        buffer[i++] = (rem > 9) ? (char16_t)((rem - 10) + u'a') : (char16_t)(rem + u'0');
        n = n / base;
    } while (n != 0);

    if (isNegative)
        buffer[i++] = u'-';

    buffer[i] = 0;
    str16_reverse(buffer);

    return buffer;
}

char16_t *str16_fromDouble(double value, int precision, char16_t *buffer, size_t max_len)
{
    if (!buffer || max_len == 0)
        return NULL;

    char temp_ascii[64];

    int len = snprintf(temp_ascii, sizeof(temp_ascii), "%.*f", precision, value);

    if (len < 0 ||
        (size_t)len >= sizeof(temp_ascii) ||
        (size_t)len >= max_len)
        return NULL;

    if ((size_t)len >= max_len)
        len = max_len - 1;

    for (int i = 0; i < len; i++)
        buffer[i] = (char16_t)temp_ascii[i];

    buffer[len] = 0;

    return buffer;
}
