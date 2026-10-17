#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <stdbool.h>

// Music Player app-specific settings
// These are separate from the global NextUI settings (CFG_*)

// Initialize settings (loads from file if exists)
void Settings_init(void);
void Settings_setOwnerMode(bool owner);
void Settings_setAudioValues(int bass_filter_hz, float soft_limiter_threshold,
							 int rate_mode_follow, int resampler_quality, int buffer_frames);

// Cleanup settings (saves and frees resources)
void Settings_quit(void);

// Auto screen-off timeout in seconds. Always follows the system-wide screen
// timeout (Settings.pak), so there is no in-app setting for it.
int Settings_getScreenOffTimeout(void);

// 自动睡眠时间（分钟）：0=关，10/20/30/40/50/60。播放累计达到设定时长后
// 由 daemon 自动停止播放；停止后系统休眠链恢复正常。
int Settings_getAutoSleepMinutes(void);
void Settings_cycleAutoSleepNext(void);
void Settings_cycleAutoSleepPrev(void);
const char* Settings_getAutoSleepDisplayStr(void);
// 重新从磁盘读取 auto_sleep_minutes（daemon 侧定时刷新，感知 UI 的修改）
void Settings_reloadAutoSleep(void);

// Lyrics enabled setting
bool Settings_getLyricsEnabled(void);
void Settings_toggleLyrics(void);

// Speaker bass filter (high-pass cutoff in Hz, 0 = off)
int Settings_getBassFilterHz(void);
void Settings_cycleBassFilterNext(void);
void Settings_cycleBassFilterPrev(void);
const char* Settings_getBassFilterDisplayStr(void);

// Speaker soft limiter (0 = off, 1=mild, 2=medium, 3=strong)
float Settings_getSoftLimiterThreshold(void);
void Settings_cycleSoftLimiterNext(void);
void Settings_cycleSoftLimiterPrev(void);
const char* Settings_getSoftLimiterDisplayStr(void);

// Output sample-rate mode (0 = fixed device-default pipeline, 1 = follow source rate)
int Settings_getRateModeFollowSource(void);
void Settings_cycleRateModeNext(void);
void Settings_cycleRateModePrev(void);
const char* Settings_getRateModeDisplayStr(void);

// Resampler quality: 0=Fast (SRC_SINC_FASTEST), 1=Medium, 2=Best
int Settings_getResamplerQuality(void);
void Settings_cycleResamplerQualityNext(void);
void Settings_cycleResamplerQualityPrev(void);
const char* Settings_getResamplerQualityDisplayStr(void);

// SDL audio buffer size in frames (1024/2048/4096)
int Settings_getBufferFrames(void);
void Settings_cycleBufferFramesNext(void);
void Settings_cycleBufferFramesPrev(void);
const char* Settings_getBufferFramesDisplayStr(void);

// Save settings to file (auto-called on change)
void Settings_save(void);

#endif
