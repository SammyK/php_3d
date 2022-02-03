#ifndef SKETCHUP_H
#define SKETCHUP_H

#include <stdbool.h>
#include <stdlib.h>

typedef struct sketchup_building_s {
    void *ptr;
} sketchup_building;

typedef struct sketchup_room_s {
    void *ptr;
} sketchup_room;

enum sketchup_val_type {
    SKETCHUP_VAL_STRING = 0,
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
bool sketchup_building_ctor(sketchup_building *building);
bool sketchup_building_append_room(sketchup_building building, const char *name, size_t room_index);
bool sketchup_building_save(sketchup_building building, const char *file);
bool sketchup_building_dtor(sketchup_building building);

bool sketchup_room_append_variable(sketchup_building building, size_t room_index, sketchup_val val);

void sketchup_sdk_version(size_t bufsiz, char *version);

#define SKETCHUP_NULL {0}

#endif	/* SKETCHUP_H */
