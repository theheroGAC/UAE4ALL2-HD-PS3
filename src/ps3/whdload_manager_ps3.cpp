#ifdef __PS3__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#ifndef O_BINARY
#define O_BINARY 0
#endif
#include <archive.h>
#include <archive_entry.h>

#include "whdload_manager_ps3.h"
#include "uae_gui_ps3.h"
#include "ps3_paths.h"
extern "C" {
mode_t umask(mode_t mask) { (void)mask; return 0; }
struct passwd *getpwnam(const char *name) { (void)name; return NULL; }
struct group *getgrnam(const char *name) { (void)name; return NULL; }
}
#include "sysconfig.h"
#include "sysdeps.h"
#include "menu_config.h"

extern char s_lib_rom_dir[512];
void ps3_load_rom_dir_if_needed(void);

static int path_exists(const char *path, int *is_dir)
{
    struct stat st;
    if (stat(path, &st) < 0)
        return 0;
    if (is_dir)
        *is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
    return 1;
}

static void ensure_directory(const char *path)
{
    if (!path || !path[0]) return;
    char buffer[512];
    strncpy(buffer, path, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char clean[512];
    int cpos = 0;
    for (int i = 0; buffer[i] && cpos < (int)sizeof(clean) - 1; i++) {
        char ch = (buffer[i] == '\\') ? '/' : buffer[i];
        if (ch == '/' && cpos > 0 && clean[cpos - 1] == '/')
            continue;
        clean[cpos++] = ch;
    }
    clean[cpos] = '\0';

    while (cpos > 0 && clean[cpos - 1] == '/') {
        clean[--cpos] = '\0';
    }

    for (char *p = clean; *p; p++) {
        if (*p == '/') {
            if (p == clean) continue;
            *p = '\0';
            if (strcmp(clean, "/dev_hdd0") != 0 && strcmp(clean, "/dev_usb000") != 0) {
                mkdir(clean, 0777);
            }
            *p = '/';
        }
    }
    if (strcmp(clean, "/dev_hdd0") != 0 && strcmp(clean, "/dev_usb000") != 0) {
        mkdir(clean, 0777);
    }
}

static int make_safe_relative(const char *source, char *destination, size_t destination_size)
{
    if (!source || !destination || destination_size == 0)
        return 0;

    const char *colon = strrchr(source, ':');
    if (colon) {
        source = colon + 1;
    }

    char temp[512];
    strncpy(temp, source, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    for (char *p = temp; *p; p++) {
        if (*p == '\\') *p = '/';
        else if (*p == ':') *p = '_';
    }

    size_t out = 0;
    const char *part = temp;
    while (*part == '/') part++;

    while (*part) {
        while (*part == '/') part++;
        if (!*part) break;
        const char *end = strchr(part, '/');
        size_t length = end ? (size_t)(end - part) : strlen(part);
        if (length == 0 || (length == 1 && part[0] == '.')) {
            part = end ? end + 1 : part + length;
            continue;
        }
        if (length == 2 && part[0] == '.' && part[1] == '.')
            return 0;
        if (out != 0) {
            if (out + 1 >= destination_size) return 0;
            destination[out++] = '/';
        }
        if (out + length >= destination_size) return 0;
        memcpy(destination + out, part, length);
        out += length;
        part = end ? end + 1 : part + length;
    }
    if (out == 0) return 0;
    destination[out] = '\0';
    return 1;
}

static void make_folder_name(const char *archive_path, char *folder, size_t folder_size)
{
    const char *name = strrchr(archive_path, '/');
    name = name ? name + 1 : archive_path;
    strncpy(folder, name, folder_size - 1);
    folder[folder_size - 1] = '\0';
    char *dot = strrchr(folder, '.');
    if (dot) *dot = '\0';
    for (size_t i = 0; folder[i]; i++) {
        unsigned char c = (unsigned char)folder[i];
        if (!isalnum(c) && c != '_' && c != '-' && c != ' ' && c != '.')
            folder[i] = '_';
    }
    if (folder[0] == '\0')
        strncpy(folder, "game", folder_size - 1);
    folder[folder_size - 1] = '\0';
}

static void make_unique_folder(const char *base, char *folder, size_t folder_size)
{
    char candidate[512];
    int number = 1;
    strncpy(folder, base, folder_size - 1);
    folder[folder_size - 1] = '\0';
    snprintf(candidate, sizeof(candidate), "%s/%s", PS3_WHDLOAD_ROOT, folder);
    int is_dir = 0;
    while (path_exists(candidate, &is_dir)) {
        number++;
        snprintf(folder, folder_size, "%s_%d", base, number);
        snprintf(candidate, sizeof(candidate), "%s/%s", PS3_WHDLOAD_ROOT, folder);
    }
}

#define LHA_DICSIZ (1 << 13)
#define LHA_MAX_DICSIZ (1 << 16)
#define LHA_THRESHOLD 3
#define LHA_NC 510
#define LHA_NP 14
#define LHA_MAX_NP 17
#define LHA_NT 19
#define LHA_PBIT 4
#define LHA_TBIT 5
#define LHA_NPT (LHA_NT > LHA_MAX_NP ? LHA_NT : LHA_MAX_NP)

#define LHA_IN_BUF_SIZE (64 * 1024)

typedef struct {
    int in_fd;
    int out_fd;
    uint32_t comp_size;
    uint32_t orig_size;
    uint64_t bitbuf;
    int bitcount;
    int input_exhausted;
    int pad_bytes;      /* zero bytes fed after the real end of input */
    uint8_t *dtext;
    int dpos;
    int dicsiz;         /* 8K (-lh5-), 32K (-lh6-), 64K (-lh7-) */
    int np;             /* position table size: dicbit + 1 */
    int pbit;           /* bits used to encode np */

    uint8_t in_buf[LHA_IN_BUF_SIZE];
    off_t in_pos;
    off_t in_len;

    uint16_t left[2 * LHA_NC + 1];
    uint16_t right[2 * LHA_NC + 1];
    uint8_t pt_len[LHA_NPT];
    uint16_t pt_table[256];
    uint8_t c_len[LHA_NC];
    uint16_t c_table[4096];
    uint16_t blocksize;
} LhaNativeDecoder;

static inline uint16_t lha_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t lha_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int lha_read1(LhaNativeDecoder *dec)
{
    if (dec->comp_size == 0) {
        dec->input_exhausted = 1;
        return -1;
    }
    if (dec->in_pos >= dec->in_len) {
        int want = (dec->comp_size < LHA_IN_BUF_SIZE) ? (int)dec->comp_size : LHA_IN_BUF_SIZE;
        int got = (int)read(dec->in_fd, dec->in_buf, want);
        if (got <= 0) {
            dec->comp_size = 0;
            dec->input_exhausted = 1;
            return -1;
        }
        dec->in_pos = 0;
        dec->in_len = got;
    }
    int b = dec->in_buf[dec->in_pos++];
    dec->comp_size--;
    if (dec->comp_size == 0 && dec->in_pos >= dec->in_len)
        dec->input_exhausted = 1;
    return b;
}

static void lha_fillbuf(LhaNativeDecoder *dec, int n)
{
    while (dec->bitcount < n) {
        int c = lha_read1(dec);
        if (c < 0)
            dec->pad_bytes++;
        dec->bitbuf = (dec->bitbuf << 8) | (uint64_t)(c >= 0 ? (c & 0xff) : 0);
        dec->bitcount += 8;
    }
}

static uint32_t lha_getbits(LhaNativeDecoder *dec, int n)
{
    lha_fillbuf(dec, n);
    uint32_t x = (uint32_t)((dec->bitbuf >> (dec->bitcount - n)) & ((1U << n) - 1));
    dec->bitcount -= n;
    return x;
}

static uint32_t lha_peekbits(LhaNativeDecoder *dec, int n)
{
    lha_fillbuf(dec, n);
    return (uint32_t)((dec->bitbuf >> (dec->bitcount - n)) & ((1U << n) - 1));
}

static void lha_dropbits(LhaNativeDecoder *dec, int n)
{
    dec->bitcount -= n;
    if (dec->bitcount > 0) {
        dec->bitbuf &= ((uint64_t)1 << dec->bitcount) - 1;
    } else {
        dec->bitbuf = 0;
        dec->bitcount = 0;
    }
}

static void lha_make_table(int nchar, uint8_t *bitlen, int tablebits, uint16_t *table, uint16_t *left, uint16_t *right)
{
    uint16_t count[17] = {0};
    uint16_t start[17] = {0};
    int i;
    for (i = 0; i < nchar; i++) {
        if (bitlen[i] <= 16) count[bitlen[i]]++;
    }
    for (i = 1; i <= 16; i++) {
        start[i] = start[i - 1] + (count[i - 1] << (16 - (i - 1)));
    }
    int tablesize = 1 << tablebits;
    for (i = 0; i < tablesize; i++) table[i] = 0;
    int avail = nchar;
    for (i = 0; i < nchar; i++) {
        int len = bitlen[i];
        if (len == 0) continue;
        uint16_t k = start[len];
        start[len] += 1 << (16 - len);
        if (len <= tablebits) {
            int step = 1 << (tablebits - len);
            int idx = k >> (16 - tablebits);
            for (int j = 0; j < step; j++) table[idx + j] = i;
        } else {
            int idx = k >> (16 - tablebits);
            uint16_t *p = &table[idx];
            for (int j = tablebits; j < len; j++) {
                if (*p == 0) { left[avail] = right[avail] = 0; *p = avail++; }
                if (k & (1 << (15 - j))) p = &right[*p];
                else p = &left[*p];
            }
            *p = i;
        }
    }
}

static void lha_read_pt_len(LhaNativeDecoder *dec, int nn, int nbit, int i_special)
{
    int n = lha_getbits(dec, nbit);
    if (n == 0) {
        int c = lha_getbits(dec, nbit);
        for (int i = 0; i < nn; i++) dec->pt_len[i] = 0;
        for (int i = 0; i < 256; i++) dec->pt_table[i] = c;
    } else {
        int i = 0;
        while (i < n) {
            int c = lha_peekbits(dec, 3);
            if (c == 7) {
                lha_fillbuf(dec, 20);
                uint64_t mask = (uint64_t)1 << (dec->bitcount - 4);
                while (mask & dec->bitbuf) {
                    mask >>= 1;
                    c++;
                    if (!mask || c > 32) break;
                }
            }
            lha_dropbits(dec, (c < 7) ? 3 : c - 3);
            if (i < nn) dec->pt_len[i++] = c;
            else i++;
            if (i == i_special) {
                int c2 = lha_getbits(dec, 2);
                while (--c2 >= 0) {
                    if (i < nn) dec->pt_len[i++] = 0;
                    else i++;
                }
            }
        }
        while (i < nn) dec->pt_len[i++] = 0;
        lha_make_table(nn, dec->pt_len, 8, dec->pt_table, dec->left, dec->right);
    }
}

static void lha_read_c_len(LhaNativeDecoder *dec)
{
    int n = lha_getbits(dec, 9);
    if (n == 0) {
        int c = lha_getbits(dec, 9);
        for (int i = 0; i < LHA_NC; i++) dec->c_len[i] = 0;
        for (int i = 0; i < 4096; i++) dec->c_table[i] = c;
    } else {
        int i = 0;
        while (i < n) {
            int c = dec->pt_table[lha_peekbits(dec, 8)];
            if (c >= LHA_NT) {
                lha_fillbuf(dec, 20);
                uint64_t mask = (uint64_t)1 << (dec->bitcount - 9);
                int depth = 0;
                do {
                    if (dec->bitbuf & mask) c = dec->right[c];
                    else c = dec->left[c];
                    mask >>= 1;
                    if (!mask || ++depth > 32 || c >= (2 * LHA_NC)) {
                        c = 0;
                        break;
                    }
                } while (c >= LHA_NT);
            }
            lha_dropbits(dec, dec->pt_len[c]);
            if (c <= 2) {
                if (c == 0) c = 1;
                else if (c == 1) c = lha_getbits(dec, 4) + 3;
                else c = lha_getbits(dec, 9) + 20;
                while (--c >= 0) {
                    if (i < LHA_NC) dec->c_len[i++] = 0;
                    else i++;
                }
            } else {
                if (i < LHA_NC) dec->c_len[i++] = c - 2;
                else i++;
            }
        }
        while (i < LHA_NC) dec->c_len[i++] = 0;
        lha_make_table(LHA_NC, dec->c_len, 12, dec->c_table, dec->left, dec->right);
    }
}

static int lha_decode_c(LhaNativeDecoder *dec)
{
    if (dec->blocksize == 0) {
        dec->blocksize = lha_getbits(dec, 16);
        lha_read_pt_len(dec, LHA_NT, LHA_TBIT, 3);
        lha_read_c_len(dec);
        lha_read_pt_len(dec, dec->np, dec->pbit, -1);
    }
    dec->blocksize--;
    int j = dec->c_table[lha_peekbits(dec, 12)];
    if (j >= LHA_NC) {
        lha_fillbuf(dec, 24);
        uint64_t mask = (uint64_t)1 << (dec->bitcount - 13);
        int depth = 0;
        do {
            if (dec->bitbuf & mask) j = dec->right[j];
            else j = dec->left[j];
            mask >>= 1;
            if (!mask || ++depth > 32 || j >= (2 * LHA_NC)) {
                j = 0;
                break;
            }
        } while (j >= LHA_NC);
    }
    lha_dropbits(dec, dec->c_len[j]);
    return j;
}

static int lha_decode_p(LhaNativeDecoder *dec)
{
    int j = dec->pt_table[lha_peekbits(dec, 8)];
    if (j >= dec->np) {
        lha_fillbuf(dec, 20);
        uint64_t mask = (uint64_t)1 << (dec->bitcount - 9);
        int depth = 0;
        do {
            if (dec->bitbuf & mask) j = dec->right[j];
            else j = dec->left[j];
            mask >>= 1;
            if (!mask || ++depth > 32 || j >= (2 * LHA_NC)) {
                j = 0;
                break;
            }
        } while (j >= dec->np);
    }
    lha_dropbits(dec, dec->pt_len[j]);
    if (j != 0) j = (1 << (j - 1)) + lha_getbits(dec, j - 1);
    return j;
}

static int lha_flush_output(int out_fd, char *buffer, int *buffer_pos)
{
    if (*buffer_pos <= 0)
        return 1;
    int written = write(out_fd, buffer, *buffer_pos);
    if (written != *buffer_pos)
        return 0;
    *buffer_pos = 0;
    return 1;
}

static int lha_decode_file(int in_fd, int out_fd, uint32_t comp_size, uint32_t orig_size, const char *method)
{
    LhaNativeDecoder *dec = (LhaNativeDecoder *)calloc(1, sizeof(LhaNativeDecoder));
    if (!dec) return 0;
    dec->in_fd = in_fd;
    dec->out_fd = out_fd;
    dec->comp_size = comp_size;
    dec->orig_size = orig_size;

    if (method && strcmp(method, "-lh6-") == 0) {
        dec->dicsiz = 1 << 15;
        dec->np = 16;
        dec->pbit = 5;
    } else if (method && strcmp(method, "-lh7-") == 0) {
        dec->dicsiz = 1 << 16;
        dec->np = 17;
        dec->pbit = 5;
    } else {
        dec->dicsiz = 1 << 13;
        dec->np = 14;
        dec->pbit = 4;
    }

    dec->dtext = (uint8_t *)malloc(dec->dicsiz);
    if (!dec->dtext) { free(dec); return 0; }
    memset(dec->dtext, ' ', dec->dicsiz);

    const int out_capacity = 64 * 1024;
    char *out_buf = (char *)malloc(out_capacity);
    if (!out_buf) { free(dec->dtext); free(dec); return 0; }
    int out_buf_pos = 0;
    int success = 1;
    uint32_t count = 0;

    if (orig_size > (32u * 1024u * 1024u)) {
        write_log("[PS3] lha_decode_file: rifiuto file sospetto orig=%u comp=%u\n", orig_size, comp_size);
        free(out_buf);
        free(dec->dtext);
        free(dec);
        return 0;
    }

    while (success && count < orig_size) {
        if (dec->pad_bytes > 32) {
            write_log("[PS3] lha_decode_file: input troncato a %u/%u byte (pad=%d)\n", count, orig_size, dec->pad_bytes);
            success = 0;
            break;
        }
        int c = lha_decode_c(dec);
        if (c < 256) {
            out_buf[out_buf_pos++] = (char)c;
            if (out_buf_pos >= out_capacity)
                success = lha_flush_output(out_fd, out_buf, &out_buf_pos);
            dec->dtext[dec->dpos++] = (uint8_t)c;
            if (dec->dpos >= dec->dicsiz) dec->dpos = 0;
            count++;
        } else {
            int match_len = c - 256 + LHA_THRESHOLD;
            int offset = lha_decode_p(dec);
            int match_pos = (dec->dpos - offset - 1) & (dec->dicsiz - 1);
            for (int k = 0; success && k < match_len && count < orig_size; k++) {
                uint8_t ch = dec->dtext[match_pos++];
                if (match_pos >= dec->dicsiz) match_pos = 0;
                out_buf[out_buf_pos++] = (char)ch;
                if (out_buf_pos >= out_capacity)
                    success = lha_flush_output(out_fd, out_buf, &out_buf_pos);
                dec->dtext[dec->dpos++] = ch;
                if (dec->dpos >= dec->dicsiz) dec->dpos = 0;
                count++;
            }
        }
    }

    if (success && !lha_flush_output(out_fd, out_buf, &out_buf_pos))
        success = 0;
    free(out_buf);
    free(dec->dtext);
    free(dec);
    return success && count == orig_size;
}

extern "C" void ps3_gui_draw_progress(const char *title, const char *subtitle, float fraction, const char *item_name);

typedef struct {
    char method[6];
    uint32_t comp_size;
    uint32_t orig_size;
    uint8_t level;
    char name[512];
    char dirname[512];
    uint32_t ext_total;
} LhaHeaderInfo;

static int lha_read_header(int in_fd, LhaHeaderInfo *h)
{
    memset(h, 0, sizeof(*h));

    uint8_t raw2[2];
    if (read(in_fd, raw2, 2) != 2)
        return 0;

    if (raw2[0] == 0)
        return 0;

    uint8_t buf[19];
    if (read(in_fd, buf, 19) != 19)
        return 0;

    memcpy(h->method, buf, 5);
    h->method[5] = '\0';
    h->comp_size = lha_le32(buf + 5);
    h->orig_size = lha_le32(buf + 9);
    h->level = buf[18];

    if (h->level == 0 || h->level == 1) {
        uint8_t hsize = raw2[0];
        if (hsize == 0) return 0;
        uint8_t nlen = 0;
        if (read(in_fd, &nlen, 1) != 1)
            return 0;
        if (nlen > 0) {
            int to_read = (nlen < (int)(sizeof(h->name) - 1)) ? nlen : (int)(sizeof(h->name) - 1);
            if (read(in_fd, h->name, to_read) != to_read)
                return 0;
            h->name[to_read] = '\0';
            if (nlen > to_read)
                lseek(in_fd, nlen - to_read, SEEK_CUR);
        }
        uint8_t crc[2];
        if (read(in_fd, crc, 2) != 2)
            return 0;

        int bytes_read_so_far = 2 + 19 + 1 + nlen + 2;
        if (h->level == 1) {
            uint8_t os = 0;
            if (read(in_fd, &os, 1) != 1)
                return 0;
            bytes_read_so_far += 1;
            int pad_len = (int)hsize + 2 - bytes_read_so_far - 2;
            if (pad_len > 0)
                lseek(in_fd, pad_len, SEEK_CUR);

            uint8_t ext_buf[2];
            if (read(in_fd, ext_buf, 2) != 2)
                return 0;
            uint16_t next_ext = lha_le16(ext_buf);
            while (next_ext > 0) {
                h->ext_total += next_ext;
                uint8_t ext_type = 0;
                if (read(in_fd, &ext_type, 1) != 1)
                    return 0;
                int dlen = (int)next_ext - 3;
                if (ext_type == 0x02 && dlen > 0) {
                    int read_len = dlen > (int)(sizeof(h->dirname) - 1) ? (int)(sizeof(h->dirname) - 1) : dlen;
                    if (read(in_fd, h->dirname, read_len) != read_len)
                        return 0;
                    h->dirname[read_len] = '\0';
                    for (int i = 0; i < read_len; i++)
                        if (h->dirname[i] == (char)0xFF) h->dirname[i] = '/';
                    if (dlen > read_len)
                        lseek(in_fd, dlen - read_len, SEEK_CUR);
                } else if (ext_type == 0x01 && dlen > 0) {
                    int read_len = dlen > (int)(sizeof(h->name) - 1) ? (int)(sizeof(h->name) - 1) : dlen;
                    if (read(in_fd, h->name, read_len) != read_len)
                        return 0;
                    h->name[read_len] = '\0';
                    if (dlen > read_len)
                        lseek(in_fd, dlen - read_len, SEEK_CUR);
                } else if (dlen > 0) {
                    lseek(in_fd, dlen, SEEK_CUR);
                }
                uint8_t next_buf[2];
                if (read(in_fd, next_buf, 2) != 2)
                    break;
                next_ext = lha_le16(next_buf);
            }
        } else {
            int skip_base = (int)hsize + 2 - bytes_read_so_far;
            if (skip_base > 0)
                lseek(in_fd, skip_base, SEEK_CUR);
        }
    } else if (h->level == 2) {
        uint8_t l2_hdr[5];
        if (read(in_fd, l2_hdr, 5) != 5)
            return 0;
        uint16_t next_ext_size = lha_le16(l2_hdr + 3);
        while (next_ext_size > 0) {
            h->ext_total += next_ext_size;
            uint8_t ext_type = 0;
            if (read(in_fd, &ext_type, 1) != 1)
                return 0;
            int dlen = (int)next_ext_size - 3;
            if (ext_type == 0x02 && dlen > 0) {
                int read_len = dlen > (int)(sizeof(h->dirname) - 1) ? (int)(sizeof(h->dirname) - 1) : dlen;
                if (read(in_fd, h->dirname, read_len) != read_len)
                    return 0;
                h->dirname[read_len] = '\0';
                for (int i = 0; i < read_len; i++)
                    if (h->dirname[i] == (char)0xFF) h->dirname[i] = '/';
                if (dlen > read_len)
                    lseek(in_fd, dlen - read_len, SEEK_CUR);
            } else if (ext_type == 0x01 && dlen > 0) {
                int read_len = dlen > (int)(sizeof(h->name) - 1) ? (int)(sizeof(h->name) - 1) : dlen;
                if (read(in_fd, h->name, read_len) != read_len)
                    return 0;
                h->name[read_len] = '\0';
                if (dlen > read_len)
                    lseek(in_fd, dlen - read_len, SEEK_CUR);
            } else if (dlen > 0) {
                lseek(in_fd, dlen, SEEK_CUR);
            }
            uint8_t next_buf[2];
            if (read(in_fd, next_buf, 2) != 2)
                break;
            next_ext_size = lha_le16(next_buf);
        }
    } else {
        return 0;
    }
    return 1;
}

static int count_native_lha_files(const char *archive_path)
{
    int in_fd = open(archive_path, O_RDONLY, 0);
    if (in_fd < 0) return 0;

    int total = 0;
    LhaHeaderInfo h;
    while (lha_read_header(in_fd, &h)) {
        uint32_t actual_comp = (h.level == 1 && h.comp_size >= h.ext_total) ? h.comp_size - h.ext_total : h.comp_size;
        if (strcmp(h.method, "-lhd-") != 0) {
            total++;
        }
        lseek(in_fd, actual_comp, SEEK_CUR);
    }

    close(in_fd);
    return total;
}

static int extract_native_lha(const char *archive_path, const char *destination_root)
{
    int total_files = count_native_lha_files(archive_path);
    if (total_files <= 0) total_files = 1;

    int in_fd = open(archive_path, O_RDONLY, 0);
    if (in_fd < 0) return 0;

    int file_count = 0;
    int processed = 0;
    LhaHeaderInfo h;
    while (lha_read_header(in_fd, &h)) {
        uint32_t actual_skip = (h.level == 1 && h.comp_size >= h.ext_total) ? (h.comp_size - h.ext_total) : h.comp_size;
        char raw_path[1024];
        if (h.dirname[0]) {
            snprintf(raw_path, sizeof(raw_path), "%s%s", h.dirname, h.name);
        } else {
            snprintf(raw_path, sizeof(raw_path), "%s", h.name);
        }

        char relative[512];
        if (!make_safe_relative(raw_path, relative, sizeof(relative))) {
            lseek(in_fd, actual_skip, SEEK_CUR);
            continue;
        }

        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/%s", destination_root, relative);
        size_t rel_len = strlen(relative);
        if (rel_len > 0 && relative[rel_len - 1] == '/') {
            ensure_directory(output_path);
            lseek(in_fd, actual_skip, SEEK_CUR);
            continue;
        }
        if (strcmp(h.method, "-lhd-") == 0) {
            ensure_directory(output_path);
            lseek(in_fd, actual_skip, SEEK_CUR);
            continue;
        }

        char *slash = strrchr(output_path, '/');
        if (slash) {
            *slash = '\0';
            ensure_directory(output_path);
            *slash = '/';
        }

        uint32_t actual_comp = actual_skip;
        processed++;
        float fraction = (float)processed / (float)total_files;
        if (fraction > 1.0f) fraction = 1.0f;
        char sub[64];
        snprintf(sub, sizeof(sub), "Extracting %d / %d (%d%%)", processed, total_files, (int)(fraction * 100));
        ps3_gui_draw_progress("Extracting WHDLoad Game...", sub, fraction, raw_path);

        off_t data_start = lseek(in_fd, 0, SEEK_CUR);
        if (strcmp(h.method, "-lh0-") == 0) {
            if (h.orig_size > (32u * 1024u * 1024u)) {
                write_log("[PS3] extract_native_lha: -lh0- rifiuto file sospetto '%s' orig=%u\n", output_path, h.orig_size);
            } else {
                int out_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
                if (out_fd >= 0) {
                    char buf[4096];
                    uint32_t remaining = h.orig_size;
                    while (remaining > 0) {
                        uint32_t to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
                        int r = read(in_fd, buf, to_read);
                        if (r <= 0) break;
                        if (write(out_fd, buf, r) != r) { remaining = 1; break; }
                        remaining -= r;
                    }
                    int copy_ok = remaining == 0;
                    close(out_fd);
                    if (copy_ok) file_count++;
                    else {
                        write_log("[PS3] extract_native_lha: -lh0- copy failed for '%s'\n", output_path);
                        unlink(output_path);
                    }
                } else {
                    write_log("[PS3] extract_native_lha: open failed for '%s': %s\n", output_path, strerror(errno));
                }
            }
        } else if (strcmp(h.method, "-lh5-") == 0 || strcmp(h.method, "-lh4-") == 0 || strcmp(h.method, "-lh6-") == 0 || strcmp(h.method, "-lh7-") == 0) {
            int out_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
            if (out_fd >= 0) {
                int decode_ok = 0;
                if (h.orig_size == 0) {
                    decode_ok = 1;
                } else {
                    decode_ok = lha_decode_file(in_fd, out_fd, actual_comp, h.orig_size, h.method);
                }
                close(out_fd);
                if (decode_ok) file_count++;
                else {
                    write_log("[PS3] extract_native_lha: lha_decode_file failed for '%s' (comp=%u orig=%u)\n",
                                   output_path, actual_comp, h.orig_size);
                    unlink(output_path);
                }
            } else {
                write_log("[PS3] extract_native_lha: open failed for '%s': %s\n", output_path, strerror(errno));
            }
        } else {
            write_log("[PS3] extract_native_lha: unsupported method '%s' for '%s'\n", h.method, output_path);
        }

        lseek(in_fd, data_start + actual_comp, SEEK_SET);
    }

    ps3_gui_draw_progress("Extracting WHDLoad Game...", "Completato", 1.0f, "");
    close(in_fd);
    write_log("[PS3] extract_native_lha: %d/%d file estratti in '%s'\n", file_count, total_files, destination_root);
    return file_count;
}

struct ArchiveFileContext {
    int fd;
    char buffer[64 * 1024];
};

static int custom_archive_open(struct archive *a, void *client_data)
{
    return ARCHIVE_OK;
}

static la_ssize_t custom_archive_read(struct archive *a, void *client_data, const void **buff)
{
    ArchiveFileContext *ctx = (ArchiveFileContext *)client_data;
    if (!ctx || ctx->fd < 0) return -1;
    *buff = ctx->buffer;
    int bytes = read(ctx->fd, ctx->buffer, sizeof(ctx->buffer));
    if (bytes < 0) return -1;
    return (la_ssize_t)bytes;
}

static int custom_archive_close(struct archive *a, void *client_data)
{
    ArchiveFileContext *ctx = (ArchiveFileContext *)client_data;
    if (ctx) {
        if (ctx->fd >= 0) {
            close(ctx->fd);
            ctx->fd = -1;
        }
        free(ctx);
    }
    return ARCHIVE_OK;
}

static int write_entry(struct archive *input, const char *path)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd < 0)
        return 0;

    char *buffer = (char *)malloc(64 * 1024);
    if (!buffer) {
        close(fd);
        return 0;
    }

    la_ssize_t bytes;
    int success = 1;
    while ((bytes = archive_read_data(input, buffer, 64 * 1024)) > 0) {
        if (write(fd, buffer, bytes) != bytes) {
            success = 0;
            break;
        }
    }
    if (bytes < 0)
        success = 0;

    free(buffer);
    close(fd);
    return success;
}

