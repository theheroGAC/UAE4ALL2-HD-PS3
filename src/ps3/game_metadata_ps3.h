#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_META_YEAR_MAX  16
#define GAME_META_STR_MAX   128
#define GAME_META_LANG_MAX  200

struct GameMetadata {
    char year[GAME_META_YEAR_MAX];
    char developer[GAME_META_STR_MAX];
    char publisher[GAME_META_STR_MAX];
    char genre[GAME_META_STR_MAX];
    char players[GAME_META_STR_MAX];
    char languages[GAME_META_LANG_MAX];
};

bool meta_db_load(const char *json_path);
bool meta_db_lookup(const char *normalized_key, struct GameMetadata *out);
void meta_db_fill_unknown(struct GameMetadata *out);
void meta_db_unload(void);

#ifdef __cplusplus
}
#endif
