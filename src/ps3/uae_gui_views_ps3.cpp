#ifdef __PS3__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "uae.h"
#include "sound.h"
#include "disk.h"
#include "memory-uae.h"
#include "custom.h"
#include "xwin.h"
#include "drawing.h"
#include "menu.h"
#include "menu_config.h"
#include "savestate.h"
#include "gui.h"
#include "cfgfile.h"
#include "cdrom.h"
#include "libchdr/chd.h"
#include "libchdr/cdrom.h"
#include "hdf_manager_ps3.h"
#include "midi_synth.h"
#include "autoconf.h"
#include "zfile.h"

#include <SDL_image.h>
#include <sys/socket.h>
#include <netinet/in.h>


#include "uae_gui_ps3.h"
#include "whdload_manager_ps3.h"
#include "m3u_manager.h"
#include "sdl2_to_sdl1.h"
#include "ps3_diag.h"
#include "ps3_paths.h"
#include "cover_manager_ps3.h"
#include "game_metadata_ps3.h"
/* Reported by the PS3 stall watchdog (see ps3_diag.c). The watchdog runs on
 * its own thread, so the answer is copied into a private buffer instead of
 * handing out a pointer the GUI may be rewriting at the same moment. */
const char *ps3_diag_game_name(void)
{
    static char name[128];
    const char *src = NULL;

    if (mainMenu_whdload_game[0] != '\0')
        src = mainMenu_whdload_game;
    else if (uae4all_image_file0[0] != '\0')
        src = uae4all_image_file0;

    if (src) {
        strncpy(name, src, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    } else {
        strcpy(name, "-");
    }
    return name;
}

static const int s_shader_order[] = { 0, 8, 2, 3, 4, 5, 9 };
#define S_SHADER_ORDER_COUNT ((int)(sizeof(s_shader_order) / sizeof(s_shader_order[0])))

static const char *ps3_shader_label(int s) {
    switch (s) {
        case 0: return "None (Pixel Sharp)";
        case 1: return "Sharp Bilinear";
        case 2: return "Bilinear (Smooth)";
        case 3: return "Point (Nearest)";
        case 4: return "CRT-Lottes";
        case 5: return "CRT-Easymode";
        case 6: return "Scanlines (50%)";
        case 7: return "Scale2x";
        case 8: return "Pixel-Perfect Integer";
        case 9: return "Classic CRT";
        default: return "None (Pixel Sharp)";
    }
}
static int ps3_shader_cycle(int s, int dir) {
    int idx = 0;
    for (int i = 0; i < S_SHADER_ORDER_COUNT; i++) {
        if (s_shader_order[i] == s) {
            idx = i;
            break;
        }
    }
    int next = (idx + dir + S_SHADER_ORDER_COUNT) % S_SHADER_ORDER_COUNT;
    return s_shader_order[next];
}
#include <sys/socket.h>
#include <sys/select.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <net/netctl.h>
#include "ps3_compat.h"

static char s_ps3_ftp_ip[64] = "127.0.0.1";
static int s_ps3_ftp_port = 21;
static volatile int s_ps3_ftp_running = 0;

void ps3_library_mark_dirty(void);
static void ps3_unmount_cd32(void);
static void ps3_reset_media_state(void);
static void ps3_apply_floppy_model(int model);
static int s_ps3_server_fd = -1;
static pthread_t s_ps3_ftp_thread;

#define MAX_FTP_CLIENTS 8
static int s_ps3_client_fds[MAX_FTP_CLIENTS] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static pthread_mutex_t s_ps3_clients_mutex = PTHREAD_MUTEX_INITIALIZER;

static void ps3_ftp_register_client(int fd) {
    pthread_mutex_lock(&s_ps3_clients_mutex);
    for (int i = 0; i < MAX_FTP_CLIENTS; i++) {
        if (s_ps3_client_fds[i] < 0) {
            s_ps3_client_fds[i] = fd;
            break;
        }
    }
    pthread_mutex_unlock(&s_ps3_clients_mutex);
}

static void ps3_ftp_unregister_client(int fd) {
    pthread_mutex_lock(&s_ps3_clients_mutex);
    for (int i = 0; i < MAX_FTP_CLIENTS; i++) {
        if (s_ps3_client_fds[i] == fd) {
            s_ps3_client_fds[i] = -1;
            break;
        }
    }
    pthread_mutex_unlock(&s_ps3_clients_mutex);
}

static int ps3_ftp_get_client_count(void) {
    int count = 0;
    pthread_mutex_lock(&s_ps3_clients_mutex);
    for (int i = 0; i < MAX_FTP_CLIENTS; i++) {
        if (s_ps3_client_fds[i] >= 0) count++;
    }
    pthread_mutex_unlock(&s_ps3_clients_mutex);
    return count;
}

static int s_ps3_netctl_logs = 0;
static int s_ps3_netctl_init_tried = 0;
static volatile int s_ftp_probe_done = 0;
static volatile int s_ftp_probe_ok = 0;
static int s_ftp_probe_attempts = 0;
static volatile int s_ftp_route_seen = 0;
static char s_ps3_ftp_manual_ip[40] = "";

static int ps3_ftp_ip_invalid(void) {
    return strcmp(s_ps3_ftp_ip, "127.0.0.1") == 0 || strcmp(s_ps3_ftp_ip, "0.0.0.0") == 0 ||
           s_ps3_ftp_ip[0] == '\0';
}

static int ps3_ftp_udp_probe_ip(void) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        if (s_ps3_netctl_logs < 8) {
            s_ps3_netctl_logs++;
            write_log("[PS3] ftp: udp probe socket failed (errno=%d)\n", errno);
        }
        return 0;
    }
    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(53);
    dst.sin_addr.s_addr = 0x08080808u;
    ps3_diag_net_checkpoint("ftp:ip:udp-connect");
    int crv = connect(fd, (struct sockaddr*)&dst, sizeof(dst));
    if (crv != 0) {
        if (s_ps3_netctl_logs < 8) {
            s_ps3_netctl_logs++;
            write_log("[PS3] ftp: udp probe connect rv=%d errno=%d\n", crv, errno);
        }
        close(fd);
        return 0;
    }
    s_ftp_route_seen = 1;
    struct sockaddr_in loc;
    socklen_t lsz = sizeof(loc);
    memset(&loc, 0, sizeof(loc));
    ps3_diag_net_checkpoint("ftp:ip:udp-sockname");
    int srv = getsockname(fd, (struct sockaddr*)&loc, &lsz);
    if (srv != 0 || loc.sin_addr.s_addr == 0) {
        if (s_ps3_netctl_logs < 8) {
            s_ps3_netctl_logs++;
            write_log("[PS3] ftp: udp probe sockname rv=%d errno=%d\n", srv, errno);
        }
        close(fd);
        return 0;
    }
    unsigned int a = ntohl(loc.sin_addr.s_addr);
    if (a == 0x7f000001u) {
        if (s_ps3_netctl_logs < 8) {
            s_ps3_netctl_logs++;
            write_log("[PS3] ftp: udp probe ip 127.0.0.1 (ignored)\n");
        }
        close(fd);
        return 0;
    }
    snprintf(s_ps3_ftp_ip, sizeof(s_ps3_ftp_ip), "%u.%u.%u.%u",
             (a >> 24) & 255u, (a >> 16) & 255u, (a >> 8) & 255u, a & 255u);
    write_log("[PS3] ftp: console IP %s (udp)\n", s_ps3_ftp_ip);
    ps3_diag_net_checkpoint("ftp:ip:udp-ok");
    close(fd);
    return 1;
}

static int ps3_ftp_refresh_ip(void) {
    if (!s_ps3_netctl_init_tried) {
        s_ps3_netctl_init_tried = 1;
        ps3_diag_net_checkpoint("ftp:ip:init");
        if (s_ps3_netctl_logs < 8) {
            s_ps3_netctl_logs++;
            write_log("[PS3] ftp: netctl init call\n");
        }
        s32 irc = netCtlInit();
        if (s_ps3_netctl_logs < 8) {
            s_ps3_netctl_logs++;
            write_log("[PS3] ftp: netctl init rv=%d (0x%08x)\n", irc, irc);
        }
    }
    ps3_diag_net_checkpoint("ftp:ip:state");
    if (s_ps3_netctl_logs < 8) {
        s_ps3_netctl_logs++;
        write_log("[PS3] ftp: netctl probe\n");
    }
    s32 nstate = 0;
    s32 rv = netCtlGetState(&nstate);
    if (rv != 0) {
        if (s_ps3_netctl_logs < 8) {
            s_ps3_netctl_logs++;
            write_log("[PS3] ftp: netctl state rv=%d (0x%08x)\n", rv, rv);
        }
        return 0;
    }
    if (s_ps3_netctl_logs < 8) {
        s_ps3_netctl_logs++;
        write_log("[PS3] ftp: netctl state=%d\n", (int)nstate);
    }
    if (nstate != NET_CTL_STATE_IPObtained)
        return 0;
    ps3_diag_net_checkpoint("ftp:ip:info");
    union net_ctl_info info;
    memset(&info, 0, sizeof(info));
    s32 irv = netCtlGetInfo(NET_CTL_INFO_IP_ADDRESS, &info);
    if (irv != 0) {
        if (s_ps3_netctl_logs < 8) {
            s_ps3_netctl_logs++;
            write_log("[PS3] ftp: netctl info rv=%d (0x%08x)\n", irv, irv);
        }
        return 0;
    }
    info.ip_address[NET_CTL_IPV4_ADDR_STR_LEN - 1] = '\0';
    if (info.ip_address[0] == '\0' ||
        strcmp(info.ip_address, "0.0.0.0") == 0 ||
        strcmp(info.ip_address, "127.0.0.1") == 0) {
        if (s_ps3_netctl_logs < 8) {
            s_ps3_netctl_logs++;
            write_log("[PS3] ftp: netctl ip %s (ignored)\n", info.ip_address);
        }
        return 0;
    }
    snprintf(s_ps3_ftp_ip, sizeof(s_ps3_ftp_ip), "%s", info.ip_address);
    write_log("[PS3] ftp: console IP %s (netctl)\n", s_ps3_ftp_ip);
    ps3_diag_net_checkpoint("ftp:ip:ok");
    return 1;
}

static void* ps3_ftp_probe_child(void *arg) {
    int udp_first = (int)(long)arg;
    if (!udp_first) {
        if (ps3_ftp_refresh_ip()) {
            s_ftp_probe_ok = 1;
            s_ftp_probe_done = 1;
            return NULL;
        }
    }
    if (ps3_ftp_udp_probe_ip()) {
        s_ftp_probe_ok = 1;
        s_ftp_probe_done = 1;
        return NULL;
    }
    if (udp_first) {
        if (ps3_ftp_refresh_ip()) {
            s_ftp_probe_ok = 1;
            s_ftp_probe_done = 1;
            return NULL;
        }
    }
    s_ftp_probe_done = 1;
    return NULL;
}

static void* ps3_ftp_probe_worker(void *arg) {
    (void)arg;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 65536);
    s_ftp_probe_attempts = 0;
    while (s_ps3_ftp_running && ps3_ftp_ip_invalid()) {
        if (s_ftp_probe_attempts >= 5) {
            if (s_ps3_netctl_logs < 10) {
                s_ps3_netctl_logs++;
                write_log("[PS3] ftp: ip probe give up after %d attempts\n", s_ftp_probe_attempts);
            }
            break;
        }
        s_ftp_probe_attempts++;
        if (s_ps3_netctl_logs < 8) {
            s_ps3_netctl_logs++;
            write_log("[PS3] ftp: ip probe attempt %d (udp_first=%d)\n",
                      s_ftp_probe_attempts, (s_ftp_probe_attempts & 1) == 0 ? 1 : 0);
        }
        int udp_first = (s_ftp_probe_attempts & 1) == 0;
        s_ftp_probe_done = 0;
        s_ftp_probe_ok = 0;
        pthread_t child;
        if (pthread_create(&child, &attr, ps3_ftp_probe_child, (void*)(long)udp_first) == 0) {
            pthread_detach(child);
        } else {
            if (s_ps3_netctl_logs < 8) {
                s_ps3_netctl_logs++;
                write_log("[PS3] ftp: ip probe child create failed (errno=%d)\n", errno);
            }
            s_ftp_probe_done = 1;
        }
        unsigned int waited = 0;
        while (s_ps3_ftp_running && !s_ftp_probe_done && waited < 2500u) {
            SDL_Delay(50);
            waited += 50;
        }
        if (!s_ftp_probe_done) {
            if (s_ps3_netctl_logs < 8) {
                s_ps3_netctl_logs++;
                write_log("[PS3] ftp: ip probe attempt %d timed out\n", s_ftp_probe_attempts);
            }
        } else if (s_ftp_probe_ok) {
            if (s_ps3_netctl_logs < 8) {
                s_ps3_netctl_logs++;
                write_log("[PS3] ftp: ip probe attempt %d ok\n", s_ftp_probe_attempts);
            }
            break;
        } else {
            if (s_ps3_netctl_logs < 8) {
                s_ps3_netctl_logs++;
                write_log("[PS3] ftp: ip probe attempt %d failed\n", s_ftp_probe_attempts);
            }
        }
        if (s_ps3_ftp_running && ps3_ftp_ip_invalid())
            SDL_Delay(1500);
    }
    write_log("[PS3] ftp: ip probe worker exit\n");
    pthread_attr_destroy(&attr);
    return NULL;
}

static void ftp_normalize_virtual_path(const char *v_cwd, const char *arg, char *out, size_t out_sz) {
    char combined[768];
    char normalized[768] = "/";
    const char *p;

    if (!arg || arg[0] == '\0') {
        snprintf(combined, sizeof(combined), "%s", v_cwd ? v_cwd : "/");
    } else if (arg[0] == '/') {
        snprintf(combined, sizeof(combined), "%s", arg);
    } else if (v_cwd && strcmp(v_cwd, "/") != 0) {
        snprintf(combined, sizeof(combined), "%s/%s", v_cwd, arg);
    } else {
        snprintf(combined, sizeof(combined), "/%s", arg);
    }

    p = combined;
    while (*p) {
        const char *segment;
        size_t segment_len;
        char *last_slash;
        size_t used;

        while (*p == '/') p++;
        if (*p == '\0') break;
        segment = p;
        while (*p && *p != '/') p++;
        segment_len = (size_t)(p - segment);

        if (segment_len == 1 && segment[0] == '.') continue;
        if (segment_len == 2 && segment[0] == '.' && segment[1] == '.') {
            last_slash = strrchr(normalized + 1, '/');
            if (last_slash)
                *last_slash = '\0';
            else
                normalized[1] = '\0';
            continue;
        }

        used = strlen(normalized);
        if (used > 1) {
            if (used + 1 + segment_len >= sizeof(normalized)) break;
            normalized[used++] = '/';
        } else {
            if (used + segment_len >= sizeof(normalized)) break;
        }
        memcpy(normalized + used, segment, segment_len);
        normalized[used + segment_len] = '\0';
    }

    snprintf(out, out_sz, "%s", normalized);
}

static void ftp_resolve_path(const char *v_cwd, const char *arg, char *out_fs, size_t out_sz) {
    char virtual_path[768];
    ftp_normalize_virtual_path(v_cwd, arg, virtual_path, sizeof(virtual_path));
    if (strcmp(virtual_path, "/sdmc") == 0 || strcmp(virtual_path, "/sdmc/") == 0) {
        snprintf(out_fs, out_sz, "/dev_hdd0/");
    } else if (strncmp(virtual_path, "/sdmc/", 6) == 0) {
        snprintf(out_fs, out_sz, "/dev_hdd0/%s", virtual_path + 6);
    } else if (strcmp(virtual_path, "/") == 0) {
        snprintf(out_fs, out_sz, "./");
    } else {
        snprintf(out_fs, out_sz, ".%s", virtual_path);
    }
}

static void ftp_ensure_dir(const char *path) {
    char buffer[512];
    strncpy(buffer, path, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    for (char *p = buffer; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    char *start = buffer;
    if (strncmp(buffer, "/dev_hdd0/", 10) == 0) {
        start = buffer + 10;
    } else if (strncmp(buffer, "./", 2) == 0) {
        start = buffer + 2;
    }
    for (char *p = start; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(buffer, 0777);
            *p = '/';
        }
    }
    mkdir(buffer, 0777);
}

static int ftp_accept_data(int *pasv_fd_ptr) {
    if (!pasv_fd_ptr || *pasv_fd_ptr < 0) return -1;
    struct sockaddr_in d_addr;
    socklen_t d_len = sizeof(d_addr);
    struct timeval tv;
    tv.tv_sec = 15;
    tv.tv_usec = 0;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(*pasv_fd_ptr, &rfds);
    int s = select(*pasv_fd_ptr + 1, &rfds, NULL, NULL, &tv);
    if (s <= 0) {
        close(*pasv_fd_ptr);
        *pasv_fd_ptr = -1;
        return -1;
    }
    int data_client = accept(*pasv_fd_ptr, (struct sockaddr*)&d_addr, &d_len);
    close(*pasv_fd_ptr);
    *pasv_fd_ptr = -1;
    if (data_client >= 0) {
        struct timeval dtv;
        dtv.tv_sec = 15;
        dtv.tv_usec = 0;
        setsockopt(data_client, SOL_SOCKET, SO_RCVTIMEO, &dtv, sizeof(dtv));
        setsockopt(data_client, SOL_SOCKET, SO_SNDTIMEO, &dtv, sizeof(dtv));
    }
    return data_client;
}

struct FtpClientContext {
    int client_fd;
};

static void* ps3_ftp_client_session(void *arg) {
    FtpClientContext *ctx = (FtpClientContext*)arg;
    int client_fd = ctx->client_fd;
    free(ctx);
    ps3_ftp_register_client(client_fd);

    struct sockaddr_in client_loc;
    socklen_t loc_len = sizeof(client_loc);
    if (getsockname(client_fd, (struct sockaddr*)&client_loc, &loc_len) == 0 && client_loc.sin_addr.s_addr != 0) {
        if (client_loc.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
            char *conn_ip = inet_ntoa(client_loc.sin_addr);
            if (conn_ip && strcmp(conn_ip, "0.0.0.0") != 0 && strcmp(conn_ip, "127.0.0.1") != 0) {
                snprintf(s_ps3_ftp_ip, sizeof(s_ps3_ftp_ip), "%s", conn_ip);
            }
        }
    }

    struct timeval ctv;
    ctv.tv_sec = 60;
    ctv.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &ctv, sizeof(ctv));

    const char *welcome = "220 UAE4All2 HD PS3 FTP Server Ready\r\n";
    send(client_fd, welcome, strlen(welcome), 0);

    char buf[1024];
    char pending[4096];
    size_t pending_len = 0;
    int pasv_fd = -1;
    char v_cwd[512] = "/";
    char rnfr_path[768] = "";
    off_t rest_offset = 0;

    while (s_ps3_ftp_running) {
        size_t line_len = 0;
        bool have_line = false;
        bool disconnected = false;

        while (!have_line) {
            char *newline = (char*)memchr(pending, '\n', pending_len);
            if (newline) {
                line_len = (size_t)(newline - pending) + 1;
                have_line = true;
                break;
            }
            if (pending_len >= sizeof(pending) - 1) {
                const char *err = "500 Command line too long.\r\n";
                send(client_fd, err, strlen(err), 0);
                pending_len = 0;
                continue;
            }
            int len = recv(client_fd, pending + pending_len, sizeof(pending) - pending_len - 1, 0);
            if (len == 0) {
                disconnected = true;
                break;
            }
            if (len < 0) {
                if (s_ps3_ftp_running && (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT)) {
                    continue;
                }
                pending_len = 0;
                disconnected = true;
                break;
            }
            pending_len += (size_t)len;
            pending[pending_len] = '\0';
        }
        if (!have_line) {
            if (!s_ps3_ftp_running || disconnected) break;
            continue;
        }
        if (line_len >= sizeof(buf)) {
            const char *err = "500 Command line too long.\r\n";
            send(client_fd, err, strlen(err), 0);
        } else {
            memcpy(buf, pending, line_len);
            buf[line_len] = '\0';
        }
        memmove(pending, pending + line_len, pending_len - line_len);
        pending_len -= line_len;
        if (line_len >= sizeof(buf)) continue;

        char *p = buf;
        while (*p == ' ' || *p == '\t') p++;
        char *cmd = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;
        if (*p) {
            *p++ = '\0';
            while (*p == ' ' || *p == '\t') p++;
        }
        char *arg_val = p;
        char *end = arg_val + strlen(arg_val);
        while (end > arg_val && (*(end-1) == '\r' || *(end-1) == '\n' || *(end-1) == ' ')) {
            *(--end) = '\0';
        }
        if (arg_val[0] == '\0') arg_val = NULL;
        if (arg_val && arg_val[0] == '"') {
            arg_val++;
            char *q = strrchr(arg_val, '"');
            if (q) *q = '\0';
        }
        if (arg_val && arg_val[0] == '\0') arg_val = NULL;
        if (cmd[0] == '\0') continue;

        if (strcasecmp(cmd, "USER") == 0 || strcasecmp(cmd, "PASS") == 0) {
            const char *res = "230 User logged in.\r\n";
            send(client_fd, res, strlen(res), 0);
        } else if (strcasecmp(cmd, "SYST") == 0) {
            const char *res = "215 UNIX Type: L8\r\n";
            send(client_fd, res, strlen(res), 0);
        } else if (strcasecmp(cmd, "FEAT") == 0) {
            const char *res = "211-Features:\r\n PASV\r\n EPSV\r\n UTF8\r\n SIZE\r\n MDTM\r\n MLST type*;size*;modify*;\r\n REST STREAM\r\n211 End\r\n";
            send(client_fd, res, strlen(res), 0);
        } else if (strcasecmp(cmd, "OPTS") == 0) {
            const char *res = "200 OPTS command successful.\r\n";
            send(client_fd, res, strlen(res), 0);
        } else if (strcasecmp(cmd, "NOOP") == 0) {
            const char *res = "200 OK.\r\n";
            send(client_fd, res, strlen(res), 0);
        } else if (strcasecmp(cmd, "PWD") == 0 || strcasecmp(cmd, "XPWD") == 0) {
            char res[256];
            snprintf(res, sizeof(res), "257 \"%s\" is current directory\r\n", v_cwd);
            send(client_fd, res, strlen(res), 0);
        } else if (strcasecmp(cmd, "TYPE") == 0) {
            const char *res = "200 Type set to I.\r\n";
            send(client_fd, res, strlen(res), 0);
        } else if (strcasecmp(cmd, "MODE") == 0) {
            const char *res = "200 Mode set to S.\r\n";
            send(client_fd, res, strlen(res), 0);
        } else if (strcasecmp(cmd, "STRU") == 0) {
            const char *res = "200 Structure set to F.\r\n";
            send(client_fd, res, strlen(res), 0);
        } else if (strcasecmp(cmd, "ABOR") == 0) {
            const char *res = "226 ABOR command successful.\r\n";
            send(client_fd, res, strlen(res), 0);
        } else if (strcasecmp(cmd, "ALLO") == 0) {
            const char *res = "200 OK.\r\n";
            send(client_fd, res, strlen(res), 0);
        } else if (strcasecmp(cmd, "STAT") == 0) {
            const char *res = "211 UAE4All2 PS3 FTP Server status OK.\r\n";
            send(client_fd, res, strlen(res), 0);
        } else if (strcasecmp(cmd, "SITE") == 0) {
            const char *res = "200 Command OK.\r\n";
            send(client_fd, res, strlen(res), 0);
        } else if (strcasecmp(cmd, "REST") == 0) {
            if (arg_val) {
                rest_offset = (off_t)strtoll(arg_val, NULL, 10);
                if (rest_offset < 0) rest_offset = 0;
            } else {
                rest_offset = 0;
            }
            char res[128];
            snprintf(res, sizeof(res), "350 Restart position accepted (%lld).\r\n", (long long)rest_offset);
            send(client_fd, res, strlen(res), 0);
        } else if (strcasecmp(cmd, "PASV") == 0) {
            if (pasv_fd >= 0) close(pasv_fd);
            pasv_fd = socket(AF_INET, SOCK_STREAM, 0);
            int opt = 1;
            setsockopt(pasv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            struct sockaddr_in p_addr;
            memset(&p_addr, 0, sizeof(p_addr));
            p_addr.sin_family = AF_INET;
            p_addr.sin_addr.s_addr = INADDR_ANY;
            p_addr.sin_port = 0;
            bind(pasv_fd, (struct sockaddr*)&p_addr, sizeof(p_addr));
            listen(pasv_fd, 1);

            socklen_t addr_len = sizeof(p_addr);
            getsockname(pasv_fd, (struct sockaddr*)&p_addr, &addr_len);
            int assigned_port = ntohs(p_addr.sin_port);

            unsigned int ip1 = 127, ip2 = 0, ip3 = 0, ip4 = 1;
            struct sockaddr_in cl_loc;
            socklen_t cl_len = sizeof(cl_loc);
            if (getsockname(client_fd, (struct sockaddr*)&cl_loc, &cl_len) == 0 && cl_loc.sin_addr.s_addr != 0) {
                unsigned char *ip_bytes = (unsigned char*)&cl_loc.sin_addr.s_addr;
                ip1 = ip_bytes[0];
                ip2 = ip_bytes[1];
                ip3 = ip_bytes[2];
                ip4 = ip_bytes[3];
            } else {
                sscanf(s_ps3_ftp_ip, "%u.%u.%u.%u", &ip1, &ip2, &ip3, &ip4);
            }
            char pasv_res[128];
            snprintf(pasv_res, sizeof(pasv_res), "227 Entering Passive Mode (%u,%u,%u,%u,%d,%d)\r\n",
                     ip1, ip2, ip3, ip4, assigned_port >> 8, assigned_port & 0xFF);
            send(client_fd, pasv_res, strlen(pasv_res), 0);
        } else if (strcasecmp(cmd, "EPSV") == 0) {
            if (pasv_fd >= 0) close(pasv_fd);
            pasv_fd = socket(AF_INET, SOCK_STREAM, 0);
            int opt = 1;
            setsockopt(pasv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            struct sockaddr_in p_addr;
            memset(&p_addr, 0, sizeof(p_addr));
            p_addr.sin_family = AF_INET;
            p_addr.sin_addr.s_addr = INADDR_ANY;
            p_addr.sin_port = 0;
            bind(pasv_fd, (struct sockaddr*)&p_addr, sizeof(p_addr));
            listen(pasv_fd, 1);

            socklen_t addr_len = sizeof(p_addr);
            getsockname(pasv_fd, (struct sockaddr*)&p_addr, &addr_len);
            int assigned_port = ntohs(p_addr.sin_port);

            char epsv_res[128];
            snprintf(epsv_res, sizeof(epsv_res), "229 Entering Extended Passive Mode (|||%d|)\r\n", assigned_port);
            send(client_fd, epsv_res, strlen(epsv_res), 0);
        } else if (strcasecmp(cmd, "LIST") == 0) {
            const char *res = "150 Opening data connection for directory list.\r\n";
            send(client_fd, res, strlen(res), 0);

            int data_client = ftp_accept_data(&pasv_fd);
            if (data_client >= 0) {
                char virt_check[512];
                const char *target = (arg_val && arg_val[0] != '-') ? arg_val : "";
                ftp_normalize_virtual_path(v_cwd, target, virt_check, sizeof(virt_check));
                char fs_path[768];
                ftp_resolve_path(v_cwd, target, fs_path, sizeof(fs_path));
                DIR *d = opendir(fs_path);
                if (!d) {
                    char with_slash[780];
                    snprintf(with_slash, sizeof(with_slash), "%s/", fs_path);
                    d = opendir(with_slash);
                }
                if (strcmp(virt_check, "/") == 0) {
                    const char *sdmc_line = "drwxr-xr-x 1 root root 0 Jan 01 2026 sdmc\r\n";
                    send(data_client, sdmc_line, strlen(sdmc_line), 0);
                }
                if (d) {
                    static const char *k_ftp_months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
                    struct dirent *de;
                    size_t plen = strlen(fs_path);
                    bool has_slash = (plen > 0 && fs_path[plen - 1] == '/');
                    while ((de = readdir(d)) != NULL) {
                        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
                        if (strcmp(virt_check, "/") == 0 && strcmp(de->d_name, "sdmc") == 0) continue;
                        char lbuf[512];
                        char fullp[768];
                        if (has_slash)
                            snprintf(fullp, sizeof(fullp), "%s%s", fs_path, de->d_name);
                        else
                            snprintf(fullp, sizeof(fullp), "%s/%s", fs_path, de->d_name);
                        struct stat st;
                        memset(&st, 0, sizeof(st));
                        stat(fullp, &st);
                        bool is_d = S_ISDIR(st.st_mode);
                        struct tm *gm = gmtime(&st.st_mtime);
                        int mon = gm ? gm->tm_mon : 0;
                        int mday = gm ? gm->tm_mday : 1;
                        int year = gm ? gm->tm_year + 1900 : 2026;
                        if (mon < 0 || mon > 11) mon = 0;
                        snprintf(lbuf, sizeof(lbuf), "%crwxr-xr-x 1 root root %llu %s %02d %04d %s\r\n",
                                 is_d ? 'd' : '-', (unsigned long long)st.st_size, k_ftp_months[mon], mday, year, de->d_name);
                        send(data_client, lbuf, strlen(lbuf), 0);
                    }
                    closedir(d);
                }
                close(data_client);
                const char *done = "226 Directory send OK.\r\n";
                send(client_fd, done, strlen(done), 0);
            } else {
                const char *err = "425 Can't open data connection.\r\n";
                send(client_fd, err, strlen(err), 0);
            }
        } else if (strcasecmp(cmd, "NLST") == 0) {
            const char *res = "150 Opening data connection for file list.\r\n";
            send(client_fd, res, strlen(res), 0);

            int data_client = ftp_accept_data(&pasv_fd);
            if (data_client >= 0) {
                char virt_check[512];
                const char *target = (arg_val && arg_val[0] != '-') ? arg_val : "";
                ftp_normalize_virtual_path(v_cwd, target, virt_check, sizeof(virt_check));
                char fs_path[768];
                ftp_resolve_path(v_cwd, target, fs_path, sizeof(fs_path));
                DIR *d = opendir(fs_path);
                if (!d) {
                    char with_slash[780];
                    snprintf(with_slash, sizeof(with_slash), "%s/", fs_path);
                    d = opendir(with_slash);
                }
                if (strcmp(virt_check, "/") == 0) {
                    const char *sdmc_line = "sdmc\r\n";
                    send(data_client, sdmc_line, strlen(sdmc_line), 0);
                }
                if (d) {
                    struct dirent *de;
                    while ((de = readdir(d)) != NULL) {
                        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
                        if (strcmp(virt_check, "/") == 0 && strcmp(de->d_name, "sdmc") == 0) continue;
                        char lbuf[512];
                        snprintf(lbuf, sizeof(lbuf), "%s\r\n", de->d_name);
                        send(data_client, lbuf, strlen(lbuf), 0);
                    }
                    closedir(d);
                }
                close(data_client);
                const char *done = "226 Transfer complete.\r\n";
                send(client_fd, done, strlen(done), 0);
            } else {
                const char *err = "425 Can't open data connection.\r\n";
                send(client_fd, err, strlen(err), 0);
            }
        } else if (strcasecmp(cmd, "MLSD") == 0) {
            const char *res = "150 Opening BINARY mode data connection for MLSD.\r\n";
            send(client_fd, res, strlen(res), 0);

            int data_client = ftp_accept_data(&pasv_fd);
            if (data_client >= 0) {
                char virt_check[512];
                const char *target = (arg_val && arg_val[0] != '-') ? arg_val : "";
                ftp_normalize_virtual_path(v_cwd, target, virt_check, sizeof(virt_check));
                char fs_path[768];
                ftp_resolve_path(v_cwd, target, fs_path, sizeof(fs_path));
                DIR *d = opendir(fs_path);
                if (!d) {
                    char with_slash[780];
                    snprintf(with_slash, sizeof(with_slash), "%s/", fs_path);
                    d = opendir(with_slash);
                }
                if (strcmp(virt_check, "/") == 0) {
                    const char *sdmc_line = "type=dir;modify=20260101000000; sdmc\r\n";
                    send(data_client, sdmc_line, strlen(sdmc_line), 0);
                }
                if (d) {
                    struct dirent *de;
                    size_t plen = strlen(fs_path);
                    bool has_slash = (plen > 0 && fs_path[plen - 1] == '/');
                    while ((de = readdir(d)) != NULL) {
                        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
                        if (strcmp(virt_check, "/") == 0 && strcmp(de->d_name, "sdmc") == 0) continue;
                        char lbuf[512];
                        char fullp[768];
                        if (has_slash)
                            snprintf(fullp, sizeof(fullp), "%s%s", fs_path, de->d_name);
                        else
                            snprintf(fullp, sizeof(fullp), "%s/%s", fs_path, de->d_name);
                        struct stat st;
                        memset(&st, 0, sizeof(st));
                        stat(fullp, &st);
                        bool is_d = S_ISDIR(st.st_mode);
                        struct tm *gm = gmtime(&st.st_mtime);
                        char time_str[32];
                        if (gm) {
                            snprintf(time_str, sizeof(time_str), "%04d%02d%02d%02d%02d%02d",
                                     gm->tm_year + 1900, gm->tm_mon + 1, gm->tm_mday,
                                     gm->tm_hour, gm->tm_min, gm->tm_sec);
                        } else {
                            snprintf(time_str, sizeof(time_str), "20260101000000");
                        }
                        if (is_d) {
                            snprintf(lbuf, sizeof(lbuf), "type=dir;modify=%s; %s\r\n", time_str, de->d_name);
                        } else {
                            snprintf(lbuf, sizeof(lbuf), "type=file;size=%llu;modify=%s; %s\r\n",
                                     (unsigned long long)st.st_size, time_str, de->d_name);
                        }
                        send(data_client, lbuf, strlen(lbuf), 0);
                    }
                    closedir(d);
                }
                close(data_client);
                const char *done = "226 MLSD complete.\r\n";
                send(client_fd, done, strlen(done), 0);
            } else {
                const char *err = "425 Can't open data connection.\r\n";
                send(client_fd, err, strlen(err), 0);
            }
        } else if (strcasecmp(cmd, "MLST") == 0) {
            char virt_path[768];
            ftp_normalize_virtual_path(v_cwd, arg_val, virt_path, sizeof(virt_path));
            char fs_path[768];
            ftp_resolve_path(v_cwd, arg_val, fs_path, sizeof(fs_path));
            struct stat st;
            memset(&st, 0, sizeof(st));
            bool ok = false;
            bool is_d = false;
            if (strcmp(virt_path, "/") == 0 || strcmp(virt_path, "/sdmc") == 0) {
                ok = true;
                is_d = true;
            } else if (stat(fs_path, &st) == 0) {
                ok = true;
                is_d = S_ISDIR(st.st_mode);
            }
            if (!ok) {
                const char *err = "550 File or directory not found.\r\n";
                send(client_fd, err, strlen(err), 0);
            } else {
                struct tm *gm = gmtime(&st.st_mtime);
                char time_str[32];
                if (gm) {
                    snprintf(time_str, sizeof(time_str), "%04d%02d%02d%02d%02d%02d",
                             gm->tm_year + 1900, gm->tm_mon + 1, gm->tm_mday,
                             gm->tm_hour, gm->tm_min, gm->tm_sec);
                } else {
                    snprintf(time_str, sizeof(time_str), "20260101000000");
                }
                char res[1024];
                if (is_d) {
                    snprintf(res, sizeof(res), "250-Listing %s\r\n type=dir;modify=%s; %s\r\n250 End\r\n",
                             virt_path, time_str, virt_path);
                } else {
                    snprintf(res, sizeof(res), "250-Listing %s\r\n type=file;size=%llu;modify=%s; %s\r\n250 End\r\n",
                             virt_path, (unsigned long long)st.st_size, time_str, virt_path);
                }
                send(client_fd, res, strlen(res), 0);
            }
        } else if (strcasecmp(cmd, "CWD") == 0 || strcasecmp(cmd, "XCWD") == 0) {
            char new_cwd[512];
            ftp_normalize_virtual_path(v_cwd, arg_val, new_cwd, sizeof(new_cwd));

            if (strcmp(new_cwd, "/") == 0 || strcmp(new_cwd, "/sdmc") == 0) {
                strncpy(v_cwd, new_cwd, sizeof(v_cwd) - 1);
                v_cwd[sizeof(v_cwd) - 1] = '\0';
                const char *res = "250 Directory successfully changed.\r\n";
                send(client_fd, res, strlen(res), 0);
            } else {
                char fs_path[768];
                ftp_resolve_path(new_cwd, "", fs_path, sizeof(fs_path));
                DIR *test_d = opendir(fs_path);
                if (!test_d) {
                    char with_slash[780];
                    snprintf(with_slash, sizeof(with_slash), "%s/", fs_path);
                    test_d = opendir(with_slash);
                }
                struct stat st;
                if (test_d || (stat(fs_path, &st) == 0 && S_ISDIR(st.st_mode))) {
                    if (test_d) closedir(test_d);
                    strncpy(v_cwd, new_cwd, sizeof(v_cwd) - 1);
                    v_cwd[sizeof(v_cwd) - 1] = '\0';
                    const char *res = "250 Directory successfully changed.\r\n";
                    send(client_fd, res, strlen(res), 0);
                } else {
                    const char *err = "550 Directory not found.\r\n";
                    send(client_fd, err, strlen(err), 0);
                }
            }
        } else if (strcasecmp(cmd, "CDUP") == 0 || strcasecmp(cmd, "XCUP") == 0) {
            ftp_normalize_virtual_path(v_cwd, "..", v_cwd, sizeof(v_cwd));
            const char *res = "200 Directory changed to parent.\r\n";
            send(client_fd, res, strlen(res), 0);
        } else if (strcasecmp(cmd, "MDTM") == 0) {
            char fullp[768];
            ftp_resolve_path(v_cwd, arg_val, fullp, sizeof(fullp));
            struct stat st;
            if (stat(fullp, &st) == 0) {
                struct tm *gm = gmtime(&st.st_mtime);
                char res[128];
                if (gm) {
                    snprintf(res, sizeof(res), "213 %04d%02d%02d%02d%02d%02d\r\n",
                             gm->tm_year + 1900, gm->tm_mon + 1, gm->tm_mday, gm->tm_hour, gm->tm_min, gm->tm_sec);
                } else {
                    snprintf(res, sizeof(res), "213 20260101000000\r\n");
                }
                send(client_fd, res, strlen(res), 0);
            } else {
                const char *err = "550 File not found.\r\n";
                send(client_fd, err, strlen(err), 0);
            }
        } else if (strcasecmp(cmd, "RETR") == 0) {
            char fullp[768];
            ftp_resolve_path(v_cwd, arg_val, fullp, sizeof(fullp));
            FILE *f = fopen(fullp, "rb");
            if (f) {
                if (rest_offset > 0) {
                    fseek(f, (long)rest_offset, SEEK_SET);
                    rest_offset = 0;
                }
                const char *res = "150 Opening BINARY mode data connection.\r\n";
                send(client_fd, res, strlen(res), 0);
                int data_client = ftp_accept_data(&pasv_fd);
                if (data_client >= 0) {
                    char *file_buf = (char*)malloc(64 * 1024);
                    if (file_buf) {
                        size_t rb;
                        while (s_ps3_ftp_running && (rb = fread(file_buf, 1, 64 * 1024, f)) > 0) {
                            send(data_client, file_buf, rb, 0);
                        }
                        free(file_buf);
                    }
                    close(data_client);
                    const char *done = "226 Transfer complete.\r\n";
                    send(client_fd, done, strlen(done), 0);
                } else {
                    const char *err = "425 Can't open data connection.\r\n";
                    send(client_fd, err, strlen(err), 0);
                }
                fclose(f);
            } else {
                rest_offset = 0;
                const char *err = "550 File not found.\r\n";
                send(client_fd, err, strlen(err), 0);
            }
        } else if (strcasecmp(cmd, "STOR") == 0) {
            char fullp[768];
            ftp_resolve_path(v_cwd, arg_val, fullp, sizeof(fullp));
            char *sep = strrchr(fullp, '/');
            if (sep) {
                *sep = '\0';
                ftp_ensure_dir(fullp);
                *sep = '/';
            }
            FILE *f = NULL;
            if (rest_offset > 0) {
                f = fopen(fullp, "r+b");
                if (f) {
                    fseek(f, (long)rest_offset, SEEK_SET);
                }
            }
            if (!f) {
                f = fopen(fullp, "wb");
            }
            rest_offset = 0;
            if (!f) {
                remove(fullp);
                f = fopen(fullp, "wb");
            }
            if (!f) {
                write_log("[PS3] FTP: STOR '%s' -> fopen('%s') failed: errno=%d (%s)\n",
                          arg_val ? arg_val : "", fullp, errno, strerror(errno));
            }
            if (f) {
                const char *res = "150 Ok to send data.\r\n";
                send(client_fd, res, strlen(res), 0);
                int data_client = ftp_accept_data(&pasv_fd);
                if (data_client >= 0) {
                    char *file_buf = (char*)malloc(64 * 1024);
                    if (file_buf) {
                        int rb;
                        while (s_ps3_ftp_running && (rb = recv(data_client, file_buf, 64 * 1024, 0)) > 0) {
                            fwrite(file_buf, 1, rb, f);
                        }
                        free(file_buf);
                    }
                    close(data_client);
                    const char *done = "226 Transfer complete.\r\n";
                    send(client_fd, done, strlen(done), 0);
                } else {
                    const char *err = "425 Can't open data connection.\r\n";
                    send(client_fd, err, strlen(err), 0);
                }
                fclose(f);
                if (data_client >= 0) {
                    ps3_library_mark_dirty();
                    write_log("[PS3] FTP: library refresh requested after STOR '%s'\\n", fullp);
                }
            } else {
                const char *err = "550 Failed to open file for writing.\r\n";
                send(client_fd, err, strlen(err), 0);
            }
        } else if (strcasecmp(cmd, "APPE") == 0) {
            char fullp[768];
            ftp_resolve_path(v_cwd, arg_val, fullp, sizeof(fullp));
            char *sep = strrchr(fullp, '/');
            if (sep) {
                *sep = '\0';
                ftp_ensure_dir(fullp);
                *sep = '/';
            }
            FILE *f = fopen(fullp, "ab");
            if (f) {
                const char *res = "150 Ok to send data.\r\n";
                send(client_fd, res, strlen(res), 0);
                int data_client = ftp_accept_data(&pasv_fd);
                if (data_client >= 0) {
                    char *file_buf = (char*)malloc(64 * 1024);
                    if (file_buf) {
                        int rb;
                        while (s_ps3_ftp_running && (rb = recv(data_client, file_buf, 64 * 1024, 0)) > 0) {
                            fwrite(file_buf, 1, rb, f);
                        }
                        free(file_buf);
                    }
                    close(data_client);
                    const char *done = "226 Transfer complete.\r\n";
                    send(client_fd, done, strlen(done), 0);
                } else {
                    const char *err = "425 Can't open data connection.\r\n";
                    send(client_fd, err, strlen(err), 0);
                }
                fclose(f);
                if (data_client >= 0) {
                    ps3_library_mark_dirty();
                    write_log("[PS3] FTP: library refresh requested after APPE '%s'\\n", fullp);
                }
            } else {
                const char *err = "550 Failed to open file for writing.\r\n";
                send(client_fd, err, strlen(err), 0);
            }
        } else if (strcasecmp(cmd, "RNFR") == 0) {
            ftp_resolve_path(v_cwd, arg_val, rnfr_path, sizeof(rnfr_path));
            struct stat st;
            if (stat(rnfr_path, &st) == 0) {
                const char *res = "350 File exists, ready for destination name.\r\n";
                send(client_fd, res, strlen(res), 0);
            } else {
                rnfr_path[0] = '\0';
                const char *err = "550 File not found.\r\n";
                send(client_fd, err, strlen(err), 0);
            }
        } else if (strcasecmp(cmd, "RNTO") == 0) {
            if (rnfr_path[0] == '\0') {
                const char *err = "503 Bad sequence of commands.\r\n";
                send(client_fd, err, strlen(err), 0);
            } else {
                char rnto_path[768];
                ftp_resolve_path(v_cwd, arg_val, rnto_path, sizeof(rnto_path));
                if (rename(rnfr_path, rnto_path) == 0) {
                    rnfr_path[0] = '\0';
                    const char *res = "250 Rename successful.\r\n";
                    send(client_fd, res, strlen(res), 0);
                    ps3_library_mark_dirty();
                } else {
                    rnfr_path[0] = '\0';
                    const char *err = "550 Rename failed.\r\n";
                    send(client_fd, err, strlen(err), 0);
                }
            }
        } else if (strcasecmp(cmd, "DELE") == 0) {
            char fullp[768];
            ftp_resolve_path(v_cwd, arg_val, fullp, sizeof(fullp));
            if (remove(fullp) == 0) {
                const char *res = "250 File deleted.\r\n";
                send(client_fd, res, strlen(res), 0);
                ps3_library_mark_dirty();
            } else {
                const char *err = "550 Failed to delete file.\r\n";
                send(client_fd, err, strlen(err), 0);
            }
        } else if (strcasecmp(cmd, "MKD") == 0 || strcasecmp(cmd, "XMKD") == 0) {
            char fullp[768];
            ftp_resolve_path(v_cwd, arg_val, fullp, sizeof(fullp));
            if (mkdir(fullp, 0777) == 0) {
                char res[512];
                snprintf(res, sizeof(res), "257 \"%s\" created.\r\n", arg_val ? arg_val : "directory");
                send(client_fd, res, strlen(res), 0);
            } else {
                const char *err = "550 Failed to create directory.\r\n";
                send(client_fd, err, strlen(err), 0);
            }
        } else if (strcasecmp(cmd, "RMD") == 0 || strcasecmp(cmd, "XRMD") == 0) {
            char fullp[768];
            ftp_resolve_path(v_cwd, arg_val, fullp, sizeof(fullp));
            if (rmdir(fullp) == 0) {
                const char *res = "250 Directory removed.\r\n";
                send(client_fd, res, strlen(res), 0);
            } else {
                const char *err = "550 Failed to remove directory.\r\n";
                send(client_fd, err, strlen(err), 0);
            }
        } else if (strcasecmp(cmd, "SIZE") == 0) {
            char fullp[768];
            ftp_resolve_path(v_cwd, arg_val, fullp, sizeof(fullp));
            struct stat st;
            if (stat(fullp, &st) == 0 && S_ISREG(st.st_mode)) {
                char res[128];
                snprintf(res, sizeof(res), "213 %llu\r\n", (unsigned long long)st.st_size);
                send(client_fd, res, strlen(res), 0);
            } else {
                const char *err = "550 Could not get file size.\r\n";
                send(client_fd, err, strlen(err), 0);
            }
        } else if (strcasecmp(cmd, "QUIT") == 0) {
            const char *res = "221 Goodbye.\r\n";
            send(client_fd, res, strlen(res), 0);
            break;
        } else {
            const char *res = "502 Command not implemented.\r\n";
            send(client_fd, res, strlen(res), 0);
        }
    }
    if (pasv_fd >= 0) close(pasv_fd);
    close(client_fd);
    ps3_ftp_unregister_client(client_fd);
    write_log("[PS3] ftp: client session end\n");
    return NULL;
}

static void* ps3_ftp_thread_func(void *arg) {
    (void)arg;
    while (s_ps3_ftp_running && s_ps3_server_fd >= 0) {
        fd_set readfds;
        ps3_diag_net_checkpoint("ftp:wait");
        FD_ZERO(&readfds);
        FD_SET(s_ps3_server_fd, &readfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        int sret = select(s_ps3_server_fd + 1, &readfds, NULL, NULL, &tv);
        if (sret <= 0 || !s_ps3_ftp_running) {
            continue;
        }

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        ps3_diag_net_checkpoint("ftp:accept");
        int client_fd = accept(s_ps3_server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            continue;
        }
        s_ftp_route_seen = 1;
        {
            unsigned int pa = ntohl(client_addr.sin_addr.s_addr);
            write_log("[PS3] ftp: client connected from %u.%u.%u.%u:%u\n",
                      (pa >> 24) & 255u, (pa >> 16) & 255u, (pa >> 8) & 255u, pa & 255u,
                      (unsigned)ntohs(client_addr.sin_port));
        }

        FtpClientContext *ctx = (FtpClientContext*)malloc(sizeof(FtpClientContext));
        if (ctx) {
            ctx->client_fd = client_fd;
            pthread_t client_tid;
            if (pthread_create(&client_tid, NULL, ps3_ftp_client_session, ctx) == 0) {
                pthread_detach(client_tid);
            } else {
                free(ctx);
                close(client_fd);
            }
        } else {
            close(client_fd);
        }
    }
    return NULL;
}

static unsigned int ps3_ftp_ip_to_u32(const char *ip)
{
    unsigned int a = 0, b = 0, c = 0, d = 0;
    if (!ip || sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
        return INADDR_ANY;
    if (a > 255 || b > 255 || c > 255 || d > 255)
        return INADDR_ANY;
    return htonl((a << 24) | (b << 16) | (c << 8) | d);
}

int ps3_ftp_start(void) {
    if (s_ps3_ftp_running) return 0;
    write_log("[PS3] ftp: start requested\n");
    s_ftp_route_seen = 0;
    mkdir("./screenshots", 0777);
    mkdir("./saves", 0777);
    mkdir("./roms", 0777);
    mkdir("./conf", 0777);
    mkdir("./thumbs", 0777);
    mkdir("./kickstarts", 0777);
    mkdir("./tmp", 0777);
    write_log("[PS3] ftp: open socket\n");
    s_ps3_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_ps3_server_fd < 0) {
        write_log("[PS3] ftp: socket failed (errno=%d)\n", errno);
        return -1;
    }
    int opt = 1;
    setsockopt(s_ps3_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    static const int kFtpPorts[] = { 21, 5000, 2121, 8021 };
    int bound_port = -1;
    for (unsigned i = 0; i < sizeof(kFtpPorts) / sizeof(kFtpPorts[0]) && bound_port < 0; i++) {
        serv_addr.sin_port = htons((unsigned short)kFtpPorts[i]);
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(s_ps3_server_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
            bound_port = kFtpPorts[i];
            write_log("[PS3] ftp: bind %d ok\n", bound_port);
            break;
        }
        write_log("[PS3] ftp: bind %d failed (errno=%d)\n", kFtpPorts[i], errno);
        if (strcmp(s_ps3_ftp_ip, "127.0.0.1") == 0 || strcmp(s_ps3_ftp_ip, "0.0.0.0") == 0)
            continue;
        serv_addr.sin_addr.s_addr = ps3_ftp_ip_to_u32(s_ps3_ftp_ip);
        if (bind(s_ps3_server_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
            bound_port = kFtpPorts[i];
            write_log("[PS3] ftp: bound port %d to local address %s\n", bound_port, s_ps3_ftp_ip);
        } else {
            write_log("[PS3] ftp: bind %d on %s failed (errno=%d)\n", kFtpPorts[i], s_ps3_ftp_ip, errno);
        }
    }
    if (bound_port < 0) {
        close(s_ps3_server_fd);
        s_ps3_server_fd = -1;
        return -1;
    }
    s_ps3_ftp_port = bound_port;
    write_log("[PS3] ftp: listening on port %d\n", bound_port);
    listen(s_ps3_server_fd, 4);
    s_ps3_ftp_running = 1;
    write_log("[PS3] ftp: starting server thread\n");
    if (pthread_create(&s_ps3_ftp_thread, NULL, ps3_ftp_thread_func, NULL) != 0) {
        write_log("[PS3] ftp: thread create failed (errno=%d)\n", errno);
        s_ps3_ftp_running = 0;
        close(s_ps3_server_fd);
        s_ps3_server_fd = -1;
        return -1;
    }
    pthread_t netctl_tid;
    if (pthread_create(&netctl_tid, NULL, ps3_ftp_probe_worker, NULL) == 0) {
        pthread_detach(netctl_tid);
        write_log("[PS3] ftp: ip probe worker started\n");
    } else {
        write_log("[PS3] ftp: ip probe worker create failed (errno=%d)\n", errno);
    }
    write_log("[PS3] ftp: server thread running\n");
    ps3_diag_checkpoint("ftp:running");
    return 0;
}

void ps3_ftp_stop(void) {
    if (!s_ps3_ftp_running) return;
    ps3_diag_checkpoint("ftp:stop");
    write_log("[PS3] ftp: stop requested\n");
    s_ps3_ftp_running = 0;
    s_ftp_route_seen = 0;
    ps3_diag_checkpoint("ftp:join");
    int jerr = pthread_join(s_ps3_ftp_thread, NULL);
    write_log("[PS3] ftp: server thread joined (rc=%d)\n", jerr);
    if (s_ps3_server_fd >= 0) {
        shutdown(s_ps3_server_fd, SHUT_RDWR);
        close(s_ps3_server_fd);
        s_ps3_server_fd = -1;
    }
    pthread_mutex_lock(&s_ps3_clients_mutex);
    for (int i = 0; i < MAX_FTP_CLIENTS; i++) {
        if (s_ps3_client_fds[i] >= 0) {
            shutdown(s_ps3_client_fds[i], SHUT_RDWR);
        }
    }
    pthread_mutex_unlock(&s_ps3_clients_mutex);
    ps3_diag_checkpoint("ftp:stopped");
    SDL_Delay(120);
}

int ps3_ftp_is_running(void) { return s_ps3_ftp_running; }
void ps3_ftp_get_ip(char *buf, size_t sz) { if (buf && sz) snprintf(buf, sz, "%s", s_ps3_ftp_ip); }
int ps3_ftp_get_port(void) { return s_ps3_ftp_port; }

static inline int vita_ftp_start(void) { return ps3_ftp_start(); }
static inline void vita_ftp_stop(void) { ps3_ftp_stop(); }
static inline int vita_ftp_is_running(void) { return ps3_ftp_is_running(); }
static inline void vita_ftp_get_ip(char *buf, size_t sz) { ps3_ftp_get_ip(buf, sz); }
static inline int vita_ftp_get_port(void) { return ps3_ftp_get_port(); }
static inline int vita_cover_download(const char *n, char *o, size_t s) { (void)o; (void)s; return cover_mgr_download(n); }
#include <dirent.h>
#include <sys/stat.h>

extern char uae4all_image_file0[256];
extern char uae4all_image_file1[256];
extern char uae4all_image_file2[256];
extern char uae4all_image_file3[256];
extern char uae4all_hard_file0[256];
extern char uae4all_hard_file1[256];
extern char uae4all_hard_file2[256];
extern char uae4all_hard_file3[256];
extern char uae4all_hard_dir[256];
extern int uae4all_hard_file_ro[4];
extern int mainMenu_bootHD;
extern char currentDir[300];
extern char launchDir[300];
extern int saveAdfDir(void);
extern int mainMenu_drives;
extern int mainMenu_floppyspeed;
extern int mainMenu_CPU_model;
extern int mainMenu_chipset;
extern int mainMenu_chipMemory;
extern int mainMenu_slowMemory;
extern int mainMenu_fastMemory;
extern int mainMenu_rtgMemory;
extern int kickstart;
extern int bReloadKickstart;
extern int uae4all_init_rom(const char *romfile);
extern const char *kickstarts_rom_names[];
extern const char *extended_rom_names[];
extern char romfile[256];
extern char extfile[256];
extern int mainMenu_shader;
extern int mainMenu_ntsc;
extern int mainMenu_showStatus;
extern int mainMenu_leftStickMouse;
extern int mainMenu_touchControls;
extern int mainMenu_autofire;
extern int mainMenu_autofireRate;
extern bool ps3_autofire;
extern int mainMenu_pinballMode;
extern int moveY;
extern int mainMenu_soundStereo;
extern int mainMenu_soundStereoSep;
extern int mainMenu_diskSoundVolume;
extern void disk_sound_set_volume(int percent);
extern unsigned int sound_rate;
extern int saveMenu_n_savestate;
extern char *savestate_filename;
extern char *screenshot_filename;
extern int savestate_state;
extern int presetModeId;
extern int mainMenu_frameskip;
extern int mainMenu_floppyWriteProtect[4];
extern int mainMenu_cycleExact;
extern int mainMenu_joyPort;
extern int mainMenu_singleJoycons;
extern int mainMenu_numPlayers;
extern void update_pad_mode(void);
extern int mainMenu_mouseEmulation;
extern int mainMenu_deadZone;
extern int mainMenu_scanlines;
extern int mainMenu_autosave;
extern char mainMenu_whdload_args[256];
extern void disk_set_write_protect(int num, int enabled);
extern int disk_get_write_protect(int num);
extern int mainMenu_cutLeft;
extern int mainMenu_cutRight;
extern int mainMenu_footerSize;
extern int mainMenu_screenOffsetY;
extern int mainMenu_screenOffsetX;
extern int mainMenu_autoCrop;
extern void ps3_reset_autocrop(void);
extern int visibleAreaWidth;
extern int mainMenu_displayHires;
extern int mainMenu_case;
extern int emulating;
extern int kickstart_warning;
extern volatile int vita_screenshot_request;
extern int mainMenu_sound;
extern int mainMenu_CPU_speed;
extern int mainMenu_spriteCollisions;
extern int mainMenu_customControls;
extern int mainMenu_custom_currentlyEditingControllerNr;
extern int mainMenu_custom_controlSet;
extern int mainMenu_custom_up[MAX_NUM_CONTROLLERS];
extern int mainMenu_custom_down[MAX_NUM_CONTROLLERS];
extern int mainMenu_custom_left[MAX_NUM_CONTROLLERS];
extern int mainMenu_custom_right[MAX_NUM_CONTROLLERS];
extern int mainMenu_custom_stickup[MAX_NUM_CONTROLLERS];
extern int mainMenu_custom_stickdown[MAX_NUM_CONTROLLERS];
extern int mainMenu_custom_stickleft[MAX_NUM_CONTROLLERS];
extern int mainMenu_custom_stickright[MAX_NUM_CONTROLLERS];
extern int mainMenu_custom_A[MAX_NUM_CONTROLLERS];
extern int mainMenu_custom_B[MAX_NUM_CONTROLLERS];
extern int mainMenu_custom_X[MAX_NUM_CONTROLLERS];
extern int mainMenu_custom_Y[MAX_NUM_CONTROLLERS];
extern int mainMenu_custom_L[MAX_NUM_CONTROLLERS];
extern int mainMenu_custom_R[MAX_NUM_CONTROLLERS];
extern int mainMenu_customPreset_up[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS];
extern int mainMenu_customPreset_down[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS];
extern int mainMenu_customPreset_left[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS];
extern int mainMenu_customPreset_right[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS];
extern int mainMenu_customPreset_stickup[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS];
extern int mainMenu_customPreset_stickdown[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS];
extern int mainMenu_customPreset_stickleft[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS];
extern int mainMenu_customPreset_stickright[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS];
extern int mainMenu_customPreset_A[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS];
extern int mainMenu_customPreset_B[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS];
extern int mainMenu_customPreset_X[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS];
extern int mainMenu_customPreset_Y[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS];
extern int mainMenu_customPreset_L[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS];
extern int mainMenu_customPreset_R[MAX_NUM_CUSTOM_PRESETS][MAX_NUM_CONTROLLERS];
extern int mainMenu_mouseMultiplier;
extern int mainMenu_mouseDevice;
extern int mainMenu_mouseAcceleration;
extern int mainMenu_mouseSlowFactor;
extern int mainMenu_mouseSlowButton;
extern int mainMenu_mouseFastFactor;
extern int mainMenu_mouseFastButton;
extern int mainMenu_mouseSwapButtons;
extern int mainMenu_vkbdLanguage;
extern int mainMenu_vkbdStyle;
extern int mainMenu_vkbdTransparency;
extern int mainMenu_vkbdPosition;
extern char config_load_filename[300];
extern char save_import_filename[300];
extern int saveconfig(int general);
extern const char *config_save_as_name;
extern void loadconfig(int general);
extern void SetDefaultMenuSettings(int general);
extern void remap_custom_controls(void);
extern void mapback_custom_controls(void);
extern void check_all_prefs(void);
extern void update_display(void);
extern void getChanges(void);
#include "ps3_kbd.h"
static char s_view_kbd_buf[512] = "";
static char *ps3_kbd_get_str(char *title, const char *initial_text, int maxLen, int multiline)
{
    s_view_kbd_buf[0] = '\0';
    ps3_kbd_get(title, initial_text, maxLen < 511 ? maxLen : 511, multiline, s_view_kbd_buf);
    return s_view_kbd_buf;
}
static int kbdvita_start(char *title, const char *initial_text, int maxLen, int multiline)
{
    (void)title; (void)initial_text; (void)maxLen; (void)multiline;
    return 0;
}
extern void stateFilenameToThumbFilename(char *src, char *dst);
extern void make_savestate_filenames(char *save, char *thumb);
extern int vkbd_init(void);
extern void vkbd_quit(void);

typedef struct {
    int id;
    const char *name;
} CustomActionInfo;

static const CustomActionInfo s_custom_actions[] = {
    { 0, "None (Disabled)" },
    { -3, "Joy Fire 1 (Primary)" },
    { -4, "Joy Fire 2 (Secondary)" },
    { -5, "Joy UP (Jump)" },
    { -6, "Joy DOWN" },
    { -7, "Joy LEFT" },
    { -8, "Joy RIGHT" },
    { -1, "Mouse Left Click" },
    { -2, "Mouse Right Click" },
    { -25, "Slow Down Mouse (Hold)" },
    { -26, "Speed Up Mouse (Hold)" },
    { -27, "Quick Save State" },
    { -28, "Quick Load State" },
    { -9, "Joy 2 UP" },
    { -10, "Joy 2 DOWN" },
    { -11, "Joy 2 LEFT" },
    { -12, "Joy 2 RIGHT" },
    { -13, "Joy 3 Fire 1" },
    { -14, "Joy 3 Fire 2" },
    { -15, "Joy 3 UP" },
    { -16, "Joy 3 DOWN" },
    { -17, "Joy 3 LEFT" },
    { -18, "Joy 3 RIGHT" },
    { -19, "Joy 4 Fire 1" },
    { -20, "Joy 4 Fire 2" },
    { -21, "Joy 4 UP" },
    { -22, "Joy 4 DOWN" },
    { -23, "Joy 4 LEFT" },
    { -24, "Joy 4 RIGHT" },
    { 23, "Amiga SPACE" },
    { 26, "Amiga RETURN" },
    { 27, "Amiga ESCAPE" },
    { 24, "Amiga BACKSPACE" },
    { 25, "Amiga TAB" },
    { 37, "Amiga HELP" },
    { 28, "Amiga DELETE" },
    { 29, "Amiga Left SHIFT" },
    { 30, "Amiga Right SHIFT" },
    { 31, "Amiga CAPS LOCK" },
    { 32, "Amiga CTRL" },
    { 33, "Amiga Left ALT" },
    { 34, "Amiga Right ALT" },
    { 35, "Amiga Left AMIGA" },
    { 36, "Amiga Right AMIGA" },
    { 1, "Amiga Arrow UP" },
    { 2, "Amiga Arrow DOWN" },
    { 3, "Amiga Arrow LEFT" },
    { 4, "Amiga Arrow RIGHT" },
    { 87, "Amiga F1" },
    { 88, "Amiga F2" },
    { 89, "Amiga F3" },
    { 90, "Amiga F4" },
    { 91, "Amiga F5" },
    { 92, "Amiga F6" },
    { 93, "Amiga F7" },
    { 94, "Amiga F8" },
    { 95, "Amiga F9" },
    { 96, "Amiga F10" },
    { 77, "Amiga 1" },
    { 78, "Amiga 2" },
    { 79, "Amiga 3" },
    { 80, "Amiga 4" },
    { 81, "Amiga 5" },
    { 82, "Amiga 6" },
    { 83, "Amiga 7" },
    { 84, "Amiga 8" },
    { 85, "Amiga 9" },
    { 86, "Amiga 0" },
    { 51, "Amiga A" },
    { 52, "Amiga B" },
    { 53, "Amiga C" },
    { 54, "Amiga D" },
    { 55, "Amiga E" },
    { 56, "Amiga F" },
    { 57, "Amiga G" },
    { 58, "Amiga H" },
    { 59, "Amiga I" },
    { 60, "Amiga J" },
    { 61, "Amiga K" },
    { 62, "Amiga L" },
    { 63, "Amiga M" },
    { 64, "Amiga N" },
    { 65, "Amiga O" },
    { 66, "Amiga P" },
    { 67, "Amiga Q" },
    { 68, "Amiga R" },
    { 69, "Amiga S" },
    { 70, "Amiga T" },
    { 71, "Amiga U" },
    { 72, "Amiga V" },
    { 73, "Amiga W" },
    { 74, "Amiga X" },
    { 75, "Amiga Y" },
    { 76, "Amiga Z" },
    { 5, "Numpad 0" },
    { 6, "Numpad 1" },
    { 7, "Numpad 2" },
    { 8, "Numpad 3" },
    { 9, "Numpad 4" },
    { 10, "Numpad 5" },
    { 11, "Numpad 6" },
    { 12, "Numpad 7" },
    { 13, "Numpad 8" },
    { 14, "Numpad 9" },
    { 15, "Numpad ENTER" },
    { 16, "Numpad /" },
    { 17, "Numpad *" },
    { 18, "Numpad -" },
    { 19, "Numpad +" },
    { 20, "Numpad DEL" },
    { 21, "Numpad (" },
    { 22, "Numpad )" },
    { 38, "Amiga [" },
    { 39, "Amiga ]" },
    { 40, "Amiga ;" },
    { 41, "Amiga ," },
    { 42, "Amiga ." },
    { 43, "Amiga /" },
    { 44, "Amiga \\" },
    { 45, "Amiga '" },
    { 46, "Amiga #" },
    { 47, "Amiga <>" },
    { 48, "Amiga `" },
    { 49, "Amiga -" },
    { 50, "Amiga =" }
};

static const int s_num_custom_actions = sizeof(s_custom_actions) / sizeof(s_custom_actions[0]);

static const char *vita_get_custom_action_name(int action_id)
{
    for (int i = 0; i < s_num_custom_actions; i++) {
        if (s_custom_actions[i].id == action_id)
            return s_custom_actions[i].name;
    }
    return "Custom / None";
}

static int vita_cycle_custom_action(int current_id, int direction)
{
    int current_idx = 0;
    for (int i = 0; i < s_num_custom_actions; i++) {
        if (s_custom_actions[i].id == current_id) {
            current_idx = i;
            break;
        }
    }
    current_idx = (current_idx + direction + s_num_custom_actions) % s_num_custom_actions;
    return s_custom_actions[current_idx].id;
}

static bool vita_copy_file(const char *src_path, const char *dst_path)
{
    if (!src_path || !dst_path || src_path[0] == '\0' || dst_path[0] == '\0') return false;
    FILE *src = fopen(src_path, "rb");
    if (!src) return false;
    FILE *dst = fopen(dst_path, "wb");
    if (!dst) {
        fclose(src);
        return false;
    }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        fwrite(buf, 1, n, dst);
    }
    fclose(src);
    fclose(dst);
    return true;
}

static void copy_drive_path(char *destination, const char *source)
{
    if (!destination) return;
    if (!source) {
        destination[0] = '\0';
        return;
    }
    strncpy(destination, source, 255);
    destination[255] = '\0';
}

static const char *get_filename_only(const char *path)
{
    if (!path || path[0] == '\0') return "Empty (Press X to insert)";
    const char *p = strrchr(path, '/');
    if (p) return p + 1;
    p = strrchr(path, '\\');
    if (p) return p + 1;
    return path;
}

char s_lib_rom_dir[512] = "";

void lib_load_rom_dir(void)
{
    if (s_lib_rom_dir[0] != '\0')
        return;
    char conf_path[512];
    if (launchDir[0] != '\0' && strcmp(launchDir, ".") != 0) {
        snprintf(conf_path, sizeof(conf_path), "%s/conf/romdir.conf", launchDir);
    } else {
        snprintf(conf_path, sizeof(conf_path), "./conf/romdir.conf");
    }
    FILE *f = fopen(conf_path, "rt");
    if (!f) {
        f = fopen("./conf/romdir.conf", "rt");
    }
    if (f) {
        if (fgets(s_lib_rom_dir, sizeof(s_lib_rom_dir), f)) {
            char *p = s_lib_rom_dir + strlen(s_lib_rom_dir) - 1;
            while (p >= s_lib_rom_dir && (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t' || *p == '/')) {
                *p = '\0';
                p--;
            }
        }
        fclose(f);
    }
    if (s_lib_rom_dir[0] == '\0') {
        strncpy(s_lib_rom_dir, "/dev_hdd0/uae4all/roms", sizeof(s_lib_rom_dir) - 1);
        s_lib_rom_dir[sizeof(s_lib_rom_dir) - 1] = '\0';
    }
}

void ps3_load_rom_dir_if_needed(void)
{
    lib_load_rom_dir();
}

void lib_save_rom_dir(void)
{
    char conf_path[512];
    if (launchDir[0] != '\0' && strcmp(launchDir, ".") != 0) {
        char dir_buf[512];
        snprintf(dir_buf, sizeof(dir_buf), "%s/conf", launchDir);
        ftp_ensure_dir(dir_buf);
        snprintf(conf_path, sizeof(conf_path), "%s/conf/romdir.conf", launchDir);
    } else {
        snprintf(conf_path, sizeof(conf_path), "./conf/romdir.conf");
    }
    mkdir("./conf", 0777);
    FILE *f = fopen(conf_path, "wt");
    if (!f) {
        f = fopen("./conf/romdir.conf", "wt");
    }
    if (f) {
        fputs(s_lib_rom_dir, f);
        fputc('\n', f);
        fclose(f);
    }
}

static int vita_load_kickstart(const char *path)
{
    if (uae4all_init_rom(path) == -1) {
        kickstart_warning = 1;
        write_log("[PS3] Kickstart load failed: %s\n", path ? path : "");
        ps3_show_message_box("Kickstart Missing", "The selected Kickstart ROM could not be opened.", "OK (A)");
        return 0;
    }
    kickstart_warning = 0;
    write_log("[PS3] Kickstart loaded: %s\n", path ? path : "");
    return 1;
}

/* Dump the first few entries of a directory into the log.  "The emulator does
 * not see my files" almost always means the files sit in a folder that is not
 * scanned, and on real hardware the log is the only channel we have to find
 * out which folder that is. */
static void log_dir_contents(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *ent;
    char names[256];
    int shown = 0;
    int truncated = 0;

    if (!d)
        return;

    names[0] = '\0';
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;
        if (shown >= 6) {
            truncated = 1;
            break;
        }
        size_t used = strlen(names);
        snprintf(names + used, sizeof(names) - used, "%s%s",
                 shown ? ", " : "", ent->d_name);
        shown++;
    }
    closedir(d);

    if (shown == 0) {
        write_log("[PS3]   (empty)\n");
        return;
    }
    write_log("[PS3]   %s%s\n", names, truncated ? ", ..." : "");
}

static const char *vita_kickstart_aliases[KICKSTART_ROM_COUNT][16] = {
    { "kick12.rom", "kick33180.A500", "kick33180.a500", "amiga-os-120.rom", "kick1.2.rom", "Kickstart-v1.2", NULL },
    { "kick13.rom", "kick34005.A500", "kick34005.a500", "Kick34005.a500", "amiga-os-130.rom", "kick1.3.rom", NULL },
    { "kick20.rom", "kick37175.A500", "kick37175.a500", "amiga-os-204.rom", "kick204.rom", "kick2.04.rom", "Kickstart-v2.04", NULL },
    { "kick31.rom", "kick40068.A1200", "kick40068.a1200", "amiga-os-310-a1200.rom", "kick3.1.rom", "kick40068.A4000", "kick40068.a4000", "Kickstart-v3.1", NULL },
    { "kickcustom.rom", "custom.rom", "kick.rom", "kick32.rom", "kick322.rom", "kick47102.A1200", "kick47102.A500", "kick3.2.rom", "kick3.2.2.rom", "diagrom.rom", NULL },
    { "aros-amiga-m68k-rom.bin", "aros.rom", NULL },
    { "kick40060.CD32", "kick40060.cd32", "amiga-os-310-cd32.rom", "kick31.rom", "Kickstart-v3.1-rev40.60-1993-Commodore-CD32.rom", "kick40060.CD32.rom", "kick40060.rom", "cd32.rom", "kick31cd32.rom", "CD32 ROM r40.60 (1993)(Commodore)(CD32).rom", "Kickstart v3.1 rev 40.60 (1993)(Commodore)(CD32).rom", NULL },
    { "kick31034.A1000", "amiga-os-110-ntsc.rom", "Kickstart-v1.1", NULL },
    { "kick32034.A1000", "amiga-os-110-pal.rom", NULL },
    { "kick33180.A500", "kick33180.a500", "amiga-os-120.rom", "kick12.rom", "kick1.2.rom", NULL },
    { "kick37175.A500", "kick37175.a500", "amiga-os-204.rom", "kick20.rom", "kick204.rom", NULL },
    { "kick37350.A600", "kick37350.a600", "amiga-os-205-a600.rom", "kick205.rom", "kick2.05.rom", "kick600.rom", "kick20.rom", NULL },
    { "kick40063.A600", "kick40063.a600", "amiga-os-310-a600.rom", "kick31.rom", "kick40068.A1200", NULL },
    { "kick39106.A1200", "kick39106.a1200", "amiga-os-300-a1200.rom", "kick30.rom", "kick3.0.rom", NULL },
    { "kick40068.A1200", "kick40068.a1200", "amiga-os-310-a1200.rom", "kick31.rom", "kick3.1.rom", "Kickstart-v3.1", NULL },
    { "kick39106.A4000", "kick39106.a4000", "amiga-os-300-a4000.rom", "kick30.rom", NULL },
    { "kick40068.A4000", "kick40068.a4000", "amiga-os-310-a4000.rom", "kick31.rom", NULL },
    { "kick13.rom", "amiga-os-130.rom", "kick34005.A500", "kick34005.a500", "Kick34005.a500", "kick1.3.rom", NULL }
};

static int ps3_kickstart_size_ok(int index, const char *path)
{
    struct stat st;
    if (!path || stat(path, &st) != 0) return 0;
    long sz = (long)st.st_size;
    // CD32 combined 1MB ROM (kick + extended in one file, memory.cpp loads
    // the second half from romfile when extfile is missing).
    if (index == 6 && sz >= 1000000 && sz <= 1080000) return 1;
    if (sz < 200000 || sz > 600000) return 0;
    if (index == 4 || index == 5) {
        return 1;
    }
    if (sz >= 250000 && sz <= 560000) return 1;
    return 0;
}

int ps3_set_kickstart(int index, int load_rom)
{
    if (index < 0 || index >= KICKSTART_ROM_COUNT)
        return 0;

    int found = 0;
    romfile[0] = '\0';
    char cand[512];

    for (int i = 0; i < 16 && vita_kickstart_aliases[index][i]; i++) {
        if (ps3_find_kickstart_file(vita_kickstart_aliases[index][i], cand, sizeof(cand))) {
            if (ps3_kickstart_size_ok(index, cand)) {
                snprintf(romfile, sizeof(romfile), "%s", cand);
                found = 1;
                break;
            }
            write_log("[PS3] Kickstart %d scartato '%s' (dimensione errata)\n", index, cand);
        }
    }
    if (!found && kickstarts_rom_names[index] && kickstarts_rom_names[index][0]) {
        if (ps3_find_kickstart_file(kickstarts_rom_names[index], cand, sizeof(cand))) {
            if (ps3_kickstart_size_ok(index, cand)) {
                snprintf(romfile, sizeof(romfile), "%s", cand);
                found = 1;
            } else {
                write_log("[PS3] Kickstart %d scartato '%s' (dimensione errata)\n", index, cand);
            }
        }
    }
    if (!found && index == 3) {
        static const char *const hd_fallbacks[] = {
            "kick40063.A600", "kick40063.a600", "amiga-os-310-a600.rom",
            "kick39106.A1200", "kick39106.a1200", "amiga-os-300-a1200.rom", "kick30.rom", "kick3.0.rom",
            "kick39106.A4000", "kick39106.a4000", "amiga-os-300-a4000.rom",
            "kick37350.A600", "kick37350.a600", "amiga-os-205-a600.rom", "kick205.rom",
            "kick20.rom", "kick37175.A500", "kick37175.a500", "amiga-os-204.rom",
            "kick40060.CD32", "kick40060.cd32", "amiga-os-310-cd32.rom",
            "kick13.rom", "kick34005.A500", "amiga-os-130.rom",
            NULL
        };
        for (int f = 0; hd_fallbacks[f]; f++) {
            if (ps3_find_kickstart_file(hd_fallbacks[f], cand, sizeof(cand)) &&
                ps3_kickstart_size_ok(index, cand)) {
                snprintf(romfile, sizeof(romfile), "%s", cand);
                found = 1;
                write_log("[PS3] Kickstart 3 fallback resolved to '%s'\n", romfile);
                break;
            }
        }
    }

    extfile[0] = '\0';
    if (extended_rom_names[index][0] != '\0') {
        if (!ps3_find_kickstart_file(extended_rom_names[index], extfile, sizeof(extfile))) {
            if (index == 5) {
                static const char *const aros_ext_aliases[] = {
                    "aros-ext.bin", "aros.rom.ext", NULL
                };
                for (int e = 0; aros_ext_aliases[e] && extfile[0] == '\0'; e++)
                    ps3_find_kickstart_file(aros_ext_aliases[e], extfile, sizeof(extfile));
            } else if (index == 6) {
                static const char *const cd32_ext_aliases[] = {
                    "kick40060.CD32.ext", "kick40060.cd32.ext",
                    "amiga-os-310-cd32-ext.rom", "amiga-os-310-cd32.ext",
                    "cd32.ext", "CD32.ext", "kick31.ext", "kick31cd32.ext",
                    "cd32ext.rom", "cd32_ext.rom", "kick40060.CD32.ext.rom",
                    "CD32 Extended-ROM", "CD32.ROM.ext", "kick40060.ext", "ext_cd32.rom",
                    "CD32 Extended-ROM rev 40.60 (1993)(Commodore)(CD32).rom",
                    "CD32-Extended-ROM-r40.60-1993-Commodore-CD32.rom", NULL
                };
                for (int e = 0; cd32_ext_aliases[e] && extfile[0] == '\0'; e++)
                    ps3_find_kickstart_file(cd32_ext_aliases[e], extfile, sizeof(extfile));
            } else if (index == 17) {
                static const char *const cdtv_ext_aliases[] = {
                    "amiga-os-130-cdtv-ext.rom", "amiga-os-130-cdtv.rom",
                    "cdtv.rom", "CDTV Extended-ROM", "kick34005.CDTV.rom", NULL
                };
                for (int e = 0; cdtv_ext_aliases[e] && extfile[0] == '\0'; e++)
                    ps3_find_kickstart_file(cdtv_ext_aliases[e], extfile, sizeof(extfile));
            }
        }
    }
    // CD32 needs the Extended ROM too: without it the emulated machine
    // stops on a green screen. A combined 1MB kick40060.CD32 already
    // contains it (memory.cpp loads the second half from romfile), so
    // only warn when the ROM is 512KB and the .ext is really missing.
    if (index == 6 && found) {
        struct stat rst;
        long rsz = (stat(romfile, &rst) == 0) ? (long)rst.st_size : 0;
        if (rsz > 700000) {
            write_log("[PS3] Kickstart 6: combined 1MB ROM, no separate Extended needed\n");
        } else if (extfile[0] == '\0') {
            write_log("[PS3] Kickstart 6: Extended ROM missing (kick40060.CD32.ext)\n");
            snprintf(romfile, sizeof(romfile), "/dev_hdd0/uae4all/kickstarts/%s", "kick40060.CD32");
            kickstart_warning = 1;
            return 0;
        } else {
            write_log("[PS3] Kickstart 6 extended: %s\n", extfile);
        }
    }

    if (!found) {
        snprintf(romfile, sizeof(romfile), "/dev_hdd0/uae4all/kickstarts/%s", kickstarts_rom_names[index] ? kickstarts_rom_names[index] : "kick13.rom");
        kickstart_warning = 1;
        write_log("[PS3] Kickstart %d unavailable (looked for %s)\n",
                  index, kickstarts_rom_names[index] ? kickstarts_rom_names[index] : "?");
        return 0;
    }
    write_log("[PS3] Kickstart %d found: %s\n", index, romfile);
    kickstart_warning = 0;
    uae4all_rom_reinit();
    if (load_rom)
        return vita_load_kickstart(romfile);
    return 1;
}

void ps3_view_floppy(Ps3InputState *input, int *selected_item)
{
    static bool s_swap_active = false;
    static int s_circle_cooldown = 0;
    if (s_circle_cooldown > 0)
        s_circle_cooldown--;
    for (int i = 0; i < 4; i++)
        disk_set_write_protect(i, mainMenu_floppyWriteProtect[i]);
    const int total_items = 8;
    if (*selected_item < 0) *selected_item = 0;
    if (*selected_item >= total_items) *selected_item = total_items - 1;

    if (input->pressed & PS3_BTN_UP) {
        (*selected_item)--;
        if (*selected_item < 0) *selected_item = total_items - 1;
    }
    if (input->pressed & PS3_BTN_DOWN) {
        (*selected_item)++;
        if (*selected_item >= total_items) *selected_item = 0;
    }

    if (input->pressed & PS3_BTN_LEFT) {
        if (*selected_item == 7) {
            mainMenu_diskSoundVolume -= 5;
            if (mainMenu_diskSoundVolume < 0) mainMenu_diskSoundVolume = 0;
            disk_sound_set_volume(mainMenu_diskSoundVolume);
        }
    }
    if (input->pressed & PS3_BTN_RIGHT) {
        if (*selected_item == 7) {
            mainMenu_diskSoundVolume += 5;
            if (mainMenu_diskSoundVolume > 100) mainMenu_diskSoundVolume = 100;
            disk_sound_set_volume(mainMenu_diskSoundVolume);
        }
    }

    if (input->pressed & PS3_BTN_CROSS) {
        if (*selected_item >= 0 && *selected_item < 4) {
            if (mainMenu_drives < *selected_item + 1)
                mainMenu_drives = *selected_item + 1;

            char new_file[512];
            new_file[0] = '\0';
            int res = ps3_gui_run_browser(new_file, currentDir, *selected_item);
            input->pressed = 0;
            s_circle_cooldown = 3;
            if (res == 1) {
                if (!emulating)
                    ps3_unmount_cd32();
                write_log("[PS3] floppy: selected DF%d path=%s (emulating=%d)\n", *selected_item, new_file, emulating);
                if (*selected_item == 0) {
                    const char *dot = strrchr(new_file, '.');
                    if (dot && strcasecmp(dot, ".m3u") == 0) {
                        m3u_load(new_file);
                    } else {
                        m3u_clear();
                        copy_drive_path(uae4all_image_file0, new_file);
                    }
                    if (!emulating) {
                        mainMenu_whdload_game[0] = '\0';
                        uae4all_hard_dir[0] = '\0';
                        mainMenu_bootHD = 0;
                        reset_hdConf();
                        ps3_apply_floppy_model(0);
                    }
                }
                if (*selected_item == 1) copy_drive_path(uae4all_image_file1, new_file);
                if (*selected_item == 2) copy_drive_path(uae4all_image_file2, new_file);
                if (*selected_item == 3) copy_drive_path(uae4all_image_file3, new_file);
                write_log("[PS3] floppy: updating emulator paths\n");
                gui_update();
                write_log("[PS3] floppy: emulator paths updated\n");
            } else if (res == 2) {
                if (*selected_item == 0) {
                    m3u_clear();
                    uae4all_image_file0[0] = '\0';
                }
                if (*selected_item == 1) uae4all_image_file1[0] = '\0';
                if (*selected_item == 2) uae4all_image_file2[0] = '\0';
                if (*selected_item == 3) uae4all_image_file3[0] = '\0';
                gui_update();
            }
        } else if (*selected_item == 6) {
            m3u_clear();
            char temp[256];
            strncpy(temp, uae4all_image_file0, 255); temp[255] = '\0';
            strncpy(uae4all_image_file0, uae4all_image_file1, 255); uae4all_image_file0[255] = '\0';
            strncpy(uae4all_image_file1, temp, 255); uae4all_image_file1[255] = '\0';
            s_swap_active = !s_swap_active;
            gui_update();
        } else if (*selected_item == 4) {
            if (mainMenu_floppyspeed == 100) mainMenu_floppyspeed = 200;
            else if (mainMenu_floppyspeed == 200) mainMenu_floppyspeed = 400;
            else if (mainMenu_floppyspeed == 400) mainMenu_floppyspeed = 800;
            else mainMenu_floppyspeed = 100;
        } else if (*selected_item == 5) {
            m3u_clear();
            uae4all_image_file0[0] = '\0';
            uae4all_image_file1[0] = '\0';
            uae4all_image_file2[0] = '\0';
            uae4all_image_file3[0] = '\0';
            gui_update();
        } else if (*selected_item == 7) {
            mainMenu_diskSoundVolume = (mainMenu_diskSoundVolume >= 100) ? 0 : (mainMenu_diskSoundVolume + 25);
            disk_sound_set_volume(mainMenu_diskSoundVolume);
        }
    }

    if (input->pressed & PS3_BTN_TRIANGLE) {
        if (*selected_item == 0) {
            m3u_clear();
            uae4all_image_file0[0] = '\0';
        }
        if (*selected_item == 1) uae4all_image_file1[0] = '\0';
        if (*selected_item == 2) uae4all_image_file2[0] = '\0';
        if (*selected_item == 3) uae4all_image_file3[0] = '\0';
        gui_update();
    }

    if ((input->pressed & PS3_BTN_CIRCLE) && s_circle_cooldown == 0) {
        if (*selected_item >= 0 && *selected_item <= 3) {
            mainMenu_floppyWriteProtect[*selected_item] = !mainMenu_floppyWriteProtect[*selected_item];
            disk_set_write_protect(*selected_item, mainMenu_floppyWriteProtect[*selected_item]);
        }
    }

    if (input->pressed & PS3_BTN_SQUARE) {
        if (uae4all_image_file0[0] != '\0') {
            mainMenu_whdload_game[0] = '\0';
            uae4all_hard_dir[0] = '\0';
            mainMenu_bootHD = 0;
            reset_hdConf();
            ApplyAutomaticGamePreset(0);
            ps3_set_kickstart(kickstart, 0);
        }
        mainMenu_case = MAIN_MENU_CASE_RESET;
    }

    float card_x = 20.0f;
    float card_w = PS3_SCREEN_W - 40.0f;
    const float start_y = PS3_LIST_START_Y;
    float slot_h = 56.0f;

    char *drive_files[4] = { uae4all_image_file0, uae4all_image_file1, uae4all_image_file2, uae4all_image_file3 };
    const char *drive_labels[4] = { "DF0: Internal Floppy Drive", "DF1: External Floppy Drive 1", "DF2: External Floppy Drive 2", "DF3: External Floppy Drive 3" };

    for (int i = 0; i < 4; i++) {
        float cy = start_y + (float)i * (slot_h + 8.0f);
        bool focused = (*selected_item == i);
        bool has_disk = (drive_files[i][0] != '\0');

        ps3_draw_card(card_x, cy, card_w, slot_h, focused, false);

        char drive_tag[16];
        if (i == 0 && g_m3u.is_active) {
            snprintf(drive_tag, sizeof(drive_tag), "M3U %d/%d", g_m3u.current_disk + 1, g_m3u.disk_count);
            ps3_draw_badge(card_x + 14.0f, cy + 18.0f, drive_tag, RGBA8(0, 140, 220, 255), PS3_COLOR_TEXT_WHITE);
        } else {
            snprintf(drive_tag, sizeof(drive_tag), "DF%d", i);
            ps3_draw_badge(card_x + 14.0f, cy + 18.0f, drive_tag, has_disk ? PS3_COLOR_AMIGA_RED : RGBA8(40, 50, 70, 255), PS3_COLOR_TEXT_WHITE);
        }

        float text_offset_x = (i == 0 && g_m3u.is_active) ? 96.0f : 64.0f;
        if (i == 0 && g_m3u.is_active) {
            char m3u_lbl[128];
            snprintf(m3u_lbl, sizeof(m3u_lbl), "DF0: M3U Multi-Disk (Disk %d of %d)", g_m3u.current_disk + 1, g_m3u.disk_count);
            ps3_draw_text(card_x + text_offset_x, cy + 10.0f, focused ? PS3_COLOR_TEXT_WHITE : PS3_COLOR_TEXT_MUTED, 0.85f, m3u_lbl);

            char filename_buf[128];
            ps3_truncate_text(g_m3u.disk_names[g_m3u.current_disk], card_w - 220.0f, 1.00f, filename_buf, sizeof(filename_buf));
            unsigned int file_col = focused ? PS3_COLOR_TEXT_WHITE : RGBA8(230, 240, 255, 255);
            ps3_draw_text(card_x + text_offset_x, cy + 28.0f, file_col, 1.00f, filename_buf);
        } else {
            ps3_draw_text(card_x + text_offset_x, cy + 10.0f, focused ? PS3_COLOR_TEXT_WHITE : PS3_COLOR_TEXT_MUTED, 0.85f, drive_labels[i]);

            char filename_buf[128];
            ps3_truncate_text(get_filename_only(drive_files[i]), card_w - 200.0f, 1.00f, filename_buf, sizeof(filename_buf));
            unsigned int file_col = has_disk ? (focused ? PS3_COLOR_TEXT_WHITE : RGBA8(230, 240, 255, 255)) : PS3_COLOR_TEXT_DIM;
            ps3_draw_text(card_x + text_offset_x, cy + 28.0f, file_col, 1.00f, filename_buf);
        }

        ps3_draw_led(card_x + card_w - 180.0f, cy + 18.0f, mainMenu_floppyWriteProtect[i] ? "PROT" : "RW", true, mainMenu_floppyWriteProtect[i] ? PS3_COLOR_AMIGA_RED : PS3_COLOR_AMIGA_GREEN);

        ps3_draw_led(card_x + card_w - 110.0f, cy + 18.0f, has_disk ? "LOADED" : "EMPTY", has_disk, PS3_COLOR_AMIGA_ORANGE);
    }

    float spd_y = start_y + 4.0f * (slot_h + 8.0f);
    char spd_str[32];
    snprintf(spd_str, sizeof(spd_str), "%dx (%s)", mainMenu_floppyspeed / 100, (mainMenu_floppyspeed == 100) ? "1x Standard" : "Turbo");
    ps3_draw_selector_item(card_x, spd_y, (card_w * 0.5f) - 6.0f, 44.0f, "Floppy Speed", spd_str, *selected_item == 4);

    ps3_draw_button_item(card_x + (card_w * 0.5f) + 6.0f, spd_y, (card_w * 0.5f) - 6.0f, 44.0f, "Eject All Disks", NULL, "EJECT", *selected_item == 5, false);

    float swap_y = spd_y + 50.0f;
    ps3_draw_switch_item(card_x, swap_y, card_w, 38.0f, "Swap DF0 / DF1", s_swap_active, *selected_item == 6);

    float vol_y = swap_y + 44.0f;
    ps3_draw_slider_item(card_x, vol_y, card_w, 42.0f, "Floppy Sound Volume", mainMenu_diskSoundVolume, 0, 100, "%", *selected_item == 7);
}

static int s_hdf_mgr_slot = -1;
static int s_hdf_mgr_item = 0;
static int s_hdf_mgr_analyzed = 0;
static HdfInfo s_hdf_mgr_info;
static int s_hdf_mgr_creating = 0;
static int s_hdf_mgr_create_idx = 3;

static const int s_hdf_create_sizes[] = { 32, 64, 128, 256, 512, 1024, 2048 };
#define HDF_CREATE_SIZE_COUNT ((int)(sizeof(s_hdf_create_sizes) / sizeof(s_hdf_create_sizes[0])))

static void hdf_create_blank_into_slot(int slot)
{
    char path[512];
    int size_mb = s_hdf_create_sizes[s_hdf_mgr_create_idx];

    int n = 1;
    for (;;) {
        if (n == 1)
            snprintf(path, sizeof(path), "./data/blank_%dMB.hdf", size_mb);
        else
            snprintf(path, sizeof(path), "./data/blank_%dMB_%d.hdf", size_mb, n);
        FILE *test = fopen(path, "rb");
        if (!test)
            break;
        fclose(test);
        n++;
    }

    char err[256];
    err[0] = '\0';
    if (hdf_create_blank(path, (unsigned long)size_mb, err, sizeof(err)) != 0) {
        ps3_show_message_box("HDF Creation Failed", err[0] ? err : "Unknown error", "OK (A)");
        return;
    }

    char *hdf_files[4] = { uae4all_hard_file0, uae4all_hard_file1, uae4all_hard_file2, uae4all_hard_file3 };
    ps3_reset_media_state();
    strncpy(hdf_files[slot], path, 255);
    hdf_files[slot][255] = '\0';
    uae4all_hard_file_ro[slot] = 0;
    make_hard_file_cfg_line(hdf_files[slot]);
    mainMenu_whdload_game[0] = '\0';
    mainMenu_bootHD = 2;
    reset_hdConf();
    ApplyAutomaticGamePreset(1);
    bReloadKickstart = 1;
    gui_update();

    char msg[320];
    snprintf(msg, sizeof(msg),
        "Blank %d MB HDF created and mounted in slot %d.\nFormat it from Workbench (e.g. via HD Toolbox) before use.",
        size_mb, slot + 1);
    ps3_show_message_box("Blank HDF Created", msg, "OK (A)");
}

void ps3_view_hdf_manager(Ps3InputState *input, int *selected_item)
{
    const int slot = s_hdf_mgr_slot;
    const int total_items = 5;

    char *hdf_files[4] = {
        uae4all_hard_file0, uae4all_hard_file1,
        uae4all_hard_file2, uae4all_hard_file3
    };

    if (slot < 0 || slot >= 4) {
        s_hdf_mgr_slot = -1;
        return;
    }

    if (!s_hdf_mgr_analyzed) {
        if (hdf_files[slot][0] == '\0') {
            memset(&s_hdf_mgr_info, 0, sizeof(s_hdf_mgr_info));
            s_hdf_mgr_info.valid = 0;
            strcpy(s_hdf_mgr_info.error, "No HDF mounted in this slot");
        } else if (!hdf_analyze(hdf_files[slot], &s_hdf_mgr_info)) {
            s_hdf_mgr_info.valid = 0;
        }
        s_hdf_mgr_analyzed = 1;
    }

    if (s_hdf_mgr_creating) {
        if (s_hdf_mgr_create_idx < 0) s_hdf_mgr_create_idx = 0;
        if (s_hdf_mgr_create_idx >= HDF_CREATE_SIZE_COUNT) s_hdf_mgr_create_idx = HDF_CREATE_SIZE_COUNT - 1;

        if (input->pressed & PS3_BTN_UP) {
            s_hdf_mgr_create_idx -= 2;
            if (s_hdf_mgr_create_idx < 0) s_hdf_mgr_create_idx = (s_hdf_mgr_create_idx + HDF_CREATE_SIZE_COUNT + 1) % HDF_CREATE_SIZE_COUNT;
        }
        if (input->pressed & PS3_BTN_DOWN) {
            s_hdf_mgr_create_idx += 2;
            if (s_hdf_mgr_create_idx >= HDF_CREATE_SIZE_COUNT) s_hdf_mgr_create_idx = (s_hdf_mgr_create_idx % 2 == 0) ? 0 : 1;
        }
        if (input->pressed & (PS3_BTN_LEFT | PS3_BTN_RIGHT)) {
            if (s_hdf_mgr_create_idx % 2 == 0) {
                if (s_hdf_mgr_create_idx + 1 < HDF_CREATE_SIZE_COUNT) s_hdf_mgr_create_idx++;
            } else {
                s_hdf_mgr_create_idx--;
            }
        }
        if (input->pressed & PS3_BTN_CIRCLE) {
            s_hdf_mgr_creating = 0;
            return;
        }
        if (input->pressed & PS3_BTN_CROSS) {
            s_hdf_mgr_creating = 0;
            hdf_create_blank_into_slot(slot);
            return;
        }

        float bx = 220.0f, by = 100.0f, bw = 520.0f, bh = 280.0f;
        ps3_draw_rounded_rect(0.0f, 0.0f, (float)PS3_SCREEN_W, (float)PS3_SCREEN_H, 0.0f, PS3_COLOR_OVERLAY_BG);
        ps3_draw_card_custom(bx, by, bw, bh, PS3_COLOR_HEADER, PS3_COLOR_FOCUS_BORDER);
        ps3_draw_text_centered(bx + bw * 0.5f, by + 14.0f, PS3_COLOR_AMIGA_RED, 1.0f, "Create Blank HDF");
        ps3_draw_text_centered(bx + bw * 0.5f, by + 38.0f, PS3_COLOR_TEXT_MUTED, 0.75f, "Select image capacity (format from Workbench later)");

        float col_w = (bw - 60.0f) * 0.5f;
        for (int i = 0; i < HDF_CREATE_SIZE_COUNT; i++) {
            int row = i / 2;
            int col = i % 2;
            float ix = bx + 22.0f + (float)col * (col_w + 16.0f);
            float iy = by + 60.0f + (float)row * 44.0f;
            char size_str[32];
            snprintf(size_str, sizeof(size_str), "%d MB", s_hdf_create_sizes[i]);
            ps3_draw_button_item(ix, iy, col_w, 38.0f, size_str, NULL, NULL, s_hdf_mgr_create_idx == i, false);
        }
        ps3_draw_text_centered(bx + bw * 0.5f, by + bh - 24.0f, PS3_COLOR_TEXT_MUTED, 0.75f, "A Create   B Cancel");
        return;
    }

    if (s_hdf_mgr_item < 0) s_hdf_mgr_item = 0;
    if (s_hdf_mgr_item >= total_items) s_hdf_mgr_item = total_items - 1;

    if (input->pressed & PS3_BTN_UP) {
        s_hdf_mgr_item--;
        if (s_hdf_mgr_item < 0) s_hdf_mgr_item = total_items - 1;
    }
    if (input->pressed & PS3_BTN_DOWN) {
        s_hdf_mgr_item++;
        if (s_hdf_mgr_item >= total_items) s_hdf_mgr_item = 0;
    }
    if (input->pressed & PS3_BTN_CIRCLE) {
        s_hdf_mgr_slot = -1;
        return;
    }

    if (input->pressed & PS3_BTN_CROSS) {
        if (s_hdf_mgr_item == 0) {
            if (hdf_files[slot][0] == '\0') {
                ps3_show_message_box("HDF Manager", "Mount an HDF first.", "OK (A)");
            } else {
                uae4all_hard_file_ro[slot] = uae4all_hard_file_ro[slot] ? 0 : 1;
                reset_hdConf();
                bReloadKickstart = 1;
                gui_update();
            }
        } else if (s_hdf_mgr_item == 1) {
            if (hdf_files[slot][0] == '\0') {
                ps3_show_message_box("HDF Manager", "Mount an HDF first.", "OK (A)");
            } else if (ps3_show_confirm_box("Backup HDF",
                    "Create a backup copy in ./data/backups/?",
                    "Backup (A)", "Cancel (B)")) {
                char err[256];
                int rc = hdf_backup(hdf_files[slot], "./data/backups", err, sizeof(err));
                if (rc == 0)
                    ps3_show_message_box("Backup Complete", "The HDF was backed up successfully.", "OK (A)");
                else
                    ps3_show_message_box("Backup Failed", err, "OK (A)");
            }
        } else if (s_hdf_mgr_item == 2) {
            if (hdf_files[slot][0] == '\0') {
                ps3_show_message_box("HDF Manager", "Nothing mounted in this slot.", "OK (A)");
            } else if (ps3_show_confirm_box("Unmount HDF",
                    "Remove this hard-disk image from the slot?",
                    "Unmount (A)", "Cancel (B)")) {
                hdf_files[slot][0] = 0;
                uae4all_hard_file_ro[slot] = 0;
                reset_hdConf();
                bReloadKickstart = 1;
                gui_update();
                s_hdf_mgr_slot = -1;
                return;
            }
        } else if (s_hdf_mgr_item == 3) {
            s_hdf_mgr_create_idx = 3;
            s_hdf_mgr_creating = 1;
        } else if (s_hdf_mgr_item == 4) {
            s_hdf_mgr_slot = -1;
            return;
        }
    }

    float card_x = 20.0f;
    float card_w = PS3_SCREEN_W - 40.0f;
    float start_y = 86.0f;

    char title[64];
    snprintf(title, sizeof(title), "HDF Manager - Slot %d", slot + 1);
    ps3_draw_text(card_x, start_y, PS3_COLOR_TEXT_WHITE, 1.05f, title);
    ps3_draw_text_right(PS3_SCREEN_W - 20.0f, start_y + 4.0f, PS3_COLOR_TEXT_MUTED, 0.75f, "X Select   O Back");

    float info_y = start_y + 22.0f;
    float info_h = 84.0f;
    ps3_draw_card_custom(card_x, info_y, card_w, info_h, PS3_COLOR_CARD, PS3_COLOR_CARD_BORDER);

    const HdfInfo *inf = &s_hdf_mgr_info;
    char line[128];
    float ly = info_y + 10.0f;
    float lh = 19.0f;

    if (hdf_files[slot][0] == '\0') {
        snprintf(line, sizeof(line), "File: (none)");
        ps3_draw_text(card_x + 16.0f, ly, PS3_COLOR_TEXT_WHITE, 0.82f, line);
        ly += lh;
        snprintf(line, sizeof(line), "Create a blank HDF below and it will be mounted in this slot.");
        ps3_draw_text(card_x + 16.0f, ly, PS3_COLOR_TEXT_MUTED, 0.78f, line);
        ly += lh;
        snprintf(line, sizeof(line), "Status: Empty slot");
        ps3_draw_text(card_x + 16.0f, ly, PS3_COLOR_AMIGA_ORANGE, 0.80f, line);
    } else {
        snprintf(line, sizeof(line), "File: %s", get_filename_only(hdf_files[slot]));
        ps3_draw_text(card_x + 16.0f, ly, PS3_COLOR_TEXT_WHITE, 0.85f, line);
        ly += lh;

        if (inf->size >= 1048576UL)
            snprintf(line, sizeof(line), "Size: %.2f MB (%lu bytes)  |  Mounted: %s  |  Writable: %s",
                     (double)inf->size / 1048576.0, inf->size,
                     uae4all_hard_file_ro[slot] ? "Read-Only" : "Read/Write",
                     inf->is_readonly ? "No" : "Yes");
        else
            snprintf(line, sizeof(line), "Size: %lu bytes  |  Mounted: %s",
                     inf->size, uae4all_hard_file_ro[slot] ? "Read-Only" : "Read/Write");
        ps3_draw_text(card_x + 16.0f, ly, PS3_COLOR_TEXT_MUTED, 0.76f, line);
        ly += lh;

        snprintf(line, sizeof(line), "Geometry: Sectors %d | Surfaces %d | Cylinders %d | Filesystem: %s",
                 inf->sectors_per_track, inf->surfaces, inf->cylinders, inf->filesystem);
        ps3_draw_text(card_x + 16.0f, ly, PS3_COLOR_TEXT_MUTED, 0.76f, line);
        ly += lh;

        if (inf->valid && inf->is_rdb) {
            snprintf(line, sizeof(line), "Partitions: %s (%d bootable)",
                     inf->partition_names[0] ? inf->partition_names : "?", inf->bootable_partitions);
            ps3_draw_text(card_x + 16.0f, ly, PS3_COLOR_TEXT_MUTED, 0.76f, line);
            ly += lh;
        }

        if (inf->valid)
            snprintf(line, sizeof(line), "Status: %s",
                     inf->bootable_partitions ? "Bootable hard disk image" : "Mounted, but no Amiga DOS boot block (format it from Workbench)");
        else
            snprintf(line, sizeof(line), "Status: %s", inf->error);
        ps3_draw_text(card_x + 16.0f, ly, inf->valid ? PS3_COLOR_AMIGA_GREEN : PS3_COLOR_DANGER, 0.80f, line);
    }

    float ay = info_y + info_h + 8.0f;
    float item_h = 40.0f;

    ps3_draw_switch_item(card_x, ay, card_w, item_h, "Read-Only Mode",
        uae4all_hard_file_ro[slot] != 0, s_hdf_mgr_item == 0);
    ay += item_h + 5.0f;

    ps3_draw_button_item(card_x, ay, card_w, item_h, "Backup HDF",
        "Create a copy in ./data/backups/", "BACKUP", s_hdf_mgr_item == 1, false);
    ay += item_h + 5.0f;

    ps3_draw_button_item(card_x, ay, card_w, item_h, "Unmount HDF",
        "Remove this image from the slot", "UNMOUNT", s_hdf_mgr_item == 2, false);
    ay += item_h + 5.0f;

    ps3_draw_button_item(card_x, ay, card_w, item_h, "Create Blank HDF",
        "Make a new empty image and mount it in this slot", "CREATE", s_hdf_mgr_item == 3, false);
    ay += item_h + 5.0f;

    ps3_draw_button_item(card_x, ay, card_w, item_h, "Back",
        "Return to hard disk slots", "BACK", s_hdf_mgr_item == 4, false);
}


void ps3_view_hard_disk(Ps3InputState *input, int *selected_item)
{
    if (s_hdf_mgr_slot >= 0) {
        ps3_view_hdf_manager(input, selected_item);
        return;
    }

    const int total_items = 6;
    if (*selected_item < 0) *selected_item = 0;
    if (*selected_item >= total_items) *selected_item = total_items - 1;

    if (input->pressed & PS3_BTN_UP) {
        (*selected_item)--;
        if (*selected_item < 0) *selected_item = total_items - 1;
    }
    if (input->pressed & PS3_BTN_DOWN) {
        (*selected_item)++;
        if (*selected_item >= total_items) *selected_item = 0;
    }

    char *hdf_files[4] = {
        uae4all_hard_file0, uae4all_hard_file1,
        uae4all_hard_file2, uae4all_hard_file3
    };

    if (input->pressed & PS3_BTN_CROSS) {
        if (*selected_item >= 0 && *selected_item < 4) {
            char new_file[512];
            new_file[0] = 0;
            int res = ps3_gui_run_browser(new_file, currentDir, 4 + *selected_item);
            if (res == 1) {
                HdfInfo mounted_info;
                copy_drive_path(hdf_files[*selected_item], new_file);
                make_hard_file_cfg_line(hdf_files[*selected_item]);
                mainMenu_whdload_game[0] = '\0';
                mainMenu_bootHD = 2;
                reset_hdConf();
                ApplyAutomaticGamePreset(1);
                bReloadKickstart = 1;
                gui_update();
                /* Tell the user up-front when the image cannot boot, instead
                 * of letting the Amiga show its "no disk" screen later. */
                if (hdf_analyze(hdf_files[*selected_item], &mounted_info)) {
                    if (mounted_info.is_rdb) {
                        char info_msg[360];
                        snprintf(info_msg, sizeof(info_msg),
                                 "RDB image mounted: %d partition(s) (%s), %d bootable.",
                                 mounted_info.partition_count,
                                 mounted_info.partition_names[0] ? mounted_info.partition_names : "?",
                                 mounted_info.bootable_partitions);
                        ps3_show_message_box("HDF Mounted", info_msg, "OK (A)");
                    } else if (mounted_info.bootable_partitions == 0) {
                        ps3_show_message_box("HDF Mounted",
                            "The image was mounted, but it has no Amiga DOS boot block: the Amiga will report \"no disk\".\nBoot a Workbench floppy and format it, or use a bootable image.",
                            "OK (A)");
                    }
                } else {
                    char err_msg[320];
                    snprintf(err_msg, sizeof(err_msg), "The image was mounted but could not be analyzed:\n%s",
                             mounted_info.error[0] ? mounted_info.error : "unknown error");
                    ps3_show_message_box("HDF Warning", err_msg, "OK (A)");
                }
            } else if (res == 2) {
                hdf_files[*selected_item][0] = 0;
                reset_hdConf();
                bReloadKickstart = 1;
                gui_update();
            }
        } else if (*selected_item == 4) {
            mainMenu_bootHD = (mainMenu_bootHD + 1) % 3;
            reset_hdConf();
            bReloadKickstart = 1;
        } else if (*selected_item == 5) {
            for (int i = 0; i < 4; i++)
                hdf_files[i][0] = 0;
            mainMenu_bootHD = 0;
            reset_hdConf();
            bReloadKickstart = 1;
            gui_update();
        }
    }

    if ((input->pressed & PS3_BTN_TRIANGLE) && *selected_item < 4) {
        hdf_files[*selected_item][0] = 0;
        reset_hdConf();
        bReloadKickstart = 1;
        gui_update();
    }

    if ((input->pressed & PS3_BTN_SQUARE) && *selected_item >= 0 && *selected_item < 4) {
        s_hdf_mgr_slot = *selected_item;
        s_hdf_mgr_item = 0;
        s_hdf_mgr_analyzed = 0;
        s_hdf_mgr_creating = 0;
    }

    float card_x = 20.0f;
    float card_w = PS3_SCREEN_W - 40.0f;
    const float start_y = PS3_LIST_START_Y;
    float item_h = 56.0f;

    for (int i = 0; i < 4; i++) {
        float y = start_y + (float)i * (item_h + 8.0f);
        bool focused = (*selected_item == i);
        bool loaded = hdf_files[i][0] != 0;
        char slot_title[64];
        char file_name[128];
        snprintf(slot_title, sizeof(slot_title), "HDF%d: Hard Disk Slot %d", i + 1, i + 1);
        ps3_truncate_text(get_filename_only(hdf_files[i]), card_w - 230.0f, 0.95f, file_name, sizeof(file_name));
        ps3_draw_card(card_x, y, card_w, item_h, focused, loaded);
        char slot_tag[16];
        snprintf(slot_tag, sizeof(slot_tag), "HDF%d", i + 1);
        ps3_draw_badge(card_x + 14.0f, y + 17.0f, slot_tag,
            loaded ? PS3_COLOR_AMIGA_RED : RGBA8(40, 50, 70, 255), PS3_COLOR_TEXT_WHITE);
        ps3_draw_text(card_x + 78.0f, y + 10.0f, focused ? PS3_COLOR_TEXT_WHITE : PS3_COLOR_TEXT_MUTED,
            0.85f, slot_title);
        ps3_draw_text(card_x + 78.0f, y + 29.0f,
            loaded ? PS3_COLOR_TEXT_WHITE : PS3_COLOR_TEXT_DIM, 0.95f, file_name);
        ps3_draw_led(card_x + card_w - 110.0f, y + 18.0f, loaded ? "LOADED" : "EMPTY",
            loaded, PS3_COLOR_AMIGA_ORANGE);
    }

    const char *boot_names[3] = { "Off", "HD Directory", "HDF Files" };
    ps3_draw_selector_item(card_x, start_y + 4.0f * (item_h + 8.0f), card_w,
        item_h, "Boot HD", boot_names[mainMenu_bootHD % 3], *selected_item == 4);
    ps3_draw_button_item(card_x, start_y + 5.0f * (item_h + 8.0f), card_w, 44.0f,
        "Eject All HDF Files", "Remove all mounted hard-disk images", "EJECT",
        *selected_item == 5, false);
}

static float s_whdload_cover_angle = 0.0f;

static void whdload_cover_unload(void)
{
    cover_mgr_unload();
}

static void whdload_cover_load(const char *game_name)
{
    if (!game_name || game_name[0] == '\0') {
        cover_mgr_unload();
        return;
    }
    cover_mgr_load(game_name, NULL);
}


static void whdload_install_flow(int *selected_item);

#define WHDLOAD_FAVORITES_FILE "./data/favorites.txt"
#define WHDLOAD_RECENT_FILE    "./data/recent.txt"
#define MAX_WHDLOAD_GAMES      4096

static int s_whdload_filter = 0;
static char s_whdload_last_game[128] = "";
static bool s_whd_games_loaded = false;

static char s_favs[MAX_WHDLOAD_GAMES][128];
static int s_fav_count = 0;
static char s_recents[20][128];
static int s_recent_count = 0;
static bool s_whdload_meta_loaded = false;

static void whdload_read_name_list(const char *path, char names[][128], int max_names, int *count)
{
    *count = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return;
    char line[160];
    while (*count < max_names && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        strncpy(names[*count], line, 127);
        names[*count][127] = '\0';
        (*count)++;
    }
    fclose(f);
}

static void whdload_refresh_meta(void)
{
    if (s_whdload_meta_loaded) return;
    whdload_read_name_list(WHDLOAD_FAVORITES_FILE, s_favs, MAX_WHDLOAD_GAMES, &s_fav_count);
    whdload_read_name_list(WHDLOAD_RECENT_FILE, s_recents, 20, &s_recent_count);
    s_whdload_meta_loaded = true;
}

static bool whdload_is_favorite(const char *game)
{
    for (int i = 0; i < s_fav_count; i++) {
        if (strcmp(s_favs[i], game) == 0) return true;
    }
    return false;
}

static bool whdload_is_recent(const char *game)
{
    for (int i = 0; i < s_recent_count; i++) {
        if (strcmp(s_recents[i], game) == 0) return true;
    }
    return false;
}

static void whdload_toggle_favorite(const char *game)
{
    static char lines[MAX_WHDLOAD_GAMES][128];
    int count = 0;
    bool was_favorite = false;
    FILE *f = fopen(WHDLOAD_FAVORITES_FILE, "rb");
    if (f) {
        char line[160];
        while (count < MAX_WHDLOAD_GAMES && fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] == '\0') continue;
            if (strcmp(line, game) == 0) {
                was_favorite = true;
                continue;
            }
            strncpy(lines[count], line, 127);
            lines[count][127] = '\0';
            count++;
        }
        fclose(f);
    }
    if (!was_favorite && count < MAX_WHDLOAD_GAMES) {
        strncpy(lines[count], game, 127);
        lines[count][127] = '\0';
        count++;
    }
    f = fopen(WHDLOAD_FAVORITES_FILE, "wb");
    if (f) {
        for (int i = 0; i < count; i++)
            fprintf(f, "%s\n", lines[i]);
        fclose(f);
        s_whdload_meta_loaded = false;
        whdload_refresh_meta();
    }
}

static void whdload_mark_recent(const char *game)
{
    char lines[20][128];
    int count = 0;
    FILE *f = fopen(WHDLOAD_RECENT_FILE, "rb");
    if (f) {
        char line[160];
        while (count < 20 && fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] && strcmp(line, game) != 0) {
                strncpy(lines[count], line, 127);
                lines[count][127] = '\0';
                count++;
            }
        }
        fclose(f);
    }
    f = fopen(WHDLOAD_RECENT_FILE, "wb");
    if (f) {
        fprintf(f, "%s\n", game);
        for (int i = 0; i < count; i++)
            fprintf(f, "%s\n", lines[i]);
        fclose(f);
        s_whdload_meta_loaded = false;
        whdload_refresh_meta();
    }
}

static int vita_has_inserted_floppy(void)
{
    return uae4all_image_file0[0] != '\0' || uae4all_image_file1[0] != '\0' ||
           uae4all_image_file2[0] != '\0' || uae4all_image_file3[0] != '\0';
}

static int vita_has_mounted_hdf(void)
{
    return uae4all_hard_file0[0] != '\0' || uae4all_hard_file1[0] != '\0' ||
           uae4all_hard_file2[0] != '\0' || uae4all_hard_file3[0] != '\0';
}

static void vita_eject_all_floppies(void)
{
    uae4all_image_file0[0] = '\0';
    uae4all_image_file1[0] = '\0';
    uae4all_image_file2[0] = '\0';
    uae4all_image_file3[0] = '\0';
    gui_update();
}

static void vita_eject_all_hdf(void)
{
    uae4all_hard_file0[0] = '\0';
    uae4all_hard_file1[0] = '\0';
    uae4all_hard_file2[0] = '\0';
    uae4all_hard_file3[0] = '\0';
    uae4all_hard_file_ro[0] = 0;
    uae4all_hard_file_ro[1] = 0;
    uae4all_hard_file_ro[2] = 0;
    uae4all_hard_file_ro[3] = 0;
    reset_hdConf();
    gui_update();
}

static void ps3_unmount_cd32(void)
{
    cdrom_close_image();
    cdrom_set_cd32_mode(0);
}

static void ps3_reset_media_state(void)
{
    ps3_unmount_cd32();
    vita_eject_all_floppies();
    vita_eject_all_hdf();
    m3u_clear();
    mainMenu_whdload_game[0] = '\0';
    uae4all_hard_dir[0] = '\0';
    mainMenu_bootHD = 0;
    reset_hdConf();
}

int ps3_confirm_eject_for_hard_disk_launch(void)
{
    if (!vita_has_inserted_floppy())
        return 1;
    if (!ps3_show_confirm_box("Floppy Disk Detected",
            "A floppy disk is inserted. Eject it before launching this hard-disk game?",
            "Eject and Launch (A)", "Cancel Launch (B)"))
        return 0;
    vita_eject_all_floppies();
    return 1;
}

static int vita_confirm_eject_for_whdload_launch(void)
{
    int has_floppy = vita_has_inserted_floppy();
    int has_hdf = vita_has_mounted_hdf();
    if (!has_floppy && !has_hdf)
        return 1;
    const char *message;
    if (has_floppy && has_hdf)
        message = "A floppy disk and an HDF image are mounted. Eject them before launching this WHDLoad game?";
    else if (has_hdf)
        message = "An HDF image is mounted. Eject it before launching this WHDLoad game?";
    else
        message = "A floppy disk is inserted. Eject it before launching this WHDLoad game?";
    if (!ps3_show_confirm_box("Media Detected", message,
            "Eject and Launch (A)", "Cancel Launch (B)"))
        return 0;
    if (has_floppy)
        vita_eject_all_floppies();
    if (has_hdf)
        vita_eject_all_hdf();
    return 1;
}