static int find_slave_recursive(const char *root, const char *relative, char *result, size_t result_size, int depth)
{
    if (depth > 8)
        return 0;

    char directory_path[512];
    if (relative && relative[0])
        snprintf(directory_path, sizeof(directory_path), "%s/%s", root, relative);
    else
        snprintf(directory_path, sizeof(directory_path), "%s", root);

    DIR *dir = opendir(directory_path);
    if (!dir)
        return 0;

    struct dirent *entry;
    int found = 0;
    while (!found && (entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' || strcmp(entry->d_name, "S") == 0)
            continue;

        char child_relative[384];
        if (relative && relative[0])
            snprintf(child_relative, sizeof(child_relative), "%s/%s", relative, entry->d_name);
        else
            snprintf(child_relative, sizeof(child_relative), "%s", entry->d_name);

        char child_full[512];
        snprintf(child_full, sizeof(child_full), "%s/%s", root, child_relative);
        struct stat st;
        if (stat(child_full, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                found = find_slave_recursive(root, child_relative, result, result_size, depth + 1);
            } else {
                const char *dot = strrchr(entry->d_name, '.');
                if (dot && strcasecmp(dot, ".slave") == 0) {
                    strncpy(result, child_relative, result_size - 1);
                    result[result_size - 1] = '\0';
                    found = 1;
                }
            }
        }
    }
    closedir(dir);
    return found;
}

static int copy_text_file(const char *source, const char *destination)
{
    FILE *input = fopen(source, "rb");
    if (!input)
        return 0;
    FILE *output = fopen(destination, "wb");
    if (!output) {
        fclose(input);
        return 0;
    }
    char buffer[4096];
    size_t bytes;
    int success = 1;
    while ((bytes = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (fwrite(buffer, 1, bytes, output) != bytes) {
            success = 0;
            break;
        }
    }
    fclose(input);
    fclose(output);
    return success;
}

static int read_text_file(const char *path, char *buffer, size_t buffer_size)
{
    FILE *input = fopen(path, "rb");
    if (!input)
        return 0;
    size_t length = fread(buffer, 1, buffer_size - 1, input);
    fclose(input);
    buffer[length] = '\0';
    return 1;
}

static int write_text_file(const char *path, const char *text)
{
    remove(path);
    FILE *output = fopen(path, "wb");
    if (!output) {
        write_log("[PS3] write_text_file: fopen('%s', 'wb') failed: errno=%d (%s)\n", path, errno, strerror(errno));
        return 0;
    }
    size_t length = strlen(text);
    size_t written = fwrite(text, 1, length, output);
    fflush(output);
    fclose(output);
    write_log("[PS3] write_text_file: wrote %u/%u bytes to '%s'\n",
              (unsigned)written, (unsigned)length, path);
    return (written == length);
}

static bool contains_case_insensitive(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return false;
    size_t nlen = strlen(needle);
    if (nlen == 0) return true;
    for (const char *h = haystack; *h; h++) {
        if (strncasecmp(h, needle, nlen) == 0)
            return true;
    }
    return false;
}

static void strip_whdload_lines(char *text)
{
    if (!text || text[0] == '\0') return;
    char *src = text;
    char *dst = text;

    while (*src) {
        char *line_start = src;
        while (*src && *src != '\r' && *src != '\n') {
            src++;
        }
        size_t len = (size_t)(src - line_start);

        char line_buf[512];
        if (len >= sizeof(line_buf)) len = sizeof(line_buf) - 1;
        memcpy(line_buf, line_start, len);
        line_buf[len] = '\0';

        const char *p = line_buf;
        while (*p == ' ' || *p == '\t') p++;

        bool drop = false;
        if (contains_case_insensitive(p, "WHDLoad") ||
            (strncasecmp(p, "CD ", 3) == 0 && contains_case_insensitive(p, "DH0:"))) {
            drop = true;
        }

        if (!drop) {
            if (dst != line_start) {
                memmove(dst, line_start, src - line_start);
            }
            dst += (src - line_start);
            if (*src == '\r') { *dst++ = *src++; }
            if (*src == '\n') { *dst++ = *src++; }
        } else {
            if (*src == '\r') src++;
            if (*src == '\n') src++;
        }
    }
    *dst = '\0';
}

static char *find_endcli(char *text)
{
    for (char *p = text; *p; p++) {
        if ((p == text || p[-1] == '\n') &&
            (p[0] == 'E' || p[0] == 'e') &&
            (p[1] == 'N' || p[1] == 'n') &&
            (p[2] == 'D' || p[2] == 'd') &&
            (p[3] == 'C' || p[3] == 'c') &&
            (p[4] == 'L' || p[4] == 'l') &&
            (p[5] == 'I' || p[5] == 'i'))
            return p;
    }
    return NULL;
}

static char s_last_error[256] = "";

const char *ps3_whdload_get_last_error(void)
{
    return s_last_error;
}

const char *ps3_whdload_root(void)
{
    return PS3_WHDLOAD_ROOT;
}

static int extract_libarchive_lha(const char *archive_path, const char *destination_root, int only_missing)
{
    struct archive *input = archive_read_new();
    struct archive_entry *entry = NULL;
    if (!input) return 0;
    ArchiveFileContext *ctx = (ArchiveFileContext *)malloc(sizeof(ArchiveFileContext));
    if (!ctx) { archive_read_free(input); return 0; }
    ctx->fd = open(archive_path, O_RDONLY, 0);
    if (ctx->fd < 0) { free(ctx); archive_read_free(input); return 0; }
    archive_read_support_filter_all(input);
    archive_read_support_format_lha(input);
    archive_read_support_format_zip(input);
    archive_read_support_format_7zip(input);
    archive_read_support_format_all(input);
    archive_read_set_open_callback(input, custom_archive_open);
    archive_read_set_read_callback(input, custom_archive_read);
    archive_read_set_close_callback(input, custom_archive_close);
    archive_read_set_callback_data(input, ctx);
    int open_res = archive_read_open1(input);
    if (open_res != ARCHIVE_OK && open_res != ARCHIVE_WARN) { archive_read_free(input); return 0; }
    int extracted = 0;
    int r;
    while ((r = archive_read_next_header(input, &entry)) == ARCHIVE_OK || r == ARCHIVE_WARN) {
        const char *entry_name = archive_entry_pathname(entry);
        char relative[384];
        if (!make_safe_relative(entry_name, relative, sizeof(relative))) { archive_read_data_skip(input); continue; }
        char output_path[512];
        snprintf(output_path, sizeof(output_path), "%s/%s", destination_root, relative);
        mode_t file_type = archive_entry_filetype(entry);
        size_t rel_len = strlen(relative);
        if (file_type == AE_IFDIR || (rel_len > 0 && relative[rel_len - 1] == '/')) { ensure_directory(output_path); continue; }
        if (only_missing) {
            struct stat st;
            if (stat(output_path, &st) == 0 && st.st_size > 0) { archive_read_data_skip(input); continue; }
        }
        char *slash = strrchr(output_path, '/');
        if (slash) { *slash = '\0'; ensure_directory(output_path); *slash = '/'; }
        if (write_entry(input, output_path)) extracted++;
        else archive_read_data_skip(input);
    }
    archive_read_free(input);
    return extracted;
}

int ps3_whdload_install_lha(const char *archive_path, char *installed_path, size_t installed_path_size)
{
    s_last_error[0] = '\0';
    if (!archive_path || !installed_path || installed_path_size == 0) {
        snprintf(s_last_error, sizeof(s_last_error), "Invalid parameters");
        return 0;
    }

    installed_path[0] = '\0';
    ensure_directory(PS3_WHDLOAD_ROOT);

    char base[128];
    char folder[128];
    make_folder_name(archive_path, base, sizeof(base));
    strncpy(folder, base, sizeof(folder) - 1);
    folder[sizeof(folder) - 1] = '\0';

    char destination_root[512];
    snprintf(destination_root, sizeof(destination_root), "%s/%s", PS3_WHDLOAD_ROOT, folder);
    ensure_directory(destination_root);

    int native_count = extract_native_lha(archive_path, destination_root);
    if (native_count > 0) {
        strncpy(installed_path, destination_root, installed_path_size - 1);
        installed_path[installed_path_size - 1] = '\0';
        ps3_gui_draw_progress("Extracting WHDLoad Game...", "Completato", 1.0f, "");
        return 1;
    }

    int extracted = extract_libarchive_lha(archive_path, destination_root, 0);
    if (extracted == 0) {
        if (s_last_error[0] == '\0') {
            snprintf(s_last_error, sizeof(s_last_error), "0 files extracted from archive");
        }
        return 0;
    }

    strncpy(installed_path, destination_root, installed_path_size - 1);
    installed_path[installed_path_size - 1] = '\0';
    return 1;
}

static void deploy_file_if_missing(const char *src, const char *dst)
{
    struct stat st_src, st_dst;
    if (stat(src, &st_src) != 0 || st_src.st_size <= 0)
        return;
    if (stat(dst, &st_dst) == 0 && st_dst.st_size == st_src.st_size)
        return;
    char dst_copy[512];
    strncpy(dst_copy, dst, sizeof(dst_copy) - 1);
    dst_copy[sizeof(dst_copy) - 1] = '\0';
    char *slash = strrchr(dst_copy, '/');
    if (slash) {
        *slash = '\0';
        ensure_directory(dst_copy);
        *slash = '/';
    }
    copy_text_file(src, dst);
}

static int deploy_rom_clean(const char *src, const char *dst)
{
    struct stat st_dst;
    if (stat(dst, &st_dst) == 0 && S_ISREG(st_dst.st_mode) &&
        (st_dst.st_size == 262144 || st_dst.st_size == 524288)) {
        return 1;
    }

    struct stat st_src;
    if (stat(src, &st_src) != 0 || !S_ISREG(st_src.st_mode) ||
        st_src.st_size < 200000 || st_src.st_size > 1100000) {
        return 0;
    }

    FILE *in = fopen(src, "rb");
    if (!in)
        return 0;
    long fsize = (long)st_src.st_size;

    long read_offset = 0;
    long target_len = fsize;
    if (fsize == 524800 || fsize == 262656) {
        read_offset = 512;
        target_len = fsize - 512;
    }

    unsigned char header[12];
    memset(header, 0, sizeof(header));
    fseek(in, read_offset, SEEK_SET);
    size_t hread = fread(header, 1, 11, in);
    int is_cloanto = (hread == 11 && memcmp(header, "AMIROMTYPE1", 11) == 0);
    if (is_cloanto) {
        read_offset += 11;
        target_len -= 11;
    }

    if (target_len <= 0) {
        fclose(in);
        return 0;
    }

    if (stat(dst, &st_dst) == 0 && S_ISREG(st_dst.st_mode) && st_dst.st_size == (off_t)target_len) {
        fclose(in);
        return 1;
    }

    unsigned char kbuf[2048];
    long klen = 0;
    if (is_cloanto) {
        ps3_load_rom_dir_if_needed();
        char custom_key1[512];
        char custom_key2[512];
        snprintf(custom_key1, sizeof(custom_key1), "%s/rom.key", s_lib_rom_dir);
        snprintf(custom_key2, sizeof(custom_key2), "%s/kickstarts/rom.key", s_lib_rom_dir);
        const char *key_dirs[] = {
            "/dev_hdd0/uae4all/kickstarts/rom.key",
            "/dev_hdd0/uae4all/kickstarts/rom.keys",
            custom_key1,
            custom_key2,
            "./kickstarts/rom.key",
            "./kickstarts/rom.keys",
            "./roms/kickstarts/rom.key",
            "./data/kickstarts/rom.key",
            "kickstarts/rom.key",
            "rom.key",
            NULL
        };
        FILE *kf = NULL;
        for (int k = 0; key_dirs[k]; k++) {
            kf = fopen(key_dirs[k], "rb");
            if (kf) break;
        }
        if (kf) {
            fseek(kf, 0, SEEK_END);
            long kf_len = ftell(kf);
            fseek(kf, 0, SEEK_SET);
            if (kf_len > 0 && kf_len <= (long)sizeof(kbuf)) {
                if (fread(kbuf, 1, (size_t)kf_len, kf) == (size_t)kf_len)
                    klen = kf_len;
            }
            fclose(kf);
        }
    }

    char dst_copy[512];
    strncpy(dst_copy, dst, sizeof(dst_copy) - 1);
    dst_copy[sizeof(dst_copy) - 1] = '\0';
    char *slash = strrchr(dst_copy, '/');
    if (slash) {
        *slash = '\0';
        ensure_directory(dst_copy);
        *slash = '/';
    }

    remove(dst);
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return 0;
    }

    fseek(in, read_offset, SEEK_SET);
    unsigned char chunk[4096];
    long remaining = target_len;
    long pos = 0;
    long t = 0;
    int ok = 1;
    while (remaining > 0) {
        size_t to_read = (remaining > (long)sizeof(chunk)) ? sizeof(chunk) : (size_t)remaining;
        size_t got = fread(chunk, 1, to_read, in);
        if (got != to_read) {
            ok = 0;
            break;
        }
        if (is_cloanto && klen > 0) {
            for (size_t i = 0; i < got; i++, pos++, t = (t + 1) % klen) {
                chunk[i] ^= kbuf[t];
                if (target_len == pos + 1)
                    t = klen - 1;
            }
        }
        if (fwrite(chunk, 1, got, out) != got) {
            ok = 0;
            break;
        }
        remaining -= (long)got;
    }

    fflush(out);
    fclose(out);
    fclose(in);
    if (!ok)
        remove(dst);
    return ok;
}

static void deploy_kickstart_file(const char *destination_name, const char *const *source_names)
{
    char destination[512];
    snprintf(destination, sizeof(destination), "%s/Devs/Kickstarts/%s", PS3_WHDLOAD_ROOT, destination_name);
    struct stat st_dst;
    if (stat(destination, &st_dst) == 0 && S_ISREG(st_dst.st_mode) &&
        st_dst.st_size >= 250000 && st_dst.st_size <= 1100000) {
        return;
    }

    for (int i = 0; source_names[i]; i++) {
        char source[512];
        if (ps3_find_kickstart_file(source_names[i], source, sizeof(source))) {
            if (deploy_rom_clean(source, destination))
                return;
        }
    }
}

static void deploy_kickstart_aliases(void)
{
    static const char *kick12[] = { "kick33180.A500", "kick33180.a500", "kick12.rom", "amiga-os-120.rom", NULL };
    static const char *kick13[] = { "kick34005.A500", "kick34005.a500", "Kick34005.a500", "kick13.rom", "amiga-os-130.rom", NULL };
    static const char *kick20[] = { "kick37175.A500", "kick37175.a500", "kick20.rom", "amiga-os-204.rom", NULL };
    static const char *kick31_a1200[] = { "kick40068.A1200", "kick40068.a1200", "amiga-os-310-a1200.rom", "kick31.rom", "kick3.1.rom", NULL };
    static const char *kick31_a4000[] = { "kick40068.A4000", "kick40068.a4000", "amiga-os-310-a4000.rom", "kick31.rom", NULL };
    static const char *kick31_a600[] = { "kick40063.A600", "kick40063.a600", "amiga-os-310-a600.rom", "kick31.rom", NULL };
    static const char *kick205[] = { "kick37350.A600", "kick37350.a600", "kick205.rom", "kick600.rom", "amiga-os-205-a600.rom", NULL };
    static const char *cd32[] = { "kick40060.CD32", "kick40060.cd32", "amiga-os-310-cd32.rom", NULL };
    static const struct {
        const char *name;
        const char *const *sources;
    } files[] = {
        { "kick12.rom", kick12 }, { "kick33180.A500", kick12 },
        { "kick34005.A500", kick13 }, { "kick13.rom", kick13 },
        { "kick20.rom", kick20 }, { "kick37175.A500", kick20 },
        { "kick31.rom", kick31_a1200 }, { "kick40068.A1200", kick31_a1200 },
        { "kick40068.a1200", kick31_a1200 },
        { "kick40068.A4000", kick31_a4000 }, { "kick40068.a4000", kick31_a4000 },
        { "kick40063.A600", kick31_a600 }, { "kick40063.a600", kick31_a600 },
        { "kick37350.A600", kick205 }, { "kick205.rom", kick205 },
        { "kick40060.CD32", cd32 }, { "amiga-os-310-cd32.rom", cd32 },
        { NULL, NULL }
    };
    ensure_directory(PS3_WHDLOAD_ROOT "/Devs/Kickstarts");
    for (int i = 0; files[i].name; i++)
        deploy_kickstart_file(files[i].name, files[i].sources);

    ps3_load_rom_dir_if_needed();
    char custom_dir1[512];
    snprintf(custom_dir1, sizeof(custom_dir1), "%s/kickstarts", s_lib_rom_dir);
    char bundled_kicks[512];
    snprintf(bundled_kicks, sizeof(bundled_kicks), "%s/kickstarts", ps3_base_dir());

    struct stat usb_st;
    int usb0_ok = (stat("/dev_usb000", &usb_st) == 0);
    int usb1_ok = (stat("/dev_usb001", &usb_st) == 0);

    const char *search_dirs[] = {
        bundled_kicks,
        "/dev_hdd0/uae4all/kickstarts",
        "/dev_hdd0/uae4all/kickstarts/kickstarts",
        "/dev_hdd0/game/UAEH00001/USRDIR/kickstarts",
        custom_dir1,
        "./kickstarts",
        "./roms/kickstarts",
        "./data/kickstarts",
        "kickstarts",
        "roms/kickstarts",
        "/dev_usb000/uae4all/kickstarts",
        "/dev_usb000/kickstarts",
        "/dev_usb001/uae4all/kickstarts",
        "/dev_usb001/kickstarts",
        NULL
    };
    for (int d = 0; search_dirs[d]; d++) {
        if (strncmp(search_dirs[d], "/dev_usb000", 11) == 0 && !usb0_ok) continue;
        if (strncmp(search_dirs[d], "/dev_usb001", 11) == 0 && !usb1_ok) continue;
        DIR *kdir = opendir(search_dirs[d]);
        if (kdir) {
            struct dirent *entry;
            while ((entry = readdir(kdir)) != NULL) {
                if (entry->d_name[0] == '.')
                    continue;
                const char *ext = strrchr(entry->d_name, '.');
                if ((ext && (strcasecmp(ext, ".rtb") == 0 || strcasecmp(ext, ".pat") == 0 || strcasecmp(ext, ".key") == 0 || strcasecmp(ext, ".keys") == 0)) ||
                    strcasecmp(entry->d_name, "rom.key") == 0 || strcasecmp(entry->d_name, "rom.keys") == 0) {
                    char src_path[512];
                    char dst_path[512];
                    snprintf(src_path, sizeof(src_path), "%s/%s", search_dirs[d], entry->d_name);
                    snprintf(dst_path, sizeof(dst_path), "%s/Devs/Kickstarts/%s", PS3_WHDLOAD_ROOT, entry->d_name);
                    deploy_file_if_missing(src_path, dst_path);
                }
            }
            closedir(kdir);
        }
    }
}

static void deploy_whdload_file(const char *rel_subpath, const char *dst)
{
    char base_cand[512];
    snprintf(base_cand, sizeof(base_cand), "%s/data/whdload_base", ps3_base_dir());
    const char *prefixes[] = {
        base_cand,
        "./data/whdload_base",
        "data/whdload_base",
        "/dev_hdd0/game/UAEH00001/USRDIR/data/whdload_base",
        "/dev_hdd0/game/UAEH00001/USRDIR/data/whdload_base/whdload_base",
        "/dev_hdd0/uae4all/data/whdload_base",
        "./switchdata/data/whdload_base",
        NULL
    };
    for (int i = 0; prefixes[i]; i++) {
        char src[512];
        snprintf(src, sizeof(src), "%s/%s", prefixes[i], rel_subpath);
        struct stat st;
        if (stat(src, &st) == 0 && st.st_size > 0) {
            deploy_file_if_missing(src, dst);
            return;
        }
    }
}

static void deploy_whdload_dir(const char *rel_subpath, const char *dst_dir)
{
    char base_cand[512];
    snprintf(base_cand, sizeof(base_cand), "%s/data/whdload_base", ps3_base_dir());
    const char *prefixes[] = {
        base_cand,
        "./data/whdload_base",
        "data/whdload_base",
        "/dev_hdd0/game/UAEH00001/USRDIR/data/whdload_base",
        "/dev_hdd0/game/UAEH00001/USRDIR/data/whdload_base/whdload_base",
        "/dev_hdd0/uae4all/data/whdload_base",
        "./switchdata/data/whdload_base",
        NULL
    };
    for (int i = 0; prefixes[i]; i++) {
        char src_dir[512];
        snprintf(src_dir, sizeof(src_dir), "%s/%s", prefixes[i], rel_subpath);
        DIR *dir = opendir(src_dir);
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.')
                    continue;
                char src_file[512];
                char dst_file[512];
                snprintf(src_file, sizeof(src_file), "%s/%s", src_dir, entry->d_name);
                snprintf(dst_file, sizeof(dst_file), "%s/%s", dst_dir, entry->d_name);
                deploy_file_if_missing(src_file, dst_file);
            }
            closedir(dir);
            return;
        }
    }
}

