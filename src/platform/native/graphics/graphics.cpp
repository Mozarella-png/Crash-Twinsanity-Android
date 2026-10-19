// Platform::Graphics' device and frame functions natively (src/platform/ps2/graphics.cpp's): the renderer's start-up and display
// set-up as the PS2 side does them, the PCRTC registers written to the emulated GS, the frame's chains run on the hardware's
// emulation, and the picture shown in the window (display.h)

#include "platform/graphics.h"

#include "display.h"
#include "hardware.h"
#include "renderthread.h"
#include "gs/gs.h"
#include "renderer/renderer.h"

#include "game/clock.h"
#include "game/memory.h"

namespace
{
constexpr s32 VideoNtsc = 2;
constexpr s32 VideoPal = 3;
}

extern "C"
{
    // The GS's video mode the renderer set the display up in and its field mode
    extern s32 g_VideoOutMode RETAIL(G_VideoOutMode);
    extern s32 g_VideoFieldMode RETAIL(G_VideoFFMode);

    // The display's settings the renderer worked out at start-up (the PS2 side's DisplaySettings)
    struct DisplaySettings
    {
        u32 frameBuffer;
        u32 frameWidth;
        u32 pixelFormat;
        u32 unused0C;
        u32 unused10;
        u32 unused14;
        u32 unused18;
        u32 alpha;
        u32 enable1;
        u32 enable2;
        u32 alphaFromRegister;
        u32 blendWithBackground;
        u32 crtMode;
        u32 alphaOutput;
        u32 unused38;
        u32 unused3C;
        u32 magnifyX;
        u32 magnifyY;
        u32 width;
        u32 height;
        s32 x;
        s32 y;
    };
    // Native: defined here; the split cuts the PS2's object into several labels (G_FrameBufferWidth...) the converter can't
    // join into one
    DisplaySettings g_DisplaySettings RETAIL(G_FrameBufferBasePointer);

    struct GsDisplay;
    extern GsDisplay g_GsDisplay RETAIL(D_0030A818);
    void InitDisplay(GsDisplay* display, s32 width, s32 height, s32 videoMode) RETAIL(FUN_0019b570);
    void SetDisplayPosition(GsDisplay* display, s32 x, s32 y) RETAIL(FUN_001a0848);

    extern s32 g_UnreadScreenMiddleX RETAIL(D_0030AAEC);
    extern s32 g_UnreadScreenMiddleY RETAIL(D_0030AAF4);
    extern s16 g_UnreadFrameWidth RETAIL(D_0030AB12);
    extern s16 g_UnreadFrameHeight RETAIL(D_0030AB14);
    extern u8 g_AnimationsStopped RETAIL(D_0030AB16);
}

namespace
{
constexpr s32 FrameWidth = 0x200;
constexpr u32 DisplayPixelFormat = 2;
constexpr u32 DisplayAlpha = 0x80;
constexpr u32 UnusedDisplayWord = 2;
constexpr f32 DisplayMoveAcross = 256.0f;
constexpr f32 DisplayMoveDown = 32.0f;
// SMODE2: interlaced, a field at a time
constexpr u64 InterlacedFields = 1;
// The display's corner on the TV: the second circuit a line lower
constexpr s32 TvLeft = 0x27C;
constexpr s32 TvTop = 0x32;

bool g_SkipDisplaySetUp = true;

u32 MagnifyX(u32 width, u32 current)
{
    switch (width)
    {
    case 0x100:
        return 9;
    case 0x140:
        return 7;
    case 0x180:
        return 6;
    case 0x200:
        return 4;
    case 0x280:
        return 3;
    default:
        return current;
    }
}

// The PCRTC's registers from the renderer's settings (SetupPCRTC), into the emulated GS (on its side: the settings as they are now)
void WriteDisplayRegisters(const DisplaySettings& display)
{
    Gs::Gs& gs = *NativeGraphics::GetHardware().gs;
    gs.pmode.value = 0;
    gs.pmode.en1 = display.enable1;
    gs.pmode.en2 = display.enable2;
    gs.pmode.crtmd = display.crtMode;
    gs.pmode.mmod = display.alphaFromRegister;
    gs.pmode.amod = display.alphaOutput;
    gs.pmode.slbg = display.blendWithBackground;
    gs.pmode.alp = display.alpha;
    gs.smode2.value = InterlacedFields;
    for (u32 circuit = 0; circuit < 2; circuit++)
    {
        Gs::RegDisplay& d = gs.display[circuit];
        d.value = 0;
        d.dx = static_cast<u32>(display.x + TvLeft);
        d.dy = static_cast<u32>(display.y + TvTop + static_cast<s32>(circuit));
        d.magh = display.magnifyX;
        d.magv = display.magnifyY;
        d.dw = display.width;
        d.dh = display.height;
        Gs::RegDispfb& fb = gs.dispfb[circuit];
        fb.value = 0;
        fb.fbp = display.frameBuffer;
        fb.fbw = display.frameWidth;
        fb.psm = display.pixelFormat;
    }
}

void SetUpDisplay()
{
    NativeGraphics::RenderCall([display = g_DisplaySettings] { WriteDisplayRegisters(display); });
}

void SetUpDisplayUnlessSkipped()
{
    if (g_SkipDisplaySetUp)
    {
        g_SkipDisplaySetUp = false;
    }
    else
    {
        SetUpDisplay();
    }
}
}

