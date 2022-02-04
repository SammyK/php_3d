#ifndef SKETCHUP_H
#define SKETCHUP_H

#include <stdbool.h>
#include <stdlib.h>

typedef struct sketchup_town_s {
    void *ptr;
} sketchup_town;

typedef struct sketchup_room_s {
    void *ptr;
} sketchup_room;

enum sketchup_val_type {
    SKETCHUP_VAL_UNSUPPORTED = 0,
    SKETCHUP_VAL_STRING,
    SKETCHUP_VAL_LONG,
    SKETCHUP_VAL_DOUBLE,
};

typedef struct sketchup_val_s {
    enum sketchup_val_type type;
    void *ptr;
} sketchup_val;

void sketchup_startup(void);
void sketchup_shutdown(void);

// sketchup_startup() must have been called before calling any of the functions below
bool sketchup_town_ctor(sketchup_town *town);
bool sketchup_town_append_room(sketchup_town town, const char *name, size_t room_index, size_t visit_index);
bool sketchup_town_save(sketchup_town town, const char *file);
bool sketchup_town_dtor(sketchup_town town);

bool sketchup_room_append_variable(sketchup_town town, size_t room_index, size_t visit_index, size_t var_index, const char *name, sketchup_val val);

void sketchup_sdk_version(size_t bufsiz, char *version);

#define SKETCHUP_NULL {0}

// Arbitrary limit - not sure if this is necessary, but seems like we should have some kind of upper bounds limit
#define SUP_MAX_ROOMS 5000

#endif	/* SKETCHUP_H */
