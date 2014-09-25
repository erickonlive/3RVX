//
//  Key Storage Format (24 bits):
//  
//  0111 1111 0000 0000 1111 1111
//  ---- ---- --------- ---------
//   |    |       |         |
//   |    |       |         \--- MS Virtual Keys (VK_*)
//   |    |       |
//   |    |       \--- not used
//   |    |
//   |    \--- bitfield: [win, shift, control, alt]
//   |
//   \--- xbutton2, xbutton1, wheelup, wheeldown + future?
//

#pragma once

#include <windows.h>
#include <hash_map>

// - Modifiers ------------------------------------------------------------- //
#define HKM_MOD_ALT     (MOD_ALT     << 16)
#define HKM_MOD_CONTROL (MOD_CONTROL << 16)
#define HKM_MOD_SHIFT   (MOD_SHIFT   << 16)
#define HKM_MOD_WIN     (MOD_WIN     << 16)

// - Mouse ----------------------------------------------------------------- //
#define HKM_MOUSE_WHEEL_UP   0x100000
#define HKM_MOUSE_WHEEL_DOWN 0x200000
#define HKM_MOUSE_XBUTTON_1  0x300000
#define HKM_MOUSE_XBUTTON_2  0x400000



// ------------------------------------------------------------------------- //
class HotkeyManager {  
public:
    static HotkeyManager *Instance();
    int Register(HWND parentWnd, int keyCombination);

protected:
    struct HotkeyInfo {
        int hotkeyId;
        int keys;
        HWND parentWnd;
    };

    int      m_numKeys;
    int      m_fixWin;
    HHOOK    m_keyHook;
    HHOOK    m_mouseHook;
    stdext::hash_map<int, HotkeyInfo*> m_keyMap;


    HotkeyManager(): m_numKeys(1), 
                     m_fixWin(false) 
    {} ;

    bool Hook();
    bool Unhook();
    int CalcModifiers();

    virtual LRESULT CALLBACK
        MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK 
        LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);

    virtual LRESULT CALLBACK
        KeyProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK 
        LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

private:
    static HotkeyManager* instance;
};