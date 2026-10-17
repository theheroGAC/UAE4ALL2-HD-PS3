#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

struct SDL_Surface;

void cover_mgr_load(const char *display_title, const char *full_path);
void cover_mgr_unload(void);
SDL_Surface *cover_mgr_get(void);
void cover_mgr_normalize(const char *input, char *out, size_t out_sz);

int cover_mgr_download(const char *display_title);

#ifdef __cplusplus
}
#endif