#ifndef __TEXT_WRAP_H__
#define __TEXT_WRAP_H__

// Pure word-wrap / ellipsis-truncate logic behind GFX_wrapText and
// GFX_truncateText. Header-only and font-agnostic (the caller supplies a
// width-measuring callback) so it can be unit-tested on the host without SDL:
// see tests/test_text_wrap.c.

#include <stddef.h>
#include <string.h>

// Returns the rendered pixel width of the NUL-terminated UTF-8 string.
typedef int (*TextWrap_measureFn)(void* ctx, const char* utf8);

#define TEXT_WRAP_LINE_MAX 512

// Copy in_name into out_name (out_size bytes), then shorten it with a trailing
// "..." until it measures <= max_width (padding is added to every measurement).
// Returns the final width including padding.
static int TextWrap_truncate(TextWrap_measureFn measure, void* ctx, const char* in_name, char* out_name,
							 size_t out_size, int max_width, int padding) {
	int text_width;
	// Bounded copy: the caller's tail can be arbitrarily long (e.g. release
	// notes wrapped to a few lines), so never trust it to fit. Cut on a UTF-8
	// character boundary so the ellipsis pass below only ever sees whole
	// characters.
	size_t len = strlen(in_name);
	if (len >= out_size) {
		len = out_size - 1;
		while (len > 0 && (in_name[len] & 0xC0) == 0x80)
			len--;
	}
	memcpy(out_name, in_name, len);
	out_name[len] = '\0';
	text_width = measure(ctx, out_name) + padding;

	// Each pass keeps `body` bytes of the text and appends "...". The first pass makes room for the dots
	// inside the current length (body = len - 4, never growing the string); later passes drop one more
	// character. body always steps back to a code-point boundary, so a multi-byte character is removed
	// whole and no orphan UTF-8 lead byte is left before the dots.
	size_t body = len;
	int first = 1;
	while (text_width > max_width) {
		if (first) {
			if (len < 4) // can't append "..." without writing before out_name
				break;
			body = len - 4;
			first = 0;
		} else {
			if (body == 0)
				break;
			body--;
		}
		while (body > 0 && (out_name[body] & 0xC0) == 0x80)
			body--;
		memcpy(&out_name[body], "...", 4);
		text_width = measure(ctx, out_name) + padding;
	}

	return text_width;
}

// True when [s, end) holds any multi-byte UTF-8 character (CJK text, emoji...):
// such segments wrap per character instead of per word (no spaces to break on).
static int TextWrap_hasMultibyte(const char* s, const char* end) {
	for (const char* p = s; p < end; p++)
		if (((unsigned char)*p & 0xC0) == 0x80)
			return 1;
	return 0;
}

// Break a space-less segment wider than max_width (CJK text) between UTF-8
// characters, inserting '\n' (memmove, one byte per break). Returns the number
// of lines used; stops early once max_lines is reached (0 = unlimited). The
// caller's buffer must have TEXT_WRAP_LINE_MAX bytes of headroom.
static int TextWrap_breakWide(TextWrap_measureFn measure, void* ctx, char* seg, int max_width, int max_lines) {
	int lines = 1;
	char* p = seg;
	char* line_start = seg;
	while (*p && (!max_lines || lines < max_lines)) {
		char* next = p + 1;
		while ((*next & 0xC0) == 0x80)
			next++;
		char saved = *next;
		*next = '\0';
		int w = measure(ctx, line_start);
		*next = saved;
		if (w > max_width && p > line_start) {
			size_t tail = strlen(p);
			if ((size_t)(p - seg) + tail + 2 >= TEXT_WRAP_LINE_MAX)
				break; // no room to insert; the rest gets truncated instead
			memmove(p + 1, p, tail + 1);
			p[0] = '\n';
			line_start = p + 1;
			p = next + 1;
			lines++;
		} else {
			p = next;
		}
	}
	return lines;
}

// Word-wrap str in place (spaces become '\n' at line breaks) to at most
// max_lines lines (0 = unlimited); the last line is ellipsis-truncated to fit.
// Returns the widest line's width.
static int TextWrap_wrap(TextWrap_measureFn measure, void* ctx, char* str, int max_width, int max_lines) {
	if (!str)
		return 0;

	int line_width;
	int max_line_width = 0;
	char* line = str;
	char buffer[TEXT_WRAP_LINE_MAX];

	line_width = measure(ctx, line);
	if (line_width <= max_width) {
		line_width = TextWrap_truncate(measure, ctx, line, buffer, sizeof(buffer), max_width, 0);
		strcpy(line, buffer);
		return line_width;
	}

	char* prev = NULL;
	char* tmp = line;
	int lines = 1;
	while (!max_lines || lines < max_lines) {
		tmp = strchr(tmp, ' ');
		if (!tmp) {
			if (prev) {
				line_width = measure(ctx, line);
				if (line_width >= max_width) {
					if (line_width > max_line_width)
						max_line_width = line_width;
					prev[0] = '\n';
					line = prev + 1;
				}
			} else if (measure(ctx, line) > max_width && TextWrap_hasMultibyte(line, line + strlen(line))) {
				// No space at all in the remainder (pure CJK text): wrap it
				// between UTF-8 characters instead of leaving it one wide line.
				int used = TextWrap_breakWide(measure, ctx, line, max_width,
											  max_lines ? max_lines - lines + 1 : 0);
				char* last = strrchr(line, '\n');
				if (used > 1 && last) {
					line = last + 1; // only the last line gets the final truncate
					lines += used - 1;
				}
			}
			break;
		}
		tmp[0] = '\0';

		line_width = measure(ctx, line);

		if (line_width >= max_width) { // wrap
			if (line_width > max_line_width)
				max_line_width = line_width;
			if (prev) {
				// Break at the previous space: the word just measured starts the next line, and the space after it
				// is that line's first break candidate. (prev must never point at a line's first character: a
				// later break would overwrite that letter with '\n'.)
				tmp[0] = ' ';
				prev[0] = '\n';
				line = prev + 1;
				prev = tmp;
			} else if (TextWrap_hasMultibyte(line, tmp)) {
				// No space to break on and the segment holds CJK (or other multi-byte)
				// text: wrap it between UTF-8 characters instead of per word.
				tmp[0] = ' ';
				int used = TextWrap_breakWide(measure, ctx, line, max_width,
											  max_lines ? max_lines - lines + 1 : 0);
				char* last = strrchr(line, '\n');
				if (used > 1 && last) {
					line = last + 1;
					lines += used - 1;
					prev = NULL;
					tmp = line;
					continue; // the remainder keeps word-wrapping normally
				}
				// didn't break after all (measurement tie): the word gets its own line
				tmp[0] = '\n';
				line = tmp + 1;
			} else {
				// No earlier space to break at (a single word wider than the line):
				// break after it so the word becomes its own line.
				tmp[0] = '\n';
				line = tmp + 1;
			}
			tmp += 1;
			lines += 1;
		} else { // continue
			tmp[0] = ' ';
			prev = tmp;
			tmp += 1;
		}
	}

	line_width = TextWrap_truncate(measure, ctx, line, buffer, sizeof(buffer), max_width, 0);
	strcpy(line, buffer);

	if (line_width > max_line_width)
		max_line_width = line_width;
	return max_line_width;
}

#endif // __TEXT_WRAP_H__
