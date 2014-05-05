// 4 may 2014
#define _UNICODE
#define UNICODE
#define STRICT
#define STRICT_TYPED_ITEMIDS
#define _GNU_SOURCE		// needed to declare asprintf()/vasprintf()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <windows.h>
#include <commctrl.h>		// needed for InitCommonControlsEx() (thanks Xeek in irc.freenode.net/#winapi for confirming)
#include <shlobj.h>
#include <shlwapi.h>

#define PROGNAME (L"Windows Icon Viewer")

// winiconview.c
extern HINSTANCE hInstance;
extern int nCmdShow;

void setControlFont(HWND);

// mainwin.c
void registerMainWindowClass(void);
HWND makeMainWindow(TCHAR *);

// listview.c
void makeListView(HWND, HMENU);
void resizeListView(HWND);
LRESULT handleListViewRightClick(NMHDR *);

// util.c
void panic(char *fmt, ...);
TCHAR *toWideString(char *what);
void ourWow64DisableWow64FsRedirection(PVOID *);
void ourWow64RevertWow64FsRedirection(PVOID);

// geticons.c
enum {
	msgBegin = WM_APP + 1,
	msgStep,
	msgEnd,
};

struct giThreadData {
	HWND mainwin;
	TCHAR *dirname;
};

extern HIMAGELIST largeicons, smallicons;
extern LVGROUP *groups;
extern LVITEM *items;
extern size_t nGroups, nItems;

DWORD WINAPI getIcons(LPVOID);
INT CALLBACK groupLess(INT gn1, INT gn2, VOID *data);
