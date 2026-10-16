#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <msettings.h>

#include "defines.h"
#include "api.h"
#include "config.h"
#include "settings_menu.h"
#include "ui_list.h"
#include "ui_listview.h"
#include "ui_splash.h"
#include "ui_quitrequest.h"
#include "ui_confirmdialog.h"
#include "ui_menubar.h"
#include "ui_buttonhintbar.h"
#include "ui_font.h"
#include "display_helper.h"
#include "ra_auth.h"
#include "ra_offline.h"
#include "ra_offline_net.h"
#include "ra_hash_cache.h"
#include "ra_paths.h"

#include "ratools_browser.h"
#include "ratools_data.h"
#include "ratools_prefetch.h"
#include "ratools_reset.h"
#include "ratools_sync.h"

SDL_Surface* g_screen = NULL;

// ============================================
// Pages
// ============================================
static SettingsPage ra_settings_page;
static SettingItem ra_settings_items[16];

static void init_page(SettingsPage* page, const char* title, SettingItem* items,
					  int count, int is_list) {
	memset(page, 0, sizeof(SettingsPage));
	page->title = title;
	page->items = items;
	page->item_count = count;
	page->selected = 0;
	page->scroll = 0;
	page->is_list = is_list;
	page->dynamic_start = -1;
	page->max_items = count;
}

// ============================================
// Value labels (copied from settings.c)
// ============================================
static const char* on_off_labels[] = {"关", "开"};
static int on_off_values[] = {0, 1};
static const char* notify_duration_labels[] = {"1s", "2s", "3s", "4s", "5s"};
static int notify_duration_values[] = {1, 2, 3, 4, 5};
#define NOTIFY_DURATION_COUNT 5
static const char* progress_duration_labels[] = {"关", "1s", "2s", "3s", "4s", "5s"};
static int progress_duration_values[] = {0, 1, 2, 3, 4, 5};
#define PROGRESS_DURATION_COUNT 6

/* RA sort order */
static int ra_sort_values[] = {
	RA_SORT_UNLOCKED_FIRST,
	RA_SORT_DISPLAY_ORDER_FIRST,
	RA_SORT_DISPLAY_ORDER_LAST,
	RA_SORT_WON_BY_MOST,
	RA_SORT_WON_BY_LEAST,
	RA_SORT_POINTS_MOST,
	RA_SORT_POINTS_LEAST,
	RA_SORT_TITLE_AZ,
	RA_SORT_TITLE_ZA,
	RA_SORT_TYPE_ASC,
	RA_SORT_TYPE_DESC};
static const char* ra_sort_labels[] = {
	"最早解锁",
	"列表顺序（正序）",
	"列表顺序（倒序）",
	"解锁人数（多到少）",
	"解锁人数（少到多）",
	"点数（多到少）",
	"点数（少到多）",
	"标题（A-Z）",
	"标题（Z-A）",
	"类型（正序）",
	"类型（倒序）"};
#define RA_SORT_LABEL_COUNT 11

// ============================================
// Status display helpers (reused by the custom main-screen header and,
// where noted, by the settings page)
// ============================================
static const char* get_user_display(void) {
	const char* u = CFG_getRAUsername();
	return (u && u[0]) ? u : "（未设置）";
}

static char pending_buf[64];
static const char* get_pending_display(void) {
	int n = RA_Offline_pendingCount();
	snprintf(pending_buf, sizeof(pending_buf), "%d 个解锁待同步", n);
	return pending_buf;
}

static char lastsync_buf[64];
static const char* get_lastsync_display(void) {
	time_t t = RA_Offline_lastSyncTime();
	if (t == 0)
		return "从未";
	struct tm tm;
	localtime_r(&t, &tm);
	strftime(lastsync_buf, sizeof(lastsync_buf), "%Y-%m-%d %H:%M", &tm);
	return lastsync_buf;
}

static void on_prefetch(void) {
	RATPrefetch_run(g_screen);
}

// Blocking confirm dialog: A confirms, B cancels. Returns true on confirm.
static bool ra_confirm(const char* title, const char* subtitle) {
	bool dirty = true;
	while (1) {
		GFX_startFrame();
		PAD_poll();
		if (PAD_justPressed(BTN_A)) {
			PAD_reset();
			return true;
		}
		if (PAD_justPressed(BTN_B)) {
			PAD_reset();
			return false;
		}
		if (dirty) {
			GFX_clear(g_screen);
			UI_renderConfirmDialog(g_screen, title, subtitle);
			GFX_flip(g_screen);
			dirty = false;
		} else {
			GFX_sync();
		}
	}
}

static void on_reset_account(void) {
	if (ra_confirm("重置账号数据？",
				   "清除登录、解锁进度和待同步项。游戏数据与徽章会保留。")) {
		RATReset_clearAccountData();
	}
}

static void on_reset_all(void) {
	if (ra_confirm("清除全部成就数据？",
				   "移除包括缓存游戏与徽章在内的全部数据。下次联网游玩时重新下载。")) {
		RATReset_clearAll();
	}
}

// ============================================
// RA settings callbacks (moved from settings.c)
// ============================================
static int get_ra_enable(void) {
	return CFG_getRAEnable() ? 1 : 0;
}
static void set_ra_enable(int v) {
	CFG_setRAEnable(v != 0);
}
static void reset_ra_enable(void) {
	CFG_setRAEnable(CFG_DEFAULT_RA_ENABLE);
}

// Hardcore mode is deliberately not offered: nx-redux is not an
// RA-approved hardcore-compliant emulator, so hardcore unlocks are
// softcore-locked server-side and enabling it only risks the account.

static const char* get_ra_username_display(void) {
	// identical to get_user_display() (defined above); kept as a named
	// function so it can be used as an ITEM_TEXT_INPUT_INIT getter pointer
	return get_user_display();
}
static void on_ra_username_set(const char* text) {
	if (text)
		CFG_setRAUsername(text);
}

static const char* get_ra_password_display(void) {
	const char* p = CFG_getRAPassword();
	return (p && p[0]) ? "********" : "(not set)";
}
static void on_ra_password_set(const char* text) {
	if (text)
		CFG_setRAPassword(text);
}

static char ra_auth_status_msg[256] = "";

static void on_ra_authenticate(void) {
	const char* username = CFG_getRAUsername();
	const char* password = CFG_getRAPassword();

	if (!username || !username[0] || !password || !password[0]) {
		snprintf(ra_auth_status_msg, sizeof(ra_auth_status_msg),
				 "错误：需要用户名和密码");
		return;
	}

	RA_AuthResponse response;
	RA_AuthResult result = RA_authenticateSync(username, password, &response);

	if (result == RA_AUTH_SUCCESS) {
		CFG_setRAToken(response.token);
		CFG_setRAAuthenticated(true);
		// minarch only starts an offline session from a cached "login2"
		// response, which it otherwise writes on its own first online login.
		// Write it here too so "log in, prefetch, go offline" works without
		// ever playing a game online (issue #105).
		if (RA_OfflineNet_cacheLogin(username, response.token) != 0)
			LOG_warn("ratools: authenticated but could not write offline login cache\n");
		snprintf(ra_auth_status_msg, sizeof(ra_auth_status_msg),
				 "已认证：%s", response.display_name);
	} else {
		CFG_setRAToken("");
		CFG_setRAAuthenticated(false);
		snprintf(ra_auth_status_msg, sizeof(ra_auth_status_msg),
				 "错误：%s", response.error_message);
	}
}

static const char* get_ra_status(void) {
	if (ra_auth_status_msg[0])
		return ra_auth_status_msg;
	if (CFG_getRAAuthenticated() && strlen(CFG_getRAToken()) > 0)
		return "已认证";
	return "未登录";
}

static int get_ra_show_notifications(void) {
	return CFG_getRAShowNotifications() ? 1 : 0;
}
static void set_ra_show_notifications(int v) {
	CFG_setRAShowNotifications(v != 0);
}
static void reset_ra_show_notifications(void) {
	CFG_setRAShowNotifications(CFG_DEFAULT_RA_SHOW_NOTIFICATIONS);
}

static int get_ra_notify_duration(void) {
	return CFG_getRANotificationDuration();
}
static void set_ra_notify_duration(int val) {
	CFG_setRANotificationDuration(val);
}
static void reset_ra_notify_duration(void) {
	CFG_setRANotificationDuration(CFG_DEFAULT_RA_NOTIFICATION_DURATION);
}

