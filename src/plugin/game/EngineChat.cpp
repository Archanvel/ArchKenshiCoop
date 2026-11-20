// EngineChat.cpp - the in-game lobby chat overlay (2026-11-20).
//
// The MULTIPLAYER window's chat only exists at the title screen. In a running
// game the same Steam lobby chat is shown here: the last few lines fade in at
// the top-left when somebody writes, and ENTER opens a one-line input box.
//
// Raw MyGUI, like the lobby window: a root panel on the "Popup" layer holding a
// read-only word-wrapped view and an EditBox. The EditBox is given the MyGUI key
// focus explicitly, which is what makes Kenshi hand it the keyboard (its own
// rename / save-name boxes work the same way) instead of running hotkeys.
//
// Owner state: section-private statics only. Main thread only. The GUI drops
// every widget on a world load and tells nobody, so all pointers are cleared
// through an IUnlinkWidget and the overlay is re-minted lazily.

#include "EngineInternal.h"
#include "EngineUi.h"

#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_WidgetManager.h>
#include <mygui/MyGUI_IUnlinkWidget.h>
#include <mygui/MyGUI_RenderManager.h>
#include <mygui/MyGUI_InputManager.h>
#include <mygui/MyGUI_TextBox.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_Delegate.h>

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace coop {
namespace engine {
namespace {

void** chatMyguiData(const char* sym) {
    HMODULE m = GetModuleHandleA("MyGUIEngine_x64.dll");
    return m ? (void**)GetProcAddress(m, sym) : 0;
}
MyGUI::Gui* chatGui() {
    static void** p = chatMyguiData("?msInstance@?$Singleton@VGui@MyGUI@@@MyGUI@@0PEAVGui@2@EA");
    return p ? (MyGUI::Gui*)*p : 0;
}
MyGUI::WidgetManager* chatWidgets() {
    static void** p = chatMyguiData("?msInstance@?$Singleton@VWidgetManager@MyGUI@@@MyGUI@@0PEAVWidgetManager@2@EA");
    return p ? (MyGUI::WidgetManager*)*p : 0;
}
MyGUI::RenderManager* chatRender() {
    static void** p = chatMyguiData("?msInstance@?$Singleton@VRenderManager@MyGUI@@@MyGUI@@0PEAVRenderManager@2@EA");
    return p ? (MyGUI::RenderManager*)*p : 0;
}
MyGUI::InputManager* chatInput() {
    static void** p = chatMyguiData("?msInstance@?$Singleton@VInputManager@MyGUI@@@MyGUI@@0PEAVInputManager@2@EA");
    return p ? (MyGUI::InputManager*)*p : 0;
}

struct ChatUi {
    MyGUI::Widget*  root;
    MyGUI::Widget*  card;     // backdrop, shown while typing
    MyGUI::EditBox* view;     // the lines
    MyGUI::EditBox* edit;     // the input
    bool            typing;
    bool            accept;   // Enter pressed in the input (handled next tick)
    bool            enterWasDown;
    bool            escWasDown;
    unsigned        seen;     // chat serial shown
    bool            seenInit;
    DWORD           showUntil;
    DWORD           closedAt;
    bool            hinted;
    bool            unlinkerOn;
    ChatUi() : root(0), card(0), view(0), edit(0), typing(false), accept(false),
               enterWasDown(false), escWasDown(false), seen(0), seenInit(false),
               showUntil(0), closedAt(0), hinted(false), unlinkerOn(false) {}
};
ChatUi g_chat;

class ChatUnlinker : public MyGUI::IUnlinkWidget {
public:
    virtual void _unlinkWidget(MyGUI::Widget* w) {
        if (w == g_chat.view) g_chat.view = 0;
        if (w == g_chat.edit) { g_chat.edit = 0; g_chat.typing = false; }
        if (w == g_chat.card) g_chat.card = 0;
        if (w == g_chat.root) {
            g_chat.root = 0; g_chat.card = 0; g_chat.view = 0; g_chat.edit = 0;
            g_chat.typing = false;
        }
    }
};
ChatUnlinker g_chatUnlinker;

void onChatEditAccept(MyGUI::EditBox*) { g_chat.accept = true; }

// MyGUI reads '#' as a colour tag; double it so chat text is shown literally.
std::string chatEsc(const char* s) {
    std::string o;
    for (; s && *s; ++s) { if (*s == '#') o += '#'; o += *s; }
    return o;
}

bool gameHasFocus() {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

void chatDestroy() {
    MyGUI::Gui* gui = chatGui();
    if (gui && g_chat.root) gui->destroyWidget(g_chat.root);
    g_chat.root = 0; g_chat.card = 0; g_chat.view = 0; g_chat.edit = 0;
    g_chat.typing = false;
}

bool chatBuild() {
    MyGUI::Gui* gui = chatGui();
    MyGUI::RenderManager* rm = chatRender();
    if (!gui || !rm) return false;
    const MyGUI::IntSize vs = rm->getViewSize();
    int w = vs.width * 30 / 100;
    if (w < 360) w = 360;
    const int h = 170;
    const int x = (vs.width - w) / 2, y = vs.height / 100; // top centre: the left edge is the game's own message list
    g_chat.root = gui->createWidget<MyGUI::Widget>("PanelEmpty", MyGUI::IntCoord(x, y, w, h),
                                                   MyGUI::Align::Default, "Popup", "KenshiCoopChat");
    if (!g_chat.root) return false;
    g_chat.root->setNeedMouseFocus(false);
    g_chat.card = g_chat.root->createWidget<MyGUI::Widget>("Kenshi_GenericTextBoxFlat",
        MyGUI::IntCoord(0, 0, w, h), MyGUI::Align::Default);
    if (g_chat.card) { g_chat.card->setNeedMouseFocus(false); g_chat.card->setVisible(false); }
    g_chat.view = g_chat.root->createWidget<MyGUI::EditBox>("Kenshi_WordWrapEmpty",
        MyGUI::IntCoord(6, 4, w - 12, h - 46), MyGUI::Align::Default);
    if (g_chat.view) {
        g_chat.view->setEditMultiLine(true);
        g_chat.view->setEditWordWrap(true);
        g_chat.view->setEditReadOnly(true);
        g_chat.view->setEditStatic(true);
        g_chat.view->setFontName("Kenshi_StandardFont_Medium");
        g_chat.view->setTextColour(MyGUI::Colour(1.0f, 1.0f, 1.0f, 1.0f));
        g_chat.view->setTextShadow(true);
        g_chat.view->setNeedMouseFocus(false);
    }
    g_chat.edit = g_chat.root->createWidget<MyGUI::EditBox>("Kenshi_EditBox",
        MyGUI::IntCoord(4, h - 38, w - 8, 34), MyGUI::Align::Default);
    if (g_chat.edit) {
        g_chat.edit->setFontName("Kenshi_StandardFont_Medium");
        g_chat.edit->setMaxTextLength(180);
        g_chat.edit->eventEditSelectAccept += MyGUI::newDelegate(&onChatEditAccept);
        g_chat.edit->setVisible(false);
    }
    g_chat.root->setVisible(false);
    g_chat.seenInit = false;
    coop::logLine("[chat] in-game overlay built");
    return g_chat.view != 0 && g_chat.edit != 0;
}

void chatOpenInput() {
    MyGUI::InputManager* in = chatInput();
    if (!in || !g_chat.edit || !g_chat.root) return;
    g_chat.typing = true;
    g_chat.accept = false;
    g_chat.root->setVisible(true);
    if (g_chat.card) g_chat.card->setVisible(true);
    g_chat.edit->setVisible(true);
    g_chat.edit->setOnlyText(MyGUI::UString(""));
    in->setKeyFocusWidget(g_chat.edit);
    coop::logLine("[chat] input opened");
}

void chatCloseInput() {
    MyGUI::InputManager* in = chatInput();
    if (in && g_chat.edit && in->getKeyFocusWidget() == g_chat.edit) in->resetKeyFocusWidget();
    if (g_chat.edit) g_chat.edit->setVisible(false);
    if (g_chat.card) g_chat.card->setVisible(false);
    g_chat.typing = false;
    g_chat.accept = false;
    g_chat.closedAt = GetTickCount();
}

} // namespace

void chatOverlayTick(const ChatOverlayState* st) {
    if (!st) return;
    if (!st->active) {
        if (g_chat.root) chatDestroy();
        g_chat.enterWasDown = false;
        return;
    }
    MyGUI::Gui* gui = chatGui();
    if (!gui) return;
    if (!g_chat.unlinkerOn) {
        MyGUI::WidgetManager* wm = chatWidgets();
        if (!wm) return; // never hold widget pointers without the dangling guard
        wm->registerUnlinker(&g_chatUnlinker);
        g_chat.unlinkerOn = true;
    }
    if (!g_chat.root && !chatBuild()) { chatDestroy(); return; }
    const DWORD now = GetTickCount();

    // New lines: show the overlay for a while.
    if (!g_chat.seenInit || g_chat.seen != st->serial) {
        const bool fresh = g_chat.seenInit && st->count > 0;
        g_chat.seenInit = true;
        g_chat.seen = st->serial;
        std::string all;
        const int first = st->count > 6 ? st->count - 6 : 0;
        for (int i = first; i < st->count; ++i) {
            if (!all.empty()) all += "\n";
            all += chatEsc(st->lines[i]);
        }
        if (all.empty() && st->hint) all = chatEsc(st->hint);
        if (g_chat.view) g_chat.view->setCaption(MyGUI::UString(all));
        if (fresh) g_chat.showUntil = now + 15000;
    }
    // Once per session: tell the player the key (shown for a few seconds).
    if (!g_chat.hinted) {
        g_chat.hinted = true;
        g_chat.showUntil = now + 8000;
    }

    // Test driver (KENSHICOOP_TEST_CHAT): open the input, report whether the
    // box holds the keyboard focus a second later, then send a line through the
    // same path ENTER takes. Synthetic key presses do not reach the game.
    {
        static int   s_test = -1;
        static DWORD s_t0 = 0;
        if (s_test < 0) { const char* e = getenv("KENSHICOOP_TEST_CHAT"); s_test = (e && e[0]) ? 1 : 0; s_t0 = now; }
        if (s_test == 1 && now - s_t0 > 8000) { s_test = 2; chatOpenInput(); }
        else if (s_test == 2 && now - s_t0 > 9500) {
            s_test = 3;
            MyGUI::InputManager* ti = chatInput();
            const bool has = ti && g_chat.edit && ti->getKeyFocusWidget() == g_chat.edit;
            coop::logLine(has ? "[chat] TEST focus held=1" : "[chat] TEST focus held=0");
            if (g_chat.edit) g_chat.edit->setOnlyText(MyGUI::UString("in-game chat test"));
            g_chat.typing = true;
            g_chat.accept = true;
        }
    }

    const bool focus = gameHasFocus();
    const bool enterDown = focus && (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
    const bool escDown   = focus && (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    MyGUI::InputManager* in = chatInput();

    if (g_chat.typing) {
        if (g_chat.accept) {
            std::string text = g_chat.edit ? std::string(g_chat.edit->getOnlyText().asUTF8()) : std::string();
            if (!text.empty() && st->send) st->send(text.c_str());
            chatCloseInput();
            g_chat.showUntil = now + 15000;
        } else if ((escDown && !g_chat.escWasDown) ||
                   (in && g_chat.edit && in->getKeyFocusWidget() != g_chat.edit)) {
            chatCloseInput(); // Esc, or the player clicked somewhere else
        }
    } else if (enterDown && !g_chat.enterWasDown && (now - g_chat.closedAt) > 300 &&
               in && in->getKeyFocusWidget() == 0) {
        // ENTER with no text box of the game's own in use: start typing.
        chatOpenInput();
    }
    g_chat.enterWasDown = enterDown;
    g_chat.escWasDown   = escDown;

    if (g_chat.root)
        g_chat.root->setVisible(g_chat.typing || (int)(g_chat.showUntil - now) > 0);
}

} // namespace engine
} // namespace coop
