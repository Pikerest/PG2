#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audio.hpp"
#include <stdio.h>

static ma_engine  g_engine;
static ma_sound   g_shoot;
static ma_sound   g_hurt;
static ma_sound   g_door;
static bool       g_shoot_ok  = false;
static bool       g_hurt_ok   = false;
static bool       g_door_ok   = false;
static bool       g_engine_ok = false;

void audio_init() {
    ma_engine_config cfg = ma_engine_config_init();
    if (ma_engine_init(&cfg, &g_engine) != MA_SUCCESS) {
        fprintf(stderr, "[audio] engine init failed\n");
        return;
    }
    g_engine_ok = true;

    const unsigned int flags =
        MA_SOUND_FLAG_DECODE |
        MA_SOUND_FLAG_NO_PITCH |
        MA_SOUND_FLAG_NO_SPATIALIZATION;

    if (ma_sound_init_from_file(&g_engine, "sounds/shoot.wav", flags, NULL, NULL, &g_shoot) == MA_SUCCESS)
        g_shoot_ok = true;
    else
        fprintf(stderr, "[audio] shoot.wav failed\n");

    if (ma_sound_init_from_file(&g_engine, "sounds/hurt.wav", flags, NULL, NULL, &g_hurt) == MA_SUCCESS)
        g_hurt_ok = true;
    else
        fprintf(stderr, "[audio] hurt.wav failed\n");

    if (ma_sound_init_from_file(&g_engine, "sounds/door_open.wav", flags, NULL, NULL, &g_door) == MA_SUCCESS)
        g_door_ok = true;
    else
        fprintf(stderr, "[audio] door_open.wav failed\n");
}

void audio_destroy() {
    if (g_shoot_ok)  ma_sound_uninit(&g_shoot);
    if (g_hurt_ok)   ma_sound_uninit(&g_hurt);
    if (g_door_ok)   ma_sound_uninit(&g_door);
    if (g_engine_ok) ma_engine_uninit(&g_engine);
}

void audio_play_shoot() {
    if (!g_shoot_ok) return;
    ma_sound_seek_to_pcm_frame(&g_shoot, 0);
    ma_sound_start(&g_shoot);
}

void audio_play_hurt() {
    if (!g_hurt_ok) return;
    ma_sound_seek_to_pcm_frame(&g_hurt, 0);
    ma_sound_start(&g_hurt);
}

void audio_play_door() {
    if (!g_door_ok) return;
    ma_sound_seek_to_pcm_frame(&g_door, 0);
    ma_sound_start(&g_door);
}