static int get_ra_progress_duration(void) {
	return CFG_getRAProgressNotificationDuration();
}
static void set_ra_progress_duration(int val) {
	CFG_setRAProgressNotificationDuration(val);
}
static void reset_ra_progress_duration(void) {
	CFG_setRAProgressNotificationDuration(CFG_DEFAULT_RA_PROGRESS_NOTIFICATION_DURATION);
}

static int get_ra_sort_order(void) {
	return CFG_getRAAchievementSortOrder();
}
static void set_ra_sort_order(int val) {
	CFG_setRAAchievementSortOrder(val);
}
static void reset_ra_sort_order(void) {
	CFG_setRAAchievementSortOrder(CFG_DEFAULT_RA_ACHIEVEMENT_SORT_ORDER);
}

static void reset_ra_page(void) {
	reset_ra_enable();
	reset_ra_show_notifications();
	reset_ra_notify_duration();
	reset_ra_progress_duration();
	reset_ra_sort_order();
	for (int i = 0; i < ra_settings_page.item_count; i++)
		settings_item_sync(&ra_settings_page.items[i]);
}

// ============================================
// Menu construction (Settings sub-page only — the main page is
// custom-rendered, see render_main_screen())
// ============================================
static void build_menu_tree(void) {
	int idx = 0;
	ra_settings_items[idx++] = (SettingItem)ITEM_CYCLE_INIT(
		"启用成就", "启用 RetroAchievements 集成",
		on_off_labels, 2, on_off_values, get_ra_enable, set_ra_enable, reset_ra_enable);
	ra_settings_items[idx++] = (SettingItem)ITEM_TEXT_INPUT_INIT(
		"用户名", "RetroAchievements 用户名",
		get_ra_username_display, on_ra_username_set);
	ra_settings_items[idx++] = (SettingItem)ITEM_TEXT_INPUT_INIT(
		"密码", "RetroAchievements 密码",
		get_ra_password_display, on_ra_password_set);
	ra_settings_items[idx++] = (SettingItem)ITEM_BUTTON_INIT(
		"认证", "校验凭据并获取 API 令牌",
		on_ra_authenticate);
	ra_settings_items[idx++] = (SettingItem)ITEM_STATIC_INIT(
		"状态", "认证状态",
		get_ra_status);
	ra_settings_items[idx++] = (SettingItem)ITEM_BUTTON_INIT(
		"下载全部游戏数据", "为库中的每个游戏缓存成就数据",
		on_prefetch);
	ra_settings_items[idx++] = (SettingItem)ITEM_CYCLE_INIT(
		"显示通知", "显示成就解锁通知",
		on_off_labels, 2, on_off_values, get_ra_show_notifications, set_ra_show_notifications, reset_ra_show_notifications);
	ra_settings_items[idx++] = (SettingItem)ITEM_CYCLE_INIT(
		"通知时长", "成就通知在屏幕上的停留时间",
		notify_duration_labels, NOTIFY_DURATION_COUNT, notify_duration_values, get_ra_notify_duration, set_ra_notify_duration, reset_ra_notify_duration);
	ra_settings_items[idx++] = (SettingItem)ITEM_CYCLE_INIT(
		"进度时长", "进度提示（左上角）的显示时长。选关则关闭。",
		progress_duration_labels, PROGRESS_DURATION_COUNT, progress_duration_values, get_ra_progress_duration, set_ra_progress_duration, reset_ra_progress_duration);
	ra_settings_items[idx++] = (SettingItem)ITEM_CYCLE_INIT(
		"成就排序方式", "游戏内菜单中成就的排序方式",
		ra_sort_labels, RA_SORT_LABEL_COUNT, ra_sort_values, get_ra_sort_order, set_ra_sort_order, reset_ra_sort_order);
	ra_settings_items[idx++] = (SettingItem)ITEM_BUTTON_INIT(
		"重置账号数据", "清除登录与进度，保留游戏数据。",
		on_reset_account);
	ra_settings_items[idx++] = (SettingItem)ITEM_BUTTON_INIT(
		"清除全部成就数据", "移除全部数据，包括游戏与徽章。",
		on_reset_all);
	ra_settings_items[idx++] = (SettingItem)ITEM_BUTTON_INIT(
		"恢复默认设置", "将以上选项恢复为默认值。",
		reset_ra_page);
	init_page(&ra_settings_page, "RetroAchievements | 设置", ra_settings_items, idx, 0);

	for (int i = 0; i < ra_settings_page.item_count; i++)
		settings_item_sync(&ra_settings_page.items[i]);
}