static int s_whdload_base_deployed = 0;
static void deploy_whdload_base(void)
{
    if (s_whdload_base_deployed)
        return;

    char whd_check[512];
    struct stat st_chk;
    snprintf(whd_check, sizeof(whd_check), "%s/C/WHDLoad", PS3_WHDLOAD_ROOT);
    if (stat(whd_check, &st_chk) == 0 && st_chk.st_size > 0) {
        s_whdload_base_deployed = 1;
        return;
    }

    ensure_directory(PS3_WHDLOAD_ROOT);
    ensure_directory(PS3_WHDLOAD_ROOT "/C");
    ensure_directory(PS3_WHDLOAD_ROOT "/S");
    ensure_directory(PS3_WHDLOAD_ROOT "/L");
    ensure_directory(PS3_WHDLOAD_ROOT "/Libs");
    ensure_directory(PS3_WHDLOAD_ROOT "/Fonts");
    ensure_directory(PS3_WHDLOAD_ROOT "/T");
    ensure_directory(PS3_WHDLOAD_ROOT "/ENV");
    ensure_directory(PS3_WHDLOAD_ROOT "/Devs");
    ensure_directory(PS3_WHDLOAD_ROOT "/Devs/Kickstarts");
    deploy_whdload_dir("Devs/Kickstarts", PS3_WHDLOAD_ROOT "/Devs/Kickstarts");
    deploy_kickstart_aliases();
    deploy_whdload_file("C/WHDLoad", PS3_WHDLOAD_ROOT "/C/WHDLoad");
    deploy_whdload_file("C/WHDLoadCD32", PS3_WHDLOAD_ROOT "/C/WHDLoadCD32");
    deploy_whdload_file("C/DIC", PS3_WHDLOAD_ROOT "/C/DIC");
    deploy_whdload_file("C/Patcher", PS3_WHDLOAD_ROOT "/C/Patcher");
    deploy_whdload_file("C/RawDIC", PS3_WHDLOAD_ROOT "/C/RawDIC");
    deploy_whdload_file("C/WArc", PS3_WHDLOAD_ROOT "/C/WArc");
    deploy_whdload_file("C/WHDLoad.VFS", PS3_WHDLOAD_ROOT "/C/WHDLoad.VFS");
    deploy_whdload_file("S/WHDLoad.prefs", PS3_WHDLOAD_ROOT "/S/WHDLoad.prefs");
    deploy_whdload_file("S/WHDLoad-Startup", PS3_WHDLOAD_ROOT "/S/WHDLoad-Startup");
    deploy_whdload_file("S/WHDLoad-Cleanup", PS3_WHDLOAD_ROOT "/S/WHDLoad-Cleanup");
    s_whdload_base_deployed = 1;
}

