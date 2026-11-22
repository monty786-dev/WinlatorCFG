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
#include <commdlg.h>
#include <shellapi.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* ---------- configure these two for your own repository ---------- */
#define APP_VERSION  "v1.0.0"
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
    ID_TAB = 100, ID_APPLY, ID_CLOSE,
    ID_APPLIST = 200, ID_APPADD, ID_APPDEL, ID_APPVER,
    ID_RENDERER, ID_VRAM, ID_CSMT, ID_STRICT, ID_VDESK, ID_VDW, ID_VDH, ID_DPI,
    ID_DLLLIST, ID_DLLNAME, ID_DLLMODE, ID_DLLADD, ID_DLLDEL,
    ID_AUDIO, ID_DRIVES,
    ID_T_CRASH, ID_T_GECKO, ID_T_GRAB, ID_T_NODECOR, ID_T_FOCUS, ID_T_DXVK, ID_T_VKD3D,
    ID_TOOL_TASK, ID_TOOL_REG, ID_TOOL_EXPL, ID_TOOL_UNINST, ID_TOOL_CMD,
    ID_ABOUT_INFO, ID_UPD_CHECK, ID_UPD_INSTALL, ID_UPD_STATUS
};

#define NPAGES 6
static HINSTANCE g_inst;
static HWND g_main, g_tab, g_page[NPAGES];
static HFONT g_font, g_font_title, g_font_sub;
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
#define BTN(pg, t, x, y, w, h, id) mk(pg, L"BUTTON", t, BS_PUSHBUTTON | WS_TABSTOP, 0, x, y, w, h, id)
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
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetBkMode((HDC)w, TRANSPARENT);
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
/* Window construction                                                 */
/* ------------------------------------------------------------------ */
static const WCHAR *g_tabs[NPAGES] = {
    L"Applications", L"Graphics", L"Libraries", L"Audio / Drives", L"Winlator Tweaks", L"About / Update"
};

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
    RECT rc;

    GetClientRect(g_tab, &rc);
    TabCtrl_AdjustRect(g_tab, FALSE, &rc);
    for (i = 0; i < NPAGES; i++)
        g_page[i] = CreateWindowExW(0, L"WCPage", NULL, WS_CHILD | WS_CLIPCHILDREN | (i == 0 ? WS_VISIBLE : 0),
                                    rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                                    g_tab, NULL, g_inst, NULL);

    /* 0: Applications */
    LABEL(0, L"Choose an application, or Default Settings, then pick the Windows version it should see:", 15, 12, 580);
    mk(0, L"LISTBOX", NULL, LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, 15, 40, 390, 215, ID_APPLIST);
    BTN(0, L"Add application...", 420, 40, 165, 30, ID_APPADD);
    BTN(0, L"Remove", 420, 78, 165, 30, ID_APPDEL);
    LABEL(0, L"Windows version:", 15, 272, 200);
    COMBO(0, 15, 294, 300, ID_APPVER);
    for (i = 0; i < NVER; i++) SendMessageW(ctl(ID_APPVER), CB_ADDSTRING, 0, (LPARAM)g_ver[i].label);

    /* 1: Graphics */
    GROUP(1, L"Direct3D", 10, 8, 585, 160);
    LABEL(1, L"Renderer:", 25, 38, 170);
    COMBO(1, 205, 34, 220, ID_RENDERER);
    LABEL(1, L"Video memory:", 25, 72, 170);
    COMBO(1, 205, 68, 220, ID_VRAM);
    CHECK(1, L"Enable command stream multithreading (CSMT)", 25, 102, 550, ID_CSMT);
    CHECK(1, L"Strict draw ordering (fixes glitches, slower)", 25, 128, 550, ID_STRICT);
    GROUP(1, L"Screen", 10, 178, 585, 150);
    CHECK(1, L"Emulate a virtual desktop", 25, 204, 550, ID_VDESK);
    LABEL(1, L"Desktop size:", 45, 238, 150);
    mk(1, L"EDIT", NULL, ES_NUMBER | ES_CENTER | WS_TABSTOP, WS_EX_CLIENTEDGE, 205, 234, 75, 24, ID_VDW);
    LABEL(1, L"x", 288, 238, 14);
    mk(1, L"EDIT", NULL, ES_NUMBER | ES_CENTER | WS_TABSTOP, WS_EX_CLIENTEDGE, 308, 234, 75, 24, ID_VDH);
    LABEL(1, L"Screen resolution (DPI):", 25, 280, 175);
    mk(1, L"COMBOBOX", NULL, CBS_DROPDOWN | WS_VSCROLL | WS_TABSTOP, 0, 205, 276, 120, 200, ID_DPI);
    cb_fill(ctl(ID_RENDERER), g_renderers_lbl, 4);
    cb_fill(ctl(ID_VRAM), g_vram_lbl, 5);
    cb_fill(ctl(ID_DPI), g_dpi_lbl, 5);

    /* 2: Libraries */
    LABEL(2, L"New override for library (e.g. d3d11):", 15, 12, 400);
    mk(2, L"EDIT", NULL, ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, 15, 36, 200, 26, ID_DLLNAME);
    COMBO(2, 225, 36, 200, ID_DLLMODE);
    BTN(2, L"Add / Update", 435, 34, 150, 30, ID_DLLADD);
    mk(2, L"LISTBOX", NULL, LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, 15, 76, 410, 245, ID_DLLLIST);
    BTN(2, L"Remove", 435, 76, 150, 30, ID_DLLDEL);
    for (i = 0; i < NMODE; i++) SendMessageW(ctl(ID_DLLMODE), CB_ADDSTRING, 0, (LPARAM)g_mode[i].label);

    /* 3: Audio & Drives */
    GROUP(3, L"Audio", 10, 8, 585, 70);
    LABEL(3, L"Audio driver:", 25, 38, 170);
    COMBO(3, 205, 34, 260, ID_AUDIO);
    cb_fill(ctl(ID_AUDIO), g_audio_lbl, 4);
    GROUP(3, L"Drives in this Wine prefix", 10, 90, 585, 240);
    mk(3, L"LISTBOX", NULL, WS_VSCROLL | LBS_NOSEL, WS_EX_CLIENTEDGE, 25, 116, 555, 200, ID_DRIVES);

    /* 4: Winlator tweaks */
    GROUP(4, L"Compatibility", 10, 8, 585, 215);
    for (i = 0; i < 7; i++) CHECK(4, tweaks[i].t, 25, 32 + 25 * i, 555, tweaks[i].id);
    GROUP(4, L"Quick tools", 10, 232, 585, 90);
    for (i = 0, x = 25; i < 5; i++, x += 111) BTN(4, tools[i].t, x, 264, 105, 36, tools[i].id);

    /* 5: About & Update */
    mk(5, L"STATIC", NULL, SS_LEFT, 0, 15, 12, 570, 150, ID_ABOUT_INFO);
    GROUP(5, L"Updates", 10, 175, 585, 150);
    BTN(5, L"Check for updates", 25, 204, 210, 34, ID_UPD_CHECK);
    BTN(5, L"Install as default winecfg", 245, 204, 230, 34, ID_UPD_INSTALL);
    mk(5, L"STATIC", L"", SS_LEFT, 0, 25, 252, 555, 60, ID_UPD_STATUS);

    EnumChildWindows(g_tab, set_font_cb, (LPARAM)g_font);
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

static void paint_header(HWND hwnd)
{
    PAINTSTRUCT ps; RECT rc; HDC dc = BeginPaint(hwnd, &ps);
    TRIVERTEX tv[2]; GRADIENT_RECT gr = { 0, 1 };
    HFONT old; int hh = S(60);
    GetClientRect(hwnd, &rc);
    tv[0].x = 0; tv[0].y = 0;
    tv[0].Red = 0x0B00; tv[0].Green = 0x5F00; tv[0].Blue = 0xFF00; tv[0].Alpha = 0;
    tv[1].x = rc.right; tv[1].y = hh;
    tv[1].Red = 0x6A00; tv[1].Green = 0x3D00; tv[1].Blue = 0xE800; tv[1].Alpha = 0;
    GradientFill(dc, tv, 2, &gr, 1, GRADIENT_FILL_RECT_H);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    old = (HFONT)SelectObject(dc, g_font_title);
    TextOutW(dc, S(18), S(8), L"Winlator Configuration", 22);
    SelectObject(dc, g_font_sub);
    SetTextColor(dc, RGB(225, 230, 255));
    TextOutW(dc, S(20), S(37), L"Wine settings for Windows apps on Android", 41);
    SelectObject(dc, old);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE: {
        TCITEMW ti; int i;
        g_main = hwnd;
        g_tab = CreateWindowExW(0, WC_TABCONTROLW, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_TABSTOP,
                                S(10), S(70), S(620), S(395), hwnd, (HMENU)(INT_PTR)ID_TAB, g_inst, NULL);
        SendMessageW(g_tab, WM_SETFONT, (WPARAM)g_font, TRUE);
        memset(&ti, 0, sizeof(ti));
        ti.mask = TCIF_TEXT;
        for (i = 0; i < NPAGES; i++) { ti.pszText = (WCHAR *)g_tabs[i]; SendMessageW(g_tab, TCM_INSERTITEMW, i, (LPARAM)&ti); }
        build_pages();
        CreateWindowExW(0, L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                        S(430), S(478), S(95), S(32), hwnd, (HMENU)(INT_PTR)ID_APPLY, g_inst, NULL);
        CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                        S(535), S(478), S(95), S(32), hwnd, (HMENU)(INT_PTR)ID_CLOSE, g_inst, NULL);
        EnumChildWindows(hwnd, set_font_cb, (LPARAM)g_font);
        load_all();
        return 0;
    }
    case WM_PAINT:
        paint_header(hwnd);
        return 0;
    case WM_NOTIFY:
        if (((NMHDR *)l)->idFrom == ID_TAB && ((NMHDR *)l)->code == TCN_SELCHANGE) {
            int sel = TabCtrl_GetCurSel(g_tab), i;
            for (i = 0; i < NPAGES; i++) ShowWindow(g_page[i], i == sel ? SW_SHOW : SW_HIDE);
        }
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        WCHAR msg[512];
        switch (id) {
        case ID_APPLY: save_all(); break;
        case ID_CLOSE: DestroyWindow(hwnd); break;
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
        case ID_UPD_CHECK: do_update(); break;
        case ID_UPD_INSTALL:
            install_self(msg, 511);
            msg[511] = 0;
            set_status(msg);
            break;
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, m, w, l);
}

static HFONT make_font(int pt, int weight)
{
    return CreateFontW(-MulDiv(pt, g_dpi, 72), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show)
{
    INITCOMMONCONTROLSEX icc;
    WNDCLASSEXW wc;
    RECT r;
    HDC dc;
    MSG msg;
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
    icc.dwICC = ICC_TAB_CLASSES | ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    dc = GetDC(NULL);
    g_dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    if (g_dpi < 96) g_dpi = 96;
    g_font = make_font(9, FW_NORMAL);
    g_font_title = make_font(17, FW_SEMIBOLD);
    g_font_sub = make_font(9, FW_NORMAL);

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = PageProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"WCPage";
    RegisterClassExW(&wc);
    wc.lpfnWndProc = MainProc;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"WinlatorCfgMain";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    SetRect(&r, 0, 0, S(640), S(525));
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