// ============================================
// Custom main screen
// ============================================
#define MAIN_ITEM_COUNT 3

static const char* main_item_labels[MAIN_ITEM_COUNT] = {
	"成就", "立即同步", "设置"};

static ListView main_view;

static void main_list_get_row(void* ctx, int i, bool selected, ListViewRow* out) {
	(void)ctx;
	(void)selected;
	out->label = main_item_labels[i];
}

static const SDL_Color col_header_title = {255, 255, 255, 255};
static const SDL_Color col_header_gray = {180, 180, 180, 255};
static const SDL_Color col_header_pending = {255, 200, 80, 255};

static bool ra_is_authenticated(void) {
	return CFG_getRAAuthenticated() && *CFG_getRAToken();
}

// One header line at x, y in the font just fetched (UIFont pointers are only valid until the next
// UIFont_get). Returns the line's height.
static int header_line(SDL_Surface* screen, TTF_Font* f, const char* text, SDL_Color color, int x, int y) {
	if (!f)
		return 0;
	if (text && text[0]) {
		SDL_Surface* s = GFX_renderText(f, text, color);
		if (s) {
			SDL_BlitSurface(s, NULL, screen, &(SDL_Rect){x, y});
			SDL_FreeSurface(s);
		}
	}
	return TTF_FontHeight(f);
}

// Non-selectable status header below the menu bar (LIST-LAYOUT §10.3): the headline (username) bold at
// 1.15 x the list label, then the grey detail lines at 0.8 x (secondary), 2 dp apart, starting 8 dp under the
// title strip. Recomputed every call so it always reflects the latest auth/sync/pending state. Returns the y
// (screen px) where the list's area starts; the list's block (rows + arrow strips) centres itself below it.
static int render_main_header(SDL_Surface* screen) {
	int x = UI_listTextX(); // under the page title and the rows' text (the 14 dp inset)
	int y = UI_menuBarHeight() + NX_DP(8);
	int gap = NX_DP(2);
	bool auth = ra_is_authenticated();

	// Line 1: username, or "未登录"
	const char* line1 = auth ? get_user_display() : "未登录";
	y += header_line(screen, UI_textRole(UI_TEXT_HEADLINE, true), line1, col_header_title, x, y);

	// Line 2: cached score, prompt to authenticate, or omitted (no cached
	// login yet). Slot height is reserved either way so line 3 never shifts.
	char line2_buf[64];
	const char* line2 = NULL;
	if (!auth) {
		line2 = "请在设置中填写凭据";
	} else {
		uint32_t score = 0, soft = 0;
		if (RAT_getCachedScore(&score, &soft)) {
			// nx-redux is softcore-only (hardcore removed), so the hardcore
			// total would read 0 forever - show the real (softcore) points
			snprintf(line2_buf, sizeof(line2_buf), "%u 点", soft);
			line2 = line2_buf;
		}
	}
	y += gap;
	y += header_line(screen, UI_textRole(UI_TEXT_SECONDARY, false), line2, col_header_gray, x, y);

	// Line 3: pending unlocks, highlighted yellow when unlocks are waiting.
	int n = RA_Offline_pendingCount();
	y += gap;
	y += header_line(screen, UI_textRole(UI_TEXT_SECONDARY, false), get_pending_display(),
					 n > 0 ? col_header_pending : col_header_gray, x, y);

	// Line 4: last sync time.
	char line4_buf[96];
	snprintf(line4_buf, sizeof(line4_buf), "上次同步: %s", get_lastsync_display());
	y += gap;
	y += header_line(screen, UI_textRole(UI_TEXT_SECONDARY, false), line4_buf, col_header_gray, x, y);

	return y;
}

// Selectable list of the 3 main actions, drawn by the shared ListView widget
// with its origin pushed below the custom status header.
static void render_main_screen(SDL_Surface* screen) {
	GFX_clear(screen);
	UI_renderMenuBar(screen, "RetroAchievements");

	int list_top = render_main_header(screen);
	main_view.title = NULL; // we drew our own chrome above
	main_view.font = font.large;
	main_view.count = MAIN_ITEM_COUNT;
	main_view.get_row = main_list_get_row;
	main_view.list_id = (const void*)main_item_labels;
	main_view.list_y_override = list_top;
	main_view.hint_pairs = (char*[]){"B", "退出", "A", "打开", NULL};
	UI_listViewRender(&main_view, screen);
}

