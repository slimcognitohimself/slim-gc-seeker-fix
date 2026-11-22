/* Seeker Fix: a native OpenRAC mod for Ratchet & Clank: Going Commando (NTSC-U v1.01).
 *
 * The Seeker Gun's and HK-22's missiles (moby classes 1497 and 3329, both run by
 * UpdateHomingProjectile, func_L00_003AA708 in rac2-decomp) lock on in two steps before they
 * dive: state 1 (20 ticks) and state 2 (10 ticks). In both the game also pushes the missile
 * away from its target each tick:
 *   state 1: 0.0833 units a tick
 *   state 2: 0.0833 * (timer - 5) / 5 while the timer is above 5
 * which adds up to almost two units of backing off: enough to back into a wall or the floor and
 * blow up. After each tick the mod puts that push back, toward the target, so the missile holds
 * its place during the lock-on (its drift and its turn toward the target are kept, and so is
 * the delay) and then dives as before.
 */
#include <stddef.h>
#include <stdint.h>

#ifndef OPENRAC_MOD_TARGET_MINOR /* the build sets it from the manifest\'s "api" */
#define OPENRAC_MOD_TARGET_MINOR 0
#endif
#include "openrac/openrac_mod.h"

OPENRAC_MOD_DEFINE("rac2-seeker-fix", "0.1.0")

#define A_HERO_MOBY 0x18C0B0u /* u32: the player's moby, in the level's moby array */
#define CLASS_SEEKER 1497
#define CLASS_HK22 3329
#define PUSH 0.083333336f
#define SCAN_SLOTS 6144 /* each side of the player's moby, 0x100 bytes a moby */
#define TRACKED 64

static const openrac_api* g_api;
static openrac_mod* g_mod;

static struct {
    uint32_t moby;
    int key; /* state * 1000 + timer, after the last tick seen */
} g_tracked[TRACKED];

static uint8_t* mem(uint32_t a) { return g_api->memory + a; }
static uint8_t rd8(uint32_t a) { return *(volatile uint8_t*)mem(a); }
static uint16_t rd16(uint32_t a) { return *(volatile uint16_t*)mem(a); }
static int16_t rds16(uint32_t a) { return *(volatile int16_t*)mem(a); }
static uint32_t rd32(uint32_t a) { return *(volatile uint32_t*)mem(a); }
static float rdf(uint32_t a) { return *(volatile float*)mem(a); }
static void wrf(uint32_t a, float v) { *(volatile float*)mem(a) = v; }
static int in_ram(uint32_t a) { return a >= 0x100000u && a < 0x2000000u; }

static float fsqrt(float x) {
    float r;
    int i;
    if (x <= 0.0f) return 0.0f;
    r = x > 1.0f ? x : 1.0f;
    for (i = 0; i < 24; i++) r = 0.5f * (r + x / r);
    return r;
}

static int* tracked_key(uint32_t moby) {
    int i, free_slot = -1;
    for (i = 0; i < TRACKED; i++) {
        if (g_tracked[i].moby == moby) return &g_tracked[i].key;
        if (free_slot < 0 && g_tracked[i].moby == 0) free_slot = i;
    }
    if (free_slot < 0) free_slot = (int)(moby >> 8) % TRACKED;
    g_tracked[free_slot].moby = moby;
    g_tracked[free_slot].key = -1;
    return &g_tracked[free_slot].key;
}

static void forget(uint32_t moby) {
    int i;
    for (i = 0; i < TRACKED; i++)
        if (g_tracked[i].moby == moby) g_tracked[i].moby = 0;
}

static void fix_missile(uint32_t m) {
    uint32_t vars = rd32(m + 0x68), target;
    int state = rd8(m + 0x20), timer, key, *last;
    float amount, dx, dy, dz, len;

    if (state != 1 && state != 2) {
        forget(m);
        return;
    }
    if (!in_ram(vars)) return;
    target = rd32(vars + 0x14);
    timer = state == 1 ? rds16(vars + 0x54) : rds16(vars + 0x56);
    key = state * 1000 + timer;
    last = tracked_key(m);
    if (*last == key) return; /* not updated this tick */
    *last = key;
    if (!in_ram(target) || rd8(target + 0x20) >= 0xFD) return;

    if (state == 1) {
        amount = PUSH;
    } else if (timer == 10) {
        amount = PUSH; /* the last state-1 tick, which moved it to state 2 */
    } else {
        /* the tick ran with the timer one higher than it is now */
        float r = ((float)(timer + 1) - 5.0f) / 5.0f;
        amount = r > 0.0f ? r * PUSH : 0.0f;
    }
    if (amount <= 0.0f) return;

    dx = rdf(target + 0x10) - rdf(m + 0x10);
    dy = rdf(target + 0x14) - rdf(m + 0x14);
    dz = rdf(target + 0x18) - rdf(m + 0x18);
    len = fsqrt(dx * dx + dy * dy + dz * dz);
    if (len < 0.001f) return;
    if (amount > len * 0.5f) amount = len * 0.5f;
    wrf(m + 0x10, rdf(m + 0x10) + dx / len * amount);
    wrf(m + 0x14, rdf(m + 0x14) + dy / len * amount);
    wrf(m + 0x18, rdf(m + 0x18) + dz / len * amount);
}

static void on_frame(void* user) {
    uint32_t hero = rd32(A_HERO_MOBY), p;
    int i;
    (void)user;
    if (!in_ram(hero)) return;
    for (i = -SCAN_SLOTS; i <= SCAN_SLOTS; i++) {
        p = hero + (uint32_t)(i * 0x100);
        if (!in_ram(p)) continue;
        {
            uint16_t c = rd16(p + 0xAA);
            if ((c == CLASS_SEEKER || c == CLASS_HK22) && rd8(p + 0x20) < 0xFD) fix_missile(p);
        }
    }
}

OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api, openrac_mod* mod) {
    g_api = api;
    g_mod = mod;
    if (!api->game || api->game[0] != 'r' || api->game[3] != '2' ||
        !api->version || api->version[0] != 'n' || api->version[4] != 0) {
        api->fail(mod, "only for Going Commando NTSC-U v1.01 (\"ntsc\")");
        return 1;
    }
    api->on_frame(mod, on_frame, 0);
    api->log(mod, OPENRAC_LOG_INFO, "Seeker Gun and HK-22 missiles hold their place while they lock on");
    return 0;
}

/* Only for a build without the C runtime (clang -ffreestanding, lld-link /noentry /nodefaultlib);
 * the SDK's CMake build links the runtime, which has its own. */
#ifdef OPENRAC_FREESTANDING
int _fltused = 0;

int __stdcall _DllMainCRTStartup(void* h, unsigned reason, void* p) {
    (void)h; (void)reason; (void)p;
    return 1;
}
#endif
