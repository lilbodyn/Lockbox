#ifndef LOCKBOX_ARCHIVE_H
#define LOCKBOX_ARCHIVE_H
#include "guard.h"
#include <stddef.h>
enum { ARCHIVE_PAGE_BYTES = 2048, ARCHIVE_SLOT_BYTES = 64, ARCHIVE_SLOTS = 64,
    ARCHIVE_PENDING = 8 };
typedef struct {
    void *context;
    bool (*read)(void *, uint32_t, void *, size_t);
    bool (*erase)(void *, unsigned);
    bool (*program)(void *, uint32_t, const uint8_t[8]);
} archive_media_t;
typedef struct {
    archive_media_t media;
    guard_event_t pending[ARCHIVE_PENDING];
    uint8_t image[ARCHIVE_SLOT_BYTES];
    uint32_t ticket, boot, accepted, corrupt, lost, failures;
    uint16_t slot;
    uint8_t head, count, word, phase;
    bool failed;
} archive_t;
void archive_open(archive_t *, archive_media_t);
bool archive_enqueue(archive_t *, const guard_event_t *);
bool archive_step(archive_t *);
bool archive_read(archive_t *, unsigned, guard_event_t *, uint32_t *, uint32_t *);
uint32_t archive_crc(const uint8_t *, size_t);
#endif
