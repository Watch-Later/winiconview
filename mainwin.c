// 5 may 2014
#include "winiconview.h"

static HICON hDefaultIcon;
static HCURSOR arrowCursor, waitCursor;

#define ID_LISTVIEW 100

struct mainwinData {
	HCURSOR currentCursor;
	HWND label;
	HWND progressbar;
	HWND listview;
	TCHAR *dirname;
	RECT defaultWindowRect;
	TCHAR *labelText;
};

static void properlyLayOutProgressWindow(HWND hwnd, struct mainwinData *data)
{
	HDC dc;
	HFONT prevfont;
	TEXTMETRIC tm;
	SIZE extents;
	int baseX, baseY;

	dc = GetWindowDC(hwnd);
	if (dc == NULL)
		panic("error getting window DC for laying out progress controls");
	prevfont = selectControlFont(dc);
	if (GetTextMetrics(dc, &tm) == 0)
		panic("error getting text metrics from DC for laying out progress controls");
	baseY = tm.tmHeight;
	// via http://support.microsoft.com/kb/125681
	if (GetTextExtentPoint32(dc, L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
		52, &extents) == 0)
		panic("error getting baseY value from DC for laying out progress controls");
	baseX = (extents.cx / 26 + 1) / 2;
	// don't close the DC yet; we still need it

	int startx = 20;
	int starty = 20;
	int endx = startx;
	int endy = starty;
	int width = 0;
	int cx = startx, cy = starty;
	int cwid, cht;

	// via http://msdn.microsoft.com/en-us/library/windows/desktop/aa511279.aspx
	// TODO follow spacing rules here too
	enum {
		labelHeight = 8,
		pbarWidth = 237,
		pbarHeight = 8,
	};

	// from http://msdn.microsoft.com/en-us/library/windows/desktop/ms645502%28v=vs.85%29.aspx
	// we shouldn't need to worry about overflow...
#define FROMDLGUNITSX(x) MulDiv((x), baseX, 4)
#define FROMDLGUNITSY(y) MulDiv((y), baseY, 8)

	// first, the "please wait" label
	if (GetTextExtentPoint32(dc, data->labelText, wcslen(data->labelText), &extents) == 0)
		panic("error getting width of \"please wait\" label for laying out progress controls");
	cwid = extents.cx;
	cht = FROMDLGUNITSY(labelHeight);
	if (MoveWindow(data->label, cx, cy, cwid, cht, TRUE) == 0)
		panic("error laying out \"please wait\" label for laying out progress controls");
	cy += cht;

	if (width < (cx + cwid))		// window width
		width = (cx + cwid);

	cy += 10;

	// now the progressbar
	cwid = FROMDLGUNITSX(pbarWidth);
	cht = FROMDLGUNITSY(pbarHeight);
	if (MoveWindow(data->progressbar, cx, cy, cwid, cht, TRUE) == 0)
		panic("error laying out progressbar for laying out progress controls");
	cy += cht;

	if (width < (cx + cwid))		// window width
		width = (cx + cwid);
	width += 20;	// end of width

	// and now for the window
	// we use the same x and y position
	// the client width is width; the client height is cy + final padding
	cy += 20;		// final padding

	RECT client;

	client.left = 0;
	client.top = 0;
	client.right = width;
	client.bottom = cy;
	if (AdjustWindowRectEx(&client,
		GetWindowLongPtr(hwnd, GWL_STYLE),
		FALSE,			// no menu
		GetWindowLongPtr(hwnd, GWL_EXSTYLE)) == 0)
		panic("error getting progress window size");
	if (MoveWindow(hwnd, data->defaultWindowRect.left, data->defaultWindowRect.top,
		client.right - client.left, client.bottom - client.top, TRUE) == 0)
		panic("error resizing the window to the progress window size");

	if (SelectObject(dc, prevfont) == NULL)
		panic("error restoring previous DC font for laying out progress controls");
	if (ReleaseDC(hwnd, dc) == 0)
		panic("error releasing window DC for laying out progress controls");
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct mainwinData *data;
	NMHDR *nmhdr;
	CREATESTRUCT *cs;
	struct giThreadData *threadData;

	// we can assume the GetWindowLongPtr()/SetWindowLongPtr() calls will work; see comments of http://blogs.msdn.com/b/oldnewthing/archive/2014/02/03/10496248.aspx
	data = (struct mainwinData *) GetWindowLongPtr(hwnd, GWL_USERDATA);
	if (data == NULL) {
		data = (struct mainwinData *) malloc(sizeof (struct mainwinData));
		if (data == NULL)
			panic("error allocating main window data structure");
		ZeroMemory(data, sizeof (struct mainwinData));
		data->currentCursor = arrowCursor;
		SetWindowLongPtr(hwnd, GWL_USERDATA, (LONG_PTR) data);
	}

	switch (msg) {
	case WM_NCCREATE:
		cs = (CREATESTRUCT *) lparam;
		data->dirname = cs->lpCreateParams;
		threadData = (struct giThreadData *) malloc(sizeof (struct giThreadData));
		if (threadData == NULL)
			panic("error allocating getIcons() thread data structure for \"%S\"", data->dirname);
		threadData->mainwin = hwnd;
		threadData->dirname = data->dirname;
		if (CreateThread(NULL, 0, getIcons, threadData, 0, NULL) == NULL)
			panic("error creating worker thread to get icons");
		// TODO free threadData in the thread
		// let's defer this to DefWindowProc(); just to be safe, instead of just returning TRUE (TODO)
		// I THINK that's what the oldnewthing link above does anyway
		return DefWindowProc(hwnd, msg, wparam, lparam);
	case msgBegin:
		if (GetWindowRect(hwnd, &data->defaultWindowRect) == 0)
			panic("error saving default window rect");
		data->currentCursor = waitCursor;
		data->labelText = L"Gathering icons. Please wait.";
		data->label = CreateWindowEx(0,
			L"STATIC", data->labelText,
			SS_NOPREFIX | SS_LEFTNOWORDWRAP | WS_CHILD | WS_VISIBLE,
			20, 20, 200, 20,
			hwnd, NULL, hInstance, NULL);
		if (data->label == NULL)
			panic("error making \"please wait\" label");
		setControlFont(data->label);
		data->progressbar = CreateWindowEx(0,
			PROGRESS_CLASS, L"",
			PBS_SMOOTH | WS_CHILD | WS_VISIBLE,
			20, 45, 200, 40,
			hwnd, NULL, hInstance, NULL);
		if (data->progressbar == NULL)
			panic("error making progressbar");
		SendMessage(data->progressbar, PBM_SETRANGE32,
			0, lparam);
		SendMessage(data->progressbar, PBM_SETSTEP, 1, 0);
		properlyLayOutProgressWindow(hwnd, data);
		// and now that everything's ready we can FINALLY show the main window
		ShowWindow(hwnd, nCmdShow);
		if (UpdateWindow(hwnd) == 0)
			panic("UpdateWindow() failed in first show");
		return 0;
	case msgStep:
		SendMessage(data->progressbar, PBM_STEPIT, 0, 0);
		return 0;
	case msgEnd: for(;;);
		// kill redraw because this is a heavy operation
		SendMessage(hwnd, WM_SETREDRAW, (WPARAM) FALSE, 0);
		if (DestroyWindow(data->label) == 0)
			panic("error removing \"please wait\" label");
		if (DestroyWindow(data->progressbar) == 0)
			panic("error removing progressbar");
		makeListView(hwnd, (HMENU) ID_LISTVIEW);
		data->currentCursor = arrowCursor;		// TODO move to end and make atomic
		if (MoveWindow(hwnd, data->defaultWindowRect.left, data->defaultWindowRect.top,
			data->defaultWindowRect.right - data->defaultWindowRect.left,
			data->defaultWindowRect.bottom - data->defaultWindowRect.top,
			TRUE) == 0)
			panic("error restoring the original window rect");
		resizeListView(hwnd);
		SendMessage(hwnd, WM_SETREDRAW, (WPARAM) TRUE, 0);
		RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);		// MSDN says to
		// TODO set focus on the listview so I can use the scroll wheel
		// while I'm here, TODO figure out why the icons now have a black border around them
		return 0;
	case WM_SETCURSOR:
		SetCursor(data->currentCursor);
		return TRUE;
	case WM_NOTIFY:
		nmhdr = (NMHDR *) lparam;
		if (nmhdr->idFrom == ID_LISTVIEW)
			return handleListViewRightClick(nmhdr);
		return 0;
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		resizeListView(hwnd);
		return 0;
	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
	panic("oops: message %ud does not return anything; bug in wndproc()", msg);
}

void registerMainWindowClass(void)
{
	WNDCLASS cls;

	hDefaultIcon = LoadIcon(NULL, MAKEINTRESOURCE(IDI_APPLICATION));
	if (hDefaultIcon == NULL)
		panic("error getting default window class icon");
	arrowCursor = LoadCursor(NULL, IDC_ARROW);
	if (arrowCursor == NULL)
		panic("error getting arrow cursor");
	waitCursor = LoadCursor(NULL, IDC_WAIT);
	if (waitCursor == NULL)
		panic("error getting wait cursor");
	ZeroMemory(&cls, sizeof (WNDCLASS));
	cls.lpszClassName = L"mainwin";
	cls.lpfnWndProc = wndproc;
	cls.hInstance = hInstance;
	cls.hIcon = hDefaultIcon;
	cls.hCursor = arrowCursor;
	cls.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
	if (RegisterClass(&cls) == 0)
		panic("error registering window class");
}

HWND makeMainWindow(TCHAR *dirname)
{
	HWND mainwin;

	mainwin = CreateWindowEx(0,
		L"mainwin", L"Main Window",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hInstance, dirname);
	if (mainwin == NULL)
		panic("opening main window failed");
	return mainwin;
}