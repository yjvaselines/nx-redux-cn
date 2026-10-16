#ifndef SEARCH_H
#define SEARCH_H

#include "sdl.h"
#include "ui_listview.h"
#include <stdbool.h>

typedef struct {
	bool dirty;
	bool startgame;
	bool folderbgchanged;
	int screen;
} SearchResult;

void Search_init(void);
void Search_quit(void);

// Opens keyboard, performs search, returns true if user entered a query.
// Resets the return screen to SCREEN_GAMELIST (see Search_setReturnScreen).
bool Search_open(void);

// Where Search_handleInput's BACK (and a START tap on the results) returns to.
// Defaults to SCREEN_GAMELIST; a caller opening search from another screen
// (the game switcher) sets it before Search_open so search returns there.
void Search_setReturnScreen(int screen);

SearchResult Search_handleInput(unsigned long now);
void Search_render(SDL_Surface* screen, int lastScreen);

// The results-list widget — nextui.c drives its busy/marquee/tick hooks
// (UI_listViewBusy / UI_listViewMarqueeBusy / UI_listViewTickIdle) directly.
ListView* Search_view(void);

#endif // SEARCH_H
