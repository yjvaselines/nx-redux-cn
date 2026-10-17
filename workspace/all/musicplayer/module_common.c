#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <msettings.h>
#include "defines.h"
#include "api.h"
#include "module_common.h"
#include "ui_menubar.h"
#include "ui_toast.h"
#include "module_player.h"
#include "settings.h"
#include "ui_main.h"
#include "ui_confirmdialog.h"
#include "ui_music.h"
#include "ui_radio.h"
#include "music_client.h"
#include "spectrum.h"
#include "background.h"

static bool autosleep_disabled = false;
static uint32_t last_input_time = 0;

// Dialog states
static bool show_controls_help = false;

// Overlay state tracking - force hide after button release
static bool overlay_buttons_were_active = false;
static uint32_t overlay_release_time = 0;
#define OVERLAY_VISIBLE_AFTER_RELEASE_MS 800 // How long overlay stays visible after release
#define OVERLAY_FORCE_HIDE_DURATION_MS 500	 // How long to keep forcing hide

bool ModuleCommon_confirmAppExit(SDL_Surface* screen) {
	MusicClient_update();
	const MusicSnapshotWire* snapshot = MusicClient_snapshot();
	if (snapshot->state != MUSIC_STATE_PLAYING)
		return true;

	int choice = UI_choiceModal(screen, "是否在后台继续播放？",
								"应用退出后，音乐将继续播放。", NULL, true, true);
	if (choice == UI_CHOICE_CANCEL)
		return false;
	if (choice == UI_CHOICE_NO) {
		if (MusicClient_stop() != MUSIC_STATUS_OK) {
			// The user asked to leave, so let them; playback just keeps running.
			(void)UI_confirmModal(screen, "无法停止播放",
								  "播放无法停止，音乐将在后台继续播放。", NULL, true,
								  true);
		}
	}
	return true;
}

void ModuleCommon_tickToast(char* message, uint32_t toast_time, bool* dirty) {
	if (message[0] == '\0')
		return;
	if (SDL_GetTicks() - toast_time < TOAST_DURATION) {
		*dirty = 1;
	} else {
		message[0] = '\0';
		*dirty = 1;
	}
}

void ModuleCommon_init(void) {
	autosleep_disabled = false;
	last_input_time = SDL_GetTicks();
        show_controls_help = false;
	overlay_buttons_were_active = false;
	overlay_release_time = 0;
}

// app_state of the music Now Playing page (see module_player.c): the only
// page where a MENU tap still opens the controls-help modal directly.
#define APP_STATE_MUSIC_PLAYING 2

// Run platform power management (power button, autosleep, volume/brightness
// indicators). Must run every frame — including while an overlay (context menu,
// controls help) is open — or the power button and autosleep go dead until the
// overlay is dismissed.
static void pump_power(GlobalInputResult* result, IndicatorType* show_setting) {
	bool dirty_tmp = result->dirty;
	IndicatorType indicator = (IndicatorType)*show_setting;
	PWR_update(&dirty_tmp, &indicator, NULL, NULL);
	*show_setting = (int)indicator;
	if (dirty_tmp && !result->dirty)
		result->dirty = true;
}

