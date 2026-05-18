#pragma once

/*
 * Tiny audio facade around miniaudio.
 * Gameplay code calls these functions without knowing about ma_engine/ma_sound.
 */

// Load the engine and all short sound effects.
void audio_init();

// Release loaded sounds and the miniaudio engine.
void audio_destroy();

// One-shot effects. Each call rewinds the sound before starting it.
void audio_play_shoot();
void audio_play_hurt();
void audio_play_door();