static int ps3_whdload_preflight(const char *game)
{
    if (!ps3_set_kickstart(3, 0)) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "Kickstart 3.1 non trovato!\n\n"
                 "WHDLoad richiede la ROM Kickstart 3.1 (512 KB) per avviarsi.\n\n"
                 "Copia 'kick31.rom' o 'kick40068.A1200' in:\n"
                 "/dev_hdd0/uae4all/kickstarts/\n"
                 "(oppure su USB in uae4all/kickstarts/)");
        ps3_show_message_box("Kickstart Mancante", msg, "OK (X)");
        return 0;
    }

    if (ps3_whdload_prepare_launch(game))
        return 1;

    {
        char msg[420];
        const char *err = ps3_whdload_get_last_error();
        snprintf(msg, sizeof(msg), "The WHDLoad launch could not be prepared.\n%s\n\nNothing was started, the emulator keeps running.",
                 (err && err[0]) ? err : "unknown error");
        ps3_show_message_box("WHDLoad Error", msg, "OK (X)");
    }
    return 0;
}

static void whdload_ensure_game_dir(const char *game)
{
    mkdir(SAVE_PREFIX, 0777);
    char dir[256];
    snprintf(dir, sizeof(dir), "%s%s", SAVE_PREFIX, game);
    mkdir(dir, 0777);
}

static char whdload_get_entry_letter(const char *name)
{
    if (!name || name[0] == '\0') return ' ';
    int idx = 0;
    while (name[idx] != '\0' && (name[idx] == '[' || name[idx] == '(' || name[idx] == '_' || name[idx] == '.' || name[idx] == ' ' || name[idx] == '-')) {
        idx++;
    }
    if (name[idx] == '\0') return (char)toupper((unsigned char)name[0]);
    return (char)toupper((unsigned char)name[idx]);
}

static int whdload_find_next_letter_index(char games[][128], const int *vis_index, int vis_count, int cur_vis_idx, int direction)
{
    if (vis_count <= 1) return 0;
    if (cur_vis_idx < 0) cur_vis_idx = 0;
    if (cur_vis_idx >= vis_count) cur_vis_idx = vis_count - 1;
    char cur_char = whdload_get_entry_letter(games[vis_index[cur_vis_idx]]);

    if (direction > 0) {
        for (int i = cur_vis_idx + 1; i < vis_count; i++) {
            char c = whdload_get_entry_letter(games[vis_index[i]]);
            if (c != cur_char) {
                return i;
            }
        }
        return 0;
    } else {
        for (int i = cur_vis_idx - 1; i >= 0; i--) {
            char c = whdload_get_entry_letter(games[vis_index[i]]);
            if (c != cur_char) {
                while (i > 0 && whdload_get_entry_letter(games[vis_index[i - 1]]) == c) {
                    i--;
                }
                return i;
            }
        }
        return vis_count - 1;
    }
}

static void whdload_install_flow(int *selected_item)
{
    char archive_path[512];
    char installed_path[512];
    archive_path[0] = '\0';
    installed_path[0] = '\0';
    int result = ps3_gui_run_browser(archive_path, currentDir, 11);
    if (result == 1) {
        if (ps3_whdload_install_lha(archive_path, installed_path, sizeof(installed_path))) {
            const char *folder = strrchr(installed_path, '/');
            folder = folder ? folder + 1 : installed_path;
            if (folder && folder[0]) {
                whdload_ensure_game_dir(folder);
                strncpy(s_whdload_last_game, folder, sizeof(s_whdload_last_game) - 1);
                s_whdload_last_game[sizeof(s_whdload_last_game) - 1] = '\0';
                whdload_cover_load(folder);
                s_whdload_filter = 0;
                ps3_library_mark_dirty();

                if (selected_item) {
                    static char games[MAX_WHDLOAD_GAMES][128];
                    int game_count = ps3_whdload_list(games, MAX_WHDLOAD_GAMES);
                    for (int i = 0; i < game_count; i++) {
                        if (strcmp(games[i], folder) == 0) {
                            *selected_item = 5 + i;
                            break;
                        }
                    }
                }

                if (ps3_whdload_can_launch(folder)) {
                    char msg[384];
                    snprintf(msg, sizeof(msg), "\"%s\" has been installed successfully.\n\nDo you want to launch the game now or return to the WHDLoad menu?", folder);
                    if (ps3_show_confirm_box("Installation Complete", msg, "Launch Game", "WHDLoad Menu")) {
                        if (vita_confirm_eject_for_whdload_launch()) {
                            strncpy(mainMenu_whdload_game, folder, sizeof(mainMenu_whdload_game) - 1);
                            mainMenu_whdload_game[sizeof(mainMenu_whdload_game) - 1] = '\0';
                            whdload_ensure_game_dir(folder);
                            whdload_mark_recent(folder);

                            ps3_gui_draw_progress("Loading...", "Starting WHDLoad game...", 0.5f, folder);
                            strncpy(uae4all_hard_dir, ps3_whdload_root(), 255);
                            uae4all_hard_dir[255] = '\0';
                            if (!ps3_whdload_preflight(folder))
                                return;
                            ApplyWHDLoadPreset(folder);
                            ps3_set_kickstart(kickstart, 0);
                            bReloadKickstart = 1;

                            gui_update();
                            mainMenu_case = MAIN_MENU_CASE_RUN;
                            return;
                        }
                    }
                    return;
                }
            }
            s_whd_games_loaded = false;
            ps3_show_message_box("Installation Complete", "The LHA archive was extracted to the WHDLoad library.", "OK (A)");
        } else {
            char err_buf[300];
            snprintf(err_buf, sizeof(err_buf), "The LHA archive could not be extracted.\n%s", ps3_whdload_get_last_error());
            ps3_show_message_box("Installation Failed", err_buf, "OK (A)");
        }
    }
}

void ps3_view_whdload(Ps3InputState *input, int *selected_item)
{
    static char games[MAX_WHDLOAD_GAMES][128];
    static int s_whd_game_count = 0;
    if (!s_whd_games_loaded) {
        s_whd_game_count = ps3_whdload_list(games, MAX_WHDLOAD_GAMES);
        s_whd_games_loaded = true;
    }
    int game_count = s_whd_game_count;

    whdload_refresh_meta();

    static int vis_index[MAX_WHDLOAD_GAMES];
    int vis_count = 0;
    for (int k = 0; k < game_count; k++) {
        bool keep;
        if (s_whdload_filter == 1)
            keep = whdload_is_favorite(games[k]);
        else if (s_whdload_filter == 2)
            keep = whdload_is_recent(games[k]);
        else
            keep = true;
        if (keep)
            vis_index[vis_count++] = k;
    }

    const int total_items = 7 + vis_count;
    if (*selected_item < 0) *selected_item = 0;
    if (*selected_item >= total_items) *selected_item = total_items - 1;
    if (total_items <= 0) return;

    static int s_whd_hold_up = 0;
    static int s_whd_hold_down = 0;

    if (input->pressed & PS3_BTN_UP) {
        (*selected_item)--;
        if (*selected_item < 0) *selected_item = total_items - 1;
    }
    if (input->pressed & PS3_BTN_DOWN) {
        (*selected_item)++;
        if (*selected_item >= total_items) *selected_item = 0;
    }

    if (input->held & PS3_BTN_UP && total_items > 0) {
        s_whd_hold_up++;
        if (s_whd_hold_up > 12 && (s_whd_hold_up % 3 == 0)) {
            (*selected_item)--;
            if (*selected_item < 0) *selected_item = total_items - 1;
        }
    } else {
        s_whd_hold_up = 0;
    }

    if (input->held & PS3_BTN_DOWN && total_items > 0) {
        s_whd_hold_down++;
        if (s_whd_hold_down > 12 && (s_whd_hold_down % 3 == 0)) {
            (*selected_item)++;
            if (*selected_item >= total_items) *selected_item = 0;
        }
    } else {
        s_whd_hold_down = 0;
    }

    if (vis_count > 0 && (input->pressed & (PS3_BTN_L1 | PS3_BTN_R1 | PS3_BTN_LEFT | PS3_BTN_RIGHT))) {
        if (*selected_item >= 7) {
            int cur_vis = *selected_item - 7;
            int dir = (input->pressed & (PS3_BTN_R1 | PS3_BTN_RIGHT)) ? 1 : -1;
            int next_vis = whdload_find_next_letter_index(games, vis_index, vis_count, cur_vis, dir);
            *selected_item = 7 + next_vis;
        } else if (*selected_item < 7 && (input->pressed & (PS3_BTN_L1 | PS3_BTN_R1))) {
            *selected_item = 7;
        }
    }

    static int s_last_whd_selected_item = -1;
    if (*selected_item >= 7) {
        if (*selected_item != s_last_whd_selected_item) {
            s_last_whd_selected_item = *selected_item;
            const char *game = games[vis_index[*selected_item - 7]];
            strncpy(s_whdload_last_game, game, sizeof(s_whdload_last_game) - 1);
            s_whdload_last_game[sizeof(s_whdload_last_game) - 1] = '\0';
            whdload_cover_load(game);
        }
    } else {
        if (s_last_whd_selected_item != -1) {
            s_last_whd_selected_item = -1;
            whdload_cover_unload();
        }
    }

    if ((input->pressed & (PS3_BTN_LEFT | PS3_BTN_RIGHT)) && *selected_item == 3) {
        mainMenu_whdload_mode = (mainMenu_whdload_mode + ((input->pressed & PS3_BTN_RIGHT) ? 1 : 2)) % 3;
    }

    if ((input->pressed & (PS3_BTN_LEFT | PS3_BTN_RIGHT)) && *selected_item == 4) {
        mainMenu_whdload_blitter = (mainMenu_whdload_blitter + ((input->pressed & PS3_BTN_RIGHT) ? 1 : 3)) % 4;
    }

    if ((input->pressed & (PS3_BTN_LEFT | PS3_BTN_RIGHT)) && *selected_item == 6) {
        s_whdload_filter = (s_whdload_filter + ((input->pressed & PS3_BTN_RIGHT) ? 1 : -1) + 3) % 3;
        s_last_whd_selected_item = -1;
    }

    if (input->pressed & PS3_BTN_CROSS) {
        if (*selected_item == 0) {
            if (s_whdload_last_game[0] == '\0') {
                ps3_show_message_box("Download Cover", "Select a game first, then choose Download Cover Art.", "OK (X)");
            } else {
                char out_path[512];
                out_path[0] = '\0';
                int rc = vita_cover_download(s_whdload_last_game, out_path, sizeof(out_path));
                if (rc == 0) {
                    whdload_cover_load(s_whdload_last_game);
                    ps3_show_message_box("Cover Downloaded", "Boxart saved to:\n./data/covers/", "OK (X)");
                } else if (rc == -6) {
                    ps3_show_message_box("Cover Not Found", "No boxart for this game on the cover server.", "OK (X)");
                } else {
                    ps3_show_message_box("Download Failed", "Network error or no internet connection.\nServer: ./data/covers/source.txt", "OK (X)");
                }
            }
        } else if (*selected_item == 1) {
            whdload_install_flow(selected_item);
        } else if (*selected_item == 2) {
            char *args = ps3_kbd_get_str((char*)"WHDLoad Arguments:", mainMenu_whdload_args, 200, 0);
            if (args) {
                strncpy(mainMenu_whdload_args, args, sizeof(mainMenu_whdload_args) - 1);
                mainMenu_whdload_args[sizeof(mainMenu_whdload_args) - 1] = '\0';
                if (mainMenu_whdload_args[0])
                    ps3_show_message_box("WHDLoad Arguments", mainMenu_whdload_args, "OK (X)");
                else
                    ps3_show_message_box("WHDLoad Arguments", "Arguments cleared. Using default.", "OK (X)");
            }
        } else if (*selected_item == 3) {
            mainMenu_whdload_mode = (mainMenu_whdload_mode + 1) % 3;
        } else if (*selected_item == 4) {
            mainMenu_whdload_blitter = (mainMenu_whdload_blitter + 1) % 4;
        } else if (*selected_item == 5) {
            strncpy(uae4all_hard_dir, ps3_whdload_root(), 255);
            uae4all_hard_dir[255] = '\0';
            mainMenu_bootHD = 1;
            reset_hdConf();
            gui_update();
            mainMenu_whdload_game[0] = '\0';
            ps3_show_message_box("WHDLoad Directory", "The WHDLoad library is selected as the HD directory. A Workbench environment is required.", "OK (X)");
        } else if (*selected_item == 6) {
            s_whdload_filter = (s_whdload_filter + 1) % 3;
            s_last_whd_selected_item = -1;
        } else {
            const char *game_name = games[vis_index[*selected_item - 7]];
            if (ps3_whdload_can_launch(game_name)) {
                if (!vita_confirm_eject_for_whdload_launch())
                    return;

                strncpy(mainMenu_whdload_game, game_name, sizeof(mainMenu_whdload_game) - 1);
                mainMenu_whdload_game[sizeof(mainMenu_whdload_game) - 1] = '\0';
                whdload_ensure_game_dir(game_name);
                whdload_mark_recent(game_name);

                ps3_gui_draw_progress("Loading...", "Starting WHDLoad game...", 0.5f, game_name);
                strncpy(uae4all_hard_dir, ps3_whdload_root(), 255);
                uae4all_hard_dir[255] = '\0';
                if (!ps3_whdload_preflight(game_name))
                    return;
                ApplyWHDLoadPreset(game_name);
                ps3_set_kickstart(kickstart, 0);
                bReloadKickstart = 1;

                gui_update();
                mainMenu_case = MAIN_MENU_CASE_RUN;
            } else {
                ps3_show_message_box("WHDLoad Error", "No .slave file was found in the game directory.", "OK (X)");
            }
        }
    }

    if (input->pressed & PS3_BTN_SQUARE) {
        whdload_install_flow(selected_item);
    }

    if (input->pressed & PS3_BTN_TRIANGLE) {
        mainMenu_case = MAIN_MENU_CASE_RESET;
    }

    if ((input->pressed & PS3_BTN_MINUS) && *selected_item >= 7) {
        whdload_toggle_favorite(games[vis_index[*selected_item - 7]]);
    }

    float card_x = 20.0f;
    float card_w = 560.0f;
    const float start_y = PS3_LIST_START_Y;
    const float item_h = 56.0f;
    const float item_gap = 8.0f;
    const int visible_items = ps3_list_visible_rows(start_y, item_h, item_gap);
    int first_item = *selected_item >= visible_items ? *selected_item - visible_items + 1 : 0;

    for (int i = 0; i < visible_items; i++) {
        int item = first_item + i;
        if (item >= total_items) break;
        float y = start_y + (float)i * (item_h + item_gap);
        const char *title;
        const char *subtitle;
        const char *badge;
        unsigned int badge_col = PS3_COLOR_AMIGA_BLUE;

        if (item == 0) {
            title = "Download Cover Art";
            subtitle = s_whdload_last_game[0] ? s_whdload_last_game : "Select a game first, then fetch its boxart";
            badge = "WEB";
            badge_col = PS3_COLOR_AMIGA_ORANGE;
        } else if (item == 1) {
            title = "Install Game from LHA";
            subtitle = "Extract a WHDLoad archive into the PS3 game library";
            badge = "INSTALL";
        } else if (item == 2) {
            title = "WHDLoad Arguments";
            subtitle = mainMenu_whdload_args[0] ? mainMenu_whdload_args : "Custom Slave parameters (e.g. CUSTOM1=1 QUITKEY=$5D)";
            badge = mainMenu_whdload_args[0] ? "ARGS" : "DEFAULT";
            badge_col = mainMenu_whdload_args[0] ? PS3_COLOR_AMIGA_BLUE : PS3_COLOR_TEXT_MUTED;
        } else if (item == 3) {
            title = "WHDLoad Hardware";
            subtitle = (mainMenu_whdload_mode == 1) ? "Force A500: 68000 CPU + OCS/ECS (Best for classic games)" : (mainMenu_whdload_mode == 2) ? "Force A1200: 68020 CPU + AGA (Required for AGA games)" : "Auto: A500 for classic games, A1200 for AGA games";
            badge = (mainMenu_whdload_mode == 1) ? "FORCE A500" : (mainMenu_whdload_mode == 2) ? "FORCE A1200" : "AUTO";
            badge_col = (mainMenu_whdload_mode == 1) ? PS3_COLOR_AMIGA_ORANGE : (mainMenu_whdload_mode == 2) ? PS3_COLOR_AMIGA_BLUE : PS3_COLOR_AMIGA_GREEN;
        } else if (item == 4) {
            title = "WHDLoad Blitter";
            subtitle = (mainMenu_whdload_blitter == 1) ? "Force Normal: Accurate timing & speed (Golden Axe)" : (mainMenu_whdload_blitter == 2) ? "Force Immediate: Fast blitter (Shadow Dancer $168CE)" : (mainMenu_whdload_blitter == 3) ? "Force Improved: Partial scanline blitter (Turrican)" : "Auto: Normal (standard), Immediate (Shadow Dancer), Improved (Turrican)";
            badge = (mainMenu_whdload_blitter == 1) ? "NORMAL" : (mainMenu_whdload_blitter == 2) ? "IMMEDIATE" : (mainMenu_whdload_blitter == 3) ? "IMPROVED" : "AUTO";
            badge_col = (mainMenu_whdload_blitter == 1) ? PS3_COLOR_AMIGA_BLUE : (mainMenu_whdload_blitter == 2) ? PS3_COLOR_AMIGA_ORANGE : (mainMenu_whdload_blitter == 3) ? RGBA8(180, 80, 220, 255) : PS3_COLOR_AMIGA_GREEN;
        } else if (item == 5) {
            title = "Use WHDLoad Directory";
            subtitle = "Select the library as the Amiga HD directory";
            badge = "HD DIR";
        } else if (item == 6) {
            title = "Library Filter";
            subtitle = "All games / favorites only / recently played";
            badge = (s_whdload_filter == 1) ? "FAVORITES" : (s_whdload_filter == 2) ? "RECENT" : "ALL GAMES";
            badge_col = PS3_COLOR_AMIGA_ORANGE;
        } else {
            int orig = vis_index[item - 7];
            bool fav = whdload_is_favorite(games[orig]);
            bool rec = whdload_is_recent(games[orig]);
            title = games[orig];
            subtitle = fav ? "Favorite - installed WHDLoad game" : "Installed WHDLoad game";
            badge = fav ? "* FAV" : (rec ? "RECENT" : "READY");
            if (fav) badge_col = PS3_COLOR_AMIGA_RED;
            else if (rec) badge_col = PS3_COLOR_AMIGA_GREEN;
        }
        ps3_draw_button_item_custom(card_x, y, card_w, item_h, title, subtitle, badge, badge_col, *selected_item == item, false);
    }
    ps3_draw_list_page_indicator(*selected_item, total_items, visible_items);

    if (game_count == 0) {
        ps3_draw_text(card_x + 16.0f, PS3_LIST_BOTTOM_Y - 22.0f, PS3_COLOR_TEXT_MUTED, 0.85f, "No LHA games installed");
    } else if (vis_count == 0) {
        ps3_draw_text(card_x + 16.0f, PS3_LIST_BOTTOM_Y - 22.0f, PS3_COLOR_TEXT_MUTED, 0.85f, "No games match the current filter");
    }

    float preview_x = 596.0f;
    float preview_w = PS3_SCREEN_W - 20.0f - preview_x;
    float preview_y = PS3_LIST_START_Y;
    float preview_h = PS3_LIST_BOTTOM_Y - preview_y;
    ps3_draw_card_custom(preview_x, preview_y, preview_w, preview_h, PS3_COLOR_CARD, PS3_COLOR_CARD_BORDER);

    bool is_game = (*selected_item >= 7);
    const char *preview_title = is_game ? games[vis_index[*selected_item - 7]]
                              : (*selected_item == 0 ? "Download Cover Art"
                                  : (*selected_item == 1 ? "Install Game from LHA"
                                      : (*selected_item == 2 ? "WHDLoad Arguments"
                                          : (*selected_item == 3 ? "WHDLoad Hardware"
                                              : (*selected_item == 4 ? "WHDLoad Blitter"
                                                  : (*selected_item == 5 ? "Use WHDLoad Directory" : "Library Filter"))))));
    bool preview_fav = is_game && whdload_is_favorite(games[vis_index[*selected_item - 7]]);

    ps3_draw_badge(preview_x + 14.0f, preview_y + 12.0f,
        is_game ? "WHDLOAD GAME" : "WHDLOAD LIBRARY",
        is_game ? PS3_COLOR_AMIGA_RED : RGBA8(40, 50, 70, 255), PS3_COLOR_TEXT_WHITE);
    float art_x = preview_x + 16.0f;
    float art_y = preview_y + 42.0f;
    float art_w = preview_w - 32.0f;
    float art_h = 210.0f;

    SDL_Surface *wh_cover_surf = cover_mgr_get();
    if (wh_cover_surf) {
        float surf_w = (float)wh_cover_surf->w;
        float surf_h = (float)wh_cover_surf->h;
        float scale_x = art_w / surf_w;
        float scale_y = art_h / surf_h;
        float final_scale = (scale_x < scale_y) ? scale_x : scale_y;
        if (final_scale > 1.0f) final_scale = 1.0f;
        if (final_scale < 0.05f) final_scale = 0.05f;

        int draw_w = (int)(surf_w * final_scale);
        int draw_h = (int)(surf_h * final_scale);
        int draw_x = (int)(art_x + (art_w - draw_w) * 0.5f);
        int draw_y = (int)(art_y + (art_h - draw_h) * 0.5f);

        ps3_draw_rounded_rect(art_x - 3.0f, art_y - 3.0f, art_w + 6.0f, art_h + 6.0f, 6.0f, RGBA8(10, 13, 20, 255));
        SDL_Rect dst_r = { (Sint16)draw_x, (Sint16)draw_y, (Uint16)draw_w, (Uint16)draw_h };
        SDL_SoftStretch(wh_cover_surf, NULL, prSDLScreen, &dst_r);
    } else {
        s_whdload_cover_angle += 0.08f;
        ps3_draw_rounded_rect(art_x, art_y, art_w, art_h, 6.0f, RGBA8(18, 22, 32, 255));
        ps3_draw_boing_ball_icon(art_x + (art_w * 0.5f), art_y + (art_h * 0.5f) - 10.0f, 34.0f, s_whdload_cover_angle);
        ps3_draw_text_centered(art_x + (art_w * 0.5f), art_y + art_h - 28.0f,
            is_game ? PS3_COLOR_TEXT_MUTED : PS3_COLOR_AMIGA_ORANGE, 0.85f,
            is_game ? "No cover found" : "Cover: <GameName>.png");
    }

    ps3_draw_text(preview_x + 16.0f, preview_y + 262.0f, PS3_COLOR_TEXT_DIM, 0.72f, "GAME TITLE");
    char title_buf[128];
    float title_max_w = preview_w - 32.0f - (preview_fav ? 78.0f : 0.0f);
    ps3_truncate_text(preview_title, title_max_w, 0.90f, title_buf, sizeof(title_buf));
    ps3_draw_text(preview_x + 16.0f, preview_y + 278.0f, PS3_COLOR_TEXT_WHITE, 0.90f, title_buf);
    if (preview_fav) {
        ps3_draw_badge(preview_x + preview_w - 86.0f, preview_y + 274.0f, "* FAV", PS3_COLOR_AMIGA_RED, PS3_COLOR_TEXT_WHITE);
    }

    if (is_game) {
        char wh_mk[256];
        cover_mgr_normalize(preview_title, wh_mk, sizeof(wh_mk));
        struct GameMetadata wh_gm;
        bool has_meta = meta_db_lookup(wh_mk, &wh_gm);

        char wh_info[128];
        if (has_meta && strcmp(wh_gm.year, "N/A") != 0) {
            snprintf(wh_info, sizeof(wh_info), "Year: %s  |  %s", wh_gm.year, wh_gm.genre);
        } else {
            snprintf(wh_info, sizeof(wh_info), "Year: ----  |  Amiga Game");
        }
        ps3_truncate_text(wh_info, preview_w - 32.0f, 0.72f, wh_info, sizeof(wh_info));
        ps3_draw_text(preview_x + 16.0f, preview_y + 300.0f, PS3_COLOR_AMIGA_ORANGE, 0.72f, wh_info);

        char wh_dev[128];
        if (has_meta && strcmp(wh_gm.developer, "N/A") != 0) {
            snprintf(wh_dev, sizeof(wh_dev), "Dev: %s  |  Players: %s", wh_gm.developer, wh_gm.players);
        } else {
            snprintf(wh_dev, sizeof(wh_dev), "Dev: Unknown");
        }
        ps3_truncate_text(wh_dev, preview_w - 32.0f, 0.70f, wh_dev, sizeof(wh_dev));
        ps3_draw_text(preview_x + 16.0f, preview_y + 316.0f, PS3_COLOR_TEXT_MUTED, 0.70f, wh_dev);
    }

    ps3_draw_text(preview_x + 16.0f, preview_y + 338.0f, PS3_COLOR_TEXT_DIM, 0.72f, "RECOMMENDED HARDWARE");
    if (is_game) {
        int is_aga = 0;
        if (mainMenu_whdload_mode == 1) {
            is_aga = 0;
        } else if (mainMenu_whdload_mode == 2) {
            is_aga = 1;
        } else {
            is_aga = ps3_whdload_is_aga(preview_title);
        }
        if (is_aga) {
            ps3_draw_badge(preview_x + 16.0f, preview_y + 356.0f, "A1200 AGA", PS3_COLOR_AMIGA_ORANGE, RGBA8(20, 24, 34, 255));
            ps3_draw_hint_item(preview_x + preview_w - 150.0f, preview_y + 356.0f, PS3_GLYPH_CROSS, "LAUNCH");
            char hw_buf[128];
            ps3_truncate_text("68020 14MHz | Kickstart 3.1 | 2MB Chip + 4MB Fast", preview_w - 32.0f, 0.76f, hw_buf, sizeof(hw_buf));
            ps3_draw_text(preview_x + 16.0f, preview_y + 384.0f, PS3_COLOR_TEXT_MUTED, 0.76f, hw_buf);
        } else {
            int needs_imm = ps3_whdload_needs_immediate_blitter(preview_title);
            int needs_improved = ps3_whdload_needs_improved_blitter(preview_title);
            if (mainMenu_whdload_blitter == 1) {
                needs_imm = 0;
                needs_improved = 0;
            } else if (mainMenu_whdload_blitter == 2) {
                needs_imm = 1;
                needs_improved = 0;
            } else if (mainMenu_whdload_blitter == 3) {
                needs_imm = 0;
                needs_improved = 1;
            }
            ps3_draw_badge(preview_x + 16.0f, preview_y + 356.0f, "A500 OCS", PS3_COLOR_AMIGA_GREEN, RGBA8(20, 24, 34, 255));
            ps3_draw_hint_item(preview_x + preview_w - 150.0f, preview_y + 356.0f, PS3_GLYPH_CROSS, "LAUNCH");
            char hw_buf[128];
            if (needs_imm) {
                ps3_truncate_text("68000 7MHz | Immediate Blitter | 2MB Chip + 4MB Fast", preview_w - 32.0f, 0.76f, hw_buf, sizeof(hw_buf));
            } else if (needs_improved) {
                ps3_truncate_text("68000 7MHz | Improved Partial Blitter | 2MB Chip + 4MB Fast", preview_w - 32.0f, 0.76f, hw_buf, sizeof(hw_buf));
            } else {
                ps3_truncate_text("68000 7MHz | Normal Blitter | 2MB Chip + 4MB Fast", preview_w - 32.0f, 0.76f, hw_buf, sizeof(hw_buf));
            }
            ps3_draw_text(preview_x + 16.0f, preview_y + 384.0f, PS3_COLOR_TEXT_MUTED, 0.76f, hw_buf);
        }
    } else {
        char hw_buf[128];
        ps3_truncate_text("WHDLoad slave games run best on an A1200 AGA setup", preview_w - 190.0f, 0.80f, hw_buf, sizeof(hw_buf));
        ps3_draw_text(preview_x + 16.0f, preview_y + 360.0f, PS3_COLOR_TEXT_MUTED, 0.80f, hw_buf);
        ps3_draw_hint_item(preview_x + preview_w - 150.0f, preview_y + 356.0f, PS3_GLYPH_CROSS, "SELECT");
    }
}

enum Ps3LibGameType {
    PS3_LIB_WHDLOAD = 0,
    PS3_LIB_FLOPPY,
    PS3_LIB_M3U,
    PS3_LIB_LHA,
    PS3_LIB_ZIP,
    PS3_LIB_CD32,
    PS3_LIB_HDF
};

struct Ps3LibGame {
    char title[128];
    char path[512];
    Ps3LibGameType type;
    int model;
};

static bool str_contains_token(const char *str, const char *tok)
{
    if (!str || !tok || !*tok) return false;
    size_t tok_len = strlen(tok);
    const char *p = str;
    while (*p) {
        if (strncasecmp(p, tok, tok_len) == 0) {
            bool pre_ok = (p == str || !isalnum((unsigned char)*(p - 1)));
            bool post_ok = (*(p + tok_len) == '\0' || !isalnum((unsigned char)*(p + tok_len)));
            if (pre_ok && post_ok) return true;
        }
        p++;
    }
    return false;
}

static int ps3_detect_floppy_profile(const char *path, const char *title)
{
    if (str_contains_token(title, "AGA") || str_contains_token(path, "AGA") ||
        str_contains_token(title, "A1200") || str_contains_token(path, "A1200") ||
        str_contains_token(title, "A4000") || str_contains_token(path, "A4000") ||
        str_contains_token(title, "68020") || str_contains_token(path, "68020")) {
        return 2;
    }

    if (str_contains_token(title, "ECS") || str_contains_token(path, "ECS") ||
        str_contains_token(title, "A600") || str_contains_token(path, "A600") ||
        str_contains_token(title, "Kick20") || str_contains_token(path, "Kick20") ||
        str_contains_token(title, "2.04") || str_contains_token(path, "2.04") ||
        str_contains_token(title, "2.05") || str_contains_token(path, "2.05")) {
        return 1;
    }

    if (path) {
        const char *dot = strrchr(path, '.');
        if (dot && strcasecmp(dot, ".adf") == 0) {
            FILE *f = fopen(path, "rb");
            if (f) {
                char buf[1024];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                if (n >= 64) {
                    for (size_t i = 0; i + 3 <= n; i++) {
                        if (memcmp(buf + i, "AGA", 3) == 0 ||
                            memcmp(buf + i, "aga", 3) == 0) {
                            return 2;
                        }
                        if (i + 5 <= n && (memcmp(buf + i, "A1200", 5) == 0 ||
                                           memcmp(buf + i, "68020", 5) == 0)) {
                            return 2;
                        }
                        if (i + 4 <= n && (memcmp(buf + i, "A600", 4) == 0 ||
                                           memcmp(buf + i, "a600", 4) == 0)) {
                            return 1;
                        }
                    }
                }
            }
        }
    }

    return 0;
}

static void ps3_apply_floppy_model(int model)
{
    mainMenu_rtgMemory = 0;
    if (model == 2) {
        kickstart = 3;
        if (!ps3_set_kickstart(kickstart, 0)) {
            kickstart = 1;
            ps3_set_kickstart(kickstart, 0);
        }
        extfile[0] = '\0';
        mainMenu_CPU_model = 1;
        mainMenu_chipset = 2;
        mainMenu_chipMemory = 2;
        mainMenu_slowMemory = 0;
        mainMenu_fastMemory = 3;
        UpdateCPUModelSettings();
        UpdateMemorySettings();
        UpdateChipsetSettings();
        bReloadKickstart = 1;
    } else if (model == 1) {
        kickstart = 11;
        if (!ps3_set_kickstart(kickstart, 0)) {
            kickstart = 2;
            if (!ps3_set_kickstart(kickstart, 0)) {
                kickstart = 3;
                if (!ps3_set_kickstart(kickstart, 0)) {
                    kickstart = 1;
                    ps3_set_kickstart(kickstart, 0);
                }
            }
        }
        extfile[0] = '\0';
        mainMenu_CPU_model = 0;
        mainMenu_chipset = 1;
        mainMenu_chipMemory = 1;
        mainMenu_slowMemory = 0;
        mainMenu_fastMemory = 1;
        UpdateCPUModelSettings();
        UpdateMemorySettings();
        UpdateChipsetSettings();
        bReloadKickstart = 1;
    } else {
        kickstart = 1;
        if (!ps3_set_kickstart(kickstart, 0)) {
            kickstart = 3;
            ps3_set_kickstart(kickstart, 0);
        }
        extfile[0] = '\0';
        mainMenu_CPU_model = 0;
        mainMenu_chipset = 0;
        mainMenu_chipMemory = 0;
        mainMenu_slowMemory = 1;
        mainMenu_fastMemory = 0;
        UpdateCPUModelSettings();
        UpdateMemorySettings();
        UpdateChipsetSettings();
        bReloadKickstart = 1;
    }
}

static Ps3LibGame s_lib_games[4096];
static int s_lib_game_count = 0;
static bool s_lib_scanned = false;
static int s_lib_filter = 0;

struct Ps3LibCacheEntry {
    char path[512];
    char title[128];
    int32_t type;
    int32_t model;
    int64_t mtime;
    int64_t size;
};

static Ps3LibCacheEntry s_lib_cache[4096];
static int s_lib_cache_count = 0;
static bool s_lib_cache_modified = false;
static volatile int s_lib_refresh_requested = 0;

static void get_cache_path(char *out, size_t sz)
{
    if (launchDir[0] != '\0' && strcmp(launchDir, ".") != 0) {
        snprintf(out, sz, "%s/conf/game_library.cache", launchDir);
    } else {
        snprintf(out, sz, "./conf/game_library.cache");
    }
}

static bool str_contains_icase(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return false;
    size_t nlen = strlen(needle);
    if (nlen == 0) return true;
    for (; *haystack; haystack++) {
        if (strncasecmp(haystack, needle, nlen) == 0)
            return true;
    }
    return false;
}

static bool is_excluded_game_file(const char *name, const char *path)
{
    if (!name && !path) return true;
    if (name) {
        const char *slash = strrchr(name, '/');
        const char *bslash = strrchr(name, '\\');
        const char *base = slash ? slash + 1 : (bslash ? bslash + 1 : name);
        if (strcasecmp(base, "EBOOT.BIN") == 0 ||
            strcasecmp(base, "PARAM.SFO") == 0 ||
            strcasecmp(base, "EBOOT") == 0 ||
            strcasecmp(base, "PARAM") == 0 ||
            strcasecmp(base, "ps3log.txt") == 0)
            return true;
    }
    if (path) {
        if (str_contains_icase(path, "EBOOT.BIN") ||
            str_contains_icase(path, "PARAM.SFO") ||
            str_contains_icase(path, "ps3log.txt"))
            return true;
    }
    const char *check = name ? name : path;
    const char *dot = strrchr(check, '.');
    if (dot) {
        if (strcasecmp(dot, ".pkg") == 0 ||
            strcasecmp(dot, ".self") == 0 ||
            strcasecmp(dot, ".elf") == 0 ||
            strcasecmp(dot, ".sfo") == 0 ||
            strcasecmp(dot, ".txt") == 0 ||
            strcasecmp(dot, ".rom") == 0 ||
            strcasecmp(dot, ".bmp") == 0 ||
            strcasecmp(dot, ".png") == 0 ||
            strcasecmp(dot, ".ttf") == 0 ||
            strcasecmp(dot, ".sprx") == 0)
            return true;
    }
    return false;
}

static void lib_cache_save(void);

static void lib_cache_load(void)
{
    ps3_diag_checkpoint("lib:cache-load");
    s_lib_cache_count = 0;
    s_lib_cache_modified = false;
    char cpath[512];
    get_cache_path(cpath, sizeof(cpath));
    FILE *f = fopen(cpath, "rb");
    if (!f) {
        f = fopen("./conf/game_library.cache", "rb");
    }
    if (!f) return;
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "U4LC", 4) != 0) {
        fclose(f);
        return;
    }
    uint32_t ver = 0;
    if (fread(&ver, sizeof(ver), 1, f) != 1 || ver != 1) {
        fclose(f);
        return;
    }
    uint32_t cnt = 0;
    if (fread(&cnt, sizeof(cnt), 1, f) != 1 || cnt > 4096) {
        fclose(f);
        return;
    }
    size_t r = fread(s_lib_cache, sizeof(Ps3LibCacheEntry), cnt, f);
    s_lib_cache_count = (int)r;
    fclose(f);

    /* Prune any invalid/binary entries (e.g. EBOOT.BIN, PARAM.SFO, .pkg, .self) that might have been cached */
    int valid_count = 0;
    bool pruned = false;
    for (int i = 0; i < s_lib_cache_count; i++) {
        if (is_excluded_game_file(s_lib_cache[i].title, s_lib_cache[i].path)) {
            pruned = true;
            continue;
        }
        if (valid_count != i) {
            s_lib_cache[valid_count] = s_lib_cache[i];
        }
        valid_count++;
    }
    s_lib_cache_count = valid_count;
    if (pruned) {
        lib_cache_save();
    }
}

static void lib_cache_save(void)
{
    ps3_diag_checkpoint("lib:cache-save");
    char conf_dir[512];
    char cpath[512];
    get_cache_path(cpath, sizeof(cpath));
    if (launchDir[0] != '\0' && strcmp(launchDir, ".") != 0) {
        snprintf(conf_dir, sizeof(conf_dir), "%s/conf", launchDir);
        ftp_ensure_dir(conf_dir);
    }
    mkdir("./conf", 0777);
    FILE *f = fopen(cpath, "wb");
    if (!f) {
        f = fopen("./conf/game_library.cache", "wb");
    }
    if (!f) return;
    fwrite("U4LC", 1, 4, f);
    uint32_t ver = 1;
    fwrite(&ver, sizeof(ver), 1, f);
    uint32_t cnt = (uint32_t)s_lib_cache_count;
    fwrite(&cnt, sizeof(cnt), 1, f);
    if (cnt > 0) {
        fwrite(s_lib_cache, sizeof(Ps3LibCacheEntry), cnt, f);
    }
    fclose(f);
    s_lib_cache_modified = false;
}

static int lib_cache_find(const char *path)
{
    if (!path) return -1;
    for (int i = 0; i < s_lib_cache_count; i++) {
        if (strcmp(s_lib_cache[i].path, path) == 0)
            return i;
    }
    return -1;
}

/* Directory names that are never part of a game library: WHDLoad runtime
 * folders created inside the WHDLoad root (C/S/ENV/T/Devs/...), emulator
 * housekeeping folders and the dedicated sub roots that are scanned separately.
 * Without this the library would walk through (and mis-classify) the WHDLoad
 * runtime directories that live next to the actual games, which surfaced as
 * "T" and "ENV" entries appearing in the game list. */
static bool lib_skip_dir_name(const char *name)
{
    static const char *const skip[] = {
        "covers", "save", "saves", "savestates", "kickstarts", "data", "conf",
        "C", "S", "L", "Devs", "Libs", "Fonts", "ENV", "T",
        "Locale", "Prefs", "Rexx", "System", "Utilities", "Trashcan",
        "tmp", "thumbs", "screenshots", "roms", "whdload",
    };
    if (!name || name[0] == '\0')
        return true;
    for (size_t i = 0; i < sizeof(skip) / sizeof(skip[0]); i++) {
        if (strcasecmp(name, skip[i]) == 0)
            return true;
    }
    return false;
}

/* A WHDLoad title is only worth showing when it actually holds a .slave file.
 * Half-extracted installations and the WHDLoad runtime directories (T, ENV,
 * Trashcan ...) have no slave and are useless in the library, so keep them out
 * no matter how they found their way onto the disk. */
static bool lib_whd_title_playable(const char *title)
{
    return ps3_whdload_can_launch(title) != 0;
}

/* Does a path sit inside the WHDLoad install root (/dev_hdd0/uae4all/whdload)? */
static bool lib_path_under_whd_root(const char *path)
{
    const char *root = ps3_whdload_root();
    size_t rl = strlen(root);
    if (strncmp(path, root, rl) != 0)
        return false;
    return path[rl] == '/' || path[rl] == '\0';
}

static int32_t lib_entry_true_type(const char *path, const char *title, int32_t cached_type)
{
    if (!path || path[0] == '\0')
        return cached_type;

    size_t plen = strlen(path);
    if (plen >= 4) {
        char ext[5];
        for (int i = 0; i < 4; i++)
            ext[i] = (char)tolower((unsigned char)path[plen - 4 + i]);
        ext[4] = '\0';
        if (strcmp(ext, ".lha") == 0 || strcmp(ext, ".lzx") == 0)
            return (int32_t)PS3_LIB_LHA;
    }

    if (lib_path_under_whd_root(path)) {
        /* The first path component under the WHD root is the game folder;
         * trust it more than a possibly decorated cached title. */
        const char *after = path + strlen(ps3_whdload_root()) + 1;
        char folder[128];
        const char *slash = strchr(after, '/');
        size_t n = slash ? (size_t)(slash - after) : strlen(after);
        if (n > 0 && n < sizeof(folder)) {
            memcpy(folder, after, n);
            folder[n] = '\0';
            if (ps3_whdload_can_launch(folder))
                return (int32_t)PS3_LIB_WHDLOAD;
        }
        if (title && title[0] && lib_whd_title_playable(title))
            return (int32_t)PS3_LIB_WHDLOAD;
    }

    return cached_type;
}

void ps3_library_mark_dirty(void)
{
    s_lib_refresh_requested = 1;
}

static int compare_lib_games(const void *a, const void *b)
{
    const Ps3LibGame *ga = (const Ps3LibGame *)a;
    const Ps3LibGame *gb = (const Ps3LibGame *)b;
    return strcasecmp(ga->title, gb->title);
}

static int count_dir_entries(const char *dir_path, int depth)
{
    if (!dir_path || dir_path[0] == '\0' || depth > 2)
        return 0;
    DIR *d = opendir(dir_path);
    if (!d)
        return 0;
    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir_path, ent->d_name);
        struct stat st;
        if (stat(full, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode)) {
            if (depth < 2) {
                if (!lib_skip_dir_name(ent->d_name)) {
                    count += count_dir_entries(full, depth + 1);
                }
            }
        } else {
            count++;
        }
    }
    closedir(d);
    return count;
}

static float s_splash_ball_angle = 0.0f;
static float s_splash_bounce_frame = 0.0f;

static void report_scan_progress(bool is_splash, float fraction, const char *status, const char *item_name)
{
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    if (is_splash) {
        s_splash_bounce_frame += 1.0f;
        float bounce = fabsf(sinf(s_splash_bounce_frame * 0.12f));
        s_splash_ball_angle += 0.08f;
        ps3_gui_draw_splash_progress(fraction, status, s_splash_ball_angle, bounce);
    } else {
        ps3_gui_draw_progress("Scanning Game Library...", status, fraction, item_name);
    }
}

static int ps3_chd_is_cdrom(const char *path)
{
    chd_file *chd = NULL;
    char meta[512];
    uint32_t meta_len = 0;
    int is_cd = 0;

    if (!path || chd_open(path, CHD_OPEN_READ, NULL, &chd) != CHDERR_NONE || !chd)
        return 0;
    if (chd_get_metadata(chd, CDROM_TRACK_METADATA2_TAG, 0, meta, sizeof(meta) - 1, &meta_len, NULL, NULL) == CHDERR_NONE)
        is_cd = 1;
    else if (chd_get_metadata(chd, CDROM_TRACK_METADATA_TAG, 0, meta, sizeof(meta) - 1, &meta_len, NULL, NULL) == CHDERR_NONE)
        is_cd = 1;
    else if (chd_get_metadata(chd, GDROM_TRACK_METADATA_TAG, 0, meta, sizeof(meta) - 1, &meta_len, NULL, NULL) == CHDERR_NONE)
        is_cd = 1;
    chd_close(chd);
    return is_cd;
}

