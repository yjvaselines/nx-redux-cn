#ifndef UI_KEYBOARD_H
#define UI_KEYBOARD_H

// Kept for backward compatibility (scraper/musicplayer call it); no-op.
void UIKeyboard_init(void);

// Show the modal in-process on-screen keyboard, blocking until the user
// confirms or cancels. Returns a malloc'd string with the input, or NULL
// if cancelled or empty. Caller must free() the result.
// `prompt` is rendered as the title above the input line.
char* UIKeyboard_open(const char* prompt);

// UIKeyboard_openEx flags.
// KB_START_CANCELS: a START tap cancels like B. Opt-in for prompts that
// START itself opened (launcher Search, #112) so the same button toggles
// the screen closed again; off by default because the keyboard is shared
// by password / rename / playlist prompts where START has no meaning.
#define KB_START_CANCELS (1 << 0)
// KB_SEARCH_MODE: game-search keyboard. X is RANDOM instead of SHIFT (search
// matching is case-insensitive, so case toggling is useless there) and a
// START tap confirms the query, exactly like the Confirm key. X returns the
// UIKB_RANDOM sentinel for the caller to launch a random game.
#define KB_SEARCH_MODE (1 << 1)
char* UIKeyboard_openEx(const char* prompt, int flags);

// Sentinel returned by UIKeyboard_openEx under KB_SEARCH_MODE when the user
// pressed X (RANDOM): the caller launches a random game. Never a possible
// user input (the keyboard inserts ASCII only).
#define UIKB_RANDOM "\x01random"

// UIKeyboard_openEx with the input line pre-filled with `initial` (NULL or ""
// for none; cut at a character boundary if longer than the input limit), for
// prompts that edit an existing value such as Rename.
char* UIKeyboard_openWith(const char* prompt, const char* initial, int flags);

#endif // UI_KEYBOARD_H
