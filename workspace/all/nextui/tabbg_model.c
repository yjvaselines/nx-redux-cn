#include "tabbg_model.h"

#include <stdio.h>
#include <string.h>

// The card's user art can be any format: a same-named .webp, then .jpg, then .png.
static const char* bg_exts[] = {".webp", ".jpg", ".png"};

// Formats into out and keeps it only when it fit and the file is on the card.
static bool tryPath(char* out, size_t size, TabBgExistsFn exists_fn, const char* fmt, int len, const char* dir,
					const char* name) {
	int n = snprintf(out, size, fmt, len, dir, name);
	return n > 0 && (size_t)n < size && exists_fn(out);
}

// <dir>/.media/<stem>.webp, then .jpg, then .png.
static bool tryBgStem(char* out, size_t size, TabBgExistsFn exists_fn, int dir_len, const char* dir,
					  const char* stem, int stem_len) {
	char name[264];
	for (int i = 0; i < (int)(sizeof(bg_exts) / sizeof(bg_exts[0])); i++) {
		int n = snprintf(name, sizeof(name), "%.*s%s", stem_len, stem, bg_exts[i]);
		if (n <= 0 || n >= (int)sizeof(name))
			continue;
		if (tryPath(out, size, exists_fn, "%.*s/.media/%s", dir_len, dir, name))
			return true;
	}
	return false;
}

bool TabBg_path(TabBgKind kind, const char* entry_path, bool controller_art, TabBgExistsFn exists_fn, char* out,
				size_t size) {
	if (!entry_path || !exists_fn || !out || size == 0)
		return false;
	int len = (int)strlen(entry_path);
	while (len > 1 && entry_path[len - 1] == '/')
		len--;
	if (len == 0)
		return false;

	if (kind == TABBG_CONSOLES) {
		if (controller_art)
			return false;
		return tryBgStem(out, size, exists_fn, len, entry_path, "bg", 2);
	}

	// collection: the folder holding the .txt, and the file name without .txt
	const char* slash = NULL;
	for (int i = len - 1; i >= 0; i--) {
		if (entry_path[i] == '/') {
			slash = entry_path + i;
			break;
		}
	}
	if (!slash || slash == entry_path)
		return false;
	int dir_len = (int)(slash - entry_path);
	int name_len = len - dir_len - 1;
	if (name_len >= 4 && strncmp(slash + 1 + name_len - 4, ".txt", 4) == 0)
		name_len -= 4;
	if (name_len <= 0)
		return false;

	if (tryBgStem(out, size, exists_fn, dir_len, entry_path, slash + 1, name_len))
		return true;
	return tryBgStem(out, size, exists_fn, dir_len, entry_path, "bg", 2);
}