static void lib_scan_directory(const char *dir_path, int depth, int *processed, int total, bool is_splash)
{
    ps3_diag_checkpoint("lib:scan-dir");
    if (!dir_path || s_lib_game_count >= 4096)
        return;
    DIR *d = opendir(dir_path);
    if (!d)
        return;
    struct dirent *ent;
    while (s_lib_game_count < 4096 && (ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir_path, ent->d_name);
        struct stat st;
        if (stat(full, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode)) {
            if (depth < 2) {
                if (!lib_skip_dir_name(ent->d_name)) {
                    lib_scan_directory(full, depth + 1, processed, total, is_splash);
                }
            }
            continue;
        }

        if (processed && total > 0) {
            (*processed)++;
            int step = (total > 100) ? (total / 40) : 4;
            if (step < 2) step = 2;
            if (((*processed) % step) == 0 || (*processed) == total) {
                float frac = (float)(*processed) / (float)total;
                if (is_splash) {
                    frac = 0.20f + frac * 0.75f;
                }
                if (frac > 0.98f) frac = 0.98f;
                char sub[128];
                snprintf(sub, sizeof(sub), "Scanning games: %d / %d (%d%%)", *processed, total, (int)(frac * 100.0f));
                report_scan_progress(is_splash, frac, sub, ent->d_name);
            }
        }

        const char *dot = strrchr(ent->d_name, '.');
        if (!dot)
            continue;
        /* Never list emulator binary/data files as games */
        if (is_excluded_game_file(ent->d_name, full))
            continue;
        Ps3LibGameType gtype;
        if (strcasecmp(dot, ".adf") == 0 ||
            strcasecmp(dot, ".adz") == 0 ||
            strcasecmp(dot, ".ipf") == 0 ||
            strcasecmp(dot, ".dms") == 0) {
            gtype = PS3_LIB_FLOPPY;
        } else if (strcasecmp(dot, ".m3u") == 0) {
            gtype = PS3_LIB_M3U;
        } else if (strcasecmp(dot, ".lha") == 0 ||
                   strcasecmp(dot, ".lzh") == 0) {
            gtype = PS3_LIB_LHA;
        } else if (strcasecmp(dot, ".zip") == 0 ||
                   strcasecmp(dot, ".7z") == 0) {
            gtype = PS3_LIB_ZIP;
        } else if (strcasecmp(dot, ".chd") == 0) {
            gtype = ps3_chd_is_cdrom(full) ? PS3_LIB_CD32 : PS3_LIB_HDF;
        } else if (strcasecmp(dot, ".iso") == 0 ||
                   strcasecmp(dot, ".cue") == 0 ||
                   strcasecmp(dot, ".bin") == 0 ||
                   strcasecmp(dot, ".img") == 0) {
            gtype = PS3_LIB_CD32;
        } else if (strcasecmp(dot, ".hdf") == 0) {
            gtype = PS3_LIB_HDF;
        } else {
            continue;
        }

        bool duplicate = false;
        for (int ex = 0; ex < s_lib_game_count; ex++) {
            if (strcmp(s_lib_games[ex].path, full) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        Ps3LibGame *g = &s_lib_games[s_lib_game_count];
        strncpy(g->path, full, sizeof(g->path) - 1);
        g->path[sizeof(g->path) - 1] = '\0';

        int c_idx = lib_cache_find(full);
        if (c_idx >= 0 &&
            s_lib_cache[c_idx].mtime == (int64_t)st.st_mtime &&
            s_lib_cache[c_idx].size == (int64_t)st.st_size) {
            g->type = (Ps3LibGameType)s_lib_cache[c_idx].type;
            g->model = s_lib_cache[c_idx].model;
            strncpy(g->title, s_lib_cache[c_idx].title, sizeof(g->title) - 1);
            g->title[sizeof(g->title) - 1] = '\0';
        } else {
            g->type = gtype;
            size_t title_len = (size_t)(dot - ent->d_name);
            if (title_len >= sizeof(g->title))
                title_len = sizeof(g->title) - 1;
            strncpy(g->title, ent->d_name, title_len);
            g->title[title_len] = '\0';
            if (gtype == PS3_LIB_FLOPPY || gtype == PS3_LIB_M3U || gtype == PS3_LIB_ZIP) {
                g->model = ps3_detect_floppy_profile(full, g->title);
            } else if (gtype == PS3_LIB_CD32) {
                g->model = 3;
            } else {
                g->model = 2;
            }
            if (c_idx >= 0) {
                strncpy(s_lib_cache[c_idx].title, g->title, sizeof(s_lib_cache[c_idx].title) - 1);
                s_lib_cache[c_idx].title[sizeof(s_lib_cache[c_idx].title) - 1] = '\0';
                s_lib_cache[c_idx].type = (int32_t)g->type;
                s_lib_cache[c_idx].model = g->model;
                s_lib_cache[c_idx].mtime = (int64_t)st.st_mtime;
                s_lib_cache[c_idx].size = (int64_t)st.st_size;
                s_lib_cache_modified = true;
            } else if (s_lib_cache_count < 4096) {
                strncpy(s_lib_cache[s_lib_cache_count].path, full, sizeof(s_lib_cache[s_lib_cache_count].path) - 1);
                s_lib_cache[s_lib_cache_count].path[sizeof(s_lib_cache[s_lib_cache_count].path) - 1] = '\0';
                strncpy(s_lib_cache[s_lib_cache_count].title, g->title, sizeof(s_lib_cache[s_lib_cache_count].title) - 1);
                s_lib_cache[s_lib_cache_count].title[sizeof(s_lib_cache[s_lib_cache_count].title) - 1] = '\0';
                s_lib_cache[s_lib_cache_count].type = (int32_t)g->type;
                s_lib_cache[s_lib_cache_count].model = g->model;
                s_lib_cache[s_lib_cache_count].mtime = (int64_t)st.st_mtime;
                s_lib_cache[s_lib_cache_count].size = (int64_t)st.st_size;
                s_lib_cache_count++;
                s_lib_cache_modified = true;
            }
        }
        s_lib_game_count++;
    }
    closedir(d);
}

/* Every place a user can reasonably drop games: the configured ROM directory,
 * ./roms, the last browsed directory, the application directory itself, the
 * internal /dev_hdd0/uae4all folder and any mounted USB mass storage device
 * (the device root as well as the usual uae4all/roms subfolders, because users
 * copy their ADFs to whatever folder they like on the stick).  Duplicates are
 * filtered out. */
#define LIB_MAX_SCAN_ROOTS 24

static int lib_add_scan_root(const char **roots, int count, const char *dir)
{
    if (!dir || !dir[0] || count >= LIB_MAX_SCAN_ROOTS)
        return count;
    for (int i = 0; i < count; i++) {
        if (strcmp(roots[i], dir) == 0)
            return count;
    }
    roots[count] = dir;
    return count + 1;
}

static int lib_build_scan_roots(const char **roots)
{
    int n = 0;
    n = lib_add_scan_root(roots, n, "/dev_hdd0/uae4all/roms");
    n = lib_add_scan_root(roots, n, "/dev_hdd0/uae4all");
    n = lib_add_scan_root(roots, n, s_lib_rom_dir);
    n = lib_add_scan_root(roots, n, "/dev_hdd0/roms");
    n = lib_add_scan_root(roots, n, "/dev_usb000/uae4all/roms");
    n = lib_add_scan_root(roots, n, "/dev_usb000/roms");
    n = lib_add_scan_root(roots, n, "/dev_usb000");
    n = lib_add_scan_root(roots, n, "/dev_usb001/uae4all/roms");
    n = lib_add_scan_root(roots, n, "/dev_usb001/roms");
    n = lib_add_scan_root(roots, n, "/dev_usb001");
    n = lib_add_scan_root(roots, n, "/dev_usb002/uae4all/roms");
    n = lib_add_scan_root(roots, n, "/dev_usb002/roms");
    n = lib_add_scan_root(roots, n, "/dev_usb002");
    n = lib_add_scan_root(roots, n, "/dev_usb003/uae4all/roms");
    n = lib_add_scan_root(roots, n, "/dev_usb003/roms");
    n = lib_add_scan_root(roots, n, "/dev_usb003");
    n = lib_add_scan_root(roots, n, "./roms");
    return n;
}

static void lib_rescan_internal(bool is_splash)
{
    ps3_diag_checkpoint("lib:rescan");
    report_scan_progress(is_splash, 0.05f, "Loading ROM configuration...", "");
    lib_load_rom_dir();
    lib_cache_load();
    s_lib_game_count = 0;

    report_scan_progress(is_splash, 0.10f, "Scanning WHDLoad titles...", "");
    static char whd_names[4096][128];
    int whd_count = ps3_whdload_list(whd_names, 4096);
    const char *whd_root = ps3_whdload_root();

    report_scan_progress(is_splash, 0.15f, "Counting files in ROM directory...", "");
    const char *scan_roots[LIB_MAX_SCAN_ROOTS];
    int scan_root_entries[LIB_MAX_SCAN_ROOTS];
    int scan_root_count = lib_build_scan_roots(scan_roots);
    int rom_entries = 0;
    for (int i = 0; i < scan_root_count; i++) {
        scan_root_entries[i] = count_dir_entries(scan_roots[i], 0);
        rom_entries += scan_root_entries[i];
    }
    write_log("[PS3] lib scan: whdload root=%s (%d title(s)), %d game root(s)\n",
              whd_root, whd_count, scan_root_count);

    int total_items = whd_count + rom_entries;
    if (total_items < 1) total_items = 1;
    int processed = 0;

    for (int i = 0; i < whd_count && s_lib_game_count < 4096; i++) {
        /* Entries without a .slave file (WHDLoad runtime folders like T/ENV,
         * half-extracted installs) must never reach the library or the cache. */
        if (!lib_whd_title_playable(whd_names[i])) {
            write_log("[PS3] lib: skipped WHDLoad entry '%s' (no .slave file)\n", whd_names[i]);
            continue;
        }
        Ps3LibGame *g = &s_lib_games[s_lib_game_count];
        g->type = PS3_LIB_WHDLOAD;
        g->model = ps3_whdload_is_aga(whd_names[i]) ? 2 : 0;
        strncpy(g->title, whd_names[i], sizeof(g->title) - 1);
        g->title[sizeof(g->title) - 1] = '\0';
        snprintf(g->path, sizeof(g->path), "%s/%s", whd_root, whd_names[i]);
        s_lib_game_count++;
        processed++;

        int whd_step = (whd_count > 100) ? (whd_count / 20) : 4;
        if (whd_step < 2) whd_step = 2;
        if ((i % whd_step) == 0 || i == whd_count - 1) {
            float frac = (float)processed / (float)total_items;
            if (is_splash) frac = 0.20f + frac * 0.75f;
            if (frac > 0.98f) frac = 0.98f;
            char sub[128];
            snprintf(sub, sizeof(sub), "Scanning games: %d / %d (%d%%)", processed, total_items, (int)(frac * 100.0f));
            report_scan_progress(is_splash, frac, sub, whd_names[i]);
        }
    }

    for (int i = 0; i < scan_root_count; i++) {
        DIR *probe = opendir(scan_roots[i]);
        bool readable = (probe != NULL);
        if (probe)
            closedir(probe);
        int before = s_lib_game_count;
        lib_scan_directory(scan_roots[i], 0, &processed, total_items, is_splash);
        write_log("[PS3] lib root %-30s readable=%d entries=%d games=%d\n",
                  scan_roots[i], readable ? 1 : 0,
                  scan_root_entries[i], s_lib_game_count - before);
        if (readable)
            log_dir_contents(scan_roots[i]);
    }

    report_scan_progress(is_splash, 0.98f, "Sorting games alphabetically...", "");
    if (s_lib_game_count > 1) {
        qsort(s_lib_games, s_lib_game_count, sizeof(Ps3LibGame), compare_lib_games);
    }

    static Ps3LibCacheEntry new_cache[4096];
    int new_cache_count = 0;
    for (int i = 0; i < s_lib_game_count && new_cache_count < 4096; i++) {
        int c_idx = lib_cache_find(s_lib_games[i].path);
        if (c_idx >= 0) {
            new_cache[new_cache_count] = s_lib_cache[c_idx];
            new_cache[new_cache_count].model = s_lib_games[i].model;
            new_cache_count++;
        } else {
            strncpy(new_cache[new_cache_count].path, s_lib_games[i].path, sizeof(new_cache[new_cache_count].path) - 1);
            new_cache[new_cache_count].path[sizeof(new_cache[new_cache_count].path) - 1] = '\0';
            strncpy(new_cache[new_cache_count].title, s_lib_games[i].title, sizeof(new_cache[new_cache_count].title) - 1);
            new_cache[new_cache_count].title[sizeof(new_cache[new_cache_count].title) - 1] = '\0';
            new_cache[new_cache_count].type = (int32_t)s_lib_games[i].type;
            new_cache[new_cache_count].model = s_lib_games[i].model;
            struct stat st;
            if (stat(s_lib_games[i].path, &st) == 0) {
                new_cache[new_cache_count].mtime = (int64_t)st.st_mtime;
                new_cache[new_cache_count].size = (int64_t)st.st_size;
            } else {
                new_cache[new_cache_count].mtime = 0;
                new_cache[new_cache_count].size = 0;
            }
            new_cache_count++;
        }
    }
    memcpy(s_lib_cache, new_cache, sizeof(Ps3LibCacheEntry) * new_cache_count);
    s_lib_cache_count = new_cache_count;
    lib_cache_save();

    char fin[128];
    snprintf(fin, sizeof(fin), "Scan finished: %d games found", s_lib_game_count);
    report_scan_progress(is_splash, 1.0f, fin, "");
    SDL_Delay(250);

    s_lib_scanned = true;
}

static void lib_rescan(void)
{
    lib_rescan_internal(false);
}

static void lib_splash_progress(int processed, int total, const char *phase, float frac_from, float frac_to)
{
    if (total < 1)
        total = 1;
    if (processed < 0)
        processed = 0;
    if (processed > total)
        processed = total;
    if (processed != total && (processed % 16) != 0)
        return;
    float p = (float)processed / (float)total;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    float frac = frac_from + (frac_to - frac_from) * p;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 0.98f) frac = 0.98f;
    char sub[128];
    snprintf(sub, sizeof(sub), "%s: %d / %d (%d%%)", phase, processed, total, (int)(frac * 100.0f));
    report_scan_progress(true, frac, sub, "");
}

static void lib_fast_scan_dir(const char *dir_path, int depth, bool *modified)
{
    if (!dir_path || dir_path[0] == '\0' || depth > 2 || s_lib_game_count >= 4096)
        return;
    DIR *d = opendir(dir_path);
    if (!d)
        return;
    struct dirent *ent;
    while (s_lib_game_count < 4096 && (ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir_path, ent->d_name);
        struct stat st;
        if (stat(full, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode)) {
            if (depth < 2) {
                if (!lib_skip_dir_name(ent->d_name)) {
                    lib_fast_scan_dir(full, depth + 1, modified);
                }
            }
            continue;
        }

        const char *dot = strrchr(ent->d_name, '.');
        if (!dot)
            continue;
        /* Never list emulator binary/data files as games */
        if (is_excluded_game_file(ent->d_name, full))
            continue;
        Ps3LibGameType gtype;
        if (strcasecmp(dot, ".adf") == 0 ||
            strcasecmp(dot, ".adz") == 0 ||
            strcasecmp(dot, ".ipf") == 0 ||
            strcasecmp(dot, ".dms") == 0) {
            gtype = PS3_LIB_FLOPPY;
        } else if (strcasecmp(dot, ".m3u") == 0) {
            gtype = PS3_LIB_M3U;
        } else if (strcasecmp(dot, ".lha") == 0 ||
                   strcasecmp(dot, ".lzh") == 0) {
            gtype = PS3_LIB_LHA;
        } else if (strcasecmp(dot, ".zip") == 0 ||
                   strcasecmp(dot, ".7z") == 0) {
            gtype = PS3_LIB_ZIP;
        } else if (strcasecmp(dot, ".chd") == 0) {
            gtype = ps3_chd_is_cdrom(full) ? PS3_LIB_CD32 : PS3_LIB_HDF;
        } else if (strcasecmp(dot, ".iso") == 0 ||
                   strcasecmp(dot, ".cue") == 0 ||
                   strcasecmp(dot, ".bin") == 0 ||
                   strcasecmp(dot, ".img") == 0) {
            gtype = PS3_LIB_CD32;
        } else if (strcasecmp(dot, ".hdf") == 0) {
            gtype = PS3_LIB_HDF;
        } else {
            continue;
        }

        int c_idx = lib_cache_find(full);
        if (c_idx >= 0)
            continue;

        bool duplicate = false;
        for (int ex = 0; ex < s_lib_game_count; ex++) {
            if (strcmp(s_lib_games[ex].path, full) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;

        Ps3LibGame *g = &s_lib_games[s_lib_game_count];
        strncpy(g->path, full, sizeof(g->path) - 1);
        g->path[sizeof(g->path) - 1] = '\0';
        g->type = gtype;
        size_t title_len = (size_t)(dot - ent->d_name);
        if (title_len >= sizeof(g->title))
            title_len = sizeof(g->title) - 1;
        strncpy(g->title, ent->d_name, title_len);
        g->title[title_len] = '\0';
        if (gtype == PS3_LIB_FLOPPY || gtype == PS3_LIB_M3U || gtype == PS3_LIB_ZIP) {
            g->model = ps3_detect_floppy_profile(full, g->title);
        } else if (gtype == PS3_LIB_CD32) {
            g->model = 3;
        } else {
            g->model = 2;
        }

        if (s_lib_cache_count < 4096) {
            strncpy(s_lib_cache[s_lib_cache_count].path, full, sizeof(s_lib_cache[s_lib_cache_count].path) - 1);
            s_lib_cache[s_lib_cache_count].path[sizeof(s_lib_cache[s_lib_cache_count].path) - 1] = '\0';
            strncpy(s_lib_cache[s_lib_cache_count].title, g->title, sizeof(s_lib_cache[s_lib_cache_count].title) - 1);
            s_lib_cache[s_lib_cache_count].title[sizeof(s_lib_cache[s_lib_cache_count].title) - 1] = '\0';
            s_lib_cache[s_lib_cache_count].type = (int32_t)g->type;
            s_lib_cache[s_lib_cache_count].model = g->model;
            s_lib_cache[s_lib_cache_count].mtime = (int64_t)st.st_mtime;
            s_lib_cache[s_lib_cache_count].size = (int64_t)st.st_size;
            s_lib_cache_count++;
        }
        s_lib_game_count++;
        if (modified) *modified = true;
    }
    closedir(d);
}

void ps3_library_initial_load(void)
{
    meta_db_load("./data/gameinfo/games.json");

    lib_load_rom_dir();
    lib_cache_load();
    if (s_lib_cache_count == 0) {
        lib_rescan_internal(true);
        return;
    }

    report_scan_progress(true, 0.20f, "Loading game library...", "");

    s_lib_game_count = 0;
    int removed_whd_from_cache = 0;
    int healed_types = 0;
    for (int i = 0; i < s_lib_cache_count && s_lib_game_count < 4096; i++) {
        if (is_excluded_game_file(s_lib_cache[i].title, s_lib_cache[i].path))
            continue;
        if (s_lib_cache[i].type == (int32_t)PS3_LIB_WHDLOAD &&
            !lib_whd_title_playable(s_lib_cache[i].title)) {
            write_log("[PS3] lib: pruned cached WHDLoad entry '%s' (no .slave file)\n", s_lib_cache[i].title);
            removed_whd_from_cache++;
            continue;
        }
        {
            int32_t true_type = lib_entry_true_type(s_lib_cache[i].path,
                                                    s_lib_cache[i].title,
                                                    s_lib_cache[i].type);
            if (true_type != s_lib_cache[i].type) {
                write_log("[PS3] lib: healed cached entry '%s' type %d -> %d (path evidence)\n",
                          s_lib_cache[i].title, (int)s_lib_cache[i].type, (int)true_type);
                s_lib_cache[i].type = true_type;
                healed_types++;
            }
        }
        s_lib_games[s_lib_game_count].type = (Ps3LibGameType)s_lib_cache[i].type;
        s_lib_games[s_lib_game_count].model = s_lib_cache[i].model;
        strncpy(s_lib_games[s_lib_game_count].title, s_lib_cache[i].title, sizeof(s_lib_games[s_lib_game_count].title) - 1);
        s_lib_games[s_lib_game_count].title[sizeof(s_lib_games[s_lib_game_count].title) - 1] = '\0';
        strncpy(s_lib_games[s_lib_game_count].path, s_lib_cache[i].path, sizeof(s_lib_games[s_lib_game_count].path) - 1);
        s_lib_games[s_lib_game_count].path[sizeof(s_lib_games[s_lib_game_count].path) - 1] = '\0';
        s_lib_game_count++;
        lib_splash_progress(i, s_lib_cache_count, "Loading game library", 0.20f, 0.40f);
    }

    bool modified = removed_whd_from_cache > 0;
    report_scan_progress(true, 0.40f, "Checking WHDLoad titles...", "");
    static char whd_names[4096][128];
    int whd_count = ps3_whdload_list(whd_names, 4096);
    const char *whd_root = ps3_whdload_root();
    for (int i = 0; i < whd_count && s_lib_game_count < 4096; i++) {
        /* Same rule as the cache loader: no .slave file, no library entry. */
        if (!lib_whd_title_playable(whd_names[i])) {
            write_log("[PS3] lib: skipped WHDLoad entry '%s' (no .slave file)\n", whd_names[i]);
            continue;
        }
        char full_whd[512];
        snprintf(full_whd, sizeof(full_whd), "%s/%s", whd_root, whd_names[i]);
        if (lib_cache_find(full_whd) >= 0)
            continue;
        bool dup = false;
        for (int ex = 0; ex < s_lib_game_count; ex++) {
            if (strcmp(s_lib_games[ex].path, full_whd) == 0) {
                dup = true;
                break;
            }
        }
        if (dup)
            continue;
        Ps3LibGame *g = &s_lib_games[s_lib_game_count];
        g->type = PS3_LIB_WHDLOAD;
        g->model = ps3_whdload_is_aga(whd_names[i]) ? 2 : 0;
        strncpy(g->title, whd_names[i], sizeof(g->title) - 1);
        g->title[sizeof(g->title) - 1] = '\0';
        strncpy(g->path, full_whd, sizeof(g->path) - 1);
        g->path[sizeof(g->path) - 1] = '\0';
        s_lib_game_count++;
        modified = true;
        lib_splash_progress(i, whd_count, "Checking WHDLoad titles", 0.40f, 0.55f);
    }

    report_scan_progress(true, 0.55f, "Checking ROM folders...", "");
    const char *fast_roots[LIB_MAX_SCAN_ROOTS];
    int fast_root_count = lib_build_scan_roots(fast_roots);
    for (int i = 0; i < fast_root_count; i++) {
        lib_fast_scan_dir(fast_roots[i], 0, &modified);
    }

    bool removed = false;
    int dst = 0;
    for (int src = 0; src < s_lib_game_count; src++) {
        struct stat st_chk;
        if (stat(s_lib_games[src].path, &st_chk) != 0) {
            removed = true;
            lib_splash_progress(src, s_lib_game_count, "Verifying games", 0.60f, 0.85f);
            continue;
        }
        lib_splash_progress(src, s_lib_game_count, "Verifying games", 0.60f, 0.85f);
        if (dst != src) {
            s_lib_games[dst] = s_lib_games[src];
        }
        dst++;
    }
    s_lib_game_count = dst;

    report_scan_progress(true, 0.88f, "Updating library cache...", "");
    if (modified || removed) {
        if (s_lib_game_count > 1) {
            qsort(s_lib_games, s_lib_game_count, sizeof(Ps3LibGame), compare_lib_games);
        }
        s_lib_cache_count = 0;
        for (int i = 0; i < s_lib_game_count && s_lib_cache_count < 4096; i++) {
            strncpy(s_lib_cache[s_lib_cache_count].path, s_lib_games[i].path, sizeof(s_lib_cache[s_lib_cache_count].path) - 1);
            s_lib_cache[s_lib_cache_count].path[sizeof(s_lib_cache[s_lib_cache_count].path) - 1] = '\0';
            strncpy(s_lib_cache[s_lib_cache_count].title, s_lib_games[i].title, sizeof(s_lib_cache[s_lib_cache_count].title) - 1);
            s_lib_cache[s_lib_cache_count].title[sizeof(s_lib_cache[s_lib_cache_count].title) - 1] = '\0';
            s_lib_cache[s_lib_cache_count].type = (int32_t)s_lib_games[i].type;
            s_lib_cache[s_lib_cache_count].model = s_lib_games[i].model;
            struct stat st;
            if (stat(s_lib_games[i].path, &st) == 0) {
                s_lib_cache[s_lib_cache_count].mtime = (int64_t)st.st_mtime;
                s_lib_cache[s_lib_cache_count].size = (int64_t)st.st_size;
            } else {
                s_lib_cache[s_lib_cache_count].mtime = 0;
                s_lib_cache[s_lib_cache_count].size = 0;
            }
            s_lib_cache_count++;
        }
        lib_cache_save();
    }

    char fin[128];
    snprintf(fin, sizeof(fin), "Ready: %d games loaded from cache", s_lib_game_count);
    report_scan_progress(true, 1.0f, fin, "");
    SDL_Delay(100);
    s_lib_scanned = true;
}

static float s_lib_cover_angle = 0.0f;

static void lib_cover_unload(void)
{
    cover_mgr_unload();
}

static void lib_cover_load(const char *title, const char *path)
{
    if (!title || title[0] == '\0') {
        cover_mgr_unload();
        return;
    }
    cover_mgr_load(title, path);
}

static int lib_find_next_letter_index(const int *vis_index, int vis_count, int cur_vis, int direction)
{
    if (vis_count <= 1)
        return 0;
    char cur_c = (char)toupper((unsigned char)s_lib_games[vis_index[cur_vis]].title[0]);
    if (direction > 0) {
        for (int i = cur_vis + 1; i < vis_count; i++) {
            char c = (char)toupper((unsigned char)s_lib_games[vis_index[i]].title[0]);
            if (c != cur_c)
                return i;
        }
        return 0;
    } else {
        for (int i = cur_vis - 1; i >= 0; i--) {
            char c = (char)toupper((unsigned char)s_lib_games[vis_index[i]].title[0]);
            if (c != cur_c) {
                while (i > 0) {
                    char prev = (char)toupper((unsigned char)s_lib_games[vis_index[i - 1]].title[0]);
                    if (prev == c) i--;
                    else break;
                }
                return i;
            }
        }
        return vis_count - 1;
    }
}

static int s_last_lib_selected_item = -1;

void ps3_view_library(Ps3InputState *input, int *selected_item)
{
    if (!s_lib_scanned || s_lib_refresh_requested) {
        s_lib_refresh_requested = 0;
        lib_rescan();
    }

    whdload_refresh_meta();

    static int vis_index[4096];
    int vis_count = 0;
    for (int k = 0; k < s_lib_game_count; k++) {
        bool keep = false;
        if (s_lib_filter == 0) {
            keep = true;
        } else if (s_lib_filter == 1) {
            keep = (s_lib_games[k].type == PS3_LIB_FLOPPY);
        } else if (s_lib_filter == 2) {
            keep = (s_lib_games[k].type == PS3_LIB_ZIP);
        } else if (s_lib_filter == 3) {
            keep = (s_lib_games[k].type == PS3_LIB_M3U);
        } else if (s_lib_filter == 4) {
            keep = (s_lib_games[k].type == PS3_LIB_WHDLOAD);
        } else if (s_lib_filter == 5) {
            keep = (s_lib_games[k].type == PS3_LIB_LHA);
        } else if (s_lib_filter == 6) {
            keep = (s_lib_games[k].type == PS3_LIB_CD32);
        } else if (s_lib_filter == 7) {
            keep = (s_lib_games[k].type == PS3_LIB_HDF);
        } else if (s_lib_filter == 8) {
            keep = whdload_is_favorite(s_lib_games[k].title);
        }
        if (keep) {
            vis_index[vis_count++] = k;
        }
    }

    if (*selected_item < 0) *selected_item = 0;
    if (vis_count > 0 && *selected_item >= vis_count) *selected_item = vis_count - 1;

    static int s_lib_hold_up = 0;
    static int s_lib_hold_down = 0;

    if (input->pressed & PS3_BTN_UP) {
        (*selected_item)--;
        if (*selected_item < 0) *selected_item = (vis_count > 0) ? (vis_count - 1) : 0;
    }
    if (input->pressed & PS3_BTN_DOWN) {
        (*selected_item)++;
        if (vis_count > 0 && *selected_item >= vis_count) *selected_item = 0;
    }

    if (input->held & PS3_BTN_UP && vis_count > 0) {
        s_lib_hold_up++;
        if (s_lib_hold_up > 12 && (s_lib_hold_up % 3 == 0)) {
            (*selected_item)--;
            if (*selected_item < 0) *selected_item = vis_count - 1;
        }
    } else {
        s_lib_hold_up = 0;
    }

    if (input->held & PS3_BTN_DOWN && vis_count > 0) {
        s_lib_hold_down++;
        if (s_lib_hold_down > 12 && (s_lib_hold_down % 3 == 0)) {
            (*selected_item)++;
            if (*selected_item >= vis_count) *selected_item = 0;
        }
    } else {
        s_lib_hold_down = 0;
    }

    if (vis_count > 0 && *selected_item >= 0 && *selected_item < vis_count) {
        Ps3LibGame *cur_g = &s_lib_games[vis_index[*selected_item]];
        if (cur_g->type == PS3_LIB_FLOPPY || cur_g->type == PS3_LIB_M3U || cur_g->type == PS3_LIB_ZIP) {
            if (input->pressed & PS3_BTN_R1) {
                cur_g->model = (cur_g->model + 1) % 3;
                int c_idx = lib_cache_find(cur_g->path);
                if (c_idx >= 0) {
                    s_lib_cache[c_idx].model = cur_g->model;
                    lib_cache_save();
                } else if (s_lib_cache_count < 4096) {
                    strncpy(s_lib_cache[s_lib_cache_count].path, cur_g->path, sizeof(s_lib_cache[s_lib_cache_count].path) - 1);
                    s_lib_cache[s_lib_cache_count].path[sizeof(s_lib_cache[s_lib_cache_count].path) - 1] = '\0';
                    strncpy(s_lib_cache[s_lib_cache_count].title, cur_g->title, sizeof(s_lib_cache[s_lib_cache_count].title) - 1);
                    s_lib_cache[s_lib_cache_count].title[sizeof(s_lib_cache[s_lib_cache_count].title) - 1] = '\0';
                    s_lib_cache[s_lib_cache_count].type = (int32_t)cur_g->type;
                    s_lib_cache[s_lib_cache_count].model = cur_g->model;
                    s_lib_cache[s_lib_cache_count].mtime = 0;
                    s_lib_cache[s_lib_cache_count].size = 0;
                    s_lib_cache_count++;
                    lib_cache_save();
                }
                const char *mname = (cur_g->model == 2) ? "AMIGA 1200 (AGA)" : ((cur_g->model == 1) ? "AMIGA 600 (ECS)" : "AMIGA 500 (OCS 1.3)");
                char mmsg[96];
                snprintf(mmsg, sizeof(mmsg), "PROFILE: %s", mname);
                ps3_osd_show(mmsg, 1800);
            } else if (input->pressed & PS3_BTN_L1) {
                cur_g->model = (cur_g->model + 2) % 3;
                int c_idx = lib_cache_find(cur_g->path);
                if (c_idx >= 0) {
                    s_lib_cache[c_idx].model = cur_g->model;
                    lib_cache_save();
                } else if (s_lib_cache_count < 4096) {
                    strncpy(s_lib_cache[s_lib_cache_count].path, cur_g->path, sizeof(s_lib_cache[s_lib_cache_count].path) - 1);
                    s_lib_cache[s_lib_cache_count].path[sizeof(s_lib_cache[s_lib_cache_count].path) - 1] = '\0';
                    strncpy(s_lib_cache[s_lib_cache_count].title, cur_g->title, sizeof(s_lib_cache[s_lib_cache_count].title) - 1);
                    s_lib_cache[s_lib_cache_count].title[sizeof(s_lib_cache[s_lib_cache_count].title) - 1] = '\0';
                    s_lib_cache[s_lib_cache_count].type = (int32_t)cur_g->type;
                    s_lib_cache[s_lib_cache_count].model = cur_g->model;
                    s_lib_cache[s_lib_cache_count].mtime = 0;
                    s_lib_cache[s_lib_cache_count].size = 0;
                    s_lib_cache_count++;
                    lib_cache_save();
                }
                const char *mname = (cur_g->model == 2) ? "AMIGA 1200 (AGA)" : ((cur_g->model == 1) ? "AMIGA 600 (ECS)" : "AMIGA 500 (OCS 1.3)");
                char mmsg[96];
                snprintf(mmsg, sizeof(mmsg), "PROFILE: %s", mname);
                ps3_osd_show(mmsg, 1800);
            }
        } else if (input->pressed & (PS3_BTN_L1 | PS3_BTN_R1)) {
            int dir = (input->pressed & PS3_BTN_R1) ? 1 : -1;
            *selected_item = lib_find_next_letter_index(vis_index, vis_count, *selected_item, dir);
        }
    }

    if (vis_count > 0 && (input->pressed & (PS3_BTN_LEFT | PS3_BTN_RIGHT))) {
        int dir = (input->pressed & PS3_BTN_RIGHT) ? 1 : -1;
        *selected_item = lib_find_next_letter_index(vis_index, vis_count, *selected_item, dir);
    }

    if (input->pressed & PS3_BTN_SQUARE) {
        s_lib_filter = (s_lib_filter + 1) % 9;
        *selected_item = 0;
        s_last_lib_selected_item = -1;
        const char *fnames[9] = {
            "ALL GAMES", "FLOPPY (ADF)", "ZIP ARCHIVES", "M3U PLAYLISTS",
            "WHDLOAD INSTALLED", "LHA ARCHIVES", "CD32 IMAGES",
            "HARDFILES (HDF)", "FAVORITES"
        };
        char f_msg[64];
        snprintf(f_msg, sizeof(f_msg), "FILTER: %s", fnames[s_lib_filter]);
        ps3_osd_show(f_msg, 1500);
    }

    if (input->pressed & PS3_BTN_TRIANGLE) {
        lib_load_rom_dir();
        int opt = ps3_show_library_options_dialog(s_lib_rom_dir);
        if (opt == 1) {
            char chosen_dir[512];
            chosen_dir[0] = '\0';
            int res = ps3_gui_run_browser(chosen_dir, s_lib_rom_dir[0] ? s_lib_rom_dir : currentDir, 12);
            if (res == 1 && chosen_dir[0] != '\0') {
                strncpy(s_lib_rom_dir, chosen_dir, sizeof(s_lib_rom_dir) - 1);
                s_lib_rom_dir[sizeof(s_lib_rom_dir) - 1] = '\0';
                strncpy(currentDir, chosen_dir, 299);
                currentDir[299] = '\0';
                lib_save_rom_dir();
                saveAdfDir();

                char chg_msg[384];
                snprintf(chg_msg, sizeof(chg_msg), "ROM directory successfully changed to:\n%s\n\nThe library will now be rescanned.", chosen_dir);
                ps3_show_message_box("ROM Directory Changed", chg_msg, "OK (X)");

                lib_cover_unload();
                lib_rescan();
                *selected_item = 0;
                s_last_lib_selected_item = -1;
                char msg[96];
                snprintf(msg, sizeof(msg), "ROM DIR UPDATED: %d GAMES", s_lib_game_count);
                ps3_osd_show(msg, 2500);
            }
        } else if (opt == 2) {
            lib_cover_unload();
            lib_rescan();
            *selected_item = 0;
            s_last_lib_selected_item = -1;
            char msg[96];
            snprintf(msg, sizeof(msg), "RESCAN COMPLETE: %d GAMES", s_lib_game_count);
            ps3_osd_show(msg, 2500);
        } else if (opt == 3) {
            if (vis_count > 0 && *selected_item >= 0 && *selected_item < vis_count) {
                Ps3LibGame *cur_g = &s_lib_games[vis_index[*selected_item]];
                int rc = cover_mgr_download(cur_g->title);
                if (rc == 0) {
                    lib_cover_unload();
                    lib_cover_load(cur_g->title, cur_g->path);
                    s_last_lib_selected_item = *selected_item;
                    ps3_show_message_box("Cover Downloaded", "Boxart saved to:\n./data/covers/", "OK (X)");
                } else if (rc == -6) {
                    ps3_show_message_box("Cover Not Found", "No boxart found on the server for this title.", "OK (X)");
                } else if (rc == -2) {
                    ps3_show_message_box("No Server Configured", "Configure server URL in:\n./data/covers/source.txt", "OK (X)");
                } else {
                    ps3_show_message_box("Download Failed", "Network error or no internet connection.\nServer: ./data/covers/source.txt", "OK (X)");
                }
            } else {
                ps3_show_message_box("Download Cover", "No game currently selected.", "OK (X)");
            }
        }
        while (1) {
            ps3_gui_update_input(input);
            if (!(input->pad.buttons & (PS3_BTN_CROSS | PS3_BTN_CIRCLE | PS3_BTN_SQUARE | PS3_BTN_TRIANGLE)))
                break;
            SDL_Delay(10);
        }
        input->pressed = 0;
        input->held = 0;
        return;
    }

    if ((input->pressed & PS3_BTN_MINUS) && vis_count > 0 && *selected_item >= 0 && *selected_item < vis_count) {
        whdload_toggle_favorite(s_lib_games[vis_index[*selected_item]].title);
        bool now_fav = whdload_is_favorite(s_lib_games[vis_index[*selected_item]].title);
        char fav_msg[128];
        snprintf(fav_msg, sizeof(fav_msg), "%s: %s", now_fav ? "ADDED TO FAVORITES" : "REMOVED FROM FAVORITES", s_lib_games[vis_index[*selected_item]].title);
        ps3_osd_show(fav_msg, 2000);
    }

    if (vis_count > 0 && *selected_item >= 0 && *selected_item < vis_count) {
        if (*selected_item != s_last_lib_selected_item) {
            s_last_lib_selected_item = *selected_item;
            const Ps3LibGame *cur_g = &s_lib_games[vis_index[*selected_item]];
            lib_cover_load(cur_g->title, cur_g->path);
        }
    } else {
        if (s_last_lib_selected_item != -1) {
            s_last_lib_selected_item = -1;
            lib_cover_unload();
        }
    }

    if (input->pressed & PS3_BTN_CROSS) {
        if (vis_count > 0 && *selected_item >= 0 && *selected_item < vis_count) {
            Ps3LibGame *game = &s_lib_games[vis_index[*selected_item]];
            /* Path evidence beats the cached type: a WHDLoad archive or an
             * installed game must never reach the floppy launch path, which
             * clears the HD mount and inserts the game into DF0:. */
            {
                int32_t clicked_type = lib_entry_true_type(game->path, game->title, (int32_t)game->type);
                if (clicked_type != (int32_t)game->type) {
                    write_log("[PS3] lib: click '%s' healed type %d -> %d (path evidence)\n",
                              game->title, (int)game->type, (int)clicked_type);
                    game->type = (Ps3LibGameType)clicked_type;
                    int c_idx = lib_cache_find(game->path);
                    if (c_idx >= 0) {
                        s_lib_cache[c_idx].type = clicked_type;
                        lib_cache_save();
                    }
                }
            }
            if (game->type == PS3_LIB_FLOPPY || game->type == PS3_LIB_ZIP) {
                ps3_unmount_cd32();
                if (emulating && uae4all_image_file0[0] != '\0') {
                    int act = ps3_show_disk_swap_dialog(game->title, game->model);
                    if (act == 1) {
                        m3u_clear();
                        copy_drive_path(uae4all_image_file0, game->path);
                        ps3_gui_draw_progress("Loading...", "Switching floppy disk...", 0.5f, game->title);
                        gui_update();
                        char osd_msg[128];
                        snprintf(osd_msg, sizeof(osd_msg), "INSERTED IN DF0: %s", game->title);
                        ps3_osd_show(osd_msg, 2500);
                        mainMenu_case = MAIN_MENU_CASE_RUN;
                        return;
                    } else if (act >= 2) {
                        int chosen_model = act - 2;
                        game->model = chosen_model;
                        int c_idx = lib_cache_find(game->path);
                        if (c_idx >= 0) {
                            s_lib_cache[c_idx].model = chosen_model;
                            lib_cache_save();
                        } else if (s_lib_cache_count < 4096) {
                            strncpy(s_lib_cache[s_lib_cache_count].path, game->path, sizeof(s_lib_cache[s_lib_cache_count].path) - 1);
                            s_lib_cache[s_lib_cache_count].path[sizeof(s_lib_cache[s_lib_cache_count].path) - 1] = '\0';
                            strncpy(s_lib_cache[s_lib_cache_count].title, game->title, sizeof(s_lib_cache[s_lib_cache_count].title) - 1);
                            s_lib_cache[s_lib_cache_count].title[sizeof(s_lib_cache[s_lib_cache_count].title) - 1] = '\0';
                            s_lib_cache[s_lib_cache_count].type = (int32_t)game->type;
                            s_lib_cache[s_lib_cache_count].model = chosen_model;
                            s_lib_cache[s_lib_cache_count].mtime = 0;
                            s_lib_cache[s_lib_cache_count].size = 0;
                            s_lib_cache_count++;
                            lib_cache_save();
                        }
                        vita_eject_all_floppies();
                        vita_eject_all_hdf();
                        m3u_clear();
                        copy_drive_path(uae4all_image_file0, game->path);
                        mainMenu_whdload_game[0] = '\0';
                        uae4all_hard_dir[0] = '\0';
                        mainMenu_bootHD = 0;
                        reset_hdConf();
                        ps3_apply_floppy_model(chosen_model);
                        ps3_gui_draw_progress("Loading...", "Starting Amiga floppy...", 0.5f, game->title);
                        gui_update();
                        mainMenu_case = MAIN_MENU_CASE_RESET;
                        return;
                    } else {
                        while (1) {
                            ps3_gui_update_input(input);
                            if (!(input->pad.buttons & (PS3_BTN_CROSS | PS3_BTN_CIRCLE | PS3_BTN_SQUARE | PS3_BTN_TRIANGLE)))
                                break;
                            SDL_Delay(10);
                        }
                        input->pressed = 0;
                        input->held = 0;
                        return;
                    }
                }
                vita_eject_all_floppies();
                vita_eject_all_hdf();
                m3u_clear();
                copy_drive_path(uae4all_image_file0, game->path);
                mainMenu_whdload_game[0] = '\0';
                uae4all_hard_dir[0] = '\0';
                mainMenu_bootHD = 0;
                reset_hdConf();
                ps3_apply_floppy_model(game->model);
                ps3_gui_draw_progress("Loading...", "Starting Amiga floppy...", 0.5f, game->title);
                gui_update();
                mainMenu_case = MAIN_MENU_CASE_RESET;
                return;
            } else if (game->type == PS3_LIB_M3U) {
                ps3_unmount_cd32();
                if (emulating && uae4all_image_file0[0] != '\0') {
                    int act = ps3_show_disk_swap_dialog(game->title, game->model);
                    if (act == 1) {
                        m3u_load(game->path);
                        ps3_gui_draw_progress("Loading...", "Mounting floppy playlist...", 0.5f, game->title);
                        ps3_gui_draw_progress("Loading...", "Mounting floppy playlist...", 0.5f, game->title);
                        gui_update();
                        ps3_osd_show("M3U PLAYLIST LOADED IN DF0:", 2500);
                        mainMenu_case = MAIN_MENU_CASE_RUN;
                        return;
                    } else if (act >= 2) {
                        int chosen_model = act - 2;
                        game->model = chosen_model;
                        int c_idx = lib_cache_find(game->path);
                        if (c_idx >= 0) {
                            s_lib_cache[c_idx].model = chosen_model;
                            lib_cache_save();
                        } else if (s_lib_cache_count < 4096) {
                            strncpy(s_lib_cache[s_lib_cache_count].path, game->path, sizeof(s_lib_cache[s_lib_cache_count].path) - 1);
                            s_lib_cache[s_lib_cache_count].path[sizeof(s_lib_cache[s_lib_cache_count].path) - 1] = '\0';
                            strncpy(s_lib_cache[s_lib_cache_count].title, game->title, sizeof(s_lib_cache[s_lib_cache_count].title) - 1);
                            s_lib_cache[s_lib_cache_count].title[sizeof(s_lib_cache[s_lib_cache_count].title) - 1] = '\0';
                            s_lib_cache[s_lib_cache_count].type = (int32_t)game->type;
                            s_lib_cache[s_lib_cache_count].model = chosen_model;
                            s_lib_cache[s_lib_cache_count].mtime = 0;
                            s_lib_cache[s_lib_cache_count].size = 0;
                            s_lib_cache_count++;
                            lib_cache_save();
                        }
                        vita_eject_all_floppies();
                        vita_eject_all_hdf();
                        m3u_load(game->path);
                        mainMenu_whdload_game[0] = '\0';
                        uae4all_hard_dir[0] = '\0';
                        mainMenu_bootHD = 0;
                        reset_hdConf();
                        ps3_apply_floppy_model(chosen_model);
                        ps3_gui_draw_progress("Loading...", "Starting Amiga floppy...", 0.5f, game->title);
                        gui_update();
                        mainMenu_case = MAIN_MENU_CASE_RESET;
                        return;
                    } else {
                        while (1) {
                            ps3_gui_update_input(input);
                            if (!(input->pad.buttons & (PS3_BTN_CROSS | PS3_BTN_CIRCLE | PS3_BTN_SQUARE | PS3_BTN_TRIANGLE)))
                                break;
                            SDL_Delay(10);
                        }
                        input->pressed = 0;
                        input->held = 0;
                        return;
                    }
                }
                vita_eject_all_floppies();
                vita_eject_all_hdf();
                m3u_load(game->path);
                mainMenu_whdload_game[0] = '\0';
                uae4all_hard_dir[0] = '\0';
                mainMenu_bootHD = 0;
                reset_hdConf();
                ps3_apply_floppy_model(game->model);
                ps3_gui_draw_progress("Loading...", "Starting Amiga floppy...", 0.5f, game->title);
                gui_update();
                mainMenu_case = MAIN_MENU_CASE_RESET;
                return;
            } else if (game->type == PS3_LIB_LHA) {
                ps3_unmount_cd32();
                char installed_path[512];
                installed_path[0] = '\0';
                if (ps3_whdload_install_lha(game->path, installed_path, sizeof(installed_path))) {
                    const char *folder = strrchr(installed_path, '/');
                    folder = folder ? folder + 1 : installed_path;
                    if (folder && folder[0] && ps3_whdload_can_launch(folder)) {
                        vita_eject_all_floppies();
                        vita_eject_all_hdf();
                        strncpy(mainMenu_whdload_game, folder, sizeof(mainMenu_whdload_game) - 1);
                        mainMenu_whdload_game[sizeof(mainMenu_whdload_game) - 1] = '\0';
                        whdload_ensure_game_dir(folder);
                        whdload_mark_recent(folder);
                        ps3_gui_draw_progress("Loading...", "Starting WHDLoad game...", 0.5f, folder);
                        if (!ps3_whdload_preflight(folder))
                            return;
                        strncpy(uae4all_hard_dir, ps3_whdload_root(), 255);
                        uae4all_hard_dir[255] = '\0';
                        ApplyWHDLoadPreset(folder);
                        ps3_set_kickstart(kickstart, 0);
                        bReloadKickstart = 1;
                        gui_update();
                        mainMenu_case = MAIN_MENU_CASE_RUN;
                        return;
                    } else {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "Game extracted to '%s' but no .slave launcher found", folder ? folder : "?");
                        ps3_show_message_box("WHDLoad Warning", msg, "OK (A)");
                    }
                } else {
                    char err_buf[300];
                    snprintf(err_buf, sizeof(err_buf), "The LHA archive could not be extracted.\n%s", ps3_whdload_get_last_error());
                    ps3_show_message_box("Installation Failed", err_buf, "OK (A)");
                }
            } else if (game->type == PS3_LIB_WHDLOAD) {
                ps3_unmount_cd32();
                if (ps3_whdload_can_launch(game->title)) {
                    vita_eject_all_floppies();
                    vita_eject_all_hdf();
                    strncpy(mainMenu_whdload_game, game->title, sizeof(mainMenu_whdload_game) - 1);
                    mainMenu_whdload_game[sizeof(mainMenu_whdload_game) - 1] = '\0';
                    whdload_ensure_game_dir(game->title);
                    whdload_mark_recent(game->title);
                    ps3_gui_draw_progress("Loading...", "Starting WHDLoad game...", 0.5f, game->title);
                    if (!ps3_whdload_preflight(game->title))
                        return;
                    strncpy(uae4all_hard_dir, ps3_whdload_root(), 255);
                    uae4all_hard_dir[255] = '\0';
                    ApplyWHDLoadPreset(game->title);
                    ps3_set_kickstart(kickstart, 0);
                    bReloadKickstart = 1;
                    gui_update();
                    mainMenu_case = MAIN_MENU_CASE_RUN;
                    return;
                } else {
                    ps3_show_message_box("WHDLoad Error", "No .slave file was found in the game directory.", "OK (X)");
                }
            } else if (game->type == PS3_LIB_CD32) {
                ps3_reset_media_state();
                if (cdrom_open_image(game->path)) {
                    mainMenu_whdload_game[0] = '\0';
                    uae4all_hard_dir[0] = '\0';
                    mainMenu_bootHD = 0;
                    reset_hdConf();
                    ApplyCd32Profile();
                    ps3_set_kickstart(kickstart, 0);
                    bReloadKickstart = 1;
                    ps3_gui_draw_progress("Loading...", "Starting CD32 game...", 0.5f, game->title);
                    gui_update();
                    mainMenu_case = MAIN_MENU_CASE_RESET;
                    return;
                } else {
                    ps3_show_message_box("CD32 Error", "The selected CD image could not be loaded.", "OK (X)");
                }
            } else if (game->type == PS3_LIB_HDF) {
                HdfInfo hdf_info;
                if (!hdf_analyze(game->path, &hdf_info)) {
                    char message[320];
                    snprintf(message, sizeof(message), "HDF cannot be opened: %.250s", hdf_info.error);
                    ps3_show_message_box("HDF Error", message, "OK (X)");
                    return;
                }
                if (hdf_info.bootable_partitions == 0) {
                    char message[400];
                    if (hdf_info.is_rdb) {
                        snprintf(message, sizeof(message),
                            "This RDB image holds %d partition(s) (%s) but none carries an Amiga DOS boot block, so the Amiga reports \"no disk\".\n\n\"Mount Anyway\" keeps it mounted: boot a Workbench floppy and format a partition first.",
                            hdf_info.partition_count,
                            hdf_info.partition_names[0] ? hdf_info.partition_names : "?");
                    } else {
                        snprintf(message, sizeof(message),
                            "No Amiga DOS boot block was found at sector 0. The image may be unformatted, non-bootable or have an unsupported geometry.\n\n\"Mount Anyway\" keeps it mounted: boot a Workbench floppy and format it first.");
                    }
                    write_log("[PS3] HDF not bootable: path='%s' RDB=%d parts=%d bootable=%d DOS=0x%08x size=%lu\n",
                              game->path, hdf_info.is_rdb, hdf_info.partition_count,
                              hdf_info.bootable_partitions, hdf_info.dostype, hdf_info.size);
                    if (!ps3_show_confirm_box("HDF Not Bootable", message, "Mount Anyway", "Cancel (B)"))
                        return;
                }
                ps3_reset_media_state();
                copy_drive_path(uae4all_hard_file0, game->path);
                make_hard_file_cfg_line(uae4all_hard_file0);
                mainMenu_whdload_game[0] = '\0';
                uae4all_hard_dir[0] = '\0';
                mainMenu_bootHD = 2;
                reset_hdConf();
                ApplyAutomaticGamePreset(1);
                ps3_set_kickstart(kickstart, 0);
                bReloadKickstart = 1;
                ps3_gui_draw_progress("Loading...", "Starting hard disk game...", 0.5f, game->title);
                gui_update();
                mainMenu_case = MAIN_MENU_CASE_RESET;
                return;
            }
        }
    }

    float card_x = 20.0f;
    float card_w = 560.0f;
    const float start_y = PS3_LIST_START_Y;
    const float item_h = 56.0f;
    const float item_gap = 8.0f;
    const int visible_items = ps3_list_visible_rows(start_y, item_h, item_gap);
    int first_item = *selected_item >= visible_items ? *selected_item - visible_items + 1 : 0;

    if (vis_count == 0) {
        ps3_draw_card_custom(card_x, start_y, card_w, 120.0f, PS3_COLOR_CARD, PS3_COLOR_CARD_BORDER);
        ps3_draw_text(card_x + 20.0f, start_y + 24.0f, PS3_COLOR_TEXT_WHITE, 0.95f, "No Games Found");
        ps3_draw_text(card_x + 20.0f, start_y + 54.0f, PS3_COLOR_TEXT_MUTED, 0.78f, "Place .adf / .m3u / .chd / .lha in your ROM folder or install WHDLoad games.");
        ps3_draw_text(card_x + 20.0f, start_y + 78.0f, PS3_COLOR_AMIGA_ORANGE, 0.78f, "Press (Y) for Library Options or (X) to Change Filter.");
    } else {
        for (int i = 0; i < visible_items; i++) {
            int item = first_item + i;
            if (item >= vis_count) break;
            float y = start_y + (float)i * (item_h + item_gap);
            int orig = vis_index[item];
            const Ps3LibGame *g = &s_lib_games[orig];
            bool fav = whdload_is_favorite(g->title);

            const char *sub = "Game";
            const char *badge = "READY";
            unsigned int badge_col = PS3_COLOR_AMIGA_BLUE;

            if (g->type == PS3_LIB_WHDLOAD) {
                sub = "Installed WHDLoad AGA / ECS game";
                badge = "WHDLOAD";
                badge_col = RGBA8(168, 85, 247, 255);
            } else if (g->type == PS3_LIB_LHA) {
                sub = "WHDLoad LHA Game Archive";
                badge = "LHA";
                badge_col = RGBA8(234, 179, 8, 255);
            } else if (g->type == PS3_LIB_FLOPPY) {
                sub = "Amiga Floppy Disk (.adf)";
                badge = "FLOPPY";
                badge_col = RGBA8(14, 165, 233, 255);
            } else if (g->type == PS3_LIB_M3U) {
                sub = "Multi-Disk Floppy Playlist (.m3u)";
                badge = "M3U";
                badge_col = RGBA8(249, 115, 22, 255);
            } else if (g->type == PS3_LIB_ZIP) {
                sub = "Compressed Game Archive (.zip)";
                badge = "ZIP";
                badge_col = RGBA8(236, 72, 153, 255);
            } else if (g->type == PS3_LIB_CD32) {
                sub = "Amiga CD32 Disc Image";
                badge = "CD32";
                badge_col = RGBA8(229, 37, 33, 255);
            } else if (g->type == PS3_LIB_HDF) {
                sub = "Bootable Hardfile (.hdf)";
                badge = "HARDFILE";
                badge_col = RGBA8(16, 185, 129, 255);
            }

            if (fav) {
                badge = "* FAV";
                badge_col = PS3_COLOR_AMIGA_RED;
            }

            bool focused = (item == *selected_item);
            ps3_draw_button_item_custom(card_x, y, card_w, item_h, g->title, sub, badge, badge_col, focused, false);
        }
        ps3_draw_list_page_indicator(*selected_item, vis_count, visible_items);
    }

    float preview_x = 596.0f;
    float preview_w = PS3_SCREEN_W - 20.0f - preview_x;
    float preview_y = PS3_LIST_START_Y;
    float preview_h = PS3_LIST_BOTTOM_Y - preview_y;
    ps3_draw_card_custom(preview_x, preview_y, preview_w, preview_h, PS3_COLOR_CARD, PS3_COLOR_CARD_BORDER);

    const char *filter_names[9] = {
        "ALL GAMES",
        "FLOPPY (ADF)",
        "ZIP ARCHIVES",
        "M3U PLAYLISTS",
        "WHDLOAD INSTALLED",
        "LHA ARCHIVES",
        "CD32 IMAGES",
        "HARDFILES (HDF)",
        "FAVORITES"
    };
    Uint32 filter_badge_col = PS3_COLOR_AMIGA_ORANGE;
    if (s_lib_filter == 1) filter_badge_col = RGBA8(14, 165, 233, 255);
    else if (s_lib_filter == 2) filter_badge_col = RGBA8(236, 72, 153, 255);
    else if (s_lib_filter == 3) filter_badge_col = RGBA8(249, 115, 22, 255);
    else if (s_lib_filter == 4) filter_badge_col = RGBA8(168, 85, 247, 255);
    else if (s_lib_filter == 5) filter_badge_col = RGBA8(234, 179, 8, 255);
    else if (s_lib_filter == 6) filter_badge_col = RGBA8(229, 37, 33, 255);
    else if (s_lib_filter == 7) filter_badge_col = RGBA8(16, 185, 129, 255);
    else if (s_lib_filter == 8) filter_badge_col = RGBA8(239, 68, 68, 255);

    float fbw = (float)ps3_get_text_width(0.80f, filter_names[s_lib_filter]) + 16.0f;
    ps3_draw_badge(preview_x + preview_w - fbw - 14.0f, preview_y + 12.0f, filter_names[s_lib_filter], filter_badge_col, RGBA8(20, 24, 34, 255));

    if (vis_count > 0 && *selected_item >= 0 && *selected_item < vis_count) {
        const Ps3LibGame *g = &s_lib_games[vis_index[*selected_item]];
        bool is_fav = whdload_is_favorite(g->title);

        const char *type_badge = "GAME";
        Uint32 type_badge_col = PS3_COLOR_AMIGA_RED;
        if (g->type == PS3_LIB_WHDLOAD) { type_badge = "WHDLOAD GAME"; type_badge_col = RGBA8(168, 85, 247, 255); }
        else if (g->type == PS3_LIB_LHA) { type_badge = "WHDLOAD ARCHIVE"; type_badge_col = RGBA8(234, 179, 8, 255); }
        else if (g->type == PS3_LIB_FLOPPY) { type_badge = "AMIGA FLOPPY DISK"; type_badge_col = RGBA8(14, 165, 233, 255); }
        else if (g->type == PS3_LIB_M3U) { type_badge = "MULTI-DISK PLAYLIST"; type_badge_col = RGBA8(249, 115, 22, 255); }
        else if (g->type == PS3_LIB_ZIP) { type_badge = "ZIP ARCHIVE"; type_badge_col = RGBA8(236, 72, 153, 255); }
        else if (g->type == PS3_LIB_CD32) { type_badge = "AMIGA CD32 CONSOLE"; type_badge_col = RGBA8(229, 37, 33, 255); }
        else if (g->type == PS3_LIB_HDF) { type_badge = "BOOTABLE HARDFILE"; type_badge_col = RGBA8(16, 185, 129, 255); }

        ps3_draw_badge(preview_x + 14.0f, preview_y + 12.0f, type_badge, type_badge_col, PS3_COLOR_TEXT_WHITE);

        float art_x = preview_x + 16.0f;
        float art_y = preview_y + 42.0f;
        float art_w = preview_w - 32.0f;
        float art_h = 210.0f;

        SDL_Surface *lib_cover_surf = cover_mgr_get();
        if (lib_cover_surf) {
            float surf_w = (float)lib_cover_surf->w;
            float surf_h = (float)lib_cover_surf->h;
            float scale_x = art_w / surf_w;
            float scale_y = art_h / surf_h;
            float final_scale = (scale_x < scale_y) ? scale_x : scale_y;
            if (final_scale > 1.0f) final_scale = 1.0f;
            if (final_scale < 0.05f) final_scale = 0.05f;

            int draw_w = (int)(surf_w * final_scale);
            int draw_h = (int)(surf_h * final_scale);
            int draw_x = (int)(art_x + (art_w - draw_w) * 0.5f);
            int draw_y = (int)(art_y + (art_h - draw_h) * 0.5f);

            ps3_draw_rounded_rect(art_x - 3.0f, art_y - 3.0f, art_w + 6.0f, art_h + 6.0f, 6.0f, RGBA8(10, 13, 20, 255));
            SDL_Rect dst_r = { (Sint16)draw_x, (Sint16)draw_y, (Uint16)draw_w, (Uint16)draw_h };
            SDL_SoftStretch(lib_cover_surf, NULL, prSDLScreen, &dst_r);
        } else {
            s_lib_cover_angle += 0.08f;
            ps3_draw_rounded_rect(art_x, art_y, art_w, art_h, 6.0f, RGBA8(18, 22, 32, 255));
            ps3_draw_boing_ball_icon(art_x + (art_w * 0.5f), art_y + (art_h * 0.5f) - 10.0f, 34.0f, s_lib_cover_angle);
            ps3_draw_text_centered(art_x + (art_w * 0.5f), art_y + art_h - 28.0f, PS3_COLOR_TEXT_MUTED, 0.85f, "No cover found");
        }

        ps3_draw_text(preview_x + 16.0f, preview_y + 262.0f, PS3_COLOR_TEXT_DIM, 0.72f, "GAME TITLE");
        char title_buf[128];
        float title_max_w = preview_w - 32.0f - (is_fav ? 78.0f : 0.0f);
        ps3_truncate_text(g->title, title_max_w, 0.90f, title_buf, sizeof(title_buf));
        ps3_draw_text(preview_x + 16.0f, preview_y + 278.0f, PS3_COLOR_TEXT_WHITE, 0.90f, title_buf);
        if (is_fav) {
            ps3_draw_badge(preview_x + preview_w - 86.0f, preview_y + 274.0f, "* FAV", PS3_COLOR_AMIGA_RED, PS3_COLOR_TEXT_WHITE);
        }

        char lib_meta_key[256];
        cover_mgr_normalize(g->title, lib_meta_key, sizeof(lib_meta_key));
        struct GameMetadata lib_gm;
        bool has_meta = meta_db_lookup(lib_meta_key, &lib_gm);

        char meta_l1[128];
        if (has_meta && strcmp(lib_gm.year, "N/A") != 0) {
            snprintf(meta_l1, sizeof(meta_l1), "Year: %s  |  %s", lib_gm.year, lib_gm.genre);
        } else {
            snprintf(meta_l1, sizeof(meta_l1), "Year: ----  |  Amiga Game");
        }
        ps3_truncate_text(meta_l1, preview_w - 32.0f, 0.72f, meta_l1, sizeof(meta_l1));
        ps3_draw_text(preview_x + 16.0f, preview_y + 300.0f, PS3_COLOR_AMIGA_ORANGE, 0.72f, meta_l1);

        char meta_l2[128];
        if (has_meta && strcmp(lib_gm.developer, "N/A") != 0) {
            snprintf(meta_l2, sizeof(meta_l2), "Dev: %s  |  Players: %s", lib_gm.developer, lib_gm.players);
        } else {
            snprintf(meta_l2, sizeof(meta_l2), "Dev: Unknown");
        }
        ps3_truncate_text(meta_l2, preview_w - 32.0f, 0.70f, meta_l2, sizeof(meta_l2));
        ps3_draw_text(preview_x + 16.0f, preview_y + 316.0f, PS3_COLOR_TEXT_MUTED, 0.70f, meta_l2);

        ps3_draw_text(preview_x + 16.0f, preview_y + 338.0f, PS3_COLOR_TEXT_DIM, 0.72f, "AUTODETECTED PROFILE");
        const char *prof_badge = "AMIGA 500";
        const char *prof_desc = "Amiga 500 (Kickstart 1.3 | OCS/ECS | 512K Chip + 512K Slow)";
        Uint32 prof_badge_color = PS3_COLOR_AMIGA_ORANGE;
        if (g->type == PS3_LIB_WHDLOAD) {
            int is_aga = 0;
            if (mainMenu_whdload_mode == 1) {
                is_aga = 0;
            } else if (mainMenu_whdload_mode == 2) {
                is_aga = 1;
            } else {
                is_aga = (g->model == 2) ? 1 : ps3_whdload_is_aga(g->title);
            }
            if (is_aga) {
                prof_badge = "A1200 AGA";
                prof_desc = "68020 14MHz | Kickstart 3.1 | 2MB Chip + 4MB Fast";
                prof_badge_color = PS3_COLOR_AMIGA_ORANGE;
            } else {
                int needs_imm = ps3_whdload_needs_immediate_blitter(g->title);
                int needs_improved = ps3_whdload_needs_improved_blitter(g->title);
                if (mainMenu_whdload_blitter == 1) {
                    needs_imm = 0;
                    needs_improved = 0;
                } else if (mainMenu_whdload_blitter == 2) {
                    needs_imm = 1;
                    needs_improved = 0;
                } else if (mainMenu_whdload_blitter == 3) {
                    needs_imm = 0;
                    needs_improved = 1;
                }
                prof_badge = "A500 OCS";
                if (needs_imm) {
                    prof_desc = "68000 7MHz | Immediate Blitter | 2MB Chip + 4MB Fast";
                } else if (needs_improved) {
                    prof_desc = "68000 7MHz | Improved Partial Blitter | 2MB Chip + 4MB Fast";
                } else {
                    prof_desc = "68000 7MHz | Normal Blitter | 2MB Chip + 4MB Fast";
                }
                prof_badge_color = PS3_COLOR_AMIGA_GREEN;
            }
        } else if (g->type == PS3_LIB_LHA) {
            prof_badge = "WHDLOAD LHA";
            prof_desc = "Auto-extracts and installs to WHDLoad games directory";
        } else if (g->type == PS3_LIB_M3U) {
            if (g->model == 2) {
                prof_badge = "A1200 AGA (M3U)";
                prof_desc = "Amiga 1200 | Kickstart 3.1 | AGA | Hot-swap with ZL + D-Pad";
            } else if (g->model == 1) {
                prof_badge = "A600 ECS (M3U)";
                prof_desc = "Amiga 600 | Kickstart 2.05 | ECS | Hot-swap with ZL + D-Pad";
            } else {
                prof_badge = "A500 MULTI-DISK";
                prof_desc = "Amiga 500 (Hot-swap disks during play with ZL + D-Pad)";
            }
        } else if (g->type == PS3_LIB_ZIP || g->type == PS3_LIB_FLOPPY) {
            if (g->model == 2) {
                prof_badge = (g->type == PS3_LIB_ZIP) ? "A1200 ZIP" : "A1200 AGA";
                prof_desc = "Amiga 1200 (Kickstart 3.1 | AGA | 68020 2MB Chip + 4MB Fast)";
            } else if (g->model == 1) {
                prof_badge = (g->type == PS3_LIB_ZIP) ? "A600 ZIP" : "A600 ECS";
                prof_desc = "Amiga 600 (Kickstart 2.05 | ECS | 2MB Chip RAM)";
            } else {
                prof_badge = (g->type == PS3_LIB_ZIP) ? "AMIGA 500 ZIP" : "AMIGA 500";
                prof_desc = "Amiga 500 (Kickstart 1.3 | OCS/ECS | 512K Chip + 512K Slow)";
            }
        } else if (g->type == PS3_LIB_CD32) {
            prof_badge = "AMIGA CD32";
            prof_desc = "Akiko Chipset | CD-ROM | Kickstart 3.1 Extended";
        } else if (g->type == PS3_LIB_HDF) {
            prof_badge = "A1200 HARDFILE";
            prof_desc = "Amiga 1200 (Boot HD | Kickstart 3.1 | Fast RAM)";
        }

        ps3_draw_badge(preview_x + 16.0f, preview_y + 356.0f, prof_badge, prof_badge_color, RGBA8(20, 24, 34, 255));
        bool can_swap = (emulating && uae4all_image_file0[0] != '\0' && (g->type == PS3_LIB_FLOPPY || g->type == PS3_LIB_M3U || g->type == PS3_LIB_ZIP));
        ps3_draw_hint_item(preview_x + preview_w - (can_swap ? 190.0f : 150.0f), preview_y + 356.0f, PS3_GLYPH_CROSS, can_swap ? "SWAP / BOOT" : "LAUNCH");
        char hw_buf[128];
        ps3_truncate_text(prof_desc, preview_w - 32.0f, 0.76f, hw_buf, sizeof(hw_buf));
        ps3_draw_text(preview_x + 16.0f, preview_y + 384.0f, PS3_COLOR_TEXT_MUTED, 0.76f, hw_buf);
        if (g->type == PS3_LIB_FLOPPY || g->type == PS3_LIB_M3U || g->type == PS3_LIB_ZIP) {
            ps3_draw_text(preview_x + 16.0f, preview_y + 400.0f, PS3_COLOR_TEXT_DIM, 0.68f, "(L/R) Cycle Model: A500 / A600 / A1200");
        }
    } else {
        ps3_draw_badge(preview_x + 14.0f, preview_y + 12.0f, "GAME LIBRARY", RGBA8(40, 50, 70, 255), PS3_COLOR_TEXT_WHITE);
        float art_x = preview_x + 16.0f;
        float art_y = preview_y + 42.0f;
        float art_w = preview_w - 32.0f;
        float art_h = 210.0f;
        s_lib_cover_angle += 0.08f;
        ps3_draw_rounded_rect(art_x, art_y, art_w, art_h, 6.0f, RGBA8(18, 22, 32, 255));
        ps3_draw_boing_ball_icon(art_x + (art_w * 0.5f), art_y + (art_h * 0.5f) - 10.0f, 34.0f, s_lib_cover_angle);
        ps3_draw_text_centered(art_x + (art_w * 0.5f), art_y + art_h - 28.0f, PS3_COLOR_AMIGA_ORANGE, 0.85f, "Press (Y) for options");
        ps3_draw_text(preview_x + 16.0f, preview_y + 310.0f, PS3_COLOR_TEXT_WHITE, 0.95f, "All-in-One Game Library");
        ps3_draw_text(preview_x + 16.0f, preview_y + 340.0f, PS3_COLOR_TEXT_MUTED, 0.80f, "Supports WHDLoad, ADF floppies, M3U playlists, LHA, ZIP, HDF and CD32.");
    }
}

void ps3_view_presets(Ps3InputState *input, int *selected_item)
{
    const int total_items = 6;
    if (*selected_item < 0) *selected_item = 0;
    if (*selected_item >= total_items) *selected_item = total_items - 1;

    if (input->pressed & PS3_BTN_UP) {
        (*selected_item)--;
        if (*selected_item < 0) *selected_item = total_items - 1;
    }
    if (input->pressed & PS3_BTN_DOWN) {
        (*selected_item)++;
        if (*selected_item >= total_items) *selected_item = 0;
    }

        if (input->pressed & PS3_BTN_CROSS) {
        if (*selected_item == 0) {
            kickstart = 1;
            extfile[0] = '\0';
            mainMenu_CPU_model = 0;
            mainMenu_chipset = 0;
            mainMenu_chipMemory = 0;
            mainMenu_slowMemory = 1;
            mainMenu_fastMemory = 0;
            UpdateCPUModelSettings();
            UpdateMemorySettings();
            UpdateChipsetSettings();
            int kickstart_loaded = ps3_set_kickstart(kickstart, 0);
            bReloadKickstart = 1;
            if (kickstart_loaded)
                ps3_show_message_box("Preset Applied", "Amiga 500 (OCS 1.3, 512K+512K RAM) configured! Press Save Game Configuration to save it.", "OK (X)");
            else
                ps3_show_message_box("Kickstart Missing", "Kickstart 1.3 ROM (kick13.rom / kick34005.A500) not found in /dev_hdd0/uae4all/kickstarts/.", "OK (X)");
        } else if (*selected_item == 1) {
            kickstart = 2;
            extfile[0] = '\0';
            mainMenu_CPU_model = 0;
            mainMenu_chipset = 1;
            mainMenu_chipMemory = 1;
            mainMenu_slowMemory = 0;
            mainMenu_fastMemory = 1;
            UpdateCPUModelSettings();
            UpdateMemorySettings();
            UpdateChipsetSettings();
            int kickstart_loaded = ps3_set_kickstart(kickstart, 0);
            bReloadKickstart = 1;
            if (kickstart_loaded)
                ps3_show_message_box("Preset Applied", "Amiga 500+ (ECS 2.04, 1MB+1MB RAM) configured! Press Save Game Configuration to save it.", "OK (X)");
            else
                ps3_show_message_box("Kickstart Missing", "Kickstart 2.04 ROM (kick20.rom / kick37175.A500) not found in /dev_hdd0/uae4all/kickstarts/.", "OK (X)");
        } else if (*selected_item == 2) {
            kickstart = 11;
            extfile[0] = '\0';
            mainMenu_CPU_model = 0;
            mainMenu_chipset = 1;
            mainMenu_chipMemory = 2;
            mainMenu_slowMemory = 0;
            mainMenu_fastMemory = 4;
            UpdateCPUModelSettings();
            UpdateMemorySettings();
            UpdateChipsetSettings();
            int kickstart_loaded = ps3_set_kickstart(kickstart, 0);
            bReloadKickstart = 1;
            if (kickstart_loaded)
                ps3_show_message_box("Preset Applied", "Amiga 600 (ECS 2.05, 2MB Chip + 8MB Fast RAM) configured! Press Save Game Configuration to save it.", "OK (X)");
            else
                ps3_show_message_box("Kickstart Missing", "Kickstart 2.05 ROM (kick37350.A600 / kick205.rom) not found in /dev_hdd0/uae4all/kickstarts/.", "OK (X)");
        } else if (*selected_item == 3) {
            kickstart = 3;
            extfile[0] = '\0';
            mainMenu_CPU_model = 1;
            mainMenu_chipset = 2;
            mainMenu_chipMemory = 2;
            mainMenu_slowMemory = 0;
            mainMenu_fastMemory = 3;
            UpdateCPUModelSettings();
            UpdateMemorySettings();
            UpdateChipsetSettings();
            ApplyA1200Profile();
            int kickstart_loaded = ps3_set_kickstart(kickstart, 0);
            bReloadKickstart = 1;
            if (kickstart_loaded)
                ps3_show_message_box("Preset Applied", "Amiga 1200 (AGA 3.1, 68020 2MB+4MB RAM) configured! Press Save Game Configuration to save it.", "OK (X)");
            else
                ps3_show_message_box("Kickstart Missing", "Kickstart 3.1 ROM (kick31.rom / kick40068.A1200) not found in /dev_hdd0/uae4all/kickstarts/.", "OK (X)");
        } else if (*selected_item == 4) {
            kickstart = 6;
            mainMenu_CPU_model = 1;
            mainMenu_chipset = 2;
            mainMenu_chipMemory = 2;
            mainMenu_slowMemory = 0;
            mainMenu_fastMemory = 0;
            UpdateCPUModelSettings();
            UpdateMemorySettings();
            UpdateChipsetSettings();
            int kickstart_loaded = ps3_set_kickstart(kickstart, 0);
            bReloadKickstart = 1;
            if (kickstart_loaded)
                ps3_show_message_box("Preset Applied", "Amiga CD32 (Akiko, CD32 Kickstart + Extended ROM) configured! Press Save Game Configuration to save it.", "OK (X)");
            else
                ps3_show_message_box("Kickstart Missing", "Kickstart CD32 ROM (kick40060.CD32 + kick40060.CD32.ext, or combined 1MB ROM) not found in /dev_hdd0/uae4all/kickstarts/.", "OK (X)");
        } else if (*selected_item == 5) {
            cdrom_close_image();
            cdrom_audio_stop();
            kickstart = 4;
            extfile[0] = '\0';
            mainMenu_CPU_model = 1;
            mainMenu_chipset = 2 | 0x100;
            mainMenu_chipMemory = 2;
            mainMenu_slowMemory = 0;
            mainMenu_fastMemory = 4;
            UpdateCPUModelSettings();
            UpdateMemorySettings();
            UpdateChipsetSettings();
            int kickstart_loaded = ps3_set_kickstart(kickstart, 0);
            bReloadKickstart = 1;
            if (kickstart_loaded)
                ps3_show_message_box("Preset Applied", "Custom ROM (68020 AGA, 2MB Chip + 8MB Fast RAM) configured! Press Save Game Configuration to save it.", "OK (X)");
            else
                ps3_show_message_box("Kickstart Missing", "Custom ROM (kickcustom.rom / custom.rom) not found in /dev_hdd0/uae4all/kickstarts/.", "OK (X)");
        }
    }


    float card_x = 20.0f;
    float card_w = PS3_SCREEN_W - 40.0f;
    const float start_y = PS3_LIST_START_Y;
    const float item_h = 78.0f;
    const float item_gap = 10.0f;

    struct PresetInfo {
        const char *title;
        const char *desc;
        const char *tag;
        const char *recom;
    } presets[6] = {
        { "Amiga 500 (Classic OCS 1.3)", "68000 7MHz | Kickstart 1.3 | 512KB Chip + 512KB Slow RAM", "OCS", "Recommended for 95% of classic Amiga games (1985-1993)" },
        { "Amiga 500+ (Enhanced ECS 2.04)", "68000 7MHz | Kickstart 2.04 | 1MB Chip + 1MB Fast RAM", "ECS", "Recommended for late ECS titles and productivity software" },
        { "Amiga 600 (Enhanced ECS 2.05)", "68000 7MHz | Kickstart 2.05 | 2MB Chip + 8MB Fast RAM", "ECS", "Recommended for Amiga 600 games and ECS software" },
        { "Amiga 1200 (Advanced AGA 3.1)", "68020 14MHz | Kickstart 3.1 | 2MB Chip + 4MB Fast RAM", "AGA", "Recommended for AGA games (Alien Breed 3D, Slam Tilt, Gloom)" },
        { "Amiga CD32 (Console CD Mode)", "68020 14MHz | Kickstart 3.1 CD32 | 2MB Chip RAM + Akiko", "CD32", "Recommended for Amiga CD32 ISO and CUE disc images" },
        { "Amiga Custom (Custom ROM)", "68020 14MHz | Custom Kickstart | 2MB Chip + 8MB Fast RAM", "CUST", "Uses kickcustom.rom / custom.rom in ./data/kickstarts/" }
    };

    const int visible_items = ps3_list_visible_rows(start_y, item_h, item_gap);
    int first_item = *selected_item >= visible_items ? *selected_item - visible_items + 1 : 0;

    for (int i = 0; i < visible_items; i++) {
        int item = first_item + i;
        if (item >= total_items) break;
        float cy = start_y + (float)i * (item_h + item_gap);
        bool focused = (*selected_item == item);

        ps3_draw_card(card_x, cy, card_w, item_h, focused, false);

        unsigned int badge_col = (item == 0) ? PS3_COLOR_AMIGA_BLUE :
                                 ((item == 1 || item == 2) ? PS3_COLOR_AMIGA_GREEN :
                                 ((item == 3 || item == 4) ? PS3_COLOR_AMIGA_RED :
                                 RGBA8(147, 51, 234, 255)));
        float badge_x = card_x + ((item == 4 || item == 5) ? 8.0f : 14.0f);
        ps3_draw_badge(badge_x, cy + 14.0f, presets[item].tag, badge_col, PS3_COLOR_TEXT_WHITE);

        ps3_draw_text(card_x + 72.0f, cy + 12.0f, focused ? PS3_COLOR_TEXT_WHITE : RGBA8(230, 240, 255, 255), 1.05f, presets[item].title);

        char desc_buf[256];
        ps3_truncate_text(presets[item].desc, card_w - 180.0f, 0.85f, desc_buf, sizeof(desc_buf));
        ps3_draw_text(card_x + 72.0f, cy + 34.0f, PS3_COLOR_TEXT_MUTED, 0.85f, desc_buf);

        char rec_buf[256];
        ps3_truncate_text(presets[item].recom, card_w - 180.0f, 0.82f, rec_buf, sizeof(rec_buf));
        ps3_draw_text(card_x + 72.0f, cy + 52.0f, PS3_COLOR_AMIGA_ORANGE, 0.82f, rec_buf);

        if (focused) {
            ps3_draw_hint_item(card_x + card_w - 140.0f, cy + 28.0f, PS3_GLYPH_CROSS, "Apply");
        }
    }
    ps3_draw_list_page_indicator(*selected_item, total_items, visible_items);
}

void ps3_view_hardware(Ps3InputState *input, int *selected_item)
{
    const int total_items = 16;
    if (*selected_item < 0) *selected_item = 0;
    if (*selected_item >= total_items) *selected_item = total_items - 1;

    if (input->pressed & PS3_BTN_UP) {
        (*selected_item)--;
        if (*selected_item < 0) *selected_item = total_items - 1;
    }
    if (input->pressed & PS3_BTN_DOWN) {
        (*selected_item)++;
        if (*selected_item >= total_items) *selected_item = 0;
    }

    if ((input->pressed & PS3_BTN_SQUARE) && *selected_item == 0) {
        char new_file[512];
        new_file[0] = '\0';
        int res = ps3_gui_run_browser(new_file, "/dev_hdd0/uae4all/kickstarts", 13);
        if (res == 1 && new_file[0] != '\0') {
            strncpy(romfile, new_file, sizeof(romfile) - 1);
            romfile[sizeof(romfile) - 1] = '\0';
            bReloadKickstart = 1;
            int loaded = vita_load_kickstart(romfile);
            char msg[300];
            const char *fname = strrchr(romfile, '/');
            fname = fname ? fname + 1 : romfile;
            if (loaded) {
                kickstart_warning = 0;
                snprintf(msg, sizeof(msg), "Loaded Kickstart ROM:\n%s", fname);
                ps3_show_message_box("Kickstart Loaded", msg, "OK (X)");
            } else {
                snprintf(msg, sizeof(msg), "Failed to load Kickstart ROM:\n%s", fname);
                ps3_show_message_box("Kickstart Error", msg, "OK (X)");
            }
        }
    }

    if (input->pressed & PS3_BTN_CROSS && *selected_item == 14) {
        char new_file[512];
        new_file[0] = '\0';
        int res = ps3_gui_run_browser(new_file, currentDir, 8);
        if (res == 1) {
            ps3_reset_media_state();
            if (cdrom_open_image(new_file)) {
                mainMenu_whdload_game[0] = '\0';
                ApplyCd32Profile();
                ps3_set_kickstart(kickstart, 0);
                bReloadKickstart = 1;
                ps3_show_message_box("CD32 Image", "CD image inserted and CD32 profile applied.", "OK (X)");
            } else
                ps3_show_message_box("CD32 Image Error", "The selected image could not be opened.", "OK (X)");
        }
    }
    if (input->pressed & PS3_BTN_TRIANGLE && *selected_item == 14) {
        cdrom_close_image();
        cdrom_set_cd32_mode(0);
        ps3_show_message_box("CD32 Image", "CD image ejected.", "OK (X)");
    }

    int dir = 0;
    if (input->pressed & (PS3_BTN_RIGHT | PS3_BTN_CROSS)) dir = 1;
    if (input->pressed & PS3_BTN_LEFT) dir = -1;

    if (dir != 0) {
        switch (*selected_item) {
            case 0:
                kickstart = (kickstart + dir + KICKSTART_ROM_COUNT) % KICKSTART_ROM_COUNT;
                bReloadKickstart = 1;
                ps3_set_kickstart(kickstart, 1);
                break;
            case 1:
                mainMenu_CPU_model = (mainMenu_CPU_model + dir + 2) % 2;
                bReloadKickstart = 1;
                UpdateCPUModelSettings();
                break;
            case 2:
                mainMenu_CPU_speed = (mainMenu_CPU_speed + dir + 4) % 4;
                break;
            case 3: {
                int cs = mainMenu_chipset & 0x00ff;
                cs = (cs + dir + 3) % 3;
                mainMenu_chipset = (mainMenu_chipset & 0xff00) | cs;
                bReloadKickstart = 1;
                UpdateChipsetSettings();
                break;
            }
            case 4: {
                int blit = (mainMenu_chipset & 0xff00) >> 8;
                blit = (blit + dir + 3) % 3;
                mainMenu_chipset = (mainMenu_chipset & 0x00ff) | (blit << 8);
                UpdateChipsetSettings();
                break;
            }
            case 5:
                mainMenu_cycleExact = 1 - mainMenu_cycleExact;
                UpdateChipsetSettings();
                break;
            case 6:
                mainMenu_spriteCollisions = (mainMenu_spriteCollisions + dir + 2) % 2;
                break;
            case 7:
                mainMenu_chipMemory = (mainMenu_chipMemory + dir + 4) % 4;
                bReloadKickstart = 1;
                UpdateMemorySettings();
                break;
            case 8:
                mainMenu_fastMemory = (mainMenu_fastMemory + dir + 5) % 5;
                bReloadKickstart = 1;
                UpdateMemorySettings();
                break;
            case 9:
                mainMenu_slowMemory = (mainMenu_slowMemory + dir + 4) % 4;
                bReloadKickstart = 1;
                UpdateMemorySettings();
                break;
            case 10:
                mainMenu_rtgMemory = (mainMenu_rtgMemory + dir + 3) % 3;
                bReloadKickstart = 1;
                UpdateMemorySettings();
                break;
            case 11: {
                mainMenu_sound = 1 - mainMenu_sound;
                sound_rate = 48000;
                getChanges();
                break;
            }
            case 12:
                mainMenu_soundStereo = 1 - mainMenu_soundStereo;
                getChanges();
                break;
            case 13:
                mainMenu_soundStereoSep = (mainMenu_soundStereoSep + dir + 5) % 5;
                getChanges();
                break;
            case 15:
                mainMenu_midiSynth = 1 - mainMenu_midiSynth;
                midi_synth_set_enabled(mainMenu_midiSynth);
                if (mainMenu_midiSynth)
                    ps3_show_message_box("MIDI Synth", "Serial MIDI capture enabled. Games writing to SERDAT / serial port will play through the built-in synthesizer.", "OK (X)");
                break;
        }
    }

    float card_x = 20.0f;
    float card_w = PS3_SCREEN_W - 40.0f;
    const float start_y = PS3_LIST_START_Y;
    const float item_h = 44.0f;
    const float item_gap = 6.0f;
    const int visible_items = ps3_list_visible_rows(start_y, item_h, item_gap);
    int first_item = *selected_item >= visible_items ? *selected_item - visible_items + 1 : 0;

    const char *ks_names[KICKSTART_ROM_COUNT] = {
        "Kickstart 1.2 (A500/A2000)", "Kickstart 1.3 (A500/A2000)", "Kickstart 2.04 (A500+)",
        "Kickstart 3.1 (A1200)", "Custom ROM", "AROS ROM", "Kickstart 3.1 CD32",
        "Kickstart 1.1 NTSC (A1000)", "Kickstart 1.1 PAL (A1000)", "Kickstart 1.2 (A500)",
        "Kickstart 2.04 (A500+)", "Kickstart 2.05 (A600)", "Kickstart 3.1 (A600)",
        "Kickstart 3.0 (A1200)", "Kickstart 3.1 (A1200)", "Kickstart 3.0 (A4000)",
        "Kickstart 3.1 (A4000)", "Kickstart 1.3 + CDTV Extended ROM"
    };
    const char *cpu_names[2] = { "Motorola 68000 (7 MHz base)", "Motorola 68020 (14 MHz AGA base)" };
    const char *cpu_speeds[4] = { "Standard 7 MHz (1x)", "Turbo 14 MHz (2x - WHDLoad recommended)", "Turbo 28 MHz (4x)", "Max 56 MHz (8x)" };
    const char *chipset_names[3] = { "OCS (Original Chip Set)", "ECS (Enhanced Chip Set)", "AGA (Advanced Graphics)" };
    const char *blitter_names[3] = { "Normal (Accurate / Golden Axe)", "Immediate (Fast / Fixes Shadow Dancer)", "Improved (Partial)" };
    const char *sprite_col_names[2] = { "Disabled (Fast)", "Enabled (Space Taxi 3, etc.)" };
    const char *chip_ram_names[4] = { "512 KB (Standard)", "1 MB", "2 MB (Expanded)", "None" };
    const char *fast_ram_names[5] = { "None", "1 MB", "2 MB", "4 MB (AGA recommended)", "8 MB" };
    const char *slow_ram_names[4] = { "None", "512 KB (Trapdoor)", "1 MB", "1.5 MB" };
    const char *rtg_ram_names[3] = { "Disabled", "2 MB", "4 MB" };
    const char *sound_out_names[2] = { "Disabled (Mute)", "48000 Hz (PS3 Native Quality)" };
    const char *sound_stereo_names[2] = { "Mono", "Stereo" };
    const char *stereo_sep_names[5] = { "0% (Mono)", "25% Separation", "50% (Recommended for Headphones)", "75% Separation", "100% (Hard Amiga L/R)" };
    const char *cd_image_name = current_cd_image[0] ? get_filename_only(current_cd_image) : "No image selected";

    sound_rate = 48000;
    int curr_sound_idx = mainMenu_sound ? 1 : 0;

    int curr_chipset_idx = mainMenu_chipset & 0x00ff;
    if (curr_chipset_idx < 0 || curr_chipset_idx > 2) curr_chipset_idx = 0;

    int curr_blitter_idx = (mainMenu_chipset & 0xff00) >> 8;
    if (curr_blitter_idx < 0 || curr_blitter_idx > 2) curr_blitter_idx = 0;

    for (int i = 0; i < visible_items; i++) {
        int item = first_item + i;
        if (item >= total_items) break;
        float y = start_y + (float)i * (item_h + item_gap);
        bool focused = (*selected_item == item);
        switch (item) {
            case 0: {
                char ks_label[128];
                const char *fname = (romfile[0] != '\0' && !kickstart_warning) ? strrchr(romfile, '/') : NULL;
                fname = fname ? fname + 1 : ((romfile[0] != '\0' && !kickstart_warning) ? romfile : NULL);
                if (fname) {
                    snprintf(ks_label, sizeof(ks_label), "%s [%s]", ks_names[kickstart % KICKSTART_ROM_COUNT], fname);
                } else {
                    snprintf(ks_label, sizeof(ks_label), "%s [MISSING]", ks_names[kickstart % KICKSTART_ROM_COUNT]);
                }
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Kickstart ROM", ks_label, focused);
                break;
            }
            case 1:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "CPU Architecture", cpu_names[mainMenu_CPU_model % 2], focused);
                break;
            case 2:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "CPU Clock Speed", cpu_speeds[mainMenu_CPU_speed % 4], focused);
                break;
            case 3:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Amiga Chipset", chipset_names[curr_chipset_idx], focused);
                break;
            case 4:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Blitter Mode", blitter_names[curr_blitter_idx], focused);
                break;
            case 5:
                ps3_draw_switch_item(card_x, y, card_w, item_h, "Cycle-Exact (Accurate Blitter)", mainMenu_cycleExact == 1, focused);
                break;
            case 6:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Sprite Collisions", sprite_col_names[mainMenu_spriteCollisions % 2], focused);
                break;
            case 7:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Chip RAM", chip_ram_names[mainMenu_chipMemory % 4], focused);
                break;
            case 8:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Fast RAM", fast_ram_names[mainMenu_fastMemory % 5], focused);
                break;
            case 9:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Slow / Trapdoor RAM", slow_ram_names[mainMenu_slowMemory % 4], focused);
                break;
            case 10:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "RTG VRAM (Picasso96)", rtg_ram_names[mainMenu_rtgMemory % 3], focused);
                break;
            case 11:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Audio Output", sound_out_names[curr_sound_idx], focused);
                break;
            case 12:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Audio Channels", sound_stereo_names[mainMenu_soundStereo % 2], focused);
                break;
            case 13:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Stereo Separation", stereo_sep_names[mainMenu_soundStereoSep % 5], focused);
                break;
            case 14:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "CD32 CD Image", cd_image_name, focused);
                break;
            case 15:
                ps3_draw_switch_item(card_x, y, card_w, item_h, "MIDI / MT-32 Synth Emulation", mainMenu_midiSynth == 1, focused);
                break;
        }
    }
    ps3_draw_list_page_indicator(*selected_item, total_items, visible_items);
}

