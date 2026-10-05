#ifndef CODEX_OFFLINE_H
#define CODEX_OFFLINE_H

#include <stdbool.h>

struct NetworkSystem;
extern struct NetworkSystem gCodexOfflineSystem;

/* Explicit standalone mode. This never opens a transport or joins a lobby. */
bool codex_offline_start(void);

#endif
