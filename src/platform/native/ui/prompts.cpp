// The button prompts follow the controller in use: the last device that gave input (the keyboard, or a gamepad by its SDL type)
// decides how the game's texts show their button glyphs (SetPromptLabels): a PlayStation pad the game's own glyphs, an Xbox (or any
// other) pad A B X Y LB RB LT RT, a Nintendo pad its own letters, the keyboard the bound keys' names. The menus' select and back
// buttons are labelled as the menus take them (with a Nintendo pad's own layout: A, the right button, selects and B, the bottom
// one, goes back). A pad's buttons are drawn in the glyphs' places: the Xbox's own glyphs read from the user's Xbox disc, else
// drawn in the game's style (buttonart.cpp)
#include "ui/bindings.h"
#include "ui/overlayinternal.h"

#include "native.h"

#include "game/gamecontroller.h"
#include "game/menus.h"
#include "game/pads.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>

// The graphics' glyph images (src/platform/native/graphics/): a glyph code of the game's font drawn from an image (RGBA, red the low
// byte; none: the font's own glyph again), the glyph's place and size kept. Whether the graphics can: until they define it this
// stand-in can't, and the prompts are letters in the font
// TODO(native-graphics): drop the stand-in once NativeGraphicsSetGlyphImage is on macos-support
__attribute__((weak)) bool NativeGraphicsSetGlyphImage(u8, const u32*, int, int)
{
    return false;
}

namespace NativeUi
{
namespace
{
PromptStyle g_Style = PromptPlayStation;
// The glyphs drawn as the Xbox's (or the Switch's) art for the style in use
bool g_DrawnAsArt[PromptGlyphCount] = {};
// The style of the last gamepad used (or plugged in): the controls page names the pad's buttons by it
PromptStyle g_PadStyle = PromptXbox;
bool g_PadStyleKnown = false;
bool g_StyleKnown = false;
std::string g_Labels[PromptGlyphCount];

// The functions the glyphs stand for, in PromptGlyphs' order
constexpr BindingFunction GlyphFunctions[PromptGlyphCount] = {BindCross, BindCircle, BindSquare, BindTriangle,
                                                              BindL1,    BindR1,     BindL2,     BindR2};

// A gamepad button's label on a pad of the style
std::string PadLabel(PromptStyle style, s32 button)
{
    if (button >= GamepadTriggerBase)
    {
        bool left = button == GamepadTriggerBase;
        return style == PromptSwitch ? (left ? "zl" : "zr") : (left ? "lt" : "rt");
    }

    switch (button)
    {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        return style == PromptSwitch ? "b" : "a";
    case SDL_GAMEPAD_BUTTON_EAST:
        return style == PromptSwitch ? "a" : "b";
    case SDL_GAMEPAD_BUTTON_WEST:
        return style == PromptSwitch ? "y" : "x";
    case SDL_GAMEPAD_BUTTON_NORTH:
        return style == PromptSwitch ? "x" : "y";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        return style == PromptSwitch ? "l" : "lb";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        return style == PromptSwitch ? "r" : "rb";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        return "ls";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
        return "rs";
    case SDL_GAMEPAD_BUTTON_START:
        return style == PromptSwitch ? "+" : "start";
    case SDL_GAMEPAD_BUTTON_BACK:
        return style == PromptSwitch ? "-" : "back";
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        return "d-pad up";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        return "d-pad down";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        return "d-pad left";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        return "d-pad right";
    default:
        return "";
    }
}

std::string KeyLabel(const Binding& binding)
{
    char name[64];
    KeyName(binding.keys[0] >= 0 ? binding.keys[0] : binding.keys[1], name, sizeof(name));
    return name;
}

// A pad label's art (the Xbox's glyph of that letter or shoulder; the Switch's grey one)
ButtonArtLabel ArtLabelOf(const std::string& label)
{
    if (label == "a")
    {
        return ArtA;
    }

    if (label == "b")
    {
        return ArtB;
    }

    if (label == "x")
    {
        return ArtX;
    }

    if (label == "y")
    {
        return ArtY;
    }

    if (label == "lb" || label == "l")
    {
        return ArtLeftShoulder;
    }

    if (label == "rb" || label == "r")
    {
        return ArtRightShoulder;
    }

    if (label == "lt" || label == "zl")
    {
        return ArtLeftTrigger;
    }

    return label == "rt" || label == "zr" ? ArtRightTrigger : ButtonArtLabels;
}

PromptStyle PadStyle(SDL_Gamepad* gamepad)
{
    switch (SDL_GetGamepadType(gamepad))
    {
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
        return PromptPlayStation;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        return PromptSwitch;
    default:
        // Xbox pads, and any other (standard, unknown): the Xbox's
        return PromptXbox;
    }
}

PromptStyle StyleFromEnvironment(bool* forced)
{
    const char* forcedStyle = std::getenv("TWIN_UI_PROMPTS");
    *forced = forcedStyle != nullptr && forcedStyle[0] != '\0';
    if (!*forced)
    {
        // The options' controller choice: automatic, or the one chosen
        constexpr PromptStyle Chosen[] = {PromptKeyboard, PromptKeyboard, PromptXbox, PromptPlayStation, PromptSwitch};
        s32 choice = GetOption(OptionController);
        *forced = choice != 0;
        return Chosen[choice];
    }

    return std::strcmp(forcedStyle, "xbox") == 0         ? PromptXbox
           : std::strcmp(forcedStyle, "switch") == 0     ? PromptSwitch
           : std::strcmp(forcedStyle, "playstation") == 0 ? PromptPlayStation
                                                         : PromptKeyboard;
}

// The functions the menus' select and back are on for the style: select on the bottom button (cross's place), back on the right
// one (circle's: the Xbox build's B, the modern games' back) with triangle's place too (the original's back, kept as an alias). A
// Nintendo pad in its own layout: A, the right button, selects and B, the bottom one, goes back (triangle's place too)
void MenuFunctions(PromptStyle style, BindingFunction* select, BindingFunction* back)
{
    *select = BindCross;
    *back = BindCircle;
    if (style == PromptSwitch && GetOption(OptionNintendoLayout) != 0)
    {
        *select = BindCircle;
        *back = BindCross;
    }
}

u8 PadButtonOf(BindingFunction function)
{
    switch (function)
    {
    case BindCircle:
        return PadCircle;
    case BindTriangle:
        return PadTriangle;
    case BindSquare:
        return PadSquare;
    default:
        return PadCross;
    }
}

// The menus' bindings (OLEG's) given the style's select and back, triangle's place always back too
void ApplyMenuLayout()
{
    BindingFunction select;
    BindingFunction back;
    MenuFunctions(g_Style, &select, &back);
    GameController* controller = G_GameController;
    if (controller == nullptr || controller->oleg.bindings.actions == nullptr)
    {
        return;
    }

    ButtonBinding& selectBinding = controller->oleg.bindings.actions[MenuInput::ActionSelect];
    ButtonBinding& backBinding = controller->oleg.bindings.actions[MenuInput::ActionBack];
    selectBinding.buttons[0] = PadButtonOf(select);
    selectBinding.flags.count = 1;
    backBinding.buttons[0] = PadButtonOf(back);
    backBinding.buttons[1] = PadTriangle;
    backBinding.flags.count = 2;
}

std::string Label(PromptStyle style, BindingFunction function)
{
    const Binding& binding = GetBinding(function);
    switch (style)
    {
    case PromptKeyboard:
        return KeyLabel(binding);
    case PromptXbox:
    case PromptSwitch:
        return PadLabel(style, binding.button);
    default:
        return "";
    }
}

// A function's glyph (its first: L1's for the left shoulder), in PromptGlyphs' order
u32 GlyphOf(BindingFunction function)
{
    for (u32 glyph = 0; glyph < PromptGlyphCount; glyph++)
    {
        if (GlyphFunctions[glyph] == function)
        {
            return glyph;
        }
    }

    return 0;
}

// A glyph's label: its function's, a pad's L2 and R2 by the function's second button (L1 and L2 are one function, the shoulder:
// the bumper and, by default, the trigger)
std::string GlyphLabel(PromptStyle style, u32 glyph)
{
    const Binding& binding = GetBinding(GlyphFunctions[glyph]);
    bool trigger = PromptGlyphs[glyph] == '\xA6' || PromptGlyphs[glyph] == '\xAC';
    if (trigger && (style == PromptXbox || style == PromptSwitch) && binding.button2 >= 0)
    {
        return PadLabel(style, binding.button2);
    }

    return Label(style, GlyphFunctions[glyph]);
}
}

PromptStyle PadPromptStyle()
{
    return g_Style != PromptKeyboard ? g_Style : g_PadStyle;
}

std::string PadButtonLabel(s32 button)
{
    PromptStyle style = g_Style != PromptKeyboard ? g_Style : g_PadStyle;
    if (style == PromptPlayStation)
    {
        char name[16];
        ButtonName(button, name, sizeof(name));
        return button < 0 ? "" : name;
    }

    return PadLabel(style, button);
}

// A function's prompt in the native UI's overlay: the PlayStation's glyph code (the overlay draws its button), else its label (the
// overlay draws the Xbox's and the Switch's art for the letters, a keycap for the rest)
std::string PromptOf(BindingFunction function)
{
    for (u32 glyph = 0; glyph < PromptGlyphCount; glyph++)
    {
        if (GlyphFunctions[glyph] == function && g_Style == PromptPlayStation)
        {
            return std::string(1, PromptGlyphs[glyph]);
        }
    }

    return Label(g_Style, function);
}

std::string GlyphPrompt(char glyph)
{
    // A text's glyph code's prompt: the glyph itself on a PlayStation pad, else the function's label in the style
    for (u32 index = 0; index < PromptGlyphCount; index++)
    {
        if (PromptGlyphs[index] == glyph)
        {
            return g_Style == PromptPlayStation ? std::string(1, glyph) : GlyphLabel(g_Style, index);
        }
    }

    return std::string(1, glyph);
}

std::string PromptOfButton(s32 button)
{
    // A pad button's prompt for the style: the face buttons by the functions on them by default
    switch (button)
    {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        return PromptOf(BindCross);
    case SDL_GAMEPAD_BUTTON_EAST:
        return PromptOf(BindCircle);
    case SDL_GAMEPAD_BUTTON_WEST:
        return PromptOf(BindSquare);
    case SDL_GAMEPAD_BUTTON_NORTH:
        return PromptOf(BindTriangle);
    default:
        return PadLabel(g_Style == PromptKeyboard ? g_PadStyle : g_Style, button);
    }
}

std::string MenuSelectPrompt()
{
    BindingFunction select;
    BindingFunction back;
    MenuFunctions(g_Style, &select, &back);
    return g_Style == PromptKeyboard ? "enter" : PromptOf(select);
}

std::string MenuBackPrompt()
{
    BindingFunction select;
    BindingFunction back;
    MenuFunctions(g_Style, &select, &back);
    return g_Style == PromptKeyboard ? "esc" : PromptOf(back);
}

std::string TabPrompt(bool next)
{
    if (g_Style == PromptKeyboard)
    {
        return next ? "e" : "q";
    }

    return PromptOf(next ? BindR1 : BindL1);
}

u32 MenuSelectBit()
{
    BindingFunction select;
    BindingFunction back;
    MenuFunctions(g_Style, &select, &back);
    return select == BindCircle ? PadBitCircle : PadBitCross;
}

u32 MenuBackBit()
{
    BindingFunction select;
    BindingFunction back;
    MenuFunctions(g_Style, &select, &back);
    return back == BindCircle ? PadBitCircle : back == BindCross ? PadBitCross : PadBitTriangle;
}

PromptStyle CurrentPromptStyle()
{
    return g_Style;
}


void UpdatePrompts()
{
    // The glyphs as the gameplay has them (by the pad's places), and the menus' lines' select and back as the menus take them. With
    // a pad's art (the user's Xbox disc's, else the drawn: GlyphButtonArt) and the graphics drawing a code's glyph from an image, a
    // code keeps its glyph and the glyph is drawn as the art of its button's label; without it the label is written in the font
    bool artStyle = g_Style == PromptXbox || g_Style == PromptSwitch;
    const char* labels[PromptGlyphCount] = {};
    bool* drawnAsArt = g_DrawnAsArt;
    for (u32 glyph = 0; glyph < PromptGlyphCount; glyph++)
    {
        g_Labels[glyph] = GlyphLabel(g_Style, glyph);
        const ButtonArt* image = artStyle ? GlyphButtonArt(g_Style, ArtLabelOf(g_Labels[glyph])) : nullptr;
        u8 code = static_cast<u8>(PromptGlyphs[glyph]);
        if (image != nullptr && NativeGraphicsSetGlyphImage(code, image->pixels.data(), image->width, image->height))
        {
            drawnAsArt[glyph] = true;
            labels[glyph] = nullptr;
            continue;
        }

        // The keyboard's: a function on a mouse button (its first key) drawn as the mouse with that button lit
        s32 key = GetBinding(GlyphFunctions[glyph]).keys[0];
        OverlayImage mouse = key == KeyMouseLeft    ? OverlayImage::MouseLeft
                             : key == KeyMouseRight  ? OverlayImage::MouseRight
                             : key == KeyMouseMiddle ? OverlayImage::MouseMiddle
                             : key == KeyWheelUp     ? OverlayImage::MouseWheelUp
                             : key == KeyWheelDown   ? OverlayImage::MouseWheelDown
                                                     : OverlayImage::Count;
        std::vector<u32> mousePixels;
        s32 mouseWidth = 0;
        s32 mouseHeight = 0;
        if (g_Style == PromptKeyboard && mouse != OverlayImage::Count && MouseArt(mouse, &mousePixels, &mouseWidth, &mouseHeight) &&
            NativeGraphicsSetGlyphImage(code, mousePixels.data(), mouseWidth, mouseHeight))
        {
            drawnAsArt[glyph] = true;
            labels[glyph] = nullptr;
            continue;
        }

        drawnAsArt[glyph] = false;
        NativeGraphicsSetGlyphImage(code, nullptr, 0, 0);
        labels[glyph] = g_Labels[glyph].empty() ? nullptr : g_Labels[glyph].c_str();
    }

    // The menus' select and back: the keyboard's Enter and Escape (every menu takes them: mouse.cpp), a pad's buttons
    BindingFunction select;
    BindingFunction back;
    MenuFunctions(g_Style, &select, &back);
    static std::string selectLabel;
    static std::string backLabel;
    u32 selectGlyph = GlyphOf(select);
    u32 backGlyph = GlyphOf(back);
    bool glyphs = g_Style == PromptPlayStation;
    selectLabel = glyphs || drawnAsArt[selectGlyph] ? std::string(1, PromptGlyphs[selectGlyph]) : Label(g_Style, select);
    backLabel = glyphs || drawnAsArt[backGlyph] ? std::string(1, PromptGlyphs[backGlyph]) : Label(g_Style, back);
    if (g_Style == PromptKeyboard)
    {
        selectLabel = MenuSelectPrompt();
        backLabel = MenuBackPrompt();
    }

    SetPromptLabels(labels, selectLabel.c_str(), backLabel.c_str());
    ApplyMenuLayout();
}

void NoteInputDevice(bool keyboard, SDL_Gamepad* gamepad, SDL_Gamepad* connected)
{
    if (connected != nullptr && (!g_PadStyleKnown || gamepad != nullptr))
    {
        g_PadStyle = PadStyle(connected);
        g_PadStyleKnown = true;
    }

    bool forced = false;
    PromptStyle style = StyleFromEnvironment(&forced);
    if (forced && style != PromptKeyboard)
    {
        g_PadStyle = style;
        g_PadStyleKnown = true;
    }

    if (!forced)
    {
        if (gamepad != nullptr && !keyboard)
        {
            style = PadStyle(gamepad);
        }
        else if (!keyboard)
        {
            // No input this frame: the style stays (the keyboard's until a pad is used)
            if (g_StyleKnown)
            {
                ApplyMenuLayout();
                return;
            }

            style = PromptKeyboard;
        }
    }

    // Hysteresis: the prompts change device only on a press (or a stick past half) and once the device in use has been quiet for
    // half a second, so mixing the keyboard and a pad doesn't make them flicker
    auto now = std::chrono::steady_clock::now();
    static std::chrono::steady_clock::time_point lastOfStyle;
    if (g_StyleKnown && style == g_Style)
    {
        lastOfStyle = now;
        ApplyMenuLayout();
        return;
    }

    if (g_StyleKnown && !forced && now - lastOfStyle < std::chrono::milliseconds(500))
    {
        return;
    }

    lastOfStyle = now;
    g_Style = style;
    g_StyleKnown = true;
    UpdatePrompts();
}

void ForgetPromptStyleForTest()
{
    g_StyleKnown = false;
}
}