int ps3_whdload_can_launch(const char *game_name)
{
    if (!game_name || game_name[0] == '\0' || strchr(game_name, '/') || strchr(game_name, '\\') || strchr(game_name, ':'))
        return 0;

    char game_root[512];
    char slave_relative[384];
    snprintf(game_root, sizeof(game_root), "%s/%s", PS3_WHDLOAD_ROOT, game_name);
    int is_dir = 0;
    if (!path_exists(game_root, &is_dir) || !is_dir)
        return 0;
    return find_slave_recursive(game_root, "", slave_relative, sizeof(slave_relative), 0);
}

int ps3_whdload_prepare_launch(const char *game_name)
{
    s_last_error[0] = '\0';

    if (!game_name || game_name[0] == '\0' || strchr(game_name, '/') || strchr(game_name, '\\') || strchr(game_name, ':')) {
        snprintf(s_last_error, sizeof(s_last_error), "Invalid WHDLoad game name");
        write_log("[PS3] whdload_prepare_launch: invalid game name '%s'\n", game_name ? game_name : "(null)");
        return 0;
    }

    deploy_whdload_base();
    write_log("[PS3] whdload_prepare_launch: step 1 base ok\n");

    {
        char whdload_exe[512];
        struct stat exe_st;
        snprintf(whdload_exe, sizeof(whdload_exe), "%s/C/WHDLoad", PS3_WHDLOAD_ROOT);
        if (stat(whdload_exe, &exe_st) != 0 || !S_ISREG(exe_st.st_mode) || exe_st.st_size <= 0) {
            snprintf(s_last_error, sizeof(s_last_error),
                     "WHDLoad is not available: %s is missing or empty (the data/whdload_base payload was not deployed)",
                     whdload_exe);
            write_log("[PS3] whdload_prepare_launch: %s\n", s_last_error);
            return 0;
        }
    }
    write_log("[PS3] whdload_prepare_launch: step 2 exe ok\n");

    char game_root[512];
    char slave_relative[384];
    snprintf(game_root, sizeof(game_root), "%s/%s", PS3_WHDLOAD_ROOT, game_name);
    int is_dir = 0;
    if (!path_exists(game_root, &is_dir) || !is_dir) {
        snprintf(s_last_error, sizeof(s_last_error), "Game directory not found: %s", game_root);
        write_log("[PS3] whdload_prepare_launch: %s\n", s_last_error);
        return 0;
    }
    write_log("[PS3] whdload_prepare_launch: step 3 dir ok\n");
    if (!find_slave_recursive(game_root, "", slave_relative, sizeof(slave_relative), 0)) {
        snprintf(s_last_error, sizeof(s_last_error), "No .slave file found in %s", game_root);
        write_log("[PS3] whdload_prepare_launch: %s\n", s_last_error);
        return 0;
    }
    write_log("[PS3] whdload_prepare_launch: step 4 slave ok (%s)\n", slave_relative);

    char startup_dir[512];
    char startup_path[512];
    char backup_path[512];
    snprintf(startup_dir, sizeof(startup_dir), "%s/S", PS3_WHDLOAD_ROOT);
    snprintf(startup_path, sizeof(startup_path), "%s/Startup-Sequence", startup_dir);
    snprintf(backup_path, sizeof(backup_path), "%s/Startup-Sequence.uae4all", startup_dir);
    ensure_directory(startup_dir);
    write_log("[PS3] whdload_prepare_launch: step 5 startup dir ok\n");

    int startup_exists = path_exists(startup_path, NULL);
    write_log("[PS3] whdload_prepare_launch: step 6 startup exists=%d\n", startup_exists);
    static char original_startup[16384];
    static char updated_startup[16384];
    memset(original_startup, 0, sizeof(original_startup));
    memset(updated_startup, 0, sizeof(updated_startup));
    write_log("[PS3] whdload_prepare_launch: step 7 buffers clear\n");

    int has_real_workbench = 0;
    if (path_exists(backup_path, NULL)) {
        int rc = read_text_file(backup_path, original_startup, sizeof(original_startup));
        write_log("[PS3] whdload_prepare_launch: step 8 backup read rc=%d len=%u\n",
                  rc, (unsigned)strlen(original_startup));
        strip_whdload_lines(original_startup);
        write_log("[PS3] whdload_prepare_launch: step 9 backup stripped len=%u\n",
                  (unsigned)strlen(original_startup));
        if (original_startup[0] && strstr(original_startup, "SetPatch") != NULL) {
            has_real_workbench = 1;
        } else {
            remove(backup_path);
            original_startup[0] = '\0';
        }
    } else if (startup_exists) {
        int rc = read_text_file(startup_path, original_startup, sizeof(original_startup));
        write_log("[PS3] whdload_prepare_launch: step 8 startup read rc=%d len=%u\n",
                  rc, (unsigned)strlen(original_startup));
        strip_whdload_lines(original_startup);
        write_log("[PS3] whdload_prepare_launch: step 9 startup stripped len=%u\n",
                  (unsigned)strlen(original_startup));
        if (original_startup[0] && strstr(original_startup, "SetPatch") != NULL) {
            write_text_file(backup_path, original_startup);
            has_real_workbench = 1;
        } else {
            original_startup[0] = '\0';
        }
    }

    char amiga_dir[512];
    char slave_file[128];
    const char *last_slash = strrchr(slave_relative, '/');
    if (last_slash) {
        char sub_dir[384];
        size_t dlen = (size_t)(last_slash - slave_relative);
        strncpy(sub_dir, slave_relative, dlen);
        sub_dir[dlen] = '\0';
        snprintf(amiga_dir, sizeof(amiga_dir), "DH0:%s/%s", game_name, sub_dir);
        strncpy(slave_file, last_slash + 1, sizeof(slave_file) - 1);
        slave_file[sizeof(slave_file) - 1] = '\0';
    } else {
        snprintf(amiga_dir, sizeof(amiga_dir), "DH0:%s", game_name);
        strncpy(slave_file, slave_relative, sizeof(slave_file) - 1);
        slave_file[sizeof(slave_file) - 1] = '\0';
    }

    char launch_line[640];
    if (mainMenu_whdload_args[0]) {
        snprintf(launch_line, sizeof(launch_line), "CD \"%s\"\nC:WHDLoad \"%s\" %s PRELOAD\n", amiga_dir, slave_file, mainMenu_whdload_args);
    } else {
        snprintf(launch_line, sizeof(launch_line), "CD \"%s\"\nC:WHDLoad \"%s\" PRELOAD\n", amiga_dir, slave_file);
    }
    write_log("[PS3] whdload_prepare_launch: step 10 launch line ready (%u bytes, args='%s', workbench=%d)\n",
              (unsigned)strlen(launch_line), mainMenu_whdload_args, has_real_workbench);

    if (has_real_workbench) {
        char *endcli = find_endcli(original_startup);
        if (endcli) {
            size_t prefix_length = (size_t)(endcli - original_startup);
            if (prefix_length + strlen(launch_line) + strlen(endcli) + 1 < sizeof(updated_startup)) {
                memcpy(updated_startup, original_startup, prefix_length);
                memcpy(updated_startup + prefix_length, launch_line, strlen(launch_line));
                strcpy(updated_startup + prefix_length + strlen(launch_line), endcli);
            } else {
                snprintf(updated_startup, sizeof(updated_startup), "%s", launch_line);
            }
        } else {
            strncpy(updated_startup, original_startup, sizeof(updated_startup) - 1);
            size_t ulen = strlen(updated_startup);
            if (ulen > 0 && updated_startup[ulen - 1] != '\n' && ulen + 2 < sizeof(updated_startup)) {
                strcat(updated_startup, "\n");
            }
            if ( strlen(updated_startup) + strlen(launch_line) + 1 < sizeof(updated_startup) )
                strcat(updated_startup, launch_line);
        }
    } else {
        snprintf(updated_startup, sizeof(updated_startup), "%s", launch_line);
    }

    write_log("[PS3] whdload_prepare_launch: step 11 writing startup (%u bytes)\n",
              (unsigned)strlen(updated_startup));
    remove(startup_path);
    write_log("[PS3] whdload_prepare_launch: step 11b old startup removed\n");
    int ok = write_text_file(startup_path, updated_startup);
    write_log("[PS3] whdload_prepare_launch: game='%s', slave='%s', path='%s', ok=%d\n",
              game_name, slave_file, startup_path, ok);

    {
        struct stat startup_st;
        if (!ok || stat(startup_path, &startup_st) != 0 || !S_ISREG(startup_st.st_mode) || startup_st.st_size <= 0) {
            snprintf(s_last_error, sizeof(s_last_error),
                     "Could not write the WHDLoad startup sequence: %s", startup_path);
            write_log("[PS3] whdload_prepare_launch: %s\n", s_last_error);
            return 0;
        }
    }

    write_log("[PS3] whdload startup sequence %s (amiga_dir='%s', slave='%s'):\n%s\n",
              startup_path, amiga_dir, slave_file, updated_startup);

    return ok;
}

