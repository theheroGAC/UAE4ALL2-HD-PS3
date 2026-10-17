#ifdef __PS3__

#include "cover_manager_ps3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <SDL.h>
#include <SDL_image.h>

#include <curl/curl.h>
#include <sys/stat.h>

#include "ps3_diag.h"

static SDL_Surface *s_cover = NULL;
static char         s_cover_name[128] = "";
static char         s_last_requested_title[256] = "";

void cover_mgr_normalize(const char *input, char *out, size_t out_sz)
{
    if (!input || !out || out_sz == 0)
        return;

    const char *base = input;
    const char *p    = input;
    while (*p) {
        if (*p == '/' || *p == '\\')
            base = p + 1;
        p++;
    }

    char tmp[512];
    strncpy(tmp, base, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    size_t len = strlen(tmp);
    while (len > 0 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\'))
        tmp[--len] = '\0';

    if (len == 0) {
        const char *end = input + strlen(input);
        while (end > input && (*end == '/' || *end == '\\')) end--;
        const char *start = end;
        while (start > input && *(start - 1) != '/' && *(start - 1) != '\\') start--;
        size_t comp_len = (size_t)(end - start + 1);
        if (comp_len > 0 && comp_len < sizeof(tmp)) {
            strncpy(tmp, start, comp_len);
            tmp[comp_len] = '\0';
            len = strlen(tmp);
        }
    }

    static const char * const KNOWN_EXTS[] = {
        ".adf", ".adz", ".dms", ".ipf", ".lha", ".lzh",
        ".zip", ".7z",  ".m3u", ".hdf", ".chd", ".iso",
        ".cue", ".bin", ".img", NULL
    };
    char *dot = strrchr(tmp, '.');
    if (dot) {
        for (int i = 0; KNOWN_EXTS[i]; i++) {
            const char *ext = KNOWN_EXTS[i];
            size_t ext_len  = strlen(ext);
            size_t dot_len  = strlen(dot);
            if (dot_len == ext_len) {
                int match = 1;
                for (size_t j = 0; j < ext_len; j++) {
                    if (tolower((unsigned char)dot[j]) != ext[j]) {
                        match = 0; break;
                    }
                }
                if (match) { *dot = '\0'; len = strlen(tmp); break; }
            }
        }
    }

    for (;;) {
        len = strlen(tmp);
        while (len > 0 && tmp[len - 1] == ' ') tmp[--len] = '\0';
        if (len == 0) break;

        char last = tmp[len - 1];
        if (last == ')' || last == ']') {
            char open = (last == ')') ? '(' : '[';
            size_t j = len - 1;
            while (j > 0 && tmp[j] != open) j--;
            if (tmp[j] == open) {
                tmp[j] = '\0';
                len = j;
                while (len > 0 && tmp[len - 1] == ' ') tmp[--len] = '\0';
                continue;
            }
        }
        break;
    }

    len = strlen(tmp);
    while (len > 0 && tmp[len - 1] == '_') tmp[--len] = '\0';

    strncpy(out, tmp, out_sz - 1);
    out[out_sz - 1] = '\0';
}

static SDL_Surface *try_load(const char *path)
{
    ps3_diag_checkpoint("cover:load");
    return IMG_Load(path);
}

void cover_mgr_load(const char *display_title, const char *full_path)
{
    if (!display_title || display_title[0] == '\0') {
        cover_mgr_unload();
        return;
    }

    if (strcmp(s_last_requested_title, display_title) == 0)
        return;

    cover_mgr_unload();

    strncpy(s_last_requested_title, display_title, sizeof(s_last_requested_title) - 1);
    s_last_requested_title[sizeof(s_last_requested_title) - 1] = '\0';

    char key[256];
    cover_mgr_normalize(display_title, key, sizeof(key));
    if (key[0] == '\0') {
        strncpy(key, display_title, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';
    }

    char path[512];
    SDL_Surface *surf = NULL;

    snprintf(path, sizeof(path), "./data/covers/%s.png", key);
    surf = try_load(path);

    if (!surf) {
        snprintf(path, sizeof(path), "./data/covers/%s.jpg", key);
        surf = try_load(path);
    }

    if (!surf) {
        snprintf(path, sizeof(path), "./data/covers/%s.jpeg", key);
        surf = try_load(path);
    }

    if (!surf) {
        snprintf(path, sizeof(path), "./roms/covers/%s.png", key);
        surf = try_load(path);
    }

    if (!surf) {
        snprintf(path, sizeof(path), "./roms/covers/%s.jpg", key);
        surf = try_load(path);
    }

    if (!surf && full_path && full_path[0] != '\0') {
        const char *slash = strrchr(full_path, '/');
        if (slash) {
            size_t dir_len = (size_t)(slash - full_path);
            if (dir_len > 0 && dir_len < sizeof(path) - 1) {
                char dir[512];
                strncpy(dir, full_path, dir_len);
                dir[dir_len] = '\0';

                snprintf(path, sizeof(path), "%s/%s.png", dir, key);
                surf = try_load(path);

                if (!surf) {
                    snprintf(path, sizeof(path), "%s/%s.jpg", dir, key);
                    surf = try_load(path);
                }
            }
        }
    }

    if (surf) {
        s_cover = surf;
        strncpy(s_cover_name, display_title, sizeof(s_cover_name) - 1);
        s_cover_name[sizeof(s_cover_name) - 1] = '\0';
    }
}

void cover_mgr_unload(void)
{
    if (s_cover) {
        SDL_FreeSurface(s_cover);
        s_cover = NULL;
    }
    s_cover_name[0] = '\0';
    s_last_requested_title[0] = '\0';
}

SDL_Surface *cover_mgr_get(void)
{
    return s_cover;
}

static size_t curl_write_file(void *ptr, size_t size, size_t nmemb, void *stream)
{
    return fwrite(ptr, size, nmemb, (FILE *)stream);
}

static void cover_load_server_url(char *url_buf, size_t url_sz)
{
    url_buf[0] = '\0';
    FILE *f = fopen("./data/covers/source.txt", "r");
    if (f) {
        if (fgets(url_buf, (int)url_sz, f)) {
            size_t len = strlen(url_buf);
            while (len > 0 && (url_buf[len-1] == '\n' || url_buf[len-1] == '\r' || url_buf[len-1] == ' '))
                url_buf[--len] = '\0';
        }
        fclose(f);
    }
}

static void url_encode(const char *src, char *dst, size_t dst_sz)
{
    size_t di = 0;
    for (size_t si = 0; src[si] && di + 3 < dst_sz; si++) {
        unsigned char c = (unsigned char)src[si];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            dst[di++] = (char)c;
        } else {
            snprintf(dst + di, dst_sz - di, "%%%02X", c);
            di += 3;
        }
    }
    dst[di] = '\0';
}

int cover_mgr_download(const char *display_title)
{
    if (!display_title || display_title[0] == '\0')
        return -1;

    char key[256];
    cover_mgr_normalize(display_title, key, sizeof(key));
    if (key[0] == '\0')
        return -1;

    char out_path[512];
    snprintf(out_path, sizeof(out_path), "./data/covers/%s.png", key);

    FILE *test = fopen(out_path, "rb");
    if (test) {
        fclose(test);
        return 0;
    }

    char server_url[512];
    cover_load_server_url(server_url, sizeof(server_url));

    if (server_url[0] == '\0')
        return -2;

    char encoded_key[512];
    url_encode(key, encoded_key, sizeof(encoded_key));

    static const char * const EXTS[] = { ".png", ".jpg", NULL };

    mkdir("./data/covers", 0755);

    CURL *curl = curl_easy_init();
    if (!curl)
        return -3;

    int result = -6;

    for (int ext_i = 0; EXTS[ext_i]; ext_i++) {
        char url[1024];
        size_t slen = strlen(server_url);
        if (slen > 0 && server_url[slen - 1] == '/')
            snprintf(url, sizeof(url), "%s%s%s", server_url, encoded_key, EXTS[ext_i]);
        else
            snprintf(url, sizeof(url), "%s/%s%s", server_url, encoded_key, EXTS[ext_i]);

        char tmp_path[512];
        snprintf(tmp_path, sizeof(tmp_path), "./data/covers/%s.tmp", key);

        FILE *fp = fopen(tmp_path, "wb");
        if (!fp) {
            result = -4;
            break;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_file);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "UAE4ALL2-HD-PS3/1.0");

        ps3_diag_checkpoint("cover:download");
        CURLcode res = curl_easy_perform(curl);
        fclose(fp);

        if (res == CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code == 200) {
                snprintf(out_path, sizeof(out_path), "./data/covers/%s%s", key, EXTS[ext_i]);
                rename(tmp_path, out_path);
                result = 0;
                break;
            }
        }
        remove(tmp_path);

        if (res != CURLE_OK && res != CURLE_HTTP_RETURNED_ERROR) {
            result = -5;
            break;
        }
    }

    curl_easy_cleanup(curl);
    return result;
}

#endif