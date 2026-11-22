/*
 * Winlator Configuration - modern replacement for Wine's winecfg.exe
 *
 * Tabs: Applications | Graphics | Libraries | Audio & Drives | Winlator Tweaks | About & Update
 * Extras: DXVK / VKD3D presets, Android-friendly tweaks, self-update from GitHub
 *         releases, /install and /uninstall to replace the stock winecfg.exe.
 *
 * Pure Win32 + common controls v6 (manifest). Builds with MinGW-w64 (x86 and x86_64).
 * License: LGPL-2.1-or-later (same as Wine).
 */
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* ---------- configure these two for your own repository ---------- */
#define APP_VERSION  "v1.1.0"
#define UPDATE_REPO  "YOUR_GITHUB_USER/winlator-winecfg"
/* ------------------------------------------------------------------ */

#define W_(x) L##x
#define WW(x) W_(x)
#ifdef _WIN64
#define ASSET_NAME "winecfg-x64.exe"
#else
#define ASSET_NAME "winecfg-x86.exe"
#endif

#define K_WINE  L"Software\\Wine"
#define K_D3D   L"Software\\Wine\\Direct3D"
#define K_X11   L"Software\\Wine\\X11 Driver"
#define K_DLL   L"Software\\Wine\\DllOverrides"
#define K_APP   L"Software\\Wine\\AppDefaults"
#define K_EXPL  L"Software\\Wine\\Explorer"
#define K_DESK  L"Software\\Wine\\Explorer\\Desktops"
#define K_DRV   L"Software\\Wine\\Drivers"
#define K_DBG   L"Software\\Wine\\WineDbg"
#define K_FONTS L"Software\\Fonts"

enum {
    ID_SIDE = 100, ID_APPLY, ID_CLOSE,
    ID_APPLIST = 200, ID_APPADD, ID_APPDEL, ID_APPVER,
    ID_RENDERER, ID_VRAM, ID_CSMT, ID_STRICT, ID_VDESK, ID_VDW, ID_VDH, ID_DPI,
    ID_DLLLIST, ID_DLLNAME, ID_DLLMODE, ID_DLLADD, ID_DLLDEL,
    ID_AUDIO, ID_DRIVES,
    ID_T_CRASH, ID_T_GECKO, ID_T_GRAB, ID_T_NODECOR, ID_T_FOCUS, ID_T_DXVK, ID_T_VKD3D,
    ID_TOOL_TASK, ID_TOOL_REG, ID_TOOL_EXPL, ID_TOOL_UNINST, ID_TOOL_CMD,
    ID_ABOUT_INFO, ID_UPD_CHECK, ID_UPD_INSTALL, ID_UPD_STATUS,
    ID_F_PATH, ID_F_GO, ID_F_UP, ID_F_SEARCH, ID_F_FIND, ID_F_LIST, ID_F_NAME, ID_F_EDIT,
    ID_F_SAVE, ID_F_RELOAD, ID_F_STAT
};

/* Ubuntu / Yaru inspired palette */
#define C_SIDE    RGB(48, 10, 36)
#define C_HOT     RGB(88, 38, 74)
#define C_ORANGE  RGB(233, 84, 32)
#define C_ORANGE2 RGB(190, 64, 20)
#define C_TEXT    RGB(51, 51, 51)
#define C_SUB     RGB(119, 110, 113)
#define C_BAR     RGB(246, 245, 244)
#define SIDE_W    200
#define ITEM_TOP  86
#define ITEM_H    46
#define WM_APP_PAGE (WM_APP + 1)

#define NPAGES 7
static HINSTANCE g_inst;
static HWND g_main, g_side, g_page[NPAGES];
static HFONT g_font, g_font_head, g_font_sub, g_font_side, g_font_sideb, g_font_mono;
static int g_dpi = 96;
#define S(v) MulDiv((v), g_dpi, 96)

/* ------------------------------------------------------------------ */
/* data tables                                                         */
/* ------------------------------------------------------------------ */
static const struct { const WCHAR *id, *label; } g_ver[] = {
    {L"win11", L"Windows 11"}, {L"win10", L"Windows 10"}, {L"win81", L"Windows 8.1"},
    {L"win8", L"Windows 8"}, {L"win7", L"Windows 7"}, {L"win2008r2", L"Windows 2008 R2"},
    {L"win2008", L"Windows 2008"}, {L"winvista", L"Windows Vista"}, {L"win2003", L"Windows 2003"},
    {L"winxp", L"Windows XP"}, {L"win2k", L"Windows 2000"}, {L"winme", L"Windows ME"},
    {L"win98", L"Windows 98"}, {L"win95", L"Windows 95"}, {L"nt40", L"Windows NT 4.0"}
};
#define NVER ((int)(sizeof(g_ver) / sizeof(g_ver[0])))

static const struct { const WCHAR *val, *label; } g_mode[] = {
    {L"builtin", L"Builtin (Wine)"}, {L"native", L"Native (Windows)"},
    {L"native,builtin", L"Native, then builtin"}, {L"builtin,native", L"Builtin, then native"},
    {L"", L"Disabled"}
};
#define NMODE ((int)(sizeof(g_mode) / sizeof(g_mode[0])))

static const WCHAR *g_renderers_val[] = { L"", L"gl", L"vulkan", L"gdi" };
static const WCHAR *g_renderers_lbl[] = { L"Default", L"OpenGL (WineD3D)", L"Vulkan (WineD3D)", L"GDI (software)" };
static const int g_vram[] = { 0, 512, 1024, 2048, 4096 };
static const WCHAR *g_vram_lbl[] = { L"Default", L"512 MB", L"1024 MB", L"2048 MB", L"4096 MB" };
static const WCHAR *g_audio_lbl[] = { L"Default", L"PulseAudio", L"ALSA", L"Disabled" };
static const WCHAR *g_dpi_lbl[] = { L"96", L"120", L"144", L"192", L"240" };
static const WCHAR *g_dxvk_dlls[] = { L"d3d9", L"d3d10core", L"d3d11", L"dxgi" };
static const WCHAR *g_vkd3d_dlls[] = { L"d3d12", L"d3d12core" };
static const WCHAR *g_gecko_dlls[] = { L"mscoree", L"mshtml" };

/* ------------------------------------------------------------------ */
/* registry helpers                                                    */
/* ------------------------------------------------------------------ */
static BOOL rget_sz(const WCHAR *key, const WCHAR *name, WCHAR *buf, DWORD cch)
{
    HKEY k; DWORD type = 0, sz = (cch - 1) * sizeof(WCHAR); LONG r;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &k)) return FALSE;
    r = RegQueryValueExW(k, name, NULL, &type, (BYTE *)buf, &sz);
    RegCloseKey(k);
    if (r || type != REG_SZ) return FALSE;
    buf[sz / sizeof(WCHAR)] = 0;
    return TRUE;
}

static void rset_sz(const WCHAR *key, const WCHAR *name, const WCHAR *val)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL)) return;
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)val, (DWORD)((wcslen(val) + 1) * sizeof(WCHAR)));
    RegCloseKey(k);
}

static BOOL rget_dw(const WCHAR *key, const WCHAR *name, DWORD *out)
{
    HKEY k; DWORD type = 0, sz = sizeof(DWORD), v = 0; LONG r;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &k)) return FALSE;
    r = RegQueryValueExW(k, name, NULL, &type, (BYTE *)&v, &sz);
    RegCloseKey(k);
    if (r || type != REG_DWORD) return FALSE;
    *out = v;
    return TRUE;
}

static void rset_dw(const WCHAR *key, const WCHAR *name, DWORD val)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL)) return;
    RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE *)&val, sizeof(val));
    RegCloseKey(k);
}

static void rdel(const WCHAR *key, const WCHAR *name)
{
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_WRITE, &k)) return;
    RegDeleteValueW(k, name);
    RegCloseKey(k);
}

static void rmkkey(const WCHAR *key)
{
    HKEY k;
    if (!RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL)) RegCloseKey(k);
}

/* ------------------------------------------------------------------ */
/* UI helpers                                                          */
/* ------------------------------------------------------------------ */
static HWND ctl(int id)
{
    int i;
    for (i = 0; i < NPAGES; i++) {
        HWND h = g_page[i] ? GetDlgItem(g_page[i], id) : NULL;
        if (h) return h;
    }
    return NULL;
}

static HWND mk(int pg, const WCHAR *cls, const WCHAR *txt, DWORD st, DWORD ex,
               int x, int y, int w, int h, int id)
{
    return CreateWindowExW(ex, cls, txt, WS_CHILD | WS_VISIBLE | st, S(x), S(y), S(w), S(h),
                           g_page[pg], (HMENU)(INT_PTR)id, g_inst, NULL);
}
#define LABEL(pg, t, x, y, w)    mk(pg, L"STATIC", t, SS_LEFT, 0, x, y, w, 20, -1)
#define GROUP(pg, t, x, y, w, h) mk(pg, L"BUTTON", t, BS_GROUPBOX, 0, x, y, w, h, -1)
#define CHECK(pg, t, x, y, w, id) mk(pg, L"BUTTON", t, BS_AUTOCHECKBOX | WS_TABSTOP, 0, x, y, w, 22, id)
#define BTN(pg, t, x, y, w, h, id) mk(pg, L"BUTTON", t, BS_OWNERDRAW | WS_TABSTOP, 0, x, y, w, h, id)
#define COMBO(pg, x, y, w, id)   mk(pg, L"COMBOBOX", NULL, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, x, y, w, 220, id)

static void cb_fill(HWND cb, const WCHAR **items, int n)
{
    int i;
    for (i = 0; i < n; i++) SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)items[i]);
}

static BOOL chk_get(int id) { return SendMessageW(ctl(id), BM_GETCHECK, 0, 0) == BST_CHECKED; }
static void chk_set(int id, BOOL on) { SendMessageW(ctl(id), BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0); }

static BOOL CALLBACK set_font_cb(HWND h, LPARAM l) { SendMessageW(h, WM_SETFONT, (WPARAM)l, TRUE); return TRUE; }

static LRESULT CALLBACK PageProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_COMMAND:
        return SendMessageW(g_main, WM_COMMAND, w, l);
    case WM_DRAWITEM:
        return SendMessageW(g_main, WM_DRAWITEM, w, l);
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetBkMode((HDC)w, TRANSPARENT);
        SetTextColor((HDC)w, C_TEXT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    return DefWindowProcW(h, m, w, l);
}

/* ------------------------------------------------------------------ */
/* Applications page                                                   */
/* ------------------------------------------------------------------ */
static BOOL cur_app_key(WCHAR *key)
{
    HWND lb = ctl(ID_APPLIST);
    int i = (int)SendMessageW(lb, LB_GETCURSEL, 0, 0);
    WCHAR name[MAX_PATH];
    if (i < 0) return FALSE;
    if (i == 0) { wcscpy(key, K_WINE); return TRUE; }
    SendMessageW(lb, LB_GETTEXT, i, (LPARAM)name);
    _snwprintf(key, 511, K_APP L"\\%ls", name);
    key[511] = 0;
    return TRUE;
}

static void on_app_sel(void)
{
    WCHAR key[512], v[64];
    int i, sel = -1;
    if (!cur_app_key(key)) return;
    if (rget_sz(key, L"Version", v, 64))
        for (i = 0; i < NVER; i++) if (!_wcsicmp(v, g_ver[i].id)) { sel = i; break; }
    SendMessageW(ctl(ID_APPVER), CB_SETCURSEL, sel, 0);
}

static void refresh_apps(int select)
{
    HWND lb = ctl(ID_APPLIST);
    HKEY k; int i;
    SendMessageW(lb, LB_RESETCONTENT, 0, 0);
    SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)L"Default Settings");
    if (!RegOpenKeyExW(HKEY_CURRENT_USER, K_APP, 0, KEY_READ, &k)) {
        for (i = 0;; i++) {
            WCHAR n[MAX_PATH]; DWORD nl = MAX_PATH;
            LONG r = RegEnumKeyExW(k, i, n, &nl, NULL, NULL, NULL, NULL);
            if (r == ERROR_NO_MORE_ITEMS) break;
            if (!r) SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)n);
        }
        RegCloseKey(k);
    }
    SendMessageW(lb, LB_SETCURSEL, select, 0);
    on_app_sel();
}

static void app_add(void)
{
    WCHAR path[MAX_PATH] = L"", key[512];
    OPENFILENAMEW ofn;
    WCHAR *base, *b2;
    int idx;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main;
    ofn.lpstrFilter = L"Programs (*.exe)\0*.exe\0All files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;
    base = wcsrchr(path, L'\\');
    b2 = wcsrchr(path, L'/');
    if (b2 > base) base = b2;
    base = base ? base + 1 : path;
    _snwprintf(key, 511, K_APP L"\\%ls", base);
    key[511] = 0;
    rmkkey(key);
    refresh_apps(0);
    idx = (int)SendMessageW(ctl(ID_APPLIST), LB_FINDSTRINGEXACT, 0, (LPARAM)base);
    if (idx >= 0) { SendMessageW(ctl(ID_APPLIST), LB_SETCURSEL, idx, 0); on_app_sel(); }
}

static void app_del(void)
{
    WCHAR key[512];
    int i = (int)SendMessageW(ctl(ID_APPLIST), LB_GETCURSEL, 0, 0);
    if (i <= 0 || !cur_app_key(key)) return;
    RegDeleteTreeW(HKEY_CURRENT_USER, key);
    refresh_apps(0);
}

static void app_ver_changed(void)
{
    WCHAR key[512];
    int i = (int)SendMessageW(ctl(ID_APPVER), CB_GETCURSEL, 0, 0);
    if (i < 0 || !cur_app_key(key)) return;
    rset_sz(key, L"Version", g_ver[i].id);
}

/* ------------------------------------------------------------------ */
/* Graphics page                                                       */
/* ------------------------------------------------------------------ */
static void vdesk_enable(void)
{
    BOOL on = chk_get(ID_VDESK);
    EnableWindow(ctl(ID_VDW), on);
    EnableWindow(ctl(ID_VDH), on);
}

static void load_graphics(void)
{
    WCHAR v[128]; DWORD dw; int i, sel;

    sel = 0;
    if (rget_sz(K_D3D, L"renderer", v, 128))
        for (i = 1; i < 4; i++) if (!_wcsicmp(v, g_renderers_val[i])) sel = i;
    SendMessageW(ctl(ID_RENDERER), CB_SETCURSEL, sel, 0);

    sel = 0;
    if (rget_sz(K_D3D, L"VideoMemorySize", v, 128)) {
        int mb = _wtoi(v);
        for (i = 1; i < 5; i++) if (g_vram[i] == mb) sel = i;
    }
    SendMessageW(ctl(ID_VRAM), CB_SETCURSEL, sel, 0);

    chk_set(ID_CSMT, rget_dw(K_D3D, L"csmt", &dw) ? dw != 0 : TRUE);
    chk_set(ID_STRICT, rget_sz(K_D3D, L"StrictDrawOrdering", v, 128) && !_wcsicmp(v, L"enabled"));

    SetWindowTextW(ctl(ID_VDW), L"1280");
    SetWindowTextW(ctl(ID_VDH), L"720");
    if (rget_sz(K_EXPL, L"Desktop", v, 128)) {
        WCHAR sz[64]; int w, h;
        chk_set(ID_VDESK, TRUE);
        if (rget_sz(K_DESK, v, sz, 64) && swscanf(sz, L"%dx%d", &w, &h) == 2) {
            _snwprintf(sz, 63, L"%d", w); SetWindowTextW(ctl(ID_VDW), sz);
            _snwprintf(sz, 63, L"%d", h); SetWindowTextW(ctl(ID_VDH), sz);
        }
    } else chk_set(ID_VDESK, FALSE);
    vdesk_enable();

    dw = 96;
    rget_dw(K_FONTS, L"LogPixels", &dw);
    _snwprintf(v, 31, L"%lu", (unsigned long)dw);
    SetWindowTextW(ctl(ID_DPI), v);
}

static void save_graphics(void)
{
    WCHAR v[64], w[16], h[16]; int i; int dpi;

    i = (int)SendMessageW(ctl(ID_RENDERER), CB_GETCURSEL, 0, 0);
    if (i <= 0) rdel(K_D3D, L"renderer"); else rset_sz(K_D3D, L"renderer", g_renderers_val[i]);

    i = (int)SendMessageW(ctl(ID_VRAM), CB_GETCURSEL, 0, 0);
    if (i <= 0) rdel(K_D3D, L"VideoMemorySize");
    else { _snwprintf(v, 63, L"%d", g_vram[i]); rset_sz(K_D3D, L"VideoMemorySize", v); }

    rset_dw(K_D3D, L"csmt", chk_get(ID_CSMT) ? 3 : 0);
    if (chk_get(ID_STRICT)) rset_sz(K_D3D, L"StrictDrawOrdering", L"enabled");
    else rdel(K_D3D, L"StrictDrawOrdering");

    if (chk_get(ID_VDESK)) {
        GetWindowTextW(ctl(ID_VDW), w, 16);
        GetWindowTextW(ctl(ID_VDH), h, 16);
        if (_wtoi(w) < 640) wcscpy(w, L"640");
        if (_wtoi(h) < 480) wcscpy(h, L"480");
        _snwprintf(v, 63, L"%d" L"x" L"%d", _wtoi(w), _wtoi(h));
        rset_sz(K_EXPL, L"Desktop", L"Default");
        rset_sz(K_DESK, L"Default", v);
    } else rdel(K_EXPL, L"Desktop");

    GetWindowTextW(ctl(ID_DPI), v, 32);
    dpi = _wtoi(v);
    if (dpi >= 72 && dpi <= 480) rset_dw(K_FONTS, L"LogPixels", (DWORD)dpi);
}

/* ------------------------------------------------------------------ */
/* Libraries page                                                      */
/* ------------------------------------------------------------------ */
static void refresh_dlls(void)
{
    HWND lb = ctl(ID_DLLLIST);
    HKEY k; int i, j;
    SendMessageW(lb, LB_RESETCONTENT, 0, 0);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, K_DLL, 0, KEY_READ, &k)) return;
    for (i = 0;; i++) {
        WCHAR n[129], v[129], line[300]; DWORD nl = 128, vl = 128 * sizeof(WCHAR), type = 0;
        const WCHAR *label;
        LONG r = RegEnumValueW(k, i, n, &nl, NULL, &type, (BYTE *)v, &vl);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r || type != REG_SZ) continue;
        v[vl / sizeof(WCHAR)] = 0;
        label = v;
        for (j = 0; j < NMODE; j++) if (!_wcsicmp(v, g_mode[j].val)) { label = g_mode[j].label; break; }
        _snwprintf(line, 299, L"%ls    -    %ls", n, label);
        line[299] = 0;
        SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)line);
    }
    RegCloseKey(k);
}

static BOOL dll_sel_name(WCHAR *name)
{
    HWND lb = ctl(ID_DLLLIST);
    int i = (int)SendMessageW(lb, LB_GETCURSEL, 0, 0);
    WCHAR line[300], *sp;
    if (i < 0) return FALSE;
    SendMessageW(lb, LB_GETTEXT, i, (LPARAM)line);
    sp = wcsstr(line, L"    -    ");
    if (sp) *sp = 0;
    wcscpy(name, line);
    return TRUE;
}

static void dll_selected(void)
{
    WCHAR n[300], v[128]; int j, sel = 2;
    if (!dll_sel_name(n)) return;
    SetWindowTextW(ctl(ID_DLLNAME), n);
    if (rget_sz(K_DLL, n, v, 128))
        for (j = 0; j < NMODE; j++) if (!_wcsicmp(v, g_mode[j].val)) { sel = j; break; }
    SendMessageW(ctl(ID_DLLMODE), CB_SETCURSEL, sel, 0);
}

static void dll_add(void)
{
    WCHAR n[128], *p; int len, m;
    GetWindowTextW(ctl(ID_DLLNAME), n, 128);
    p = n; while (*p == L' ') p++;
    if (p != n) memmove(n, p, (wcslen(p) + 1) * sizeof(WCHAR));
    len = (int)wcslen(n);
    while (len > 0 && n[len - 1] == L' ') n[--len] = 0;
    if (len > 4 && !_wcsicmp(n + len - 4, L".dll")) n[len - 4] = 0;
    if (!n[0]) return;
    m = (int)SendMessageW(ctl(ID_DLLMODE), CB_GETCURSEL, 0, 0);
    if (m < 0) m = 2;
    rset_sz(K_DLL, n, g_mode[m].val);
    refresh_dlls();
}

static void dll_del(void)
{
    WCHAR n[300];
    if (!dll_sel_name(n)) return;
    rdel(K_DLL, n);
    refresh_dlls();
}

/* ------------------------------------------------------------------ */
/* Audio & drives page                                                 */
/* ------------------------------------------------------------------ */
static void load_audio(void)
{
    WCHAR v[64]; int sel = 0;
    if (rget_sz(K_DRV, L"Audio", v, 64)) {
        if (!_wcsicmp(v, L"pulse")) sel = 1;
        else if (!_wcsicmp(v, L"alsa")) sel = 2;
        else if (!v[0]) sel = 3;
    }
    SendMessageW(ctl(ID_AUDIO), CB_SETCURSEL, sel, 0);
}

static void save_audio(void)
{
    int i = (int)SendMessageW(ctl(ID_AUDIO), CB_GETCURSEL, 0, 0);
    if (i <= 0) rdel(K_DRV, L"Audio");
    else if (i == 1) rset_sz(K_DRV, L"Audio", L"pulse");
    else if (i == 2) rset_sz(K_DRV, L"Audio", L"alsa");
    else rset_sz(K_DRV, L"Audio", L"");
}

static void load_drives(void)
{
    HWND lb = ctl(ID_DRIVES);
    WCHAR buf[256], *p;
    SendMessageW(lb, LB_RESETCONTENT, 0, 0);
    if (!GetLogicalDriveStringsW(255, buf)) return;
    for (p = buf; *p; p += wcslen(p) + 1) {
        WCHAR line[256]; const WCHAR *type; ULARGE_INTEGER fb, tb, tf;
        switch (GetDriveTypeW(p)) {
        case DRIVE_FIXED: type = L"Local disk"; break;
        case DRIVE_REMOVABLE: type = L"Removable"; break;
        case DRIVE_CDROM: type = L"CD-ROM"; break;
        case DRIVE_REMOTE: type = L"Network"; break;
        case DRIVE_RAMDISK: type = L"RAM disk"; break;
        default: type = L"Unknown"; break;
        }
        if (GetDiskFreeSpaceExW(p, &fb, &tb, &tf))
            _snwprintf(line, 255, L"%.2ls    %ls    -    %lu GB free of %lu GB", p, type,
                       (unsigned long)(fb.QuadPart >> 30), (unsigned long)(tb.QuadPart >> 30));
        else
            _snwprintf(line, 255, L"%.2ls    %ls", p, type);
        line[255] = 0;
        SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)line);
    }
}

/* ------------------------------------------------------------------ */
/* Winlator tweaks page                                                */
/* ------------------------------------------------------------------ */
static BOOL ovr_all(const WCHAR **names, int n, const WCHAR *want)
{
    WCHAR v[64]; int i;
    for (i = 0; i < n; i++)
        if (!rget_sz(K_DLL, names[i], v, 64) || _wcsicmp(v, want)) return FALSE;
    return TRUE;
}

static void ovr_set(const WCHAR **names, int n, BOOL on, const WCHAR *val)
{
    WCHAR v[64]; int i;
    for (i = 0; i < n; i++) {
        if (on) rset_sz(K_DLL, names[i], val);
        else if (rget_sz(K_DLL, names[i], v, 64) && !_wcsicmp(v, val)) rdel(K_DLL, names[i]);
    }
}

static BOOL x11_is(const WCHAR *name, const WCHAR *val)
{
    WCHAR v[16];
    return rget_sz(K_X11, name, v, 16) && !_wcsicmp(v, val);
}

static void x11_flag(const WCHAR *name, BOOL on, const WCHAR *val)
{
    if (on) rset_sz(K_X11, name, val); else rdel(K_X11, name);
}

static void load_tweaks(void)
{
    DWORD dw;
    chk_set(ID_T_CRASH, rget_dw(K_DBG, L"ShowCrashDialog", &dw) && dw == 0);
    chk_set(ID_T_GECKO, ovr_all(g_gecko_dlls, 2, L""));
    chk_set(ID_T_GRAB, x11_is(L"GrabFullscreen", L"Y"));
    chk_set(ID_T_NODECOR, x11_is(L"Decorated", L"N"));
    chk_set(ID_T_FOCUS, x11_is(L"UseTakeFocus", L"N"));
    chk_set(ID_T_DXVK, ovr_all(g_dxvk_dlls, 4, L"native,builtin"));
    chk_set(ID_T_VKD3D, ovr_all(g_vkd3d_dlls, 2, L"native,builtin"));
}

static void save_tweaks(void)
{
    if (chk_get(ID_T_CRASH)) rset_dw(K_DBG, L"ShowCrashDialog", 0); else rdel(K_DBG, L"ShowCrashDialog");
    ovr_set(g_gecko_dlls, 2, chk_get(ID_T_GECKO), L"");
    x11_flag(L"GrabFullscreen", chk_get(ID_T_GRAB), L"Y");
    x11_flag(L"Decorated", chk_get(ID_T_NODECOR), L"N");
    x11_flag(L"UseTakeFocus", chk_get(ID_T_FOCUS), L"N");
    ovr_set(g_dxvk_dlls, 4, chk_get(ID_T_DXVK), L"native,builtin");
    ovr_set(g_vkd3d_dlls, 2, chk_get(ID_T_VKD3D), L"native,builtin");
    refresh_dlls();
}

static void run_tool(const WCHAR *exe)
{
    ShellExecuteW(g_main, L"open", exe, NULL, NULL, SW_SHOW);
}

/* ------------------------------------------------------------------ */
/* About / update / installer                                          */
/* ------------------------------------------------------------------ */
static void fill_about(void)
{
    typedef const char *(CDECL *wine_ver_t)(void);
    typedef LONG (WINAPI *rtlver_t)(RTL_OSVERSIONINFOW *);
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    wine_ver_t gv = nt ? (wine_ver_t)GetProcAddress(nt, "wine_get_version") : NULL;
    rtlver_t rv = nt ? (rtlver_t)GetProcAddress(nt, "RtlGetVersion") : NULL;
    RTL_OSVERSIONINFOW vi;
    WCHAR wv[64] = L"unknown", txt[800];
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(vi);
    if (rv) rv(&vi);
    if (gv) MultiByteToWideChar(CP_ACP, 0, gv(), -1, wv, 64);
    _snwprintf(txt, 799,
        L"Winlator Configuration  " WW(APP_VERSION) L"\r\n\r\n"
        L"Wine version:  %ls\r\n"
        L"Reported Windows:  %lu.%lu (build %lu)\r\n"
#ifdef _WIN64
        L"This program:  64-bit\r\n\r\n"
#else
        L"This program:  32-bit\r\n\r\n"
#endif
        L"Settings are stored in the current Wine prefix (HKCU\\Software\\Wine).\r\n"
        L"Restart running programs after pressing Apply.",
        wv, (unsigned long)vi.dwMajorVersion, (unsigned long)vi.dwMinorVersion,
        (unsigned long)vi.dwBuildNumber);
    txt[799] = 0;
    SetWindowTextW(ctl(ID_ABOUT_INFO), txt);
}

static void set_status(const WCHAR *t) { SetWindowTextW(ctl(ID_UPD_STATUS), t); UpdateWindow(g_main); }

static BOOL self_path(WCHAR *p) { return GetModuleFileNameW(NULL, p, MAX_PATH) > 0; }

/* Copy this exe over system32\winecfg.exe (backup original once). */
static BOOL install_self(WCHAR *err, size_t errn)
{
    WCHAR self[MAX_PATH], sys[MAX_PATH], dst[MAX_PATH + 8], bak[MAX_PATH + 16];
    self_path(self);
    GetSystemDirectoryW(sys, MAX_PATH);
    _snwprintf(dst, MAX_PATH + 7, L"%ls\\winecfg.exe", sys);
    dst[MAX_PATH + 7] = 0;
    if (!_wcsicmp(self, dst)) { _snwprintf(err, errn, L"Already running from %ls", dst); return FALSE; }
    _snwprintf(bak, MAX_PATH + 15, L"%ls.orig", dst);
    bak[MAX_PATH + 15] = 0;
    CopyFileW(dst, bak, TRUE);  /* keeps the first backup only */
    if (!CopyFileW(self, dst, FALSE)) {
        _snwprintf(err, errn, L"Could not write %ls (error %lu)", dst, (unsigned long)GetLastError());
        return FALSE;
    }
    _snwprintf(err, errn, L"Installed to %ls", dst);
    return TRUE;
}

static BOOL uninstall_self(WCHAR *err, size_t errn)
{
    WCHAR sys[MAX_PATH], dst[MAX_PATH + 8], bak[MAX_PATH + 16];
    GetSystemDirectoryW(sys, MAX_PATH);
    _snwprintf(dst, MAX_PATH + 7, L"%ls\\winecfg.exe", sys);
    dst[MAX_PATH + 7] = 0;
    _snwprintf(bak, MAX_PATH + 15, L"%ls.orig", dst);
    bak[MAX_PATH + 15] = 0;
    if (!CopyFileW(bak, dst, FALSE)) {
        _snwprintf(err, errn, L"No backup found (%ls)", bak);
        return FALSE;
    }
    _snwprintf(err, errn, L"Original winecfg.exe restored");
    return TRUE;
}

static char *http_get(const WCHAR *url, DWORD *outlen)
{
    HINTERNET s, h; char *buf, *nb; DWORD cap = 65536, len = 0, n = 0;
    s = InternetOpenW(L"WinlatorConfig/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!s) return NULL;
    h = InternetOpenUrlW(s, url, NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!h) { InternetCloseHandle(s); return NULL; }
    buf = (char *)malloc(cap + 1);
    while (buf && InternetReadFile(h, buf + len, cap - len, &n) && n) {
        len += n;
        if (len == cap) {
            if (cap > (128u << 20)) break;
            cap *= 2;
            nb = (char *)realloc(buf, cap + 1);
            if (!nb) { free(buf); buf = NULL; break; }
            buf = nb;
        }
    }
    InternetCloseHandle(h);
    InternetCloseHandle(s);
    if (!buf) return NULL;
    buf[len] = 0;
    *outlen = len;
    return buf;
}

/* parse  "key" : "value"  starting at p (p points at the key's opening quote) */
static BOOL json_str_at(const char *p, char *out, size_t n)
{
    size_t i = 0;
    p = strchr(p + 1, '"');           /* end of key */
    if (!p) return FALSE;
    p++;
    while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '"') return FALSE;
    p++;
    while (*p && *p != '"' && i + 1 < n) out[i++] = *p++;
    out[i] = 0;
    return i > 0;
}

static void do_update(void)
{
    char *json, tag[64], url[512] = "", *p;
    DWORD len = 0;
    WCHAR wurl[512], api[256], msg[512], self[MAX_PATH], newp[MAX_PATH + 8], oldp[MAX_PATH + 8];
    char *data; HANDLE f; DWORD wr;

    SetCursor(LoadCursor(NULL, IDC_WAIT));
    set_status(L"Checking for updates...");
    MultiByteToWideChar(CP_ACP, 0, "https://api.github.com/repos/" UPDATE_REPO "/releases/latest", -1, api, 256);
    json = http_get(api, &len);
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    if (!json) { set_status(L"Could not reach GitHub. Check the internet connection of this Wine prefix (WinINet / TLS)."); return; }
    p = strstr(json, "\"tag_name\"");
    if (!p || !json_str_at(p, tag, sizeof(tag))) {
        set_status(L"No release found in the repository yet."); free(json); return;
    }
    if (!strcmp(tag, APP_VERSION)) {
        set_status(L"You already have the latest version."); free(json); return;
    }
    for (p = json; (p = strstr(p, "\"browser_download_url\"")) != NULL; p += 20) {
        char u[512];
        if (json_str_at(p, u, sizeof(u)) && strstr(u, ASSET_NAME)) { strcpy(url, u); break; }
    }
    free(json);
    if (!url[0]) { set_status(L"Newest release has no matching " WW(ASSET_NAME) L" file."); return; }

    {
        WCHAR wtag[64];
        MultiByteToWideChar(CP_ACP, 0, tag, -1, wtag, 64);
        _snwprintf(msg, 511, L"Version %ls is available (you have " WW(APP_VERSION) L").\n\nDownload and install it now?", wtag);
        msg[511] = 0;
    }
    if (MessageBoxW(g_main, msg, L"Update available", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        set_status(L"Update postponed."); return;
    }

    set_status(L"Downloading...");
    SetCursor(LoadCursor(NULL, IDC_WAIT));
    MultiByteToWideChar(CP_ACP, 0, url, -1, wurl, 512);
    data = http_get(wurl, &len);
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    if (!data || len < 10000 || data[0] != 'M' || data[1] != 'Z') {
        free(data); set_status(L"Download failed or file is not a valid program."); return;
    }
    self_path(self);
    _snwprintf(newp, MAX_PATH + 7, L"%ls.new", self); newp[MAX_PATH + 7] = 0;
    _snwprintf(oldp, MAX_PATH + 7, L"%ls.old", self); oldp[MAX_PATH + 7] = 0;
    f = CreateFileW(newp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE || !WriteFile(f, data, len, &wr, NULL) || wr != len) {
        if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
        free(data); set_status(L"Could not write the downloaded file."); return;
    }
    CloseHandle(f);
    free(data);
    DeleteFileW(oldp);
    if (!MoveFileW(self, oldp) || !MoveFileW(newp, self)) {
        MoveFileW(oldp, self);
        set_status(L"Could not replace the running program."); return;
    }
    set_status(L"Updated successfully. Close and reopen Winlator Configuration.");
}

/* ------------------------------------------------------------------ */
/* Files page: browse / search / edit text files (default Z:\opt)     */
/* ------------------------------------------------------------------ */
static WCHAR g_dir[MAX_PATH] = L"Z:\\opt";
static WCHAR g_file[MAX_PATH];
static BOOL g_crlf, g_dirty, g_files_inited;

static void files_status(const WCHAR *t) { SetWindowTextW(ctl(ID_F_STAT), t); }

static void join_path(WCHAR *out, const WCHAR *dir, const WCHAR *name)
{
    size_t n = wcslen(dir);
    if (n && dir[n - 1] == L'\\') _snwprintf(out, MAX_PATH - 1, L"%ls%ls", dir, name);
    else _snwprintf(out, MAX_PATH - 1, L"%ls\\%ls", dir, name);
    out[MAX_PATH - 1] = 0;
}

static BOOL confirm_discard(void)
{
    if (!g_dirty) return TRUE;
    return MessageBoxW(g_main, L"The open file has unsaved changes. Discard them?", L"Unsaved changes",
                       MB_YESNO | MB_ICONWARNING) == IDYES;
}

static void list_entries(HWND lb, const WCHAR *dir, BOOL want_dir)
{
    WIN32_FIND_DATAW fd; HANDLE h; WCHAR pat[MAX_PATH], line[MAX_PATH + 8]; int idx;
    join_path(pat, dir, L"*");
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        BOOL isdir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..") || isdir != want_dir) continue;
        if (isdir) _snwprintf(line, MAX_PATH + 7, L"[dir] %ls", fd.cFileName);
        else _snwprintf(line, MAX_PATH + 7, L"%ls", fd.cFileName);
        line[MAX_PATH + 7] = 0;
        idx = (int)SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)line);
        SendMessageW(lb, LB_SETITEMDATA, idx, isdir ? 1 : 0);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void files_list_dir(const WCHAR *dir)
{
    HWND lb = ctl(ID_F_LIST);
    WCHAR msg[MAX_PATH + 40];
    DWORD a = GetFileAttributesW(dir);
    size_t n;
    if (a == INVALID_FILE_ATTRIBUTES || !(a & FILE_ATTRIBUTE_DIRECTORY)) {
        _snwprintf(msg, MAX_PATH + 39, L"Folder not found: %ls", dir);
        msg[MAX_PATH + 39] = 0;
        files_status(msg);
        return;
    }
    wcsncpy(g_dir, dir, MAX_PATH - 1);
    g_dir[MAX_PATH - 1] = 0;
    n = wcslen(g_dir);
    while (n > 3 && g_dir[n - 1] == L'\\') g_dir[--n] = 0;
    SetWindowTextW(ctl(ID_F_PATH), g_dir);
    SendMessageW(lb, LB_RESETCONTENT, 0, 0);
    list_entries(lb, g_dir, TRUE);
    list_entries(lb, g_dir, FALSE);
    files_status(L"Click a file to open it. Double-click a [dir] to enter it.");
}

static void files_go(void)
{
    WCHAR p[MAX_PATH];
    GetWindowTextW(ctl(ID_F_PATH), p, MAX_PATH);
    files_list_dir(p);
}

static void files_up(void)
{
    WCHAR p[MAX_PATH], *s;
    wcscpy(p, g_dir);
    s = wcsrchr(p, L'\\');
    if (s && s > p + 2) *s = 0; else if (wcslen(p) >= 3) p[3] = 0;
    files_list_dir(p);
}

static BOOL has_ci(const WCHAR *s, const WCHAR *needle)
{
    size_t n = wcslen(needle);
    for (; *s; s++) if (!_wcsnicmp(s, needle, n)) return TRUE;
    return FALSE;
}

static int g_found;
static void search_rec(HWND lb, const WCHAR *dir, const WCHAR *needle, int depth)
{
    WIN32_FIND_DATAW fd; HANDLE h; WCHAR pat[MAX_PATH], full[MAX_PATH];
    if (g_found >= 300 || depth > 6) return;
    join_path(pat, dir, L"*");
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        join_path(full, dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) search_rec(lb, full, needle, depth + 1);
        } else if (has_ci(fd.cFileName, needle) && g_found < 300) {
            int idx = (int)SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)full);
            SendMessageW(lb, LB_SETITEMDATA, idx, 2);
            g_found++;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void files_find(void)
{
    WCHAR needle[128], msg[128];
    HWND lb = ctl(ID_F_LIST);
    GetWindowTextW(ctl(ID_F_SEARCH), needle, 128);
    if (!needle[0]) { files_status(L"Type part of a file name to search for."); return; }
    SetCursor(LoadCursor(NULL, IDC_WAIT));
    SendMessageW(lb, LB_RESETCONTENT, 0, 0);
    g_found = 0;
    files_status(L"Searching...");
    search_rec(lb, g_dir, needle, 0);
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    _snwprintf(msg, 127, L"%d match(es) under %ls (max 300). Click one to open it.", g_found, g_dir);
    msg[127] = 0;
    files_status(g_found ? msg : L"No matching files found in this folder tree.");
}

static BOOL files_sel(WCHAR *path, int *kind)
{
    HWND lb = ctl(ID_F_LIST);
    WCHAR t[MAX_PATH + 8];
    int i = (int)SendMessageW(lb, LB_GETCURSEL, 0, 0), d;
    if (i < 0) return FALSE;
    SendMessageW(lb, LB_GETTEXT, i, (LPARAM)t);
    d = (int)SendMessageW(lb, LB_GETITEMDATA, i, 0);
    if (d == 2) { wcscpy(path, t); *kind = 0; }
    else if (d == 1) { join_path(path, g_dir, t + 6); *kind = 1; }
    else { join_path(path, g_dir, t); *kind = 0; }
    return TRUE;
}

static void file_open(const WCHAR *path)
{
    HANDLE f; DWORD sz, rd = 0, i; char *buf; WCHAR *wide, *out; int wl, j, k;
    WCHAR msg[MAX_PATH + 60];
    f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        _snwprintf(msg, MAX_PATH + 59, L"Cannot open file (error %lu).", (unsigned long)GetLastError());
        files_status(msg); return;
    }
    sz = GetFileSize(f, NULL);
    if (sz == INVALID_FILE_SIZE || sz > 1500000) { CloseHandle(f); files_status(L"File is too large to edit here (limit 1.5 MB)."); return; }
    buf = (char *)malloc(sz + 1);
    if (!buf || !ReadFile(f, buf, sz, &rd, NULL)) { CloseHandle(f); free(buf); files_status(L"Read error."); return; }
    CloseHandle(f);
    buf[rd] = 0;
    for (i = 0; i < rd && i < 8192; i++)
        if (!buf[i]) { free(buf); files_status(L"This looks like a binary file - not opened."); return; }
    g_crlf = strstr(buf, "\r\n") != NULL;
    wl = MultiByteToWideChar(CP_UTF8, 0, buf, (int)rd, NULL, 0);
    wide = (WCHAR *)malloc((wl + 1) * sizeof(WCHAR));
    out = (WCHAR *)malloc((wl * 2 + 2) * sizeof(WCHAR));
    if (wide && out) {
        if (wl) MultiByteToWideChar(CP_UTF8, 0, buf, (int)rd, wide, wl);
        for (k = 0, j = 0; k < wl; k++) {
            if (wide[k] == L'\r' && k + 1 < wl && wide[k + 1] == L'\n') continue;
            if (wide[k] == L'\n') out[j++] = L'\r';
            out[j++] = wide[k];
        }
        out[j] = 0;
        SendMessageW(ctl(ID_F_EDIT), EM_LIMITTEXT, 4000000, 0);
        SetWindowTextW(ctl(ID_F_EDIT), out);
        wcscpy(g_file, path);
        SetWindowTextW(ctl(ID_F_NAME), path);
        g_dirty = FALSE;
        files_status(L"File opened. Edit it, then press Save (a .bak backup is made).");
    }
    free(buf); free(wide); free(out);
}

static void file_save(void)
{
    int len, n, j, k, u8; WCHAR *w, *w2; char *b; HANDLE f; DWORD wr = 0; WCHAR bak[MAX_PATH + 8], msg[MAX_PATH + 80];
    if (!g_file[0]) { files_status(L"Open a file first."); return; }
    _snwprintf(msg, MAX_PATH + 79, L"Overwrite this file?\n\n%ls\n\nA backup copy (.bak) is created first.", g_file);
    msg[MAX_PATH + 79] = 0;
    if (MessageBoxW(g_main, msg, L"Save file", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    len = GetWindowTextLengthW(ctl(ID_F_EDIT));
    w = (WCHAR *)malloc((len + 2) * sizeof(WCHAR));
    w2 = (WCHAR *)malloc((len + 2) * sizeof(WCHAR));
    if (!w || !w2) { free(w); free(w2); return; }
    GetWindowTextW(ctl(ID_F_EDIT), w, len + 1);
    for (k = 0, j = 0; k < len; k++) {
        if (!g_crlf && w[k] == L'\r' && k + 1 < len && w[k + 1] == L'\n') continue;
        w2[j++] = w[k];
    }
    n = j;
    u8 = WideCharToMultiByte(CP_UTF8, 0, w2, n, NULL, 0, NULL, NULL);
    b = (char *)malloc(u8 + 1);
    if (b) {
        WideCharToMultiByte(CP_UTF8, 0, w2, n, b, u8, NULL, NULL);
        _snwprintf(bak, MAX_PATH + 7, L"%ls.bak", g_file); bak[MAX_PATH + 7] = 0;
        CopyFileW(g_file, bak, TRUE);
        f = CreateFileW(g_file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (f == INVALID_HANDLE_VALUE) {
            _snwprintf(msg, MAX_PATH + 79, L"Cannot write file (error %lu). It may be read-only.", (unsigned long)GetLastError());
            files_status(msg);
        } else {
            BOOL ok = WriteFile(f, b, u8, &wr, NULL) && (int)wr == u8;
            CloseHandle(f);
            if (ok) { g_dirty = FALSE; files_status(L"Saved."); } else files_status(L"Write error.");
        }
    }
    free(b); free(w); free(w2);
}

static void files_init(void)
{
    g_files_inited = TRUE;
    files_list_dir(L"Z:\\opt");
    if (GetFileAttributesW(L"Z:\\opt") == INVALID_FILE_ATTRIBUTES) {
        files_list_dir(L"Z:\\");
        files_status(L"Z:\\opt does not exist here - showing Z:\\. Use Search to find your file.");
    }
}

static void files_clicked(int code)
{
    WCHAR p[MAX_PATH]; int kind;
    if (!files_sel(p, &kind)) return;
    if (code == LBN_DBLCLK && kind == 1) { if (confirm_discard()) files_list_dir(p); }
    else if (code == LBN_SELCHANGE && kind == 0) {
        if (!_wcsicmp(p, g_file) || confirm_discard()) file_open(p);
    }
}

/* ------------------------------------------------------------------ */
/* Icons (drawn with GDI, 16x16 design grid)                           */
/* ------------------------------------------------------------------ */
static void draw_icon(HDC dc, int kind, int x, int y, int s, COLORREF col)
{
#define PX(v) (x + MulDiv((v), s, 16))
#define PY(v) (y + MulDiv((v), s, 16))
    HPEN pen = CreatePen(PS_SOLID, s / 12 > 1 ? s / 12 : 1, col), op;
    HBRUSH fill = CreateSolidBrush(col), ob;
    HBRUSH hollow = (HBRUSH)GetStockObject(NULL_BRUSH);
    POINT pt[6];
    int i;
    op = (HPEN)SelectObject(dc, pen);
    ob = (HBRUSH)SelectObject(dc, fill);
    switch (kind) {
    case 0: /* applications: 2x2 grid */
        for (i = 0; i < 4; i++) {
            int cx = (i & 1) ? 9 : 2, cy = (i & 2) ? 9 : 2;
            RoundRect(dc, PX(cx), PY(cy), PX(cx + 5), PY(cy + 5), 2, 2);
        }
        break;
    case 1: /* graphics: monitor */
        SelectObject(dc, hollow);
        RoundRect(dc, PX(1), PY(2), PX(15), PY(11), 3, 3);
        MoveToEx(dc, PX(8), PY(11), NULL); LineTo(dc, PX(8), PY(14));
        MoveToEx(dc, PX(4), PY(14), NULL); LineTo(dc, PX(12), PY(14));
        break;
    case 2: /* libraries: books */
        Rectangle(dc, PX(2), PY(3), PX(5), PY(14));
        Rectangle(dc, PX(6), PY(2), PX(9), PY(14));
        Rectangle(dc, PX(10), PY(5), PX(14), PY(14));
        break;
    case 3: /* audio: speaker */
        pt[0].x = PX(2); pt[0].y = PY(6); pt[1].x = PX(5); pt[1].y = PY(6);
        pt[2].x = PX(9); pt[2].y = PY(3); pt[3].x = PX(9); pt[3].y = PY(13);
        pt[4].x = PX(5); pt[4].y = PY(10); pt[5].x = PX(2); pt[5].y = PY(10);
        Polygon(dc, pt, 6);
        SelectObject(dc, hollow);
        Arc(dc, PX(7), PY(4), PX(14), PY(12), PX(11), PY(12), PX(11), PY(4));
        break;
    case 4: /* tweaks: sliders */
    {
        static const int ky[3] = { 4, 8, 12 }, kx[3] = { 5, 11, 7 };
        for (i = 0; i < 3; i++) {
            MoveToEx(dc, PX(2), PY(ky[i]), NULL); LineTo(dc, PX(14), PY(ky[i]));
            Ellipse(dc, PX(kx[i] - 2), PY(ky[i] - 2), PX(kx[i] + 2) + 1, PY(ky[i] + 2) + 1);
        }
        break;
    }
    case 5: /* files: folder */
        pt[0].x = PX(1); pt[0].y = PY(4); pt[1].x = PX(6); pt[1].y = PY(4);
        pt[2].x = PX(8); pt[2].y = PY(6); pt[3].x = PX(15); pt[3].y = PY(6);
        pt[4].x = PX(15); pt[4].y = PY(13); pt[5].x = PX(1); pt[5].y = PY(13);
        Polygon(dc, pt, 6);
        break;
    default: /* about: circled i */
        SelectObject(dc, hollow);
        Ellipse(dc, PX(1), PY(1), PX(15), PY(15));
        SelectObject(dc, fill);
        Ellipse(dc, PX(7), PY(4), PX(9) + 1, PY(6) + 1);
        Rectangle(dc, PX(7), PY(7), PX(9) + 1, PY(12));
        break;
    }
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(pen);
    DeleteObject(fill);
#undef PX
#undef PY
}

static void draw_logo(HDC dc, int x, int y, int s)
{
    HBRUSH b = CreateSolidBrush(C_ORANGE), ob;
    HPEN p = CreatePen(PS_SOLID, 1, C_ORANGE), op;
    HFONT of;
    RECT r;
    ob = (HBRUSH)SelectObject(dc, b);
    op = (HPEN)SelectObject(dc, p);
    Ellipse(dc, x, y, x + s, y + s);
    SelectObject(dc, op); SelectObject(dc, ob);
    DeleteObject(b); DeleteObject(p);
    SetRect(&r, x, y, x + s, y + s);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    of = (HFONT)SelectObject(dc, g_font_sideb);
    DrawTextW(dc, L"W", 1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
}

/* ------------------------------------------------------------------ */
/* Sidebar (vertical tabs)                                             */
/* ------------------------------------------------------------------ */
static const WCHAR *g_names[NPAGES] = {
    L"Applications", L"Graphics", L"Libraries", L"Audio / Drives", L"Tweaks", L"Files", L"About"
};
static const WCHAR *g_titles[NPAGES] = {
    L"Applications", L"Graphics", L"Libraries", L"Audio and Drives", L"Winlator Tweaks",
    L"System Files", L"About and Updates"
};
static const WCHAR *g_subs[NPAGES] = {
    L"Choose which Windows version each program sees",
    L"Direct3D renderer, virtual desktop and screen DPI",
    L"DLL overrides: builtin, native or disabled",
    L"Sound driver and the drives of this Wine prefix",
    L"DXVK / VKD3D presets and Android-friendly fixes",
    L"Browse, search and edit files (starts in Z:\\opt)",
    L"Wine version, updates and installer"
};
static int g_side_sel = 0, g_side_hot = -1;

static void paint_side(HDC dc, RECT *rc)
{
    HBRUSH bg = CreateSolidBrush(C_SIDE);
    HFONT of;
    int i;
    FillRect(dc, rc, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);
    draw_logo(dc, S(16), S(16), S(38));
    of = (HFONT)SelectObject(dc, g_font_sideb);
    SetTextColor(dc, RGB(255, 255, 255));
    TextOutW(dc, S(64), S(15), L"Winlator", 8);
    SelectObject(dc, g_font_sub);
    SetTextColor(dc, RGB(205, 190, 200));
    TextOutW(dc, S(64), S(38), L"Configuration", 13);
    for (i = 0; i < NPAGES; i++) {
        RECT it;
        int ty;
        it.left = S(8); it.right = rc->right - S(8);
        it.top = S(ITEM_TOP) + i * S(ITEM_H);
        it.bottom = it.top + S(ITEM_H) - S(4);
        if (i == g_side_sel || i == g_side_hot) {
            HBRUSH b = CreateSolidBrush(i == g_side_sel ? C_ORANGE : C_HOT);
            HPEN np = (HPEN)GetStockObject(NULL_PEN);
            HBRUSH ob = (HBRUSH)SelectObject(dc, b);
            HPEN op = (HPEN)SelectObject(dc, np);
            RoundRect(dc, it.left, it.top, it.right, it.bottom, S(12), S(12));
            SelectObject(dc, op); SelectObject(dc, ob);
            DeleteObject(b);
        }
        ty = it.top + (it.bottom - it.top - S(18)) / 2;
        draw_icon(dc, i, it.left + S(14), ty, S(18), RGB(255, 255, 255));
        SelectObject(dc, g_font_side);
        SetTextColor(dc, RGB(255, 255, 255));
        TextOutW(dc, it.left + S(46), ty, g_names[i], (int)wcslen(g_names[i]));
    }
    SelectObject(dc, g_font_sub);
    SetTextColor(dc, RGB(170, 150, 165));
    {
        RECT fr = { S(16), rc->bottom - S(34), rc->right, rc->bottom - S(10) };
        DrawTextW(dc, WW(APP_VERSION), -1, &fr, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    }
    SelectObject(dc, of);
}

static int side_hit(int y)
{
    int i;
    if (y < S(ITEM_TOP)) return -1;
    i = (y - S(ITEM_TOP)) / S(ITEM_H);
    return (i >= 0 && i < NPAGES) ? i : -1;
}

static LRESULT CALLBACK SideProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; RECT rc;
        HDC dc = BeginPaint(h, &ps), md;
        HBITMAP bm, ob;
        GetClientRect(h, &rc);
        md = CreateCompatibleDC(dc);
        bm = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
        ob = (HBITMAP)SelectObject(md, bm);
        paint_side(md, &rc);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, md, 0, 0, SRCCOPY);
        SelectObject(md, ob);
        DeleteObject(bm);
        DeleteDC(md);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int i = side_hit(GET_Y_LPARAM(l));
        SetFocus(h);
        if (i >= 0) PostMessageW(g_main, WM_APP_PAGE, (WPARAM)i, 0);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int i = side_hit(GET_Y_LPARAM(l));
        TRACKMOUSEEVENT te;
        te.cbSize = sizeof(te); te.dwFlags = TME_LEAVE; te.hwndTrack = h; te.dwHoverTime = 0;
        TrackMouseEvent(&te);
        if (i != g_side_hot) { g_side_hot = i; InvalidateRect(h, NULL, FALSE); }
        return 0;
    }
    case WM_MOUSELEAVE:
        g_side_hot = -1;
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;
    case WM_KEYDOWN:
        if (w == VK_DOWN && g_side_sel < NPAGES - 1) PostMessageW(g_main, WM_APP_PAGE, (WPARAM)(g_side_sel + 1), 0);
        if (w == VK_UP && g_side_sel > 0) PostMessageW(g_main, WM_APP_PAGE, (WPARAM)(g_side_sel - 1), 0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* ------------------------------------------------------------------ */
/* Window construction                                                 */
/* ------------------------------------------------------------------ */
static void build_pages(void)
{
    int i, x;
    static const struct { int id; const WCHAR *t; } tweaks[] = {
        { ID_T_DXVK,    L"Use DXVK for Direct3D 9/10/11 (native d3d9, d3d10core, d3d11, dxgi)" },
        { ID_T_VKD3D,   L"Use VKD3D-Proton for Direct3D 12 (native d3d12, d3d12core)" },
        { ID_T_GRAB,    L"Capture the mouse in full-screen programs" },
        { ID_T_NODECOR, L"Borderless windows (disable window decorations)" },
        { ID_T_FOCUS,   L"Fix focus problems (disable WM_TAKE_FOCUS)" },
        { ID_T_CRASH,   L"Hide the crash dialog when a program crashes" },
        { ID_T_GECKO,   L"Never ask to install Wine Mono / Gecko" }
    };
    static const struct { int id; const WCHAR *t; } tools[] = {
        { ID_TOOL_TASK, L"Task Manager" }, { ID_TOOL_REG, L"Registry Editor" },
        { ID_TOOL_EXPL, L"File Explorer" }, { ID_TOOL_UNINST, L"Uninstaller" }, { ID_TOOL_CMD, L"Command Prompt" }
    };
    RECT cr;
    int px, py, pw, ph;

    GetClientRect(g_main, &cr);
    px = S(SIDE_W + 10); py = S(74);
    pw = cr.right - px - S(10);
    ph = cr.bottom - py - S(62);
    for (i = 0; i < NPAGES; i++)
        g_page[i] = CreateWindowExW(0, L"WCPage", NULL, WS_CHILD | WS_CLIPCHILDREN | (i == 0 ? WS_VISIBLE : 0),
                                    px, py, pw, ph, g_main, NULL, g_inst, NULL);

    /* 0: Applications */
    LABEL(0, L"Choose an application, or Default Settings, then pick the Windows version it should see:", 15, 12, 590);
    mk(0, L"LISTBOX", NULL, LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, 15, 40, 400, 255, ID_APPLIST);
    BTN(0, L"Add application...", 430, 40, 165, 34, ID_APPADD);
    BTN(0, L"Remove", 430, 82, 165, 34, ID_APPDEL);
    LABEL(0, L"Windows version:", 15, 312, 200);
    COMBO(0, 15, 334, 300, ID_APPVER);
    for (i = 0; i < NVER; i++) SendMessageW(ctl(ID_APPVER), CB_ADDSTRING, 0, (LPARAM)g_ver[i].label);

    /* 1: Graphics */
    GROUP(1, L"Direct3D", 10, 8, 595, 165);
    LABEL(1, L"Renderer:", 25, 38, 170);
    COMBO(1, 205, 34, 230, ID_RENDERER);
    LABEL(1, L"Video memory:", 25, 72, 170);
    COMBO(1, 205, 68, 230, ID_VRAM);
    CHECK(1, L"Enable command stream multithreading (CSMT)", 25, 104, 550, ID_CSMT);
    CHECK(1, L"Strict draw ordering (fixes glitches, slower)", 25, 132, 550, ID_STRICT);
    GROUP(1, L"Screen", 10, 185, 595, 150);
    CHECK(1, L"Emulate a virtual desktop", 25, 211, 550, ID_VDESK);
    LABEL(1, L"Desktop size:", 45, 245, 150);
    mk(1, L"EDIT", NULL, ES_NUMBER | ES_CENTER | WS_TABSTOP, WS_EX_CLIENTEDGE, 205, 241, 75, 24, ID_VDW);
    LABEL(1, L"x", 288, 245, 14);
    mk(1, L"EDIT", NULL, ES_NUMBER | ES_CENTER | WS_TABSTOP, WS_EX_CLIENTEDGE, 308, 241, 75, 24, ID_VDH);
    LABEL(1, L"Screen resolution (DPI):", 25, 287, 175);
    mk(1, L"COMBOBOX", NULL, CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP, 0, 205, 283, 120, 200, ID_DPI);
    cb_fill(ctl(ID_RENDERER), g_renderers_lbl, 4);
    cb_fill(ctl(ID_VRAM), g_vram_lbl, 5);
    cb_fill(ctl(ID_DPI), g_dpi_lbl, 5);

    /* 2: Libraries */
    LABEL(2, L"New override for library (e.g. d3d11):", 15, 12, 400);
    mk(2, L"EDIT", NULL, ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, 15, 36, 200, 26, ID_DLLNAME);
    COMBO(2, 225, 36, 200, ID_DLLMODE);
    BTN(2, L"Add / Update", 440, 33, 155, 34, ID_DLLADD);
    mk(2, L"LISTBOX", NULL, LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, 15, 80, 410, 295, ID_DLLLIST);
    BTN(2, L"Remove", 440, 80, 155, 34, ID_DLLDEL);
    for (i = 0; i < NMODE; i++) SendMessageW(ctl(ID_DLLMODE), CB_ADDSTRING, 0, (LPARAM)g_mode[i].label);

    /* 3: Audio & Drives */
    GROUP(3, L"Audio", 10, 8, 595, 70);
    LABEL(3, L"Audio driver:", 25, 38, 170);
    COMBO(3, 205, 34, 260, ID_AUDIO);
    cb_fill(ctl(ID_AUDIO), g_audio_lbl, 4);
    GROUP(3, L"Drives in this Wine prefix", 10, 90, 595, 280);
    mk(3, L"LISTBOX", NULL, WS_VSCROLL | LBS_NOSEL, WS_EX_CLIENTEDGE, 25, 116, 565, 240, ID_DRIVES);

    /* 4: Winlator tweaks */
    GROUP(4, L"Compatibility", 10, 8, 595, 220);
    for (i = 0; i < 7; i++) CHECK(4, tweaks[i].t, 25, 34 + 26 * i, 565, tweaks[i].id);
    GROUP(4, L"Quick tools", 10, 240, 595, 95);
    for (i = 0, x = 25; i < 5; i++, x += 114) BTN(4, tools[i].t, x, 274, 108, 40, tools[i].id);

    /* 5: Files */
    LABEL(5, L"Folder:", 15, 14, 60);
    mk(5, L"EDIT", NULL, ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, 75, 10, 360, 26, ID_F_PATH);
    BTN(5, L"Go", 445, 8, 70, 30, ID_F_GO);
    BTN(5, L"Up", 525, 8, 70, 30, ID_F_UP);
    LABEL(5, L"Search:", 15, 50, 60);
    mk(5, L"EDIT", NULL, ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, 75, 46, 360, 26, ID_F_SEARCH);
    BTN(5, L"Find in folder", 445, 44, 150, 30, ID_F_FIND);
    mk(5, L"LISTBOX", NULL, LBS_NOTIFY | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, 15, 84, 230, 250, ID_F_LIST);
    mk(5, L"STATIC", L"No file open", SS_LEFT | SS_PATHELLIPSIS, 0, 260, 84, 335, 20, ID_F_NAME);
    mk(5, L"EDIT", NULL, ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP,
       WS_EX_CLIENTEDGE, 260, 108, 335, 226, ID_F_EDIT);
    BTN(5, L"Save", 260, 342, 110, 34, ID_F_SAVE);
    BTN(5, L"Reload", 380, 342, 110, 34, ID_F_RELOAD);
    mk(5, L"STATIC", L"", SS_LEFT, 0, 15, 342, 235, 40, ID_F_STAT);

    /* 6: About & Update */
    mk(6, L"STATIC", NULL, SS_LEFT, 0, 15, 12, 580, 150, ID_ABOUT_INFO);
    GROUP(6, L"Updates", 10, 175, 595, 160);
    BTN(6, L"Check for updates", 25, 206, 210, 38, ID_UPD_CHECK);
    BTN(6, L"Install as default winecfg", 245, 206, 230, 38, ID_UPD_INSTALL);
    mk(6, L"STATIC", L"", SS_LEFT, 0, 25, 258, 565, 60, ID_UPD_STATUS);

    EnumChildWindows(g_main, set_font_cb, (LPARAM)g_font);
    SendMessageW(ctl(ID_F_EDIT), WM_SETFONT, (WPARAM)g_font_mono, TRUE);
    SendMessageW(ctl(ID_F_LIST), WM_SETFONT, (WPARAM)g_font_mono, TRUE);
}

static void load_all(void)
{
    refresh_apps(0);
    load_graphics();
    refresh_dlls();
    load_audio();
    load_drives();
    load_tweaks();
    fill_about();
}

static void save_all(void)
{
    save_graphics();
    save_audio();
    save_tweaks();
    MessageBoxW(g_main, L"Settings applied.\nRestart running programs for them to take effect.",
                L"Winlator Configuration", MB_OK | MB_ICONINFORMATION);
}

static void show_page(int n)
{
    RECT cr, hr;
    int i;
    if (n < 0 || n >= NPAGES) return;
    if (n != g_side_sel && g_side_sel == 5 && !confirm_discard()) return;
    for (i = 0; i < NPAGES; i++) ShowWindow(g_page[i], i == n ? SW_SHOW : SW_HIDE);
    g_side_sel = n;
    InvalidateRect(g_side, NULL, FALSE);
    GetClientRect(g_main, &cr);
    SetRect(&hr, S(SIDE_W), 0, cr.right, S(72));
    InvalidateRect(g_main, &hr, TRUE);
    if (n == 5 && !g_files_inited) files_init();
}

static void draw_button(const DRAWITEMSTRUCT *d)
{
    HDC dc = d->hDC;
    RECT rc = d->rcItem;
    WCHAR t[96];
    BOOL primary = d->CtlID == ID_APPLY || d->CtlID == ID_UPD_CHECK || d->CtlID == ID_F_SAVE;
    BOOL down = (d->itemState & ODS_SELECTED) != 0, dis = (d->itemState & ODS_DISABLED) != 0;
    BOOL inbar = d->CtlID == ID_APPLY || d->CtlID == ID_CLOSE;
    COLORREF fill = primary ? (down ? C_ORANGE2 : C_ORANGE) : (down ? RGB(214, 210, 206) : RGB(237, 236, 235));
    COLORREF edge = primary ? fill : RGB(205, 199, 194);
    COLORREF txt = primary ? RGB(255, 255, 255) : C_TEXT;
    HBRUSH bgb = CreateSolidBrush(inbar ? C_BAR : RGB(255, 255, 255));
    HBRUSH b = CreateSolidBrush(fill), ob;
    HPEN p = CreatePen(PS_SOLID, 1, edge), op;
    HFONT of;
    FillRect(dc, &rc, bgb);
    ob = (HBRUSH)SelectObject(dc, b);
    op = (HPEN)SelectObject(dc, p);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, S(10), S(10));
    SelectObject(dc, op); SelectObject(dc, ob);
    GetWindowTextW(d->hwndItem, t, 96);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, dis ? RGB(160, 160, 160) : txt);
    of = (HFONT)SelectObject(dc, g_font);
    if (down) OffsetRect(&rc, 0, 1);
    DrawTextW(dc, t, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
    if (d->itemState & ODS_FOCUS) {
        RECT fr = d->rcItem;
        InflateRect(&fr, -S(4), -S(4));
        DrawFocusRect(dc, &fr);
    }
    DeleteObject(b); DeleteObject(p); DeleteObject(bgb);
}

static void paint_main(HWND hwnd)
{
    PAINTSTRUCT ps; RECT rc, bar;
    HDC dc = BeginPaint(hwnd, &ps);
    HBRUSH bb = CreateSolidBrush(C_BAR), lb = CreateSolidBrush(RGB(225, 222, 219)), ob2;
    HFONT old;
    GetClientRect(hwnd, &rc);
    /* header */
    SetBkMode(dc, TRANSPARENT);
    old = (HFONT)SelectObject(dc, g_font_head);
    SetTextColor(dc, C_TEXT);
    TextOutW(dc, S(SIDE_W + 26), S(12), g_titles[g_side_sel], (int)wcslen(g_titles[g_side_sel]));
    SelectObject(dc, g_font_sub);
    SetTextColor(dc, C_SUB);
    TextOutW(dc, S(SIDE_W + 27), S(45), g_subs[g_side_sel], (int)wcslen(g_subs[g_side_sel]));
    SetRect(&bar, S(SIDE_W), S(70), rc.right, S(71));
    FillRect(dc, &bar, lb);
    /* orange accent under the title */
    ob2 = CreateSolidBrush(C_ORANGE);
    SetRect(&bar, S(SIDE_W + 26), S(69), S(SIDE_W + 66), S(72));
    FillRect(dc, &bar, ob2);
    DeleteObject(ob2);
    /* bottom bar */
    SetRect(&bar, S(SIDE_W), rc.bottom - S(62), rc.right, rc.bottom);
    FillRect(dc, &bar, bb);
    bar.bottom = bar.top + 1;
    FillRect(dc, &bar, lb);
    SelectObject(dc, old);
    DeleteObject(bb); DeleteObject(lb);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE: {
        RECT cr;
        g_main = hwnd;
        GetClientRect(hwnd, &cr);
        g_side = CreateWindowExW(0, L"WCSide", NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                 0, 0, S(SIDE_W), cr.bottom, hwnd, (HMENU)(INT_PTR)ID_SIDE, g_inst, NULL);
        build_pages();
        CreateWindowExW(0, L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                        cr.right - S(238), cr.bottom - S(50), S(110), S(38), hwnd, (HMENU)(INT_PTR)ID_APPLY, g_inst, NULL);
        CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                        cr.right - S(120), cr.bottom - S(50), S(110), S(38), hwnd, (HMENU)(INT_PTR)ID_CLOSE, g_inst, NULL);
        {
            HWND b;
            if ((b = GetDlgItem(hwnd, ID_APPLY)) != NULL) SendMessageW(b, WM_SETFONT, (WPARAM)g_font, TRUE);
            if ((b = GetDlgItem(hwnd, ID_CLOSE)) != NULL) SendMessageW(b, WM_SETFONT, (WPARAM)g_font, TRUE);
        }
        load_all();
        return 0;
    }
    case WM_PAINT:
        paint_main(hwnd);
        return 0;
    case WM_DRAWITEM:
        if (((DRAWITEMSTRUCT *)l)->CtlType == ODT_BUTTON) { draw_button((DRAWITEMSTRUCT *)l); return TRUE; }
        return FALSE;
    case WM_APP_PAGE:
        show_page((int)w);
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        WCHAR msg[512];
        switch (id) {
        case IDOK:
            if (GetFocus() == ctl(ID_F_PATH)) files_go();
            else if (GetFocus() == ctl(ID_F_SEARCH)) files_find();
            break;
        case ID_APPLY: save_all(); break;
        case ID_CLOSE: SendMessageW(hwnd, WM_CLOSE, 0, 0); break;
        case ID_APPLIST: if (code == LBN_SELCHANGE) on_app_sel(); break;
        case ID_APPADD: app_add(); break;
        case ID_APPDEL: app_del(); break;
        case ID_APPVER: if (code == CBN_SELCHANGE) app_ver_changed(); break;
        case ID_VDESK: vdesk_enable(); break;
        case ID_DLLLIST: if (code == LBN_SELCHANGE) dll_selected(); break;
        case ID_DLLADD: dll_add(); break;
        case ID_DLLDEL: dll_del(); break;
        case ID_TOOL_TASK: run_tool(L"taskmgr.exe"); break;
        case ID_TOOL_REG: run_tool(L"regedit.exe"); break;
        case ID_TOOL_EXPL: run_tool(L"explorer.exe"); break;
        case ID_TOOL_UNINST: run_tool(L"uninstaller.exe"); break;
        case ID_TOOL_CMD: run_tool(L"cmd.exe"); break;
        case ID_F_GO: if (confirm_discard()) files_go(); break;
        case ID_F_UP: if (confirm_discard()) files_up(); break;
        case ID_F_FIND: files_find(); break;
        case ID_F_LIST: if (code == LBN_SELCHANGE || code == LBN_DBLCLK) files_clicked(code); break;
        case ID_F_EDIT: if (code == EN_CHANGE) g_dirty = TRUE; break;
        case ID_F_SAVE: file_save(); break;
        case ID_F_RELOAD: if (g_file[0] && confirm_discard()) file_open(g_file); break;
        case ID_UPD_CHECK: do_update(); break;
        case ID_UPD_INSTALL:
            install_self(msg, 511);
            msg[511] = 0;
            set_status(msg);
            break;
        }
        return 0;
    }
    case WM_CLOSE:
        if (!confirm_discard()) return 0;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, m, w, l);
}

static HFONT make_font(int pt, int weight, const WCHAR *face)
{
    return CreateFontW(-MulDiv(pt, g_dpi, 72), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, face);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show)
{
    INITCOMMONCONTROLSEX icc;
    WNDCLASSEXW wc;
    RECT r;
    HDC dc;
    MSG msg;
    HICON ico;
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    (void)prev;
    g_inst = inst;

    if (cmd && (wcsstr(cmd, L"/install") || wcsstr(cmd, L"/uninstall"))) {
        WCHAR msg2[512];
        BOOL ok = wcsstr(cmd, L"/uninstall") ? uninstall_self(msg2, 511) : install_self(msg2, 511);
        msg2[511] = 0;
        MessageBoxW(NULL, msg2, L"Winlator Configuration", MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONERROR));
        return ok ? 0 : 1;
    }

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    dc = GetDC(NULL);
    g_dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    if (g_dpi < 96) g_dpi = 96;
    g_font = make_font(9, FW_NORMAL, L"Ubuntu");
    g_font_head = make_font(17, FW_SEMIBOLD, L"Ubuntu");
    g_font_sub = make_font(9, FW_NORMAL, L"Ubuntu");
    g_font_side = make_font(10, FW_NORMAL, L"Ubuntu");
    g_font_sideb = make_font(12, FW_SEMIBOLD, L"Ubuntu");
    g_font_mono = make_font(9, FW_NORMAL, L"Ubuntu Mono");

    ico = LoadIconW(inst, MAKEINTRESOURCEW(2));
    if (!ico) ico = LoadIcon(NULL, IDI_APPLICATION);

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = PageProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"WCPage";
    RegisterClassExW(&wc);
    wc.lpfnWndProc = SideProc;
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"WCSide";
    RegisterClassExW(&wc);
    wc.lpfnWndProc = MainProc;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"WinlatorCfgMain";
    wc.hIcon = ico;
    wc.hIconSm = ico;
    RegisterClassExW(&wc);

    SetRect(&r, 0, 0, S(840), S(580));
    AdjustWindowRect(&r, style, FALSE);
    g_main = CreateWindowExW(0, L"WinlatorCfgMain", L"Winlator Configuration", style,
                             (GetSystemMetrics(SM_CXSCREEN) - (r.right - r.left)) / 2,
                             (GetSystemMetrics(SM_CYSCREEN) - (r.bottom - r.top)) / 2,
                             r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    if (!g_main) return 1;
    ShowWindow(g_main, show);
    UpdateWindow(g_main);
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(g_main, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    return (int)msg.wParam;
}