void ps3_view_display(Ps3InputState *input, int *selected_item)
{
    const int total_items = 12;
    const float start_y = PS3_LIST_START_Y;
    const float item_h = PS3_LIST_ITEM_H;
    const float item_gap = PS3_LIST_ITEM_GAP;
    const int visible_items = ps3_list_visible_rows(start_y, item_h, item_gap);
    if (*selected_item < 0) *selected_item = 0;
    if (*selected_item >= total_items) *selected_item = total_items - 1;

    if (input->pressed & PS3_BTN_UP) {
        (*selected_item)--;
        if (*selected_item < 0) *selected_item = total_items - 1;
    }
    if (input->pressed & PS3_BTN_DOWN) {
        (*selected_item)++;
        if (*selected_item >= total_items) *selected_item = 0;
    }

    if (input->pressed & PS3_BTN_TRIANGLE) {
        if (*selected_item == 6) {
            mainMenu_screenOffsetX = 0;
            getChanges();
            SDL_SetVideoModeScaling(0, 0, 0, 0);
        } else if (*selected_item == 7) {
            mainMenu_screenOffsetY = 0;
            getChanges();
            SDL_SetVideoModeScaling(0, 0, 0, 0);
        }
    }

    int dir = 0;
    if (input->pressed & (PS3_BTN_RIGHT | PS3_BTN_CROSS)) dir = 1;
    if (input->pressed & PS3_BTN_LEFT) dir = -1;

    if (dir != 0) {
        switch (*selected_item) {
            case 0:
                mainMenu_shader = ps3_shader_cycle(mainMenu_shader, dir);
                break;
            case 1:
                mainMenu_ntsc = (mainMenu_ntsc + dir + 2) % 2;
                break;
            case 2:
                mainMenu_showStatus = (mainMenu_showStatus + dir + 4) % 4;
                break;
            case 3: {
                int width_group = (presetModeId / 10 + dir + 6) % 6;
                int height_mode = presetModeId % 10;
                SetPresetMode(width_group * 10 + height_mode);
                break;
            }
            case 4: {
                int width_group = (presetModeId / 10) * 10;
                int height_mode = (presetModeId % 10 + dir + 10) % 10;
                SetPresetMode(width_group + height_mode);
                break;
            }
            case 5:
                mainMenu_autoCrop = (mainMenu_autoCrop + dir + 4) % 4;
                ps3_reset_autocrop();
                break;
            case 6:
                mainMenu_screenOffsetX += dir * 4;
                if (mainMenu_screenOffsetX < -256) mainMenu_screenOffsetX = -256;
                if (mainMenu_screenOffsetX > 256) mainMenu_screenOffsetX = 256;
                break;
            case 7:
                mainMenu_screenOffsetY += dir * 4;
                if (mainMenu_screenOffsetY < -256) mainMenu_screenOffsetY = -256;
                if (mainMenu_screenOffsetY > 256) mainMenu_screenOffsetY = 256;
                break;
            case 8:
                mainMenu_frameskip += dir;
                if (mainMenu_frameskip < 0) mainMenu_frameskip = 8;
                if (mainMenu_frameskip > 8) mainMenu_frameskip = 0;
                break;
            case 9:
                mainMenu_cutLeft += dir;
                if (mainMenu_cutLeft < 0) mainMenu_cutLeft = 0;
                if (mainMenu_cutLeft > 100) mainMenu_cutLeft = 100;
                break;
            case 10:
                mainMenu_cutRight += dir;
                if (mainMenu_cutRight < 0) mainMenu_cutRight = 0;
                if (mainMenu_cutRight > 100) mainMenu_cutRight = 100;
                break;
            case 11:
                moveY += dir;
                if (moveY < -26) moveY = -26;
                if (moveY > 128) moveY = 128;
                break;
        }
        getChanges();
        SDL_SetVideoModeScaling(0, 0, 0, 0);
    }

    float card_x = 20.0f;
    float card_w = PS3_SCREEN_W - 40.0f;
    int first_item = *selected_item >= visible_items ? *selected_item - visible_items + 1 : 0;

    const char *ntsc_names[2] = { "PAL (50 Hz European Standard)", "NTSC (60 Hz US Standard)" };
    const char *status_names[4] = { "Bottom Bar (Floppy LED & FPS)", "Top Bar (LED blocks)", "Disabled (Clean Screen)", "Vertical Bar" };
    const char *width_names[6] = {
        "320 px (Low-Res)", "640 px (Hi-Res)", "352 px (Low-Res)",
        "704 px (Hi-Res)", "384 px (Low-Res)", "768 px (Hi-Res)"
    };
    const char *autocrop_names[4] = {
        "Off (Standard Amiga Frame)",
        "Auto (Both - Complete Crop)",
        "Auto (Vertical - Top & Bottom)",
        "Auto (Horizontal - Left & Right)"
    };

    char aspect_mode[64];
    if (presetModeId % 10 == 7)
        snprintf(aspect_mode, sizeof(aspect_mode), "Fullscreen 16:9 - %s", presetMode);
    else if (presetModeId % 10 == 8)
        snprintf(aspect_mode, sizeof(aspect_mode), "5:4 Correct - %s", presetMode);
    else if (presetModeId % 10 == 9)
        snprintf(aspect_mode, sizeof(aspect_mode), "16:10 Wide - %s", presetMode);
    else
        snprintf(aspect_mode, sizeof(aspect_mode), "4:3 Correct - %s", presetMode);

    char vertical_position[32];
    snprintf(vertical_position, sizeof(vertical_position), "%d (higher = up)", moveY);

    char screen_offset_y[64];
    if (mainMenu_screenOffsetY < 0)
        snprintf(screen_offset_y, sizeof(screen_offset_y), "%d px up  [Y: Reset]", mainMenu_screenOffsetY);
    else if (mainMenu_screenOffsetY > 0)
        snprintf(screen_offset_y, sizeof(screen_offset_y), "+%d px down  [Y: Reset]", mainMenu_screenOffsetY);
    else
        snprintf(screen_offset_y, sizeof(screen_offset_y), "0 px (Centered)");

    char screen_offset_x[64];
    if (mainMenu_screenOffsetX < 0)
        snprintf(screen_offset_x, sizeof(screen_offset_x), "%d px left  [Y: Reset]", mainMenu_screenOffsetX);
    else if (mainMenu_screenOffsetX > 0)
        snprintf(screen_offset_x, sizeof(screen_offset_x), "+%d px right  [Y: Reset]", mainMenu_screenOffsetX);
    else
        snprintf(screen_offset_x, sizeof(screen_offset_x), "0 px (Centered)");

    char frameskip_value[16];
    snprintf(frameskip_value, sizeof(frameskip_value), "%d", mainMenu_frameskip);

    char cut_left_value[16];
    snprintf(cut_left_value, sizeof(cut_left_value), "%d px", mainMenu_cutLeft);

    char cut_right_value[16];
    snprintf(cut_right_value, sizeof(cut_right_value), "%d px", mainMenu_cutRight);

    const char *item_titles[12] = {
        "Display Scaling / Filter",
        "Screen Refresh & Region",
        "Status Bar (Floppy LED/FPS)",
        "Horizontal Resolution",
        "Vertical Lines & Aspect",
        "Auto Crop Borders",
        "Game Screen Offset X",
        "Game Screen Offset Y",
        "Frameskip",
        "Overscan Cut Left",
        "Overscan Cut Right",
        "Vertical Position"
    };
    const char *item_values[12] = {
        ps3_shader_label(mainMenu_shader),
        ntsc_names[mainMenu_ntsc % 2],
        status_names[mainMenu_showStatus % 4],
        width_names[(presetModeId / 10) % 6],
        aspect_mode,
        autocrop_names[mainMenu_autoCrop % 4],
        screen_offset_x,
        screen_offset_y,
        frameskip_value,
        cut_left_value,
        cut_right_value,
        vertical_position
    };

    for (int i = 0; i < visible_items; i++) {
        int item = first_item + i;
        if (item >= total_items) break;
        ps3_draw_selector_item(card_x, start_y + (float)i * (item_h + item_gap), card_w, item_h,
            item_titles[item], item_values[item], *selected_item == item);
    }

    ps3_draw_list_page_indicator(*selected_item, total_items, visible_items);
}

static bool s_custom_controls_modal_open = false;
static int s_custom_modal_selected = 0;
static bool s_mouse_modal_open = false;
static int s_mouse_modal_selected = 0;

void ps3_view_controls(Ps3InputState *input, int *selected_item)
{
    static const int s_mouse_mult_table[] = { 25, 50, 75, 100, 125, 150, 200, 300, 400 };
    static const int s_num_mouse_mults = sizeof(s_mouse_mult_table) / sizeof(s_mouse_mult_table[0]);

    if (s_custom_controls_modal_open) {
        const int m_total_items = 18;
        if (s_custom_modal_selected < 0) s_custom_modal_selected = 0;
        if (s_custom_modal_selected >= m_total_items) s_custom_modal_selected = m_total_items - 1;

        if (input->pressed & PS3_BTN_UP) {
            s_custom_modal_selected--;
            if (s_custom_modal_selected < 0) s_custom_modal_selected = m_total_items - 1;
        }
        if (input->pressed & PS3_BTN_DOWN) {
            s_custom_modal_selected++;
            if (s_custom_modal_selected >= m_total_items) s_custom_modal_selected = 0;
        }

        int c = mainMenu_custom_currentlyEditingControllerNr;
        if (c < 0 || c >= MAX_NUM_CONTROLLERS) c = 0;

        if (input->pressed & PS3_BTN_CIRCLE) {
            s_custom_controls_modal_open = false;
            mapback_custom_controls();
            remap_custom_controls();
            return;
        }

        if (input->pressed & PS3_BTN_CROSS && s_custom_modal_selected == 17) {
            mainMenu_custom_up[c] = -5;
            mainMenu_custom_down[c] = -6;
            mainMenu_custom_left[c] = -7;
            mainMenu_custom_right[c] = -8;
            mainMenu_custom_stickup[c] = -5;
            mainMenu_custom_stickdown[c] = -6;
            mainMenu_custom_stickleft[c] = -7;
            mainMenu_custom_stickright[c] = -8;
            mainMenu_custom_A[c] = -3;
            mainMenu_custom_B[c] = -4;
            mainMenu_custom_X[c] = 23;
            mainMenu_custom_Y[c] = 27;
            mainMenu_custom_L[c] = 0;
            mainMenu_custom_R[c] = 0;
            mapback_custom_controls();
            remap_custom_controls();
            ps3_show_message_box("Controls Reset", "Controller mappings reset to default Amiga layout.", "OK (X)");
        }

        int dir = 0;
        if (input->pressed & (PS3_BTN_RIGHT | PS3_BTN_CROSS)) dir = 1;
        if (input->pressed & PS3_BTN_LEFT) dir = -1;

        if (dir != 0) {
            switch (s_custom_modal_selected) {
                case 0:
                    mainMenu_customControls = 1 - mainMenu_customControls;
                    break;
                case 1:
                    mainMenu_custom_currentlyEditingControllerNr = (mainMenu_custom_currentlyEditingControllerNr + dir + MAX_NUM_CONTROLLERS) % MAX_NUM_CONTROLLERS;
                    break;
                case 2:
                    mainMenu_custom_controlSet = (mainMenu_custom_controlSet + dir + 6) % 6;
                    remap_custom_controls();
                    break;
                case 3:
                    mainMenu_custom_up[c] = vita_cycle_custom_action(mainMenu_custom_up[c], dir);
                    mapback_custom_controls();
                    remap_custom_controls();
                    break;
                case 4:
                    mainMenu_custom_down[c] = vita_cycle_custom_action(mainMenu_custom_down[c], dir);
                    mapback_custom_controls();
                    remap_custom_controls();
                    break;
                case 5:
                    mainMenu_custom_left[c] = vita_cycle_custom_action(mainMenu_custom_left[c], dir);
                    mapback_custom_controls();
                    remap_custom_controls();
                    break;
                case 6:
                    mainMenu_custom_right[c] = vita_cycle_custom_action(mainMenu_custom_right[c], dir);
                    mapback_custom_controls();
                    remap_custom_controls();
                    break;
                case 7:
                    mainMenu_custom_stickup[c] = vita_cycle_custom_action(mainMenu_custom_stickup[c], dir);
                    mapback_custom_controls();
                    remap_custom_controls();
                    break;
                case 8:
                    mainMenu_custom_stickdown[c] = vita_cycle_custom_action(mainMenu_custom_stickdown[c], dir);
                    mapback_custom_controls();
                    remap_custom_controls();
                    break;
                case 9:
                    mainMenu_custom_stickleft[c] = vita_cycle_custom_action(mainMenu_custom_stickleft[c], dir);
                    mapback_custom_controls();
                    remap_custom_controls();
                    break;
                case 10:
                    mainMenu_custom_stickright[c] = vita_cycle_custom_action(mainMenu_custom_stickright[c], dir);
                    mapback_custom_controls();
                    remap_custom_controls();
                    break;
                case 11:
                    mainMenu_custom_A[c] = vita_cycle_custom_action(mainMenu_custom_A[c], dir);
                    mapback_custom_controls();
                    remap_custom_controls();
                    break;
                case 12:
                    mainMenu_custom_B[c] = vita_cycle_custom_action(mainMenu_custom_B[c], dir);
                    mapback_custom_controls();
                    remap_custom_controls();
                    break;
                case 13:
                    mainMenu_custom_X[c] = vita_cycle_custom_action(mainMenu_custom_X[c], dir);
                    mapback_custom_controls();
                    remap_custom_controls();
                    break;
                case 14:
                    mainMenu_custom_Y[c] = vita_cycle_custom_action(mainMenu_custom_Y[c], dir);
                    mapback_custom_controls();
                    remap_custom_controls();
                    break;
                case 15:
                    mainMenu_custom_L[c] = vita_cycle_custom_action(mainMenu_custom_L[c], dir);
                    mapback_custom_controls();
                    remap_custom_controls();
                    break;
                case 16:
                    mainMenu_custom_R[c] = vita_cycle_custom_action(mainMenu_custom_R[c], dir);
                    mapback_custom_controls();
                    remap_custom_controls();
                    break;
            }
        }

        c = mainMenu_custom_currentlyEditingControllerNr;
        if (c < 0 || c >= MAX_NUM_CONTROLLERS) c = 0;

        const char *controller_names[MAX_NUM_CONTROLLERS] = {
            "Controller 1 (P1)",
            "Controller 2 (P2)",
            "Controller 3 (P3)",
            "Controller 4 (P4)",
            "Controller 5 (P5)",
            "Controller 6 (P6)",
            "Controller 7 (P7)",
            "Controller 8 (P8)"
        };
        const char *profile_names[6] = { "Profile 1", "Profile 2 (Pinball)", "Profile 3", "Profile 4", "Profile 5", "Profile 6" };

        ps3_draw_rounded_rect(20.0f, 20.0f, PS3_SCREEN_W - 40.0f, PS3_SCREEN_H - 40.0f, 12.0f, RGBA8(14, 18, 26, 250));
        ps3_draw_card_custom(20.0f, 20.0f, PS3_SCREEN_W - 40.0f, PS3_SCREEN_H - 40.0f, RGBA8(14, 18, 26, 250), PS3_COLOR_FOCUS_BORDER);

        ps3_draw_badge(40.0f, 32.0f, "CONTROLLERS", PS3_COLOR_AMIGA_RED, PS3_COLOR_TEXT_WHITE);
        ps3_draw_text_centered(PS3_SCREEN_W * 0.5f, 35.0f, PS3_COLOR_TEXT_WHITE, 1.15f, "CUSTOM CONTROLS REMAPPING (UP TO 8 PADS)");
        ps3_draw_text_centered(PS3_SCREEN_W * 0.5f, 62.0f, PS3_COLOR_TEXT_MUTED, 0.72f, "Press O to Close and Save  |  D-PAD Left/Right to Cycle Actions");

        float m_card_x = 40.0f;
        float m_card_w = PS3_SCREEN_W - 80.0f;
        const float m_start_y = 88.0f;
        const float m_item_h = 40.0f;
        const float m_item_gap = 4.0f;
        const int m_visible = 8;
        int m_first = s_custom_modal_selected >= m_visible ? s_custom_modal_selected - m_visible + 1 : 0;

        for (int i = 0; i < m_visible; i++) {
            int item = m_first + i;
            if (item >= m_total_items) break;
            float y = m_start_y + (float)i * (m_item_h + m_item_gap);
            bool focused = (s_custom_modal_selected == item);
            switch (item) {
                case 0:
                    ps3_draw_switch_item(m_card_x, y, m_card_w, m_item_h, "Custom Controls Master Toggle", mainMenu_customControls == 1, focused);
                    break;
                case 1:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Active Controller to Edit", controller_names[c], focused);
                    break;
                case 2:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Active Control Profile", profile_names[mainMenu_custom_controlSet % 6], focused);
                    break;
                case 3:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Map D-Pad UP", vita_get_custom_action_name(mainMenu_custom_up[c]), focused);
                    break;
                case 4:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Map D-Pad DOWN", vita_get_custom_action_name(mainMenu_custom_down[c]), focused);
                    break;
                case 5:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Map D-Pad LEFT", vita_get_custom_action_name(mainMenu_custom_left[c]), focused);
                    break;
                case 6:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Map D-Pad RIGHT", vita_get_custom_action_name(mainMenu_custom_right[c]), focused);
                    break;
                case 7:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Map Left Stick UP", vita_get_custom_action_name(mainMenu_custom_stickup[c]), focused);
                    break;
                case 8:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Map Left Stick DOWN", vita_get_custom_action_name(mainMenu_custom_stickdown[c]), focused);
                    break;
                case 9:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Map Left Stick LEFT", vita_get_custom_action_name(mainMenu_custom_stickleft[c]), focused);
                    break;
                case 10:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Map Left Stick RIGHT", vita_get_custom_action_name(mainMenu_custom_stickright[c]), focused);
                    break;
                case 11:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Map Cross (Fire 1)", vita_get_custom_action_name(mainMenu_custom_A[c]), focused);
                    break;
                case 12:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Map Circle (Fire 2)", vita_get_custom_action_name(mainMenu_custom_B[c]), focused);
                    break;
                case 13:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Map Square (Space / Extra)", vita_get_custom_action_name(mainMenu_custom_X[c]), focused);
                    break;
                case 14:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Map Triangle (Virtual Keyboard)", vita_get_custom_action_name(mainMenu_custom_Y[c]), focused);
                    break;
                case 15:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Map L1 Button", vita_get_custom_action_name(mainMenu_custom_L[c]), focused);
                    break;
                case 16:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Map R1 Button", vita_get_custom_action_name(mainMenu_custom_R[c]), focused);
                    break;
                case 17:
                    ps3_draw_button_item(m_card_x, y, m_card_w, m_item_h, "Reset Controller Mappings to Default", "Restore standard Amiga layout for this controller", "RESET", focused, false);
                    break;
            }
        }
        ps3_draw_list_page_indicator(s_custom_modal_selected, m_total_items, m_visible);
        return;
    }

    if (s_mouse_modal_open) {
        const int m_total_items = 8;
        if (s_mouse_modal_selected < 0) s_mouse_modal_selected = 0;
        if (s_mouse_modal_selected >= m_total_items) s_mouse_modal_selected = m_total_items - 1;

        if (input->pressed & PS3_BTN_UP) {
            s_mouse_modal_selected--;
            if (s_mouse_modal_selected < 0) s_mouse_modal_selected = m_total_items - 1;
        }
        if (input->pressed & PS3_BTN_DOWN) {
            s_mouse_modal_selected++;
            if (s_mouse_modal_selected >= m_total_items) s_mouse_modal_selected = 0;
        }

        if (input->pressed & PS3_BTN_CIRCLE) {
            s_mouse_modal_open = false;
            return;
        }

        int dir = 0;
        if (input->pressed & (PS3_BTN_RIGHT | PS3_BTN_CROSS)) dir = 1;
        if (input->pressed & PS3_BTN_LEFT) dir = -1;

        if (dir != 0) {
            switch (s_mouse_modal_selected) {
                case 0:
                    mainMenu_mouseDevice = (mainMenu_mouseDevice + dir + 4) % 4;
                    mainMenu_leftStickMouse = (mainMenu_mouseDevice == 1) ? 1 : 0;
                    mainMenu_mouseEmulation = (mainMenu_mouseDevice != 2) ? 1 : 0;
                    break;
                case 1:
                    mainMenu_mouseMultiplier += dir;
                    if (mainMenu_mouseMultiplier < 1) mainMenu_mouseMultiplier = 1;
                    if (mainMenu_mouseMultiplier > 10) mainMenu_mouseMultiplier = 10;
                    break;
                case 2:
                    mainMenu_mouseAcceleration = (mainMenu_mouseAcceleration + dir + 4) % 4;
                    break;
                case 3:
                    mainMenu_mouseSlowFactor = (mainMenu_mouseSlowFactor + dir + 4) % 4;
                    break;
                case 4:
                    mainMenu_mouseSlowButton = (mainMenu_mouseSlowButton + dir + 9) % 9;
                    break;
                case 5:
                    mainMenu_mouseFastFactor = (mainMenu_mouseFastFactor + dir + 5) % 5;
                    break;
                case 6:
                    mainMenu_mouseFastButton = (mainMenu_mouseFastButton + dir + 9) % 9;
                    break;
                case 7:
                    mainMenu_mouseSwapButtons = 1 - mainMenu_mouseSwapButtons;
                    break;
            }
        }

        static const char *device_names[4] = { "Right Analog Stick", "Left Analog Stick", "Touchscreen Only", "D-Pad" };
        static const char *accel_names[4] = { "Disabled (Linear)", "Low Curve", "Medium Curve", "High Curve" };
        static const char *slow_factor_names[4] = { "0.50x (Half Speed)", "0.25x (Quarter Speed)", "0.125x (1/8 Speed - Default)", "0.0625x (1/16 Speed)" };
        static const char *fast_factor_names[5] = { "1.5x Speed", "2.0x Speed", "3.0x Speed (Default)", "4.0x Speed", "5.0x Speed" };
        static const char *button_trigger_names[9] = { "Disabled / None", "ZR Trigger", "ZL Trigger", "R Button", "L Button", "R3 (Right Stick Click)", "L3 (Left Stick Click)", "Y Button", "X Button" };

        char speed_buf[32];
        snprintf(speed_buf, sizeof(speed_buf), "%dx Speed", mainMenu_mouseMultiplier);

        ps3_draw_rounded_rect(20.0f, 20.0f, PS3_SCREEN_W - 40.0f, PS3_SCREEN_H - 40.0f, 12.0f, RGBA8(14, 18, 26, 250));
        ps3_draw_card_custom(20.0f, 20.0f, PS3_SCREEN_W - 40.0f, PS3_SCREEN_H - 40.0f, RGBA8(14, 18, 26, 250), PS3_COLOR_FOCUS_BORDER);

        ps3_draw_badge(40.0f, 32.0f, "MOUSE", PS3_COLOR_AMIGA_BLUE, PS3_COLOR_TEXT_WHITE);
        ps3_draw_text_centered(PS3_SCREEN_W * 0.5f, 35.0f, PS3_COLOR_TEXT_WHITE, 1.15f, "AMIGA MOUSE CONFIGURATION");
        ps3_draw_text_centered(PS3_SCREEN_W * 0.5f, 62.0f, PS3_COLOR_TEXT_MUTED, 0.72f, "Press O to Close  |  D-PAD Left/Right or X to Change Settings");

        float m_card_x = 40.0f;
        float m_card_w = PS3_SCREEN_W - 80.0f;
        const float m_start_y = 88.0f;
        const float m_item_h = 40.0f;
        const float m_item_gap = 4.0f;
        const int m_visible = 8;
        int m_first = s_mouse_modal_selected >= m_visible ? s_mouse_modal_selected - m_visible + 1 : 0;

        for (int i = 0; i < m_visible; i++) {
            int item = m_first + i;
            if (item >= m_total_items) break;
            float y = m_start_y + (float)i * (m_item_h + m_item_gap);
            bool focused = (s_mouse_modal_selected == item);
            switch (item) {
                case 0:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Device", device_names[mainMenu_mouseDevice % 4], focused);
                    break;
                case 1:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Sensitivity", speed_buf, focused);
                    break;
                case 2:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Acceleration", accel_names[mainMenu_mouseAcceleration % 4], focused);
                    break;
                case 3:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Slow Mouse Speed", slow_factor_names[mainMenu_mouseSlowFactor % 4], focused);
                    break;
                case 4:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Slow Mouse Trigger", button_trigger_names[mainMenu_mouseSlowButton % 9], focused);
                    break;
                case 5:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Fast Mouse Speed", fast_factor_names[mainMenu_mouseFastFactor % 5], focused);
                    break;
                case 6:
                    ps3_draw_selector_item(m_card_x, y, m_card_w, m_item_h, "Fast Mouse Trigger", button_trigger_names[mainMenu_mouseFastButton % 9], focused);
                    break;
                case 7:
                    ps3_draw_switch_item(m_card_x, y, m_card_w, m_item_h, "Swap Buttons (Left / Right)", mainMenu_mouseSwapButtons == 1, focused);
                    break;
            }
        }
        ps3_draw_list_page_indicator(s_mouse_modal_selected, m_total_items, m_visible);
        return;
    }

    const int total_items = 15;
    if (*selected_item < 0) *selected_item = 0;
    if (*selected_item >= total_items) *selected_item = total_items - 1;

    static const char *touch_modes[3] = { "Direct Touchscreen (Finger Tap = Click)", "Trackpad Mode (Relative Mouse Movement)", "Disabled" };
    static const char *vkbd_lang_names[4] = { "English (US)", "English (UK)", "German (DE)", "French (FR)" };
    static const char *vkbd_style_names[4] = { "Original Amiga", "Dark Carbon", "Cool Blue", "Warm Amber" };
    static const char *vkbd_trans_names[4] = { "Opaque (100%)", "Semi-Transparent (75%)", "Subtle (50%)", "High Alpha (25%)" };
    static const char *vkbd_pos_names[3] = { "Bottom Screen", "Top Screen", "Center" };
    static const char *autofire_names[4] = { "Disabled", "Light (4 shots/sec)", "Medium (8 shots/sec)", "Turbo (16 shots/sec)" };

    if (input->pressed & PS3_BTN_UP) {
        (*selected_item)--;
        if (*selected_item < 0) *selected_item = total_items - 1;
    }
    if (input->pressed & PS3_BTN_DOWN) {
        (*selected_item)++;
        if (*selected_item >= total_items) *selected_item = 0;
    }

    if (input->pressed & (PS3_BTN_LEFT | PS3_BTN_RIGHT)) {
        int dir = (input->pressed & PS3_BTN_RIGHT) ? 1 : -1;
        switch (*selected_item) {
            case 0:
                mainMenu_joyPort = (mainMenu_joyPort == 1) ? 2 : 1;
                break;
            case 1:
                mainMenu_singleJoycons = 1 - mainMenu_singleJoycons;
                update_pad_mode();
                break;
            case 2:
                mainMenu_pinballMode = 1 - mainMenu_pinballMode;
                saveconfig(1);
                break;
            case 4:
                mainMenu_touchControls = (mainMenu_touchControls + 3 + dir) % 3;
                break;
            case 5:
                mainMenu_vkbdLanguage = (mainMenu_vkbdLanguage + 4 + dir) % 4;
                break;
            case 6:
                mainMenu_vkbdStyle = (mainMenu_vkbdStyle + 4 + dir) % 4;
                break;
            case 7:
                mainMenu_vkbdTransparency = (mainMenu_vkbdTransparency + 4 + dir) % 4;
                break;
            case 8:
                mainMenu_vkbdPosition = (mainMenu_vkbdPosition + 3 + dir) % 3;
                break;
            case 9:
                mainMenu_autofire = (mainMenu_autofire + 4 + dir) % 4;
                break;
            case 10:
                mainMenu_autofireMode = 1 - mainMenu_autofireMode;
                break;
            case 11:
                mainMenu_autoEjectFloppy = 1 - mainMenu_autoEjectFloppy;
                break;
            case 12:
                mainMenu_deadZone += dir * 1000;
                if (mainMenu_deadZone < 1000) mainMenu_deadZone = 1000;
                if (mainMenu_deadZone > 25000) mainMenu_deadZone = 25000;
                break;
            case 13:
                mainMenu_numPlayers = (mainMenu_numPlayers + 7 + dir) % 8 + 1;
                break;
        }
    }

    if (input->pressed & PS3_BTN_CROSS) {
        switch (*selected_item) {
            case 0:
                mainMenu_joyPort = (mainMenu_joyPort == 1) ? 2 : 1;
                break;
            case 1:
                mainMenu_singleJoycons = 1 - mainMenu_singleJoycons;
                update_pad_mode();
                break;
            case 2:
                mainMenu_pinballMode = 1 - mainMenu_pinballMode;
                saveconfig(1);
                break;
            case 3:
                s_mouse_modal_open = true;
                s_mouse_modal_selected = 0;
                break;
            case 4:
                mainMenu_touchControls = (mainMenu_touchControls + 1) % 3;
                break;
            case 5:
                mainMenu_vkbdLanguage = (mainMenu_vkbdLanguage + 1) % 4;
                break;
            case 6:
                mainMenu_vkbdStyle = (mainMenu_vkbdStyle + 1) % 4;
                break;
            case 7:
                mainMenu_vkbdTransparency = (mainMenu_vkbdTransparency + 1) % 4;
                break;
            case 8:
                mainMenu_vkbdPosition = (mainMenu_vkbdPosition + 1) % 3;
                break;
            case 9:
                mainMenu_autofire = (mainMenu_autofire + 1) % 4;
                break;
            case 10:
                mainMenu_autofireMode = 1 - mainMenu_autofireMode;
                break;
            case 11:
                mainMenu_autoEjectFloppy = 1 - mainMenu_autoEjectFloppy;
                break;
            case 12:
                mainMenu_deadZone = (mainMenu_deadZone + 1000) > 25000 ? 1000 : mainMenu_deadZone + 1000;
                break;
            case 13:
                mainMenu_numPlayers = (mainMenu_numPlayers % 8) + 1;
                break;
            case 14:
                s_custom_controls_modal_open = true;
                s_custom_modal_selected = 0;
                break;
        }
    }

    float card_x = 20.0f;
    float card_w = PS3_SCREEN_W - 40.0f;
    const float start_y = PS3_LIST_START_Y;
    const float item_h = 44.0f;
    const float item_gap = 6.0f;
    const int visible_items = ps3_list_visible_rows(start_y, item_h, item_gap);
    int first_item = *selected_item >= visible_items ? *selected_item - visible_items + 1 : 0;

    char deadzone_buf[32];
    snprintf(deadzone_buf, sizeof(deadzone_buf), "%d%% Threshold", (int)((float)mainMenu_deadZone / 32768.0f * 100.0f));

    char players_buf[64];
    snprintf(players_buf, sizeof(players_buf), "%d Player%s%s", mainMenu_numPlayers, mainMenu_numPlayers == 1 ? "" : "s", (mainMenu_singleJoycons && mainMenu_numPlayers > 2) ? " (max 2 with Single Joy-Con)" : "");

    for (int i = 0; i < visible_items; i++) {
        int item = first_item + i;
        if (item >= total_items) break;
        float y = start_y + (float)i * (item_h + item_gap);
        bool focused = (*selected_item == item);
        switch (item) {
            case 0:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Joystick Port", (mainMenu_joyPort == 1) ? "Amiga Port 0 (Mouse Port)" : "Amiga Port 1 (Joystick Port)", focused);
                break;
            case 1:
                ps3_draw_switch_item(card_x, y, card_w, item_h, "Single Joy-Con Mode (2 Players)", mainMenu_singleJoycons == 1, focused);
                break;
            case 2:
                ps3_draw_switch_item(card_x, y, card_w, item_h, "Pinball Mode (L/R Flippers)", mainMenu_pinballMode == 1, focused);
                break;
            case 3:
                ps3_draw_button_item(card_x, y, card_w, item_h, "Mouse Configuration...", "Device, Sensitivity, Acceleration, Slow/Fast Mouse, Swap Buttons", "CONFIGURE", focused, false);
                break;
            case 4:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Touchscreen & Mouse Mode", touch_modes[mainMenu_touchControls % 3], focused);
                break;
            case 5:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Virtual Keyboard Language", vkbd_lang_names[mainMenu_vkbdLanguage % 4], focused);
                break;
            case 6:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Virtual Keyboard Style", vkbd_style_names[mainMenu_vkbdStyle % 4], focused);
                break;
            case 7:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Virtual Keyboard Transparency", vkbd_trans_names[mainMenu_vkbdTransparency % 4], focused);
                break;
            case 8:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Virtual Keyboard Position", vkbd_pos_names[mainMenu_vkbdPosition % 3], focused);
                break;
            case 9:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Autofire Rate", autofire_names[mainMenu_autofire % 4], focused);
                break;
            case 10:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Autofire Trigger", (mainMenu_autofireMode == 1) ? "Continuous (Automatic / Always-On)" : "Hold Fire Button (A)", focused);
                break;
            case 11:
                ps3_draw_switch_item(card_x, y, card_w, item_h, "WHDLoad Auto-Eject Floppy on Launch", mainMenu_autoEjectFloppy == 1, focused);
                break;
            case 12:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Analog Stick Deadzone", deadzone_buf, focused);
                break;
            case 13:
                ps3_draw_selector_item(card_x, y, card_w, item_h, "Number of Players", players_buf, focused);
                break;
            case 14:
                ps3_draw_button_item(card_x, y, card_w, item_h, "Custom Button Remapping...", "Configure individual actions for up to 8 PS3 controllers", "REMAP", focused, false);
                break;
        }
    }
    ps3_draw_list_page_indicator(*selected_item, total_items, visible_items);
}

static bool vita_savestate_file_exists(const char *path)
{
    if (!path || path[0] == '\0') return false;
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

static const char *vita_get_active_game_path(void)
{
    if (cdrom_is_cd32_mode() && current_cd_image[0] != '\0') return current_cd_image;
    if (uae4all_image_file0[0] != '\0') return uae4all_image_file0;
    if (mainMenu_bootHD == 2) {
        if (uae4all_hard_file0[0] != '\0') return uae4all_hard_file0;
        if (uae4all_hard_file1[0] != '\0') return uae4all_hard_file1;
        if (uae4all_hard_file2[0] != '\0') return uae4all_hard_file2;
        if (uae4all_hard_file3[0] != '\0') return uae4all_hard_file3;
    }
    if (mainMenu_bootHD == 1 && uae4all_hard_dir[0] != '\0') return uae4all_hard_dir;
    if (uae4all_image_file1[0] != '\0') return uae4all_image_file1;
    if (uae4all_image_file2[0] != '\0') return uae4all_image_file2;
    if (uae4all_image_file3[0] != '\0') return uae4all_image_file3;
    return "";
}

static void vita_get_game_label(char *destination, size_t destination_size)
{
    if (!destination || destination_size == 0) return;
    const char *path = vita_get_active_game_path();
    const char *name = get_filename_only(path);
    strncpy(destination, name, destination_size - 1);
    destination[destination_size - 1] = '\0';
    char *extension = strrchr(destination, '.');
    if (extension && extension != destination)
        *extension = '\0';
    if (destination[0] == '\0')
        strncpy(destination, "No game loaded", destination_size - 1);
    destination[destination_size - 1] = '\0';
}

static void vita_get_savestate_paths(int slot, char *state_path, char *thumb_path)
{
    int old_slot = saveMenu_n_savestate;
    saveMenu_n_savestate = slot;
    make_savestate_filenames(state_path, thumb_path);
    saveMenu_n_savestate = old_slot;
}

void ps3_view_savestates(Ps3InputState *input, int *selected_item)
{
    const int total_items = 11;
    if (*selected_item < 0) *selected_item = 0;
    if (*selected_item >= total_items) *selected_item = total_items - 1;

    static int s_savestate_subaction = 0;

    if (input->pressed & PS3_BTN_LEFT) {
        (*selected_item)--;
        if (*selected_item < 0) *selected_item = total_items - 1;
    }
    if (input->pressed & PS3_BTN_RIGHT) {
        (*selected_item)++;
        if (*selected_item >= total_items) *selected_item = 0;
    }

    if (input->pressed & PS3_BTN_UP) {
        s_savestate_subaction--;
        if (s_savestate_subaction < 0) s_savestate_subaction = 4;
    }
    if (input->pressed & PS3_BTN_DOWN) {
        s_savestate_subaction++;
        if (s_savestate_subaction > 4) s_savestate_subaction = 0;
    }

    int active_slot = (*selected_item < 10) ? (*selected_item + 1) : 0;

    static SDL_Surface *s_slot_thumbs[11] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    static char s_slot_thumb_paths[11][256] = { {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0} };

    bool do_save = (input->pressed & PS3_BTN_CROSS && s_savestate_subaction == 0);
    bool do_load = (input->pressed & PS3_BTN_SQUARE) || (input->pressed & PS3_BTN_CROSS && s_savestate_subaction == 1);
    bool do_export = (input->pressed & PS3_BTN_TRIANGLE) || (input->pressed & PS3_BTN_CROSS && s_savestate_subaction == 2);
    bool do_import = (input->pressed & PS3_BTN_MINUS) || (input->pressed & PS3_BTN_CROSS && s_savestate_subaction == 3);
    bool do_delete = (input->pressed & PS3_BTN_CROSS && s_savestate_subaction == 4);

    if (do_save) {
        if (emulating) {
            saveMenu_n_savestate = active_slot;
            make_savestate_filenames(savestate_filename, screenshot_filename);
            savestate_state = STATE_DOSAVE;
            ps3_show_message_box("Save State", "Save queued. The state will be written when the game resumes.", "Resume (A)");
            mainMenu_case = MAIN_MENU_CASE_RUN;
        } else {
            ps3_show_message_box("Savestate Notice", "You must launch a game before saving state.", "OK (X)");
        }
    }

    if (do_load) {
        if (emulating) {
            saveMenu_n_savestate = active_slot;
            make_savestate_filenames(savestate_filename, screenshot_filename);
            if (!vita_savestate_file_exists(savestate_filename)) {
                ps3_show_message_box("Load State", "No save state exists in this slot.", "OK (X)");
            } else {
                savestate_state = STATE_DORESTORE;
                ps3_show_message_box("Load State", "State queued. It will be restored when the game resumes.", "Resume (A)");
                mainMenu_case = MAIN_MENU_CASE_RUN;
            }
        } else {
            ps3_show_message_box("Savestate Notice", "You must launch a game before loading state.", "OK (X)");
        }
    }

    if (do_export) {
        char state_path[256];
        char thumb_path[256];
        vita_get_savestate_paths(active_slot, state_path, thumb_path);
        if (!vita_savestate_file_exists(state_path)) {
            ps3_show_message_box("Export State", "No save state found in this slot to export.", "OK (X)");
        } else {
            char *exp_name = ps3_kbd_get_str((char*)"Enter savestate export name:", "", 64, 0);
            if (exp_name && exp_name[0] != '\0') {
                char out_asf[300];
                char out_png[300];
                snprintf(out_asf, sizeof(out_asf), "%s%s.asf", SAVE_PREFIX, exp_name);
                snprintf(out_png, sizeof(out_png), "%s%s.png", SAVE_PREFIX, exp_name);
                vita_copy_file(state_path, out_asf);
                if (vita_savestate_file_exists(thumb_path))
                    vita_copy_file(thumb_path, out_png);
                ps3_show_message_box("Export Complete", "Save state and preview exported to ./saves/", "OK (X)");
            }
        }
    }

    if (do_import) {
        char import_path[512];
        import_path[0] = '\0';
        int res = ps3_gui_run_browser(import_path, SAVE_PREFIX, 10);
        if (res == 1 && import_path[0] != '\0') {
            char state_path[256];
            char thumb_path[256];
            vita_get_savestate_paths(active_slot, state_path, thumb_path);
            vita_copy_file(import_path, state_path);

            char import_thumb[512];
            strncpy(import_thumb, import_path, sizeof(import_thumb) - 1);
            import_thumb[sizeof(import_thumb) - 1] = '\0';
            char *dot = strrchr(import_thumb, '.');
            if (dot) strcpy(dot, ".png");
            else strcat(import_thumb, ".png");
            if (vita_savestate_file_exists(import_thumb))
                vita_copy_file(import_thumb, thumb_path);

            if (s_slot_thumbs[*selected_item]) {
                SDL_FreeSurface(s_slot_thumbs[*selected_item]);
                s_slot_thumbs[*selected_item] = NULL;
            }
            s_slot_thumb_paths[*selected_item][0] = '\0';
            ps3_show_message_box("Import Complete", "Save state imported into current slot successfully.", "OK (X)");
        }
    }


    if (do_delete) {
        char state_path[256];
        char thumb_path[256];
        vita_get_savestate_paths(active_slot, state_path, thumb_path);
        if (!vita_savestate_file_exists(state_path)) {
            ps3_show_message_box("Delete State", "No save state exists in this slot to delete.", "OK (X)");
        } else {
            if (ps3_show_confirm_box("Delete Save State", "Delete save state and thumbnail for this slot?", "Delete", "Cancel")) {
                remove(state_path);
                if (vita_savestate_file_exists(thumb_path))
                    remove(thumb_path);
                if (s_slot_thumbs[*selected_item]) {
                    SDL_FreeSurface(s_slot_thumbs[*selected_item]);
                    s_slot_thumbs[*selected_item] = NULL;
                }
                s_slot_thumb_paths[*selected_item][0] = '\0';
                ps3_show_message_box("State Deleted", "Save state deleted successfully.", "OK (X)");
            }
        }
    }

    char game_label[128];
    if (mainMenu_whdload_game[0] != '\0') {
        strncpy(game_label, mainMenu_whdload_game, sizeof(game_label) - 1);
        game_label[sizeof(game_label) - 1] = '\0';
    } else {
        vita_get_game_label(game_label, sizeof(game_label));
    }

    float slot_w = (PS3_SCREEN_W - 40.0f - 36.0f) / 4.0f;
    float slot_h = PS3_LIST_BOTTOM_Y - PS3_LIST_START_Y - 4.0f;
    if (slot_h > 320.0f) slot_h = 320.0f;
    if (slot_h < 260.0f) slot_h = 260.0f;
    float start_x = 20.0f;
    const float start_y = PS3_LIST_START_Y;

    int first_slot = *selected_item >= 4 ? *selected_item - 3 : 0;
    if (first_slot > total_items - 4) first_slot = total_items - 4;
    if (first_slot < 0) first_slot = 0;

    for (int i = 0; i < 4; i++) {
        int slot_idx = first_slot + i;
        if (slot_idx >= total_items) break;
        float sx = start_x + (float)i * (slot_w + 12.0f);
        bool focused = (*selected_item == slot_idx);
        int slot_num = (slot_idx < 10) ? (slot_idx + 1) : 0;
        char state_path[256];
        char thumb_path[256];
        vita_get_savestate_paths(slot_num, state_path, thumb_path);
        bool saved = vita_savestate_file_exists(state_path);

        ps3_draw_card(sx, start_y, slot_w, slot_h, focused, saved);

        char slot_tag[32];
        if (slot_idx < 10)
            snprintf(slot_tag, sizeof(slot_tag), "SLOT %d", slot_idx + 1);
        else
            snprintf(slot_tag, sizeof(slot_tag), "AUTO-SAVE");

        ps3_draw_badge(sx + 14.0f, start_y + 14.0f, slot_tag, focused ? PS3_COLOR_AMIGA_RED : RGBA8(40, 50, 70, 255), PS3_COLOR_TEXT_WHITE);

        char game_label_buf[128];
        ps3_truncate_text(game_label, slot_w - 24.0f, 0.62f, game_label_buf, sizeof(game_label_buf));
        ps3_draw_text_centered(sx + (slot_w * 0.5f), start_y + 39.0f,
            PS3_COLOR_TEXT_MUTED, 0.62f, game_label_buf);

        float thumb_x = sx + 12.0f;
        float thumb_y = start_y + 56.0f;
        float thumb_w = slot_w - 24.0f;
        float thumb_h = 80.0f;

        ps3_draw_rounded_rect(thumb_x, thumb_y, thumb_w, thumb_h, 6.0f, RGBA8(12, 15, 22, 255));

        if (saved && thumb_path[0] != '\0' &&
            (s_slot_thumb_paths[slot_idx][0] == '\0' || strcmp(s_slot_thumb_paths[slot_idx], thumb_path) != 0)) {
            if (s_slot_thumbs[slot_idx]) {
                SDL_FreeSurface(s_slot_thumbs[slot_idx]);
                s_slot_thumbs[slot_idx] = NULL;
            }
            strncpy(s_slot_thumb_paths[slot_idx], thumb_path, 255);
            s_slot_thumb_paths[slot_idx][255] = '\0';
            FILE *tf = fopen(thumb_path, "rb");
            if (tf) {
                fclose(tf);
                s_slot_thumbs[slot_idx] = IMG_Load(thumb_path);
            }
        }
        if (!saved) {
            if (s_slot_thumbs[slot_idx]) {
                SDL_FreeSurface(s_slot_thumbs[slot_idx]);
                s_slot_thumbs[slot_idx] = NULL;
            }
            s_slot_thumb_paths[slot_idx][0] = '\0';
        }

        if (s_slot_thumbs[slot_idx]) {
            float surf_w = (float)s_slot_thumbs[slot_idx]->w;
            float surf_h = (float)s_slot_thumbs[slot_idx]->h;
            float scale_x = thumb_w / surf_w;
            float scale_y = thumb_h / surf_h;
            float final_scale = (scale_x < scale_y) ? scale_x : scale_y;
            if (final_scale > 1.0f) final_scale = 1.0f;
            if (final_scale < 0.05f) final_scale = 0.05f;

            int draw_w = (int)(surf_w * final_scale);
            int draw_h = (int)(surf_h * final_scale);
            int draw_x = (int)(thumb_x + (thumb_w - draw_w) * 0.5f);
            int draw_y = (int)(thumb_y + (thumb_h - draw_h) * 0.5f);

            SDL_Rect dst_r = { (Sint16)draw_x, (Sint16)draw_y, (Uint16)draw_w, (Uint16)draw_h };
            SDL_SoftStretch(s_slot_thumbs[slot_idx], NULL, prSDLScreen, &dst_r);
        } else {
            ps3_draw_boing_ball_icon(thumb_x + (thumb_w * 0.5f), thumb_y + 40.0f, 22.0f, 0.0f);
            ps3_draw_text_centered(thumb_x + (thumb_w * 0.5f), thumb_y + 66.0f,
                saved ? PS3_COLOR_SUCCESS : PS3_COLOR_TEXT_MUTED, 0.80f,
                saved ? "SAVED" : "EMPTY");
        }

        float act_x = sx + 8.0f;
        float act_w = slot_w - 16.0f;
        float half_w = (act_w - 6.0f) * 0.5f;
        float row1_y = start_y + 142.0f;
        float row2_y = start_y + 176.0f;
        float row3_y = start_y + 210.0f;
        float btn_h = 30.0f;

        bool f_save = (focused && s_savestate_subaction == 0);
        bool f_load = (focused && s_savestate_subaction == 1);
        ps3_draw_card(act_x, row1_y, half_w, btn_h, f_save, false);
        ps3_draw_button_glyph(act_x + 6.0f, row1_y + 4.0f, PS3_GLYPH_CROSS);
        ps3_draw_text(act_x + 35.0f, row1_y + 6.0f, f_save ? PS3_COLOR_TEXT_WHITE : PS3_COLOR_TEXT_MUTED, 0.70f, "SAVE");

        ps3_draw_card(act_x + half_w + 6.0f, row1_y, half_w, btn_h, f_load, false);
        ps3_draw_button_glyph(act_x + half_w + 6.0f, row1_y + 4.0f, PS3_GLYPH_SQUARE);
        ps3_draw_text(act_x + half_w + 35.0f, row1_y + 6.0f, f_load ? PS3_COLOR_TEXT_WHITE : PS3_COLOR_TEXT_MUTED, 0.70f, "LOAD");

        bool f_exp = (focused && s_savestate_subaction == 2);
        bool f_imp = (focused && s_savestate_subaction == 3);
        ps3_draw_card(act_x, row2_y, half_w, btn_h, f_exp, false);
        ps3_draw_button_glyph(act_x + 6.0f, row2_y + 4.0f, PS3_GLYPH_TRIANGLE);
        ps3_draw_text(act_x + 35.0f, row2_y + 6.0f, f_exp ? PS3_COLOR_TEXT_WHITE : PS3_COLOR_TEXT_MUTED, 0.70f, "EXP");

        ps3_draw_card(act_x + half_w + 6.0f, row2_y, half_w, btn_h, f_imp, false);
        ps3_draw_button_glyph(act_x + half_w + 6.0f, row2_y + 4.0f, PS3_GLYPH_MINUS);
        ps3_draw_text(act_x + half_w + 35.0f, row2_y + 6.0f, f_imp ? PS3_COLOR_TEXT_WHITE : PS3_COLOR_TEXT_MUTED, 0.70f, "IMP");

        bool f_del = (focused && s_savestate_subaction == 4);
        ps3_draw_card(act_x, row3_y, act_w, btn_h, f_del, false);
        ps3_draw_text_centered(act_x + (act_w * 0.5f), row3_y + 7.0f,
            f_del ? PS3_COLOR_AMIGA_RED : PS3_COLOR_TEXT_MUTED, 0.72f, "DELETE STATE");
    }

    ps3_draw_list_page_indicator(*selected_item, total_items, 4);
}

static bool s_ftp_modal_active = false;
static int s_ftp_modal_frames = 0;

bool ps3_gui_is_ftp_active(void)
{
    return s_ftp_modal_active;
}

void ps3_gui_stop_ftp(void)
{
    if (s_ftp_modal_active || ps3_ftp_is_running()) {
        ps3_ftp_stop();
        s_ftp_modal_active = false;
        s_ftp_modal_frames = 0;
    }
}

void ps3_view_ftp(Ps3InputState *input, int *selected_item)
{
    (void)selected_item;
    s_ftp_modal_frames++;

    if (!ps3_ftp_is_running() && ps3_ftp_start() != 0) {
        ps3_show_message_box("FTP Server", "Unable to start FTP service. Check Wi-Fi connection.", "OK (X)");
        s_ftp_modal_active = false;
        s_ftp_modal_frames = 0;
        input->pressed = 0;
        return;
    }

    char ip[32];
    char endpoint[96];
    ps3_ftp_get_ip(ip, sizeof(ip));
    snprintf(endpoint, sizeof(endpoint), "ftp://%s:%d", ip, ps3_ftp_get_port());
    bool has_valid_ip = (strcmp(ip, "127.0.0.1") != 0 && strcmp(ip, "0.0.0.0") != 0 && strlen(ip) > 0);

    if (s_ftp_modal_frames > 10 && (input->pressed & (PS3_BTN_CIRCLE | PS3_BTN_PLUS))) {
        ps3_ftp_stop();
        s_ftp_modal_active = false;
        s_ftp_modal_frames = 0;
        input->pressed = 0;
        input->held = 0;
        SDL_Delay(100);
        return;
    }

    if (s_ftp_modal_frames > 10 && (input->pressed & PS3_BTN_CROSS) && !has_valid_ip) {
        char manual[40];
        snprintf(manual, sizeof(manual), "%s", s_ps3_ftp_manual_ip);
        ps3_kbd_get((char*)"Enter console IP (XMB network status):", manual, sizeof(manual) - 1, 0, manual);
        if (manual[0] != '\0') {
            snprintf(s_ps3_ftp_manual_ip, sizeof(s_ps3_ftp_manual_ip), "%s", manual);
            write_log("[PS3] ftp: manual IP %s\n", s_ps3_ftp_manual_ip);
        }
        input->pressed = 0;
        input->held = 0;
        SDL_Delay(80);
        return;
    }

    int client_count = ps3_ftp_get_client_count();

    ps3_draw_card_custom(100.0f, 105.0f, 760.0f, 290.0f, PS3_COLOR_CARD, PS3_COLOR_FOCUS_BORDER);
    ps3_draw_text_centered(480.0f, 130.0f, PS3_COLOR_TEXT_WHITE, 1.25f, "FTP FILE TRANSFER");

    if (has_valid_ip) {
        ps3_draw_text_centered(480.0f, 175.0f, PS3_COLOR_SUCCESS, 1.10f, "FTP SERVER ACTIVE");
        ps3_draw_text_centered(480.0f, 215.0f, PS3_COLOR_TEXT_WHITE, 1.05f, endpoint);
    } else if (s_ps3_ftp_manual_ip[0] != '\0') {
        char mep[128];
        snprintf(mep, sizeof(mep), "ftp://%s:%d  (manual)", s_ps3_ftp_manual_ip, ps3_ftp_get_port());
        ps3_draw_text_centered(480.0f, 175.0f, PS3_COLOR_SUCCESS, 1.10f, "FTP SERVER ACTIVE");
        ps3_draw_text_centered(480.0f, 215.0f, PS3_COLOR_TEXT_WHITE, 1.05f, mep);
    } else if (s_ftp_route_seen) {
        ps3_draw_text_centered(480.0f, 175.0f, PS3_COLOR_SUCCESS, 1.10f, "CONNECTED");
        ps3_draw_text_centered(480.0f, 215.0f, PS3_COLOR_TEXT_MUTED, 0.95f,
                               "IP unavailable - use PS3 XMB network status");
    } else {
        ps3_draw_text_centered(480.0f, 175.0f, PS3_COLOR_AMIGA_ORANGE, 1.10f, "WAITING FOR WI-FI...");
        ps3_draw_text_centered(480.0f, 215.0f, PS3_COLOR_TEXT_MUTED, 0.95f, "Connect to Wi-Fi in PS3 System Settings");
    }

    if (client_count > 0) {
        char cbuf[64];
        snprintf(cbuf, sizeof(cbuf), "Connected Clients: %d", client_count);
        ps3_draw_text_centered(480.0f, 255.0f, PS3_COLOR_SUCCESS, 0.85f, cbuf);
    } else {
        char pbuf[96];
        snprintf(pbuf, sizeof(pbuf), "Port: %d  (Anonymous or any user/pass)", ps3_ftp_get_port());
        ps3_draw_text_centered(480.0f, 255.0f, PS3_COLOR_TEXT_MUTED, 0.85f, pbuf);
    }

    ps3_draw_text_centered(480.0f, 290.0f, PS3_COLOR_TEXT_MUTED, 0.80f, "FileZilla / WinSCP / Cyberduck supported");
    if (has_valid_ip) {
        ps3_draw_text_centered(480.0f, 325.0f, PS3_COLOR_TEXT_MUTED, 0.85f, "Press O to stop FTP server and return to System");
    } else {
        ps3_draw_text_centered(480.0f, 325.0f, PS3_COLOR_TEXT_MUTED, 0.85f, "X: enter IP manually   O: stop FTP");
    }
    ps3_draw_footer("FTP ACTIVE", "O STOP & RETURN");
}

void ps3_view_system(Ps3InputState *input, int *selected_item)
{
    if (s_ftp_modal_active) {
        ps3_view_ftp(input, selected_item);
        return;
    }

    const int total_items = 12;
    if (*selected_item < 0) *selected_item = 0;
    if (*selected_item >= total_items) *selected_item = total_items - 1;

    if (input->pressed & PS3_BTN_UP) {
        (*selected_item)--;
        if (*selected_item < 0) *selected_item = total_items - 1;
    }
    if (input->pressed & PS3_BTN_DOWN) {
        (*selected_item)++;
        if (*selected_item >= total_items) *selected_item = 0;
    }

    if (input->pressed & PS3_BTN_CROSS) {
        switch (*selected_item) {
            case 0:
                if (uae4all_image_file0[0]) {
                    saveconfig(0);
                    saveconfig(1);
                } else {
                    saveconfig(1);
                }
                ps3_show_message_box("Config Saved", "Configuration saved successfully for current game.", "OK (X)");
                break;
            case 1: {
                char conf_name[100] = "";
                ps3_kbd_get((char*)"Enter config name:", "", sizeof(conf_name) - 1, 0, conf_name);
                if (conf_name[0] != '\0') {
                    config_save_as_name = conf_name;
                    int res = saveconfig(4);
                    config_save_as_name = NULL;
                    if (res == 1)
                        ps3_show_message_box("Config Saved", "Configuration saved successfully as custom file.", "OK (X)");
                }
                break;
            }
            case 2: {
                char load_path[512];
                load_path[0] = '\0';
                int res = ps3_gui_run_browser(load_path, "./data/conf", 9);
                if (res == 1 && load_path[0] != '\0') {
                    strncpy(config_load_filename, load_path, sizeof(config_load_filename) - 1);
                    config_load_filename[sizeof(config_load_filename) - 1] = '\0';
                    loadconfig(5);
                    ps3_set_kickstart(kickstart, 1);
                    bReloadKickstart = 1;
                    UpdateChipsetSettings();
                    UpdateCPUModelSettings();
                    UpdateMemorySettings();
                    getChanges();
                    check_all_prefs();
                    gui_update();
                    ps3_show_message_box("Config Loaded", "Configuration loaded and applied successfully.", "OK (X)");
                }
                break;
            }
            case 3:
                saveconfig(1);
                ps3_show_message_box("Default Saved", "Configuration saved as default (uaeconfig.conf).", "OK (X)");
                break;
            case 4:
                loadconfig(1);
                ps3_set_kickstart(kickstart, 1);
                bReloadKickstart = 1;
                UpdateChipsetSettings();
                UpdateCPUModelSettings();
                UpdateMemorySettings();
                getChanges();
                check_all_prefs();
                gui_update();
                ps3_show_message_box("Default Loaded", "Default configuration reloaded from uaeconfig.conf.", "OK (X)");
                break;
            case 5:
                if (ps3_show_confirm_box("Reset Settings", "Restore default settings for all parameters?", "Yes", "No")) {
                    SetDefaultMenuSettings(1);
                    int default_kickstart_loaded = ps3_set_kickstart(kickstart, 1);
                    bReloadKickstart = 1;
                    UpdateChipsetSettings();
                    UpdateCPUModelSettings();
                    UpdateMemorySettings();
                    getChanges();
                    check_all_prefs();
                    gui_update();
                    if (default_kickstart_loaded)
                        ps3_show_message_box("Settings Reset", "Default settings restored successfully.", "OK (X)");
                    else
                        ps3_show_message_box("Settings Reset", "Defaults restored, but the default Kickstart ROM is missing.", "OK (X)");
                }
                break;
            case 6:
                mainMenu_autosave = 1 - mainMenu_autosave;
                break;
            case 7:
                mainMenu_case = MAIN_MENU_CASE_RESET;
                break;
            case 8:
                if (emulating) {
                    vita_screenshot_request = 1;
                    mainMenu_case = MAIN_MENU_CASE_RUN;
                } else {
                    ps3_show_message_box("Screenshot", "Launch a game before taking a screenshot.", "OK (X)");
                }
                break;
            case 9:
                s_ftp_modal_active = true;
                s_ftp_modal_frames = 0;
                input->pressed = 0;
                input->held = 0;
                break;
            case 10:
                if (ps3_show_confirm_box("About", "Open UAE4All2 and credits?", "Yes", "No")) {
                    ps3_show_about_box();
                }
                break;
            case 11:
                if (ps3_show_confirm_box("Quit to XMB", "Exit UAE4All2 and return to the PS3 XMB?", "Yes", "No")) {
                    mainMenu_case = MAIN_MENU_CASE_QUIT;
                }
                break;
        }
    }

    float card_x = 20.0f;
    float card_w = PS3_SCREEN_W - 40.0f;
    const float start_y = PS3_LIST_START_Y;
    const float item_h = 44.0f;
    const float item_gap = 6.0f;
    const int visible_items = ps3_list_visible_rows(start_y, item_h, item_gap);
    int first_item = *selected_item >= visible_items ? *selected_item - visible_items + 1 : 0;

    static const char *system_titles[12] = {
        "Save Game Configuration",
        "Save Configuration As...",
        "Load Configuration...",
        "Save Default Configuration",
        "Load Default Configuration",
        "Restore Default Settings",
        "Auto-save on Exit",
        "Reboot Amiga Emulation",
        "Take Screenshot",
        "FTP File Transfer",
        "About UAE4All2",
        "Quit to XMB"
    };
    static const char *system_subtitles[12] = {
        "Save all disk, display, and hardware settings for current game",
        "Save current configuration with a custom filename via on-screen keyboard",
        "Browse and load custom configuration from ./data/conf/",
        "Save current settings as emulator default (uaeconfig.conf)",
        "Reload default settings from uaeconfig.conf",
        "Reset all emulator configurations to factory defaults",
        "Automatically save state when exiting the emulator",
        "Hard reset the Amiga virtual machine with current settings",
        "Capture the next emulated frame as a PNG in the screenshots folder",
        "Start built-in FTP server to transfer ADF, HDF, and WHDLoad games over Wi-Fi",
        "Credits, original authors, contributors and project acknowledgements",
        "Exit UAE4All2 and return cleanly to the PlayStation 3 XMB"
    };
    static const char *system_badges[12] = { "SAVE", "SAVE AS", "LOAD", "SAVE DEF", "LOAD DEF", "RESET", "AUTO", "REBOOT", "SHOT", "START", "ABOUT", "QUIT" };

    for (int i = 0; i < visible_items; i++) {
        int item = first_item + i;
        if (item >= total_items) break;
        float y = start_y + (float)i * (item_h + item_gap);
        bool focused = (*selected_item == item);
        if (item == 6) {
            ps3_draw_switch_item(card_x, y, card_w, item_h, system_titles[item], mainMenu_autosave == 1, focused);
        } else if (item == 9) {
            ps3_draw_button_item(card_x, y, card_w, item_h, system_titles[item],
                system_subtitles[item], "START", focused, false);
        } else if (item == 11) {
            ps3_draw_button_item_custom(card_x, y, card_w, item_h, system_titles[item],
                system_subtitles[item], system_badges[item], PS3_COLOR_AMIGA_RED, focused, false);
        } else {
            ps3_draw_button_item(card_x, y, card_w, item_h, system_titles[item],
                system_subtitles[item], system_badges[item], focused, false);
        }
    }
    ps3_draw_list_page_indicator(*selected_item, total_items, visible_items);
}

#endif

