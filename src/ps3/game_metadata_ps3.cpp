#ifdef __PS3__

#include "game_metadata_ps3.h"
#include "cover_manager_ps3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define META_KEY_MAX  128
#define META_DB_MAX   10000

struct MetaEntry {
    char key[META_KEY_MAX];
    char year[GAME_META_YEAR_MAX];
    char developer[GAME_META_STR_MAX];
    char publisher[GAME_META_STR_MAX];
    char genre[GAME_META_STR_MAX];
    char players[GAME_META_STR_MAX];
    char languages[GAME_META_LANG_MAX];
};

static MetaEntry *s_entries    = NULL;
static int        s_count      = 0;
static char      *s_json_buf   = NULL;

struct JsonCtx {
    const char *buf;
    size_t      len;
    size_t      pos;
};

static void json_skip_ws(JsonCtx *ctx)
{
    while (ctx->pos < ctx->len) {
        char c = ctx->buf[ctx->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            ctx->pos++;
        else
            break;
    }
}

static bool json_read_string(JsonCtx *ctx, char *out, size_t out_sz)
{
    json_skip_ws(ctx);
    if (ctx->pos >= ctx->len || ctx->buf[ctx->pos] != '"')
        return false;
    ctx->pos++;

    size_t out_pos = 0;
    while (ctx->pos < ctx->len && ctx->buf[ctx->pos] != '"') {
        char c = ctx->buf[ctx->pos++];
        if (c == '\\' && ctx->pos < ctx->len) {
            char esc = ctx->buf[ctx->pos++];
            switch (esc) {
                case '"':  c = '"';  break;
                case '\\': c = '\\'; break;
                case '/':  c = '/';  break;
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                default:   c = esc;  break;
            }
        }
        if (out_pos < out_sz - 1)
            out[out_pos++] = c;
    }
    if (ctx->pos < ctx->len)
        ctx->pos++;
    out[out_pos] = '\0';
    return true;
}

static void json_skip_value(JsonCtx *ctx)
{
    json_skip_ws(ctx);
    if (ctx->pos >= ctx->len)
        return;

    char c = ctx->buf[ctx->pos];

    if (c == '"') {
        char tmp[512];
        json_read_string(ctx, tmp, sizeof(tmp));
        return;
    }

    if (c == '{') {
        ctx->pos++;
        int depth = 1;
        while (ctx->pos < ctx->len && depth > 0) {
            char cc = ctx->buf[ctx->pos++];
            if (cc == '"') {
                while (ctx->pos < ctx->len && ctx->buf[ctx->pos] != '"') {
                    if (ctx->buf[ctx->pos] == '\\') ctx->pos++;
                    ctx->pos++;
                }
                if (ctx->pos < ctx->len) ctx->pos++;
            } else if (cc == '{') {
                depth++;
            } else if (cc == '}') {
                depth--;
            }
        }
        return;
    }

    if (c == '[') {
        ctx->pos++;
        int depth = 1;
        while (ctx->pos < ctx->len && depth > 0) {
            char cc = ctx->buf[ctx->pos++];
            if (cc == '"') {
                while (ctx->pos < ctx->len && ctx->buf[ctx->pos] != '"') {
                    if (ctx->buf[ctx->pos] == '\\') ctx->pos++;
                    ctx->pos++;
                }
                if (ctx->pos < ctx->len) ctx->pos++;
            } else if (cc == '[') {
                depth++;
            } else if (cc == ']') {
                depth--;
            }
        }
        return;
    }

    while (ctx->pos < ctx->len) {
        char cc = ctx->buf[ctx->pos];
        if (cc == ',' || cc == '}' || cc == ']' ||
            cc == ' ' || cc == '\t' || cc == '\n' || cc == '\r')
            break;
        ctx->pos++;
    }
}

static void json_append_language(MetaEntry *entry, const char *lang)
{
    if (!lang || lang[0] == '\0') return;
    size_t cur = strlen(entry->languages);
    if (cur > 0 && cur < sizeof(entry->languages) - 3) {
        entry->languages[cur++] = ',';
        entry->languages[cur++] = ' ';
        entry->languages[cur]   = '\0';
    }
    size_t rem = sizeof(entry->languages) - strlen(entry->languages) - 1;
    strncat(entry->languages, lang, rem);
}

static void json_parse_game_object(JsonCtx *ctx, MetaEntry *entry)
{
    json_skip_ws(ctx);
    if (ctx->pos >= ctx->len || ctx->buf[ctx->pos] != '{')
        return;
    ctx->pos++;

    while (ctx->pos < ctx->len) {
        json_skip_ws(ctx);
        if (ctx->pos >= ctx->len) break;
        char ch = ctx->buf[ctx->pos];
        if (ch == '}') { ctx->pos++; break; }
        if (ch == ',') { ctx->pos++; continue; }

        char field[64];
        if (!json_read_string(ctx, field, sizeof(field)))
            break;

        json_skip_ws(ctx);
        if (ctx->pos < ctx->len && ctx->buf[ctx->pos] == ':')
            ctx->pos++;
        json_skip_ws(ctx);

        if (ctx->pos >= ctx->len) break;

        if (strcmp(field, "year") == 0) {
            if (ctx->buf[ctx->pos] == '"') {
                json_read_string(ctx, entry->year, sizeof(entry->year));
            } else {
                size_t start = ctx->pos;
                while (ctx->pos < ctx->len && isdigit((unsigned char)ctx->buf[ctx->pos]))
                    ctx->pos++;
                size_t nlen = ctx->pos - start;
                if (nlen > 0 && nlen < sizeof(entry->year)) {
                    strncpy(entry->year, ctx->buf + start, nlen);
                    entry->year[nlen] = '\0';
                }
            }
        } else if (strcmp(field, "developer") == 0) {
            json_read_string(ctx, entry->developer, sizeof(entry->developer));
        } else if (strcmp(field, "publisher") == 0) {
            json_read_string(ctx, entry->publisher, sizeof(entry->publisher));
        } else if (strcmp(field, "genre") == 0) {
            json_read_string(ctx, entry->genre, sizeof(entry->genre));
        } else if (strcmp(field, "players") == 0) {
            if (ctx->buf[ctx->pos] == '"') {
                json_read_string(ctx, entry->players, sizeof(entry->players));
            } else {
                size_t start = ctx->pos;
                while (ctx->pos < ctx->len &&
                       (isdigit((unsigned char)ctx->buf[ctx->pos]) ||
                        ctx->buf[ctx->pos] == '-'))
                    ctx->pos++;
                size_t nlen = ctx->pos - start;
                if (nlen > 0 && nlen < sizeof(entry->players)) {
                    strncpy(entry->players, ctx->buf + start, nlen);
                    entry->players[nlen] = '\0';
                }
            }
        } else if (strcmp(field, "languages") == 0) {
            if (ctx->buf[ctx->pos] == '[') {
                ctx->pos++;
                entry->languages[0] = '\0';
                while (ctx->pos < ctx->len && ctx->buf[ctx->pos] != ']') {
                    json_skip_ws(ctx);
                    if (ctx->pos >= ctx->len || ctx->buf[ctx->pos] == ']') break;
                    if (ctx->buf[ctx->pos] == ',') { ctx->pos++; continue; }
                    char lang[64];
                    if (json_read_string(ctx, lang, sizeof(lang)))
                        json_append_language(entry, lang);
                }
                if (ctx->pos < ctx->len) ctx->pos++;
            } else {
                json_read_string(ctx, entry->languages, sizeof(entry->languages));
            }
        } else {
            json_skip_value(ctx);
        }
    }
}

static int json_parse_top(JsonCtx *ctx, MetaEntry *entries, int max_entries)
{
    json_skip_ws(ctx);
    if (ctx->pos >= ctx->len || ctx->buf[ctx->pos] != '{')
        return 0;
    ctx->pos++;

    int count = 0;
    while (ctx->pos < ctx->len && count < max_entries) {
        json_skip_ws(ctx);
        if (ctx->pos >= ctx->len) break;
        char ch = ctx->buf[ctx->pos];
        if (ch == '}') break;
        if (ch == ',') { ctx->pos++; continue; }

        char game_key[META_KEY_MAX];
        if (!json_read_string(ctx, game_key, sizeof(game_key)))
            break;

        json_skip_ws(ctx);
        if (ctx->pos >= ctx->len || ctx->buf[ctx->pos] != ':')
            break;
        ctx->pos++;

        json_skip_ws(ctx);
        if (ctx->pos >= ctx->len) break;

        if (ctx->buf[ctx->pos] != '{') {
            json_skip_value(ctx);
            continue;
        }

        MetaEntry *e = &entries[count];
        memset(e, 0, sizeof(*e));
        strncpy(e->key, game_key, sizeof(e->key) - 1);
        json_parse_game_object(ctx, e);
        count++;
    }
    return count;
}

void meta_db_fill_unknown(struct GameMetadata *out)
{
    if (!out) return;
    strncpy(out->year,      "N/A", sizeof(out->year)      - 1); out->year[sizeof(out->year)-1] = '\0';
    strncpy(out->developer, "N/A", sizeof(out->developer) - 1); out->developer[sizeof(out->developer)-1] = '\0';
    strncpy(out->publisher, "N/A", sizeof(out->publisher) - 1); out->publisher[sizeof(out->publisher)-1] = '\0';
    strncpy(out->genre,     "N/A", sizeof(out->genre)     - 1); out->genre[sizeof(out->genre)-1] = '\0';
    strncpy(out->players,   "N/A", sizeof(out->players)   - 1); out->players[sizeof(out->players)-1] = '\0';
    strncpy(out->languages, "N/A", sizeof(out->languages) - 1); out->languages[sizeof(out->languages)-1] = '\0';
}

static const char *ci_strstr(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return NULL;
    size_t nlen = strlen(needle);
    if (nlen == 0) return haystack;
    for (; *haystack; haystack++) {
        if (strncasecmp(haystack, needle, nlen) == 0)
            return haystack;
    }
    return NULL;
}

bool meta_db_load(const char *json_path)
{
    meta_db_unload();

    static const char * const CANDIDATES[] = {
        "./data/gameinfo/games.json",
        "data/gameinfo/games.json",
        "/dev_hdd0/uae4all/data/gameinfo/games.json",
        "/dev_hdd0/uae4all/data/gameinfo/games.json",
        "/dev_hdd0/uae4all/data/gameinfo/games.json",
        NULL
    };

    FILE *f = NULL;
    if (json_path && json_path[0] != '\0') {
        f = fopen(json_path, "rb");
    }
    if (!f) {
        for (int i = 0; CANDIDATES[i]; i++) {
            f = fopen(CANDIDATES[i], "rb");
            if (f) break;
        }
    }
    if (!f)
        return false;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 2 || file_size > 8L * 1024L * 1024L) {
        fclose(f);
        return false;
    }

    s_json_buf = (char *)malloc((size_t)file_size + 1);
    if (!s_json_buf) {
        fclose(f);
        return false;
    }

    size_t nread = fread(s_json_buf, 1, (size_t)file_size, f);
    fclose(f);
    s_json_buf[nread] = '\0';

    s_entries = (MetaEntry *)malloc(sizeof(MetaEntry) * META_DB_MAX);
    if (!s_entries) {
        free(s_json_buf);
        s_json_buf = NULL;
        return false;
    }

    JsonCtx ctx = { s_json_buf, nread, 0 };
    s_count = json_parse_top(&ctx, s_entries, META_DB_MAX);

    free(s_json_buf);
    s_json_buf = NULL;

    return s_count > 0;
}

static char s_last_lookup_key[256] = "";
static struct GameMetadata s_last_lookup_meta;
static bool s_last_lookup_result = false;

bool meta_db_lookup(const char *normalized_key, struct GameMetadata *out)
{
    meta_db_fill_unknown(out);

    if (!normalized_key || normalized_key[0] == '\0' || !s_entries || s_count == 0)
        return false;

    if (s_last_lookup_key[0] && strcmp(s_last_lookup_key, normalized_key) == 0) {
        if (s_last_lookup_result && out)
            *out = s_last_lookup_meta;
        return s_last_lookup_result;
    }

    for (int i = 0; i < s_count; i++) {
        if (strcasecmp(s_entries[i].key, normalized_key) == 0) {
            const MetaEntry *e = &s_entries[i];
            if (e->year[0])      { strncpy(out->year,      e->year,      sizeof(out->year)      - 1); out->year[sizeof(out->year)-1]='\0'; }
            if (e->developer[0]) { strncpy(out->developer, e->developer, sizeof(out->developer) - 1); out->developer[sizeof(out->developer)-1]='\0'; }
            if (e->publisher[0]) { strncpy(out->publisher, e->publisher, sizeof(out->publisher) - 1); out->publisher[sizeof(out->publisher)-1]='\0'; }
            if (e->genre[0])     { strncpy(out->genre,     e->genre,     sizeof(out->genre)     - 1); out->genre[sizeof(out->genre)-1]='\0'; }
            if (e->players[0])   { strncpy(out->players,   e->players,   sizeof(out->players)   - 1); out->players[sizeof(out->players)-1]='\0'; }
            if (e->languages[0]) { strncpy(out->languages, e->languages, sizeof(out->languages) - 1); out->languages[sizeof(out->languages)-1]='\0'; }
            strncpy(s_last_lookup_key, normalized_key, sizeof(s_last_lookup_key) - 1);
            s_last_lookup_key[sizeof(s_last_lookup_key) - 1] = '\0';
            s_last_lookup_result = true;
            if (out) s_last_lookup_meta = *out;
            return true;
        }
    }

    for (int i = 0; i < s_count; i++) {
        if (ci_strstr(normalized_key, s_entries[i].key) != NULL ||
            ci_strstr(s_entries[i].key, normalized_key) != NULL) {
            const MetaEntry *e = &s_entries[i];
            if (e->year[0])      { strncpy(out->year,      e->year,      sizeof(out->year)      - 1); out->year[sizeof(out->year)-1]='\0'; }
            if (e->developer[0]) { strncpy(out->developer, e->developer, sizeof(out->developer) - 1); out->developer[sizeof(out->developer)-1]='\0'; }
            if (e->publisher[0]) { strncpy(out->publisher, e->publisher, sizeof(out->publisher) - 1); out->publisher[sizeof(out->publisher)-1]='\0'; }
            if (e->genre[0])     { strncpy(out->genre,     e->genre,     sizeof(out->genre)     - 1); out->genre[sizeof(out->genre)-1]='\0'; }
            if (e->players[0])   { strncpy(out->players,   e->players,   sizeof(out->players)   - 1); out->players[sizeof(out->players)-1]='\0'; }
            if (e->languages[0]) { strncpy(out->languages, e->languages, sizeof(out->languages) - 1); out->languages[sizeof(out->languages)-1]='\0'; }
            strncpy(s_last_lookup_key, normalized_key, sizeof(s_last_lookup_key) - 1);
            s_last_lookup_key[sizeof(s_last_lookup_key) - 1] = '\0';
            s_last_lookup_result = true;
            if (out) s_last_lookup_meta = *out;
            return true;
        }
    }

    strncpy(s_last_lookup_key, normalized_key, sizeof(s_last_lookup_key) - 1);
    s_last_lookup_key[sizeof(s_last_lookup_key) - 1] = '\0';
    s_last_lookup_result = false;
    return false;
}

void meta_db_unload(void)
{
    s_last_lookup_key[0] = '\0';
    s_last_lookup_result = false;
    if (s_entries) {
        free(s_entries);
        s_entries = NULL;
    }
    if (s_json_buf) {
        free(s_json_buf);
        s_json_buf = NULL;
    }
    s_count = 0;
}

#endif
