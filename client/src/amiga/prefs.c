#include "prefs.h"

static char upper(char c)
{
    return c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c;
}

static unsigned char allowed(char c)
{
    return (unsigned char)c > ' ' && (unsigned char)c <= 'Z';
}

unsigned char prefs_parse(const char *file, unsigned char len, char *handle)
{
    static const char key[] = "HANDLE=";
    unsigned char i = 0, k, n = 0;

    handle[0] = 0;
    while (i < len) {
        for (k = 0; key[k] && i + k < len && upper(file[i + k]) == key[k]; ++k)
            ;
        if (!key[k]) {
            for (i += k; i < len && file[i] != '\n' && file[i] != '\r' && n < HANDLE_MAX; ++i)
                if (allowed(upper(file[i])))
                    handle[n++] = upper(file[i]);
            handle[n] = 0;
            return n;
        }
        while (i < len && file[i] != '\n')      /* the next line */
            ++i;
        ++i;
    }
    return 0;
}

unsigned char prefs_server(const char *file, unsigned char len, char *url)
{
    static const char key[] = "SERVER=", tcp[] = "TCP://";
    unsigned char i = 0, k, n = 0, end;

    url[0] = 0;
    while (i < len) {
        for (k = 0; key[k] && i + k < len && upper(file[i + k]) == key[k]; ++k)
            ;
        if (!key[k]) {
            i += k;
            if (i + 1 < len && upper(file[i]) == 'N' && file[i + 1] == ':')
                i += 2;
            for (end = i; end < len && file[end] != '\n' && file[end] != '\r'; ++end)
                ;
            for (k = 0; tcp[k] && i + k < end && upper(file[i + k]) == tcp[k]; ++k)
                ;
            if (tcp[k] || end - i > SERVER_MAX)
                return 0;
            for (; i < end; ++i)
                url[n++] = file[i];
            url[n] = 0;
            return n;
        }
        while (i < len && file[i] != '\n')
            ++i;
        ++i;
    }
    return 0;
}

void prefs_defaults(struct prefs_set *s)
{
    s->view = 0;
    s->hints = 0;
    s->sound = 1;
}

/* the value after `key` on its line, or 0 if there is no such line: 'T'/'C' for VIEW, 'N'/'F' (the second letter of ON, OFF's
 * first would clash) is not needed: ON and OFF differ at the second letter */
static unsigned char value_of(const char *file, unsigned char len, const char *key, char *v1, char *v2)
{
    unsigned char i = 0, k;
    while (i < len) {
        for (k = 0; key[k] && i + k < len && upper(file[i + k]) == key[k]; ++k)
            ;
        if (!key[k]) {
            i += k;
            *v1 = i < len ? upper(file[i]) : 0;
            *v2 = i + 1 < len ? upper(file[i + 1]) : 0;
            return 1;
        }
        while (i < len && file[i] != '\n')
            ++i;
        ++i;
    }
    return 0;
}

void prefs_settings(const char *file, unsigned char len, struct prefs_set *s)
{
    char a, b;
    if (value_of(file, len, "VIEW=", &a, &b)) {
        if (a == 'T')
            s->view = 1;
        else if (a == 'C')
            s->view = 0;
    }
    if (value_of(file, len, "HINTS=", &a, &b)) {
        if (a == 'O' && b == 'N')
            s->hints = 1;
        else if (a == 'O' && b == 'F')
            s->hints = 0;
    }
    if (value_of(file, len, "SOUND=", &a, &b)) {
        if (a == 'O' && b == 'N')
            s->sound = 1;
        else if (a == 'O' && b == 'F')
            s->sound = 0;
    }
}

static unsigned char put(char *out, unsigned char n, const char *s)
{
    while (*s)
        out[n++] = *s++;
    return n;
}

unsigned char prefs_format(const char *handle, const char *server, const struct prefs_set *set, char *out)
{
    static const char key[] = "HANDLE=", skey[] = "SERVER=";
    unsigned char n = 0, i;

    for (i = 0; key[i]; ++i)
        out[n++] = key[i];
    for (i = 0; handle[i] && n < sizeof key - 1 + HANDLE_MAX; ++i)
        if (allowed(upper(handle[i])))
            out[n++] = upper(handle[i]);
    out[n++] = '\n';
    if (server[0]) {
        for (i = 0; skey[i]; ++i)
            out[n++] = skey[i];
        for (i = 0; server[i] && i < SERVER_MAX; ++i)
            out[n++] = server[i];
        out[n++] = '\n';
    }
    n = put(out, n, set->view ? "VIEW=TABLE\n" : "VIEW=CARDS\n");
    n = put(out, n, set->hints ? "HINTS=ON\n" : "HINTS=OFF\n");
    n = put(out, n, set->sound ? "SOUND=ON\n" : "SOUND=OFF\n");
    return n;
}