static int compare_game_names(const void *a, const void *b)
{
    int res = strcasecmp((const char *)a, (const char *)b);
    if (res != 0)
        return res;
    return strcmp((const char *)a, (const char *)b);
}

int ps3_whdload_list(char names[][128], int max_names)
{
    if (!names || max_names <= 0)
        return 0;

    ensure_directory(PS3_WHDLOAD_ROOT);
    DIR *dir = opendir(PS3_WHDLOAD_ROOT);
    if (!dir)
        return 0;

    int count = 0;
    struct dirent *entry;
    while (count < max_names && (entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        char full[512];
        snprintf(full, sizeof(full), "%s/%s", PS3_WHDLOAD_ROOT, entry->d_name);
        struct stat st;
        if (stat(full, &st) != 0 || !S_ISDIR(st.st_mode))
            continue;

        if (strcasecmp(entry->d_name, "S") == 0 ||
            strcasecmp(entry->d_name, "C") == 0 ||
            strcasecmp(entry->d_name, "Libs") == 0 ||
            strcasecmp(entry->d_name, "Devs") == 0 ||
            strcasecmp(entry->d_name, "ENV") == 0 ||
            strcasecmp(entry->d_name, "T") == 0 ||
            strcasecmp(entry->d_name, "Rexx") == 0 ||
            strcasecmp(entry->d_name, "Utilities") == 0 ||
            strcasecmp(entry->d_name, "Prefs") == 0 ||
            strcasecmp(entry->d_name, "System") == 0 ||
            strcasecmp(entry->d_name, "Trashcan") == 0 ||
            strcasecmp(entry->d_name, "L") == 0 ||
            strcasecmp(entry->d_name, "Fonts") == 0 ||
            strcasecmp(entry->d_name, "Locale") == 0 ||
            strcasecmp(entry->d_name, "EBOOT.BIN") == 0 ||
            strcasecmp(entry->d_name, "PARAM.SFO") == 0 ||
            strcasecmp(entry->d_name, "conf") == 0 ||
            strcasecmp(entry->d_name, "saves") == 0 ||
            strcasecmp(entry->d_name, "save") == 0 ||
            strcasecmp(entry->d_name, "covers") == 0)
            continue;

        strncpy(names[count], entry->d_name, 127);
        names[count][127] = '\0';
        count++;
    }
    closedir(dir);
    if (count > 1) {
        qsort(names, count, sizeof(names[0]), compare_game_names);
    }
    return count;
}

typedef struct {
    char name[128];
    int is_aga;
} Ps3AgaCacheEntry;

static Ps3AgaCacheEntry s_ps3_aga_cache[512];
static int s_ps3_aga_cache_count = 0;

static int detect_whdload_is_aga_uncached(const char *game_name)
{
    char lower_name[256];
    size_t i = 0;
    for (; game_name[i] && i < sizeof(lower_name) - 1; i++) {
        lower_name[i] = (char)tolower((unsigned char)game_name[i]);
    }
    lower_name[i] = '\0';

    if (strstr(lower_name, "cd32") != NULL)
        return 1;

    const char *p = lower_name;
    while ((p = strstr(p, "aga")) != NULL) {
        bool left_ok = (p == lower_name) || (!isalnum((unsigned char)*(p - 1)));
        bool right_ok = (!isalnum((unsigned char)*(p + 3)));
        if (left_ok && right_ok)
            return 1;
        p += 3;
    }

    char game_root[512];
    char slave_relative[384];
    snprintf(game_root, sizeof(game_root), "%s/%s", PS3_WHDLOAD_ROOT, game_name);
    if (!find_slave_recursive(game_root, "", slave_relative, sizeof(slave_relative), 0))
        return 0;

    char full_slave[512];
    snprintf(full_slave, sizeof(full_slave), "%s/%s", game_root, slave_relative);
    FILE *f = fopen(full_slave, "rb");
    if (!f)
        return 0;

    unsigned char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    if (n >= 16) {
        for (size_t k = 0; k + 12 <= n; k++) {
            if (memcmp(buf + k, "WHDLOADS", 8) == 0) {
                unsigned short flags = (unsigned short)((buf[k + 10] << 8) | buf[k + 11]);
                if (flags & 0x0018)
                    return 1;
                return 0;
            }
            if (memcmp(buf + k, "SLAV", 4) == 0) {
                unsigned short flags = (unsigned short)((buf[k + 6] << 8) | buf[k + 7]);
                if (flags & 0x0018)
                    return 1;
                return 0;
            }
        }
    }

    return 0;
}

int ps3_whdload_is_aga(const char *game_name)
{
    if (!game_name || game_name[0] == '\0')
        return 0;

    for (int c = 0; c < s_ps3_aga_cache_count; c++) {
        if (strcmp(s_ps3_aga_cache[c].name, game_name) == 0)
            return s_ps3_aga_cache[c].is_aga;
    }

    int res = detect_whdload_is_aga_uncached(game_name);

    if (s_ps3_aga_cache_count < 512) {
        strncpy(s_ps3_aga_cache[s_ps3_aga_cache_count].name, game_name, 127);
        s_ps3_aga_cache[s_ps3_aga_cache_count].name[127] = '\0';
        s_ps3_aga_cache[s_ps3_aga_cache_count].is_aga = res;
        s_ps3_aga_cache_count++;
    }

    return res;
}

int ps3_whdload_needs_immediate_blitter(const char *game_name)
{
    if (!game_name || game_name[0] == '\0')
        return 0;

    char lower_name[256];
    size_t i = 0;
    for (; game_name[i] && i < sizeof(lower_name) - 1; i++) {
        lower_name[i] = (char)tolower((unsigned char)game_name[i]);
    }
    lower_name[i] = '\0';

    if (strstr(lower_name, "shadowdancer") != NULL ||
        strstr(lower_name, "shadow dancer") != NULL ||
        strstr(lower_name, "shadow_dancer") != NULL)
        return 1;

    return 0;
}

int ps3_whdload_needs_improved_blitter(const char *game_name)
{
    if (!game_name || game_name[0] == '\0')
        return 0;

    char lower_name[256];
    size_t i = 0;
    for (; game_name[i] && i < sizeof(lower_name) - 1; i++) {
        lower_name[i] = (char)tolower((unsigned char)game_name[i]);
    }
    lower_name[i] = '\0';

    return strstr(lower_name, "turrican") != NULL;
}

#endif
