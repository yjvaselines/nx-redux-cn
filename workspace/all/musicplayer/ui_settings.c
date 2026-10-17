#include <stdio.h>
#include <string.h>

#include "api.h"
#include "ui_buttonhintbar.h"
#include "ui_menubar.h"
#include "ui_settings.h"
#include "ui_list.h"
#include "settings.h"
#include "music_balance.h"
#include "album_art.h"
#include "lyrics.h"

// Settings menu items（自动睡眠置顶）
#define SETTINGS_ITEM_AUTO_SLEEP 0
#define SETTINGS_ITEM_BALANCE 1
#define SETTINGS_ITEM_BASS_FILTER 2
#define SETTINGS_ITEM_SOFT_LIMITER 3
#define SETTINGS_ITEM_SAMPLE_RATE 4
#define SETTINGS_ITEM_RESAMPLER 5
#define SETTINGS_ITEM_BUFFER 6
#define SETTINGS_ITEM_CLEAR_CACHE 7
#define SETTINGS_ITEM_CLEAR_LYRICS 8
#define SETTINGS_ITEM_COUNT 9

// Format cache size as human-readable string
static void format_cache_size(long bytes, char* buf, int buf_size) {
	if (bytes >= 1024 * 1024) {
		snprintf(buf, buf_size, "%.1f MB", bytes / (1024.0 * 1024.0));
	} else if (bytes >= 1024) {
		snprintf(buf, buf_size, "%.1f KB", bytes / 1024.0);
	} else {
		snprintf(buf, buf_size, "%ld B", bytes);
	}
}

void render_settings_menu(SDL_Surface* screen, IndicatorType show_setting, int menu_selected, const char* balance_error) {
	GFX_clear(screen);

	UI_renderMenuBar(screen, "音乐播放器 | 设置");
	ListLayout layout = UI_calcListLayout(screen);

	// Build dynamic cache labels
	static char cache_label[128];
	long cache_size = album_art_get_cache_size();
	char size_str[32];
	format_cache_size(cache_size, size_str, sizeof(size_str));
	snprintf(cache_label, sizeof(cache_label), "清除专辑封面 (%s)", size_str);

	static char lyrics_label[128];
	long lyrics_size = Lyrics_getCacheSize();
	char lyrics_size_str[32];
	format_cache_size(lyrics_size, lyrics_size_str, sizeof(lyrics_size_str));
	snprintf(lyrics_label, sizeof(lyrics_label), "清除歌词 (%s)", lyrics_size_str);

	const char* balance_desc = balance_error && balance_error[0] ? balance_error : "调节游戏与音乐的混音比例";
	UISettingsItem items[] = {
		{.label = "自动睡眠", .value = Settings_getAutoSleepDisplayStr(), .swatch = -1, .cycleable = 1, .desc = "播放累计达到设定时长后自动停止"},
		{.label = "声道平衡", .value = MusicBalance_getDisplayString(), .swatch = -1, .cycleable = 1, .desc = balance_desc},
		{.label = "低音滤波", .value = Settings_getBassFilterDisplayStr(), .swatch = -1, .cycleable = 1, .desc = "高通滤波，减少扬声器失真"},
		{.label = "软限幅", .value = Settings_getSoftLimiterDisplayStr(), .swatch = -1, .cycleable = 1, .desc = "限制音量峰值，防止削波失真"},
		{.label = "采样率", .value = Settings_getRateModeDisplayStr(), .swatch = -1, .cycleable = 1, .desc = "跟随源可避免 USB DAC 重采样；下一曲生效"},
		{.label = "重采样质量", .value = Settings_getResamplerQualityDisplayStr(), .swatch = -1, .cycleable = 1, .desc = "质量越高占用 CPU 越多；下一曲生效"},
		{.label = "音频缓冲", .value = Settings_getBufferFramesDisplayStr(), .swatch = -1, .cycleable = 1, .desc = "缓冲越大越不易断音；下一曲生效"},
		{.label = cache_label, .swatch = -1, .desc = "删除缓存的专辑封面图片"},
		{.label = lyrics_label, .swatch = -1, .desc = "删除缓存的歌词文件"},
	};

	static int scroll = 0;
	UI_renderSettingsPage(screen, &layout, items, SETTINGS_ITEM_COUNT,
						  menu_selected, &scroll, NULL);

	bool is_cyclable = (menu_selected == SETTINGS_ITEM_BALANCE ||
						menu_selected == SETTINGS_ITEM_BASS_FILTER ||
						menu_selected == SETTINGS_ITEM_SOFT_LIMITER ||
						menu_selected == SETTINGS_ITEM_SAMPLE_RATE ||
						menu_selected == SETTINGS_ITEM_RESAMPLER ||
						menu_selected == SETTINGS_ITEM_BUFFER ||
						menu_selected == SETTINGS_ITEM_AUTO_SLEEP);

	// Left/Right leads when shown: the d-pad sits on the device's left side
	if (is_cyclable)
		UI_renderButtonHintBar(screen, (char*[]){"LEFT/RIGHT", "更改", "B", "返回", NULL});
	else
		UI_renderButtonHintBar(screen, (char*[]){"B", "返回", "A", "打开", NULL});
}