GlobalInputResult ModuleCommon_handleGlobalInput(SDL_Surface* screen, IndicatorType* show_setting, int app_state,
												 const ContextMenuItem* menu_items, int menu_count) {
	GlobalInputResult result = {false, false, false, 0};

	// Volume/brightness/color-temp combos are handled globally by keymon and by
	// PWR_update (Select + Vol = brightness, Start + Vol = color temp, Vol alone
	// = volume). We don't consume input or return early here — let PWR_update
	// detect the volume button press and set show_setting to display the UI.

	// Quitting is done via the context menu's "Quit App" item (every page has
	// one) or B from the main menu — the old MENU+SELECT chord was removed.

	// Context menu (opened on a MENU tap below): consumes all input while open.
	if (ContextMenu_isOpen()) {
		// Keep the MENU-tap state machine ticking so it doesn't fire on stale
		// state after close (mirrors nextui's main loop).
		PAD_tappedMenu(SDL_GetTicks());

		ContextMenuResult cmr = ContextMenu_handleInput();
		if (cmr.action == CONTEXTMENU_SELECTED) {
			if (cmr.id == CTX_ID_QUIT) {
				result.should_quit = ModuleCommon_confirmAppExit(screen);
			} else if (cmr.id == CTX_ID_CONTROLS) {
				show_controls_help = true;
			} else {
				result.context_id = cmr.id;
			}
			result.dirty = true;
		} else if (cmr.action == CONTEXTMENU_CANCEL) {
			result.dirty = true;
		} else if (PAD_justRepeated(BTN_UP) || PAD_justRepeated(BTN_DOWN)) {
			// Selection moved: redraw the overlay (main surface is untouched)
			ContextMenu_render(screen);
			PLAT_GPU_Flip();
		}
		pump_power(&result, show_setting); // keep power/autosleep alive while open
		result.input_consumed = true;
		return result;
	}

	// MENU tap: on the music Now Playing page it toggles the controls-help
	// modal (so lyric/visualizer shortcuts stay discoverable); everywhere else
	// it opens the page's context menu. A long MENU press is the OSD (handled
	// globally by keymon); PAD_tappedMenu only fires on a short release, so
	// the two don't collide.
	if (PAD_tappedMenu(SDL_GetTicks())) {
		if (show_controls_help || app_state == APP_STATE_MUSIC_PLAYING) {
			show_controls_help = !show_controls_help;
			GFX_clearLayers(LAYER_SCROLLTEXT);
			PLAT_clearLayers(LAYER_SPECTRUM);
			PLAT_clearLayers(LAYER_PLAYTIME);
			PLAT_GPU_Flip();
			PlayTime_clear();
		} else if (menu_count > 0) {
			ContextMenu_open((ContextMenuItem*)menu_items, menu_count);
			// Clear text/status GPU layers so they don't float above the
			// menu's dim backdrop; they re-render after close.
			// (LAYER_PLAYTIME shares its plane with LAYER_PODCAST_PROGRESS.)
			GFX_clearLayers(LAYER_SCROLLTEXT);
			PLAT_clearLayers(LAYER_SPECTRUM);
			PLAT_clearLayers(LAYER_PLAYTIME);
			PLAT_clearLayers(LAYER_BUFFER);
			PlayTime_clear();
			ContextMenu_render(screen);
			PLAT_GPU_Flip();
		}
		pump_power(&result, show_setting);
		result.input_consumed = true;
		result.dirty = true;
		return result;
	}

	// Controls help modal — any button except MENU (the toggle above) closes it.
	if (show_controls_help) {
		if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B) || PAD_justPressed(BTN_X) ||
			PAD_justPressed(BTN_Y) || PAD_justPressed(BTN_START) || PAD_justPressed(BTN_SELECT) ||
			PAD_justPressed(BTN_UP) || PAD_justPressed(BTN_DOWN) ||
			PAD_justPressed(BTN_LEFT) || PAD_justPressed(BTN_RIGHT) ||
			PAD_justPressed(BTN_L1) || PAD_justPressed(BTN_R1)) {
			show_controls_help = false;
			pump_power(&result, show_setting);
			result.input_consumed = true;
			result.dirty = true;
			return result;
		}
		// Dialog is shown, consume input and render (covers entire screen)
		render_controls_help(screen, app_state);
		GFX_flip(screen);
		pump_power(&result, show_setting); // keep power/autosleep alive while open
		result.input_consumed = true;
		return result;
	}

	// Non-overlay path: the caller runs ModuleCommon_PWR_update this frame, so
	// don't pump power again here (that double-processed the power button and
	// indicator timers). Only the overlay early-returns above pump, because the
	// module skips its own PWR_update on a consumed frame.
	return result;
}

void ModuleCommon_ctxAdd_impl(ContextMenuItem* items, int* count, int cap, const char* label, int id) {
	// Bound against BOTH the caller's array capacity and the widget's copy cap.
	if (cap > CONTEXTMENU_MAX_ITEMS)
		cap = CONTEXTMENU_MAX_ITEMS;
	if (*count >= cap)
		return;
	snprintf(items[*count].label, CONTEXTMENU_MAX_TEXT, "%s", label);
	items[*count].id = id;
	(*count)++;
}

void ModuleCommon_setAutosleepDisabled(bool disabled) {
	if (disabled && !autosleep_disabled) {
		PWR_disableAutosleep();
		// Screen-off playback: a short power press must not deep-sleep the
		// system (PWR_sleep pauses the daemon via musicplayerctl sleep) —
		// ModuleCommon_PWR_update turns the tap into a screen-off hint whose
		// expiry just kills the backlight while audio keeps playing.
		PWR_disableSleep();
		autosleep_disabled = true;
	} else if (!disabled && autosleep_disabled) {
		// Don't re-enable autosleep if background audio is still playing
		if (!Background_isPlaying()) {
			PWR_enableAutosleep();
			PWR_enableSleep();
			autosleep_disabled = false;
		}
	}
}

// Module-wide dark-screen state: backlight off, playback keeps running.
static bool module_screen_off = false;

bool ModuleCommon_isScreenOff(void) {
	return module_screen_off;
}

void ModuleCommon_setScreenOff(bool off) {
	if (module_screen_off == off)
		return;
	module_screen_off = off;
	PLAT_enableBacklight(off ? 0 : 1);
	if (!off)
		ModuleCommon_recordInputTime();
}

void ModuleCommon_recordInputTime(void) {
	last_input_time = SDL_GetTicks();
}

bool ModuleCommon_checkAutoScreenOffTimeout(void) {
	if (module_screen_off)
		return false;
	// 任何按键都算活动：子页面（设置菜单等）不跑各自的熄屏计时，
	// 返回上级页面时若停留期间已超时，这里按最后按键时间重新起算
	if (PAD_anyPressed())
		ModuleCommon_recordInputTime();
	uint32_t screen_timeout_ms = Settings_getScreenOffTimeout() * 1000;
	if (screen_timeout_ms > 0 && SDL_GetTicks() - last_input_time >= screen_timeout_ms) {
		ModuleCommon_setScreenOff(true); // 直接熄屏，无提示页
		return true;
	}
	return false;
}

DarkScreenAction ModuleCommon_handleDarkScreenInput(void) {
	// Mirrors PWR_update's power button handling, which the module skips while
	// the screen is dark (its tap would deep-sleep instead of waking up).
	static uint32_t power_pressed_at = 0;
	uint32_t now = SDL_GetTicks();

	if (PAD_justPressed(BTN_POWER))
		power_pressed_at = now;

	if (PAD_justReleased(BTN_POWEROFF) || (power_pressed_at && now - power_pressed_at >= 1000)) {
		system("gametimectl.elf stop_all");
		PWR_powerOff(0);
	}
	if (PAD_justReleased(BTN_POWER) && power_pressed_at) {
		power_pressed_at = 0;
		if (module_screen_off) {
			// 直接点亮屏幕恢复熄屏时的页面
			ModuleCommon_setScreenOff(false);
			return DARK_SCREEN_WAKE;
		}
		return DARK_SCREEN_NONE;
	}

	if (PAD_isPressed(BTN_SELECT) && PAD_isPressed(BTN_A)) {
		power_pressed_at = 0;
		ModuleCommon_setScreenOff(false);
		return DARK_SCREEN_WAKE;
	}
	if (PAD_justPressed(BTN_A))
		return DARK_SCREEN_TOGGLE;
	return DARK_SCREEN_NONE;
}

void ModuleCommon_quit(void) {
	// Ensure autosleep is re-enabled
	if (autosleep_disabled) {
		PWR_enableAutosleep();
		PWR_enableSleep();
		autosleep_disabled = false;
	}

	// Clear all GPU layers
	GFX_clearLayers(LAYER_SCROLLTEXT);
	PLAT_clearLayers(LAYER_SPECTRUM);
	PLAT_clearLayers(LAYER_PLAYTIME);
	PLAT_clearLayers(LAYER_BUFFER);
}


void ModuleCommon_PWR_update(bool* dirty, IndicatorType* show_setting) {
	// Track overlay-triggering buttons for auto-hide (check BEFORE PWR_update):
	// SELECT = brightness, START = color temp, PLUS/MINUS = volume. START must be
	// here too or the color-temp indicator never force-hides after release.
	bool overlay_buttons_active = PAD_isPressed(BTN_PLUS) || PAD_isPressed(BTN_MINUS) || PAD_isPressed(BTN_SELECT) || PAD_isPressed(BTN_START);

	if (overlay_buttons_were_active && !overlay_buttons_active) {
		// Buttons just released - start timer
		overlay_release_time = SDL_GetTicks();
	}

	// Call platform PWR_update (convert int* to expected types)
	bool dirty_bool = *dirty != 0;
	IndicatorType indicator = (IndicatorType)*show_setting;
	PWR_update(&dirty_bool, &indicator, NULL, NULL);
	*dirty = dirty_bool ? 1 : 0;
	*show_setting = (int)indicator;

	// While playback disables sleep (setAutosleepDisabled above), PWR_update
	// no longer consumes the power button (can_sleep==0): turn its short press
	// into a direct screen-off (backlight only, audio keeps playing). A >=1s
	// hold still powers off inside PWR_update.
	static uint32_t screen_power_press_at = 0;
	if (PAD_justPressed(BTN_POWER))
		screen_power_press_at = SDL_GetTicks();
	if (autosleep_disabled && PAD_justReleased(BTN_POWER) && screen_power_press_at) {
		bool short_press = SDL_GetTicks() - screen_power_press_at < 1000;
		screen_power_press_at = 0;
		if (short_press) {
			ModuleCommon_setScreenOff(true);
			*dirty = 1;
		}
	}

	// Redraw when the menu-bar wifi/bt status changes (the pages only render
	// on input, so without this the wifi icon stays stale until a keypress)
	if (UI_statusBarChanged())
		*dirty = 1;

	// After visible period, force hide overlay
	if (overlay_release_time > 0) {
		uint32_t elapsed = SDL_GetTicks() - overlay_release_time;
		if (elapsed >= OVERLAY_VISIBLE_AFTER_RELEASE_MS) {
			// Visible period passed, now force hide
			*show_setting = INDICATOR_NONE;
			*dirty = 1;
			// Stop forcing after the duration
			if (elapsed >= OVERLAY_VISIBLE_AFTER_RELEASE_MS + OVERLAY_FORCE_HIDE_DURATION_MS) {
				overlay_release_time = 0;
			}
		}
	}

	overlay_buttons_were_active = overlay_buttons_active;

	// Refresh copied daemon state. Decode, EOF, queue policy, HID, and progress
	// persistence stay in the long-lived owner.
	MusicClient_update();
}
