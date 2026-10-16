#include <stdio.h>
#include <string.h>
#include "defines.h"
#include "api.h"
#include "ui_main.h"
#include "ui_controlshelp.h"
#include "ui_listview.h"
#include "ui_toast.h"
#include "module_menu.h"
#include "resume.h"
#include "background.h"

// Menu items variants (first entry is mutable for Resume/Now Playing swap)
static const char* menu_items_with_first[] = {"继续播放", "媒体库", "在线电台", "播客", "设置"};
static const char* menu_items_no_first[] = {"媒体库", "在线电台", "播客", "设置"};

// Cached first_item_mode for callbacks
static int current_first_item_mode = MENU_FIRST_NONE;

// Get label for Now Playing based on background player type
static const char* get_now_playing_label(void) {
	switch (Background_getActive()) {
	case BG_MUSIC:
		return "音乐";
	case BG_RADIO:
		return "电台";
	case BG_PODCAST:
		return "播客";
	default:
		return "音频";
	}
}

// Label callback for first item label and Settings update badge
static const char* main_menu_get_label(int index, const char* default_label,
									   char* buffer, int buffer_size) {
	bool has_first = (current_first_item_mode != MENU_FIRST_NONE);

	// First item: return full label for pill sizing
	if (has_first && index == 0) {
		if (current_first_item_mode == MENU_FIRST_NOW_PLAYING) {
			snprintf(buffer, buffer_size, "正在播放：%s", get_now_playing_label());
			return buffer;
		}
		// Resume mode
		const char* label = Resume_getLabel();
		if (label) {
			snprintf(buffer, buffer_size, "%s", label);
			return buffer;
		}
	}

	return NULL; // Use default label
}

// Main menu ListView (full mode: the widget owns selection and input;
// MenuModule_run drives it through MusicMainMenu_view()).
static ListView main_menu_view;
static char main_menu_label_buf[256];

ListView* MusicMainMenu_view(void) {
	return &main_menu_view;
}

static void main_menu_get_row(void* ctx, int i, bool selected,
							  ListViewRow* out) {
	const char** items = ctx;
	const char* label = items[i];
	const char* custom = main_menu_get_label(i, label, main_menu_label_buf,
											 sizeof(main_menu_label_buf));
	out->label = custom ? custom : label;
	(void)selected;
}

// Render the main menu
void render_menu(SDL_Surface* screen, IndicatorType show_setting,
				 char* toast_message, uint32_t toast_time, int first_item_mode) {
	(void)show_setting;
	current_first_item_mode = first_item_mode;
	bool has_first = (first_item_mode != MENU_FIRST_NONE);

	// Update the first item label based on mode
	if (first_item_mode == MENU_FIRST_NOW_PLAYING) {
		menu_items_with_first[0] = "正在播放";
	} else {
		menu_items_with_first[0] = "继续播放";
	}

	const char** items = has_first ? menu_items_with_first : menu_items_no_first;
	int count = has_first ? 5 : 4;

	GFX_clear(screen);
	ListView* v = &main_menu_view;
	v->title = "音乐播放器";
	v->font = font.large;
	v->count = count;
	v->get_row = main_menu_get_row;
	v->ctx = (void*)items;
	v->list_id = (const void*)items;
	v->hint_pairs = (char*[]){"B", "退出", "A", "打开", NULL};
	UI_listViewRender(v, screen);

	// Toast notification
	UI_renderToast(screen, toast_message, toast_time);
}

// Controls help text. Only the playing pages still surface this modal
// (MENU on the music player; the "Playback Controls" context-menu item on
// the radio/podcast players) — list pages use the context menu instead.

// Music player controls (A/B shown in footer)
static const ControlHelp player_controls[] = {
	{"X", "切换随机播放"},
	{"Y", "切换循环"},
	{"Up/R1", "下一曲"},
	{"Down/L1", "上一曲"},
	{"Left/Right", "快进/快退"},
	{"L2/" BTN_FN1_NAME, "切换可视化"},
	{"R2/" BTN_FN2_NAME, "切换歌词"},
	{"Select", "熄屏"},
	{"Select + A", "唤醒屏幕"},
	{"Start", "后台播放"},
	{NULL, NULL}};

// Radio playing controls (B shown in footer)
static const ControlHelp radio_playing_controls[] = {
	{"Up/R1", "下一电台"},
	{"Down/L1", "上一电台"},
	{"Select", "熄屏"},
	{"Select + A", "唤醒屏幕"},
	{"Start", "后台播放"},
	{NULL, NULL}};

// Podcast playing controls
static const ControlHelp podcast_playing_controls[] = {
	{"Left", "后退 10 秒"},
	{"Right", "前进 30 秒"},
	{"Up/Down", "播放速度"},
	{"Select", "熄屏"},
	{"Select + A", "唤醒屏幕"},
	{"Start", "后台播放"},
	{NULL, NULL}};

// Generic/default controls
static const ControlHelp default_controls[] = {
	{NULL, NULL}};

// Render controls help dialog overlay
void render_controls_help(SDL_Surface* screen, int app_state) {
	const ControlHelp* controls;
	const char* page_title;

	switch (app_state) {
	case 2: // STATE_PLAYING
		controls = player_controls;
		page_title = "音乐播放器";
		break;
	case 4: // STATE_RADIO_PLAYING
		controls = radio_playing_controls;
		page_title = "电台播放器";
		break;
	case 37: // PODCAST_INTERNAL_PLAYING
		controls = podcast_playing_controls;
		page_title = "播客播放器";
		break;
	default:
		controls = default_controls;
		page_title = "控制说明";
		break;
	}

	UI_renderControlsHelp(screen, page_title, controls);
}

// Render screen off hint message (shown before screen turns off)
void render_screen_off_hint(SDL_Surface* screen) {
	int hw = screen->w;
	int hh = screen->h;

	// Fill entire screen with black
	SDL_FillRect(screen, NULL, RGB_BLACK);

	// Render hint message centered
	const char* msg = "按 SELECT + A 唤醒屏幕";
	SDL_Surface* msg_surf = GFX_renderText(font.medium, msg, COLOR_WHITE);
	if (msg_surf) {
		SDL_BlitSurface(msg_surf, NULL, screen, &(SDL_Rect){(hw - msg_surf->w) / 2, (hh - msg_surf->h) / 2});
		SDL_FreeSurface(msg_surf);
	}
}