// Run the settings_menu framework for ra_settings_page only, then return to
// the custom main screen (does NOT exit the app). settings_menu_handle_input
// pops the page and reports *quit=true once stack depth reaches 0 — since
// ra_settings_page has no submenu items, a single B press at depth 1 always
// triggers exactly that. We feed it a *local* quit flag so that report only
// breaks this sub-loop instead of the whole app.
static void run_settings_menu(SDL_Surface** screen_ptr) {
	settings_menu_push(&ra_settings_page);

	bool sub_quit = false;
	bool dirty = true;
	IndicatorType show_setting = INDICATOR_NONE;

	while (!sub_quit) {
		GFX_startFrame();
		PAD_poll();

		settings_menu_handle_input(&sub_quit, &dirty);

		if (settings_menu_glide_active())
			dirty = true;

		// TG5050: keyboard may have triggered display recovery
		{
			SDL_Surface* ns = DisplayHelper_getReinitScreen();
			if (ns) {
				*screen_ptr = ns;
				g_screen = ns;
				dirty = true;
			}
		}

		PWR_update(&dirty, &show_setting, NULL, NULL);

		if (UI_statusBarChanged())
			dirty = true;

		if (dirty) {
			GFX_clear(*screen_ptr);
			settings_menu_render(*screen_ptr, show_setting);
			GFX_flip(*screen_ptr);
			dirty = false;
		} else {
			GFX_sync();
		}
	}
}

// ============================================
// Main
// ============================================
int main(int argc, char* argv[]) {
	(void)argc;
	(void)argv;
	SDL_Surface* screen = GFX_init(MODE_MAIN);
	g_screen = screen;
	UI_showSplashScreen(screen, "RetroAchievements");

	InitSettings();
	PWR_init();
	PAD_init();

	RA_Offline_init(RA_USER_DIR, SDCARD_PATH);
	RA_HashCache_init(ROMS_PATH);

	build_menu_tree();
	settings_menu_init();

	bool quit = false;
	bool dirty = true;
	IndicatorType show_setting = INDICATOR_NONE;

	UI_listViewReset(&main_view, MAIN_ITEM_COUNT, main_item_labels);

	while (!quit) {
		GFX_startFrame();
		PAD_poll();

		UI_handleQuitRequest(screen, &quit, &dirty, "退出 RetroAchievements？",
						 "设置会自动保存");

		if (!quit) {
			ListViewAction act = UI_listViewHandleInput(&main_view);
			if (act.type == LISTVIEW_BACK) {
				quit = true;
			} else if (act.type == LISTVIEW_ACTIVATED) {
				switch (act.index) {
				case 0:
					RATBrowser_run(g_screen);
					break;
				case 1:
					RATSync_run(g_screen);
					break;
				case 2:
					run_settings_menu(&screen);
					break;
				}
				// These are blocking sub-screens with their own input loops;
				// the B (or A) press that closed them is still latched in
				// the global pad state (no PAD_poll happened since). Clear
				// it so it isn't misread this frame — same convention as
				// settings.c/settings_wifi.c after modal transitions.
				PAD_reset();
				dirty = true;
			}

			if (UI_listViewBusy(&main_view))
				dirty = true;
		}

		// TG5050: keyboard may have triggered display recovery
		{
			SDL_Surface* ns = DisplayHelper_getReinitScreen();
			if (ns) {
				screen = ns;
				g_screen = ns;
				dirty = true;
			}
		}

		PWR_update(&dirty, &show_setting, NULL, NULL);

		if (UI_statusBarChanged())
			dirty = true;

		if (dirty) {
			render_main_screen(screen);
			GFX_flip(screen);
			dirty = false;
		} else {
			UI_listViewTickIdle(&main_view);
			GFX_sync();
		}
	}

	GFX_clear(screen);
	GFX_flip(screen);

	QuitSettings();
	PWR_quit();
	PAD_quit();

	GFX_quit();

	return EXIT_SUCCESS;
}