void Platform::Graphics::ResetDevices()
{
}

void Platform::Graphics::ResetPath()
{
}

void Platform::Graphics::WaitVSync()
{
    NativeGraphics::WaitForVBlank();
}

bool Platform::Graphics::IsPalDisplay()
{
    return g_VideoOutMode == VideoPal;
}

void Platform::Graphics::WaitIdle()
{
}

void InitDisplay(GsDisplay*, s32 width, s32 height, s32 videoMode)
{
    g_UnreadScreenMiddleX = GsScreenMiddle;
    g_VideoOutMode = videoMode == VideoPal ? VideoPal : VideoNtsc;
    g_UnreadScreenMiddleY = GsScreenMiddle;
    g_DisplayWidth = width;
    g_DisplayHeight = height;
    g_VideoFieldMode = 0;
    NativeGraphics::SetVideoMode(g_VideoOutMode == VideoPal);

    DisplaySettings& display = g_DisplaySettings;
    u32 pixels = g_DisplayWidth * g_DisplayHeight;
    display.frameWidth = g_DisplayWidth >> GsWidthShift;
    display.unused3C = UnusedDisplayWord;
    display.alpha = DisplayAlpha;
    display.alphaFromRegister = 1;
    display.frameBuffer = 0;
    display.pixelFormat = DisplayPixelFormat;
    display.unused0C = 0;
    display.unused10 = (g_DrawPixelBytes + g_DisplayPixelBytes) * pixels >> GsPageShift;
    display.unused14 = 0;
    display.unused18 = 0;
    display.unused38 = 0;
    display.enable1 = 1;
    display.enable2 = 1;
    display.blendWithBackground = 0;
    display.crtMode = 0;
    display.alphaOutput = 0;
    display.magnifyX = MagnifyX(g_DisplayWidth, display.magnifyX);
    display.y = 0;
    display.height = g_DisplayHeight - 1;
    display.magnifyY = 0;
    display.x = 0;
    display.width = (display.magnifyX + 1) * g_DisplayWidth;
    WriteFrameHead();
    pixels = g_DisplayWidth * g_DisplayHeight;
    g_FrameBufferPage = pixels * g_DisplayPixelBytes >> GsPageShift;
    g_DepthBufferPage = pixels * (g_DisplayPixelBytes + g_DrawPixelBytes) >> GsPageShift;
}

void SetDisplayPosition(GsDisplay*, s32 x, s32 y)
{
    g_DisplaySettings.y = y;
    g_DisplaySettings.x = x;
}

void Platform::Graphics::StartRenderer(s32 height, bool pal)
{
    auto* memory = static_cast<u8*>(MemoryAllocate2(RendererDmaMemorySize));
    g_UnreadFrameWidth = FrameWidth;
    g_RendererDmaNext = memory;
    g_RendererDmaMemory = memory;
    g_UnreadFrameHeight = static_cast<s16>(height);
    InitDisplay(&g_GsDisplay, FrameWidth, static_cast<s16>(height), pal ? VideoPal : VideoNtsc);
}

void Platform::Graphics::FinishRendererStart()
{
    // The GS's side on a thread of its own from here (renderthread.h)
    NativeGraphics::StartRenderThread();
    NativeGraphics::RenderCall([] { NativeGraphics::ApplyGraphicsSettings(); });
    InitVuPrograms();
    g_RendererDmaNext = CarveDmaMemory(g_RendererDmaNext);
    InitialiseFrameBuckets(&g_FrameBuckets);
    InitialiseSmallBucket(&g_SmallBucket);
    InitialiseLargeBucket(&g_LargeBucket);
    MakeTextureSlots(g_TextureUploadContext);
    ClearRenderedMaterials();
    g_RendererDmaNext = InitialiseInstanceBlocks(g_RendererDmaNext);
    MakeDefaultMaterials();
    InitAlphaPresets();
    InitialiseScreenEffects();
    MakeSharedGifPacket();
}

void Platform::Graphics::MoveDisplay(const Vector2* offset)
{
    s32 x = static_cast<s32>(offset->x * DisplayMoveAcross);
    SetDisplayPosition(&g_GsDisplay, x, static_cast<s32>(offset->y * DisplayMoveDown));
}

void Platform::Graphics::StepAnimations(const TimeClock* clock)
{
    g_AnimationsStopped = clock->flags.running ^ 1;
    AnimateMaterials(clock);
    UpdateParticleWaves(clock);
}

// PerformRender: the vertical blank waited for, the frame chain sent to the GIF (the drawn frame copied into the buffer shown),
// the display set up, and the picture shown
void Platform::Graphics::Present(const void* commands)
{
    NativeGraphics::WaitForVBlank();
    if (commands != nullptr)
    {
        NativeGraphics::SendGifChain(commands);
    }

    SetUpDisplayUnlessSkipped();
    NativeGraphics::ShowFrame();
}

void Platform::Graphics::PresentFromInterrupt(const void* commands)
{
    NativeGraphics::SendGifChain(commands);
    SetUpDisplayUnlessSkipped();
    NativeGraphics::ShowFrame();
}

void Platform::Graphics::WaitSent()
{
    g_RendererDma[GifChannel].sending = 0;
}

// The VU0 microcode sets (vu0programs.cpp), loaded into the emulated VU0
void Platform::Graphics::UseHelperPrograms(u32 set, bool wait)
{
    SelectVu0Programs(g_Vu0Programs, set, wait);
}
