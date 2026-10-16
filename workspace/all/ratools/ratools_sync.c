#include "ratools_sync.h"

#include <stdio.h>
#include <string.h>

#include "api.h"
#include "config.h"
#include "defines.h"
#include "ra_offline.h"
#include "ra_offline_net.h"
#include "ratools_data.h"
#include "wifi.h"
#include "ui_buttonhintbar.h"
#include "ui_downloadprogress.h"
#include "ui_menubar.h"
#include "ui_emptystate.h"

typedef struct {
	SDL_Surface* screen;
	bool cancelled; // B pressed during either phase
} RATSyncUI;

// Same progress-bar screen "Download all game data" uses (menu bar, status
// line, bar with percentage, detail line, B CANCEL hint). Blocking flows:
// each HTTP call is one curl with a 30s cap, so the screen only updates
// between requests.
static void rat_sync_render(SDL_Surface* screen, const char* status, const char* detail,
							int done, int total) {
	GFX_clear(screen);
	UI_renderMenuBar(screen, "RetroAchievements | 立即同步");

	char detail_buf[192];
	if (total > 0)
		snprintf(detail_buf, sizeof(detail_buf), "%s (%d/%d)", detail ? detail : "", done, total);
	else
		snprintf(detail_buf, sizeof(detail_buf), "%s", detail ? detail : "");

	UIDownloadProgress info = {
		.title = NULL, // menu bar drawn above
		.status = status,
		.detail = detail_buf,
		.progress = total > 0 ? (done * 100) / total : 0,
		.show_bar = true,
	};
	UI_renderDownloadProgress(screen, &info);

	UI_renderButtonHintBar(screen, (char*[]){"B", "取消", NULL});
	GFX_flip(screen);
}

// phase 1 progress: journal entries submitted so far
static void rat_sync_progress(int done, int total, void* userdata) {
	RATSyncUI* ui = (RATSyncUI*)userdata;
	rat_sync_render(ui->screen, "正在同步离线解锁", "提交中", done, total);
}

// phase 2 progress: cloud state checked console-by-console
static void rat_sync_refresh_progress(int done, int total, const char* label,
									  void* userdata) {
	RATSyncUI* ui = (RATSyncUI*)userdata;
	rat_sync_render(ui->screen, "正在刷新云端状态", label, done, total);
}

// B cancels between requests: unsent journal entries stay for the next sync,
// already-refreshed games are kept.
static int rat_sync_cancel(void* userdata) {
	RATSyncUI* ui = (RATSyncUI*)userdata;
	PAD_poll();
	if (PAD_justPressed(BTN_B))
		ui->cancelled = true;
	return ui->cancelled ? 1 : 0;
}

// message modal using the shared empty-state component (icon + message +
// subtitle + centered B/BACK button, same as the music player library)
static void rat_sync_message_screen(SDL_Surface* screen, const char* line1, const char* line2) {
	bool quit = false, dirty = true;
	while (!quit) {
		GFX_startFrame();
		PAD_poll();
		if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B))
			quit = true;
		if (dirty) {
			GFX_clear(screen);
			UI_renderEmptyState(screen, line1, line2, NULL);
			GFX_flip(screen);
			dirty = false;
		} else {
			GFX_sync();
		}
	}
}

void RATSync_run(SDL_Surface* screen) {
	// Preconditions (same messages as before). Note: no early "Nothing to
	// sync" return — even with an empty journal we still pull fresh cloud
	// state so unlocks earned in standalone emulators (DC.pak flycast) and
	// the header's points total stop going stale.
	if (!CFG_getRAAuthenticated() || strlen(CFG_getRAToken()) == 0) {
		rat_sync_message_screen(screen, "未登录",
								"请先在设置中填写凭据并完成认证。");
		return;
	}
	if (!Wifi_isConnected()) {
		rat_sync_message_screen(screen, "无网络连接",
								"请连接 WiFi 后重试。");
		return;
	}

	const char* username = CFG_getRAUsername();
	const char* token = CFG_getRAToken();
	RATSyncUI ui = {screen, false};

	// Phase 1: submit the offline unlock journal, if any.
	int pending = RA_Offline_pendingCount();
	int synced = 0;
	bool sync_failed = false;
	if (pending > 0) {
		rat_sync_progress(0, pending, &ui);
		synced = RA_OfflineNet_syncAllEx(username, token, rat_sync_progress,
										 rat_sync_cancel, &ui);
		sync_failed = (synced < 0);
	}

	// Phase 2: pull fresh cloud state (points + per-game unlock counts).
	// Skipped when B already cancelled phase 1.
	RA_RefreshStats stats = {0};
	int refresh_rc = -1;
	if (!ui.cancelled) {
		rat_sync_refresh_progress(0, 0, "正在检查云端状态", &ui);
		refresh_rc = RA_OfflineNet_refreshCloud(username, token, rat_sync_refresh_progress,
												rat_sync_cancel, &ui, &stats);
	}

	// Result screen.
	char line1[64], line2[160];

	if (ui.cancelled) {
		int remaining = RA_Offline_pendingCount();
		snprintf(line1, sizeof(line1), "已取消");
		if (remaining > 0)
			snprintf(line2, sizeof(line2), "%d 个解锁将留待下次同步。",
					 remaining);
		else
			snprintf(line2, sizeof(line2), "停止前已刷新 %d/%d 个游戏。",
					 stats.games_updated, stats.games_checked);
		rat_sync_message_screen(screen, line1, line2);
		return;
	}

	if (sync_failed) {
		// journal submission is the primary action when unlocks were waiting
		snprintf(line1, sizeof(line1), "同步失败");
		snprintf(line2, sizeof(line2), "请检查凭据与网络连接。");
		rat_sync_message_screen(screen, line1, line2);
		return;
	}

	int remaining = RA_Offline_pendingCount();

	if (synced > 0)
		snprintf(line1, sizeof(line1), "已同步 %d 个解锁", synced);
	else if (refresh_rc == 0)
		snprintf(line1, sizeof(line1), stats.games_updated > 0 ? "已刷新" : "已是最新");
	else
		snprintf(line1, sizeof(line1), "同步失败");

	if (refresh_rc != 0) {
		// cloud refresh (login2) failed
		if (synced > 0)
			snprintf(line2, sizeof(line2), "解锁已提交，但云端刷新失败。");
		else
			snprintf(line2, sizeof(line2), "无法刷新云端状态，请重试。");
	} else if (remaining > 0) {
		snprintf(line2, sizeof(line2), "%d 个解锁将留待下次同步。",
				 remaining);
	} else {
		uint32_t score = 0, soft = 0;
		char pts[40];
		if (RAT_getCachedScore(&score, &soft))
			snprintf(pts, sizeof(pts), "%u 点", soft);
		else
			snprintf(pts, sizeof(pts), "云端状态已是最新");
		if (stats.games_checked > 0)
			snprintf(line2, sizeof(line2), "%s，已更新 %d/%d 个游戏",
					 pts, stats.games_updated, stats.games_checked);
		else
			snprintf(line2, sizeof(line2), "%s", pts);
		if (stats.consoles_failed > 0) {
			size_t used = strlen(line2);
			snprintf(line2 + used, sizeof(line2) - used, "，%d 个主机未检查",
					 stats.consoles_failed);
		}
	}
	rat_sync_message_screen(screen, line1, line2);
}
