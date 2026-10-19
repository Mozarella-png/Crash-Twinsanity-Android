#include "touch/touchpad.h"

#include <atomic>

namespace Touch
{
namespace
{
std::atomic<uint32_t> g_Buttons{0};
// Bit 31: the stick is held; then the bytes X (bits 8-15) and Y (bits 0-7)
std::atomic<uint32_t> g_Stick{0};
std::atomic<bool> g_InPlay{false};
}

void SetState(uint32_t buttons, uint8_t leftX, uint8_t leftY, bool stickActive)
{
    g_Buttons.store(buttons, std::memory_order_relaxed);
    uint32_t stick = (static_cast<uint32_t>(leftX) << 8) | leftY;
    if (stickActive)
    {
        stick |= 0x80000000u;
    }
    g_Stick.store(stick, std::memory_order_relaxed);
}

void SetInPlay(bool inPlay)
{
    g_InPlay.store(inPlay, std::memory_order_relaxed);
}

bool InPlay()
{
    return g_InPlay.load(std::memory_order_relaxed);
}

void Apply(uint32_t* buttons, uint8_t* leftX, uint8_t* leftY)
{
    *buttons |= g_Buttons.load(std::memory_order_relaxed);
    uint32_t stick = g_Stick.load(std::memory_order_relaxed);
    if ((stick & 0x80000000u) != 0)
    {
        *leftX = static_cast<uint8_t>((stick >> 8) & 0xFF);
        *leftY = static_cast<uint8_t>(stick & 0xFF);
    }
}
}

// The JNI bridge (Android only): TouchBridge.java's natives
#if defined(__ANDROID__)
#include <jni.h>

extern "C" JNIEXPORT void JNICALL Java_com_giopixel_crashtwinsanity_TouchBridge_setPad(JNIEnv*, jclass, jint buttons, jint lx,
                                                                                     jint ly, jboolean active)
{
    Touch::SetState(static_cast<uint32_t>(buttons), static_cast<uint8_t>(lx), static_cast<uint8_t>(ly), active != JNI_FALSE);
}

extern "C" JNIEXPORT jboolean JNICALL Java_com_giopixel_crashtwinsanity_TouchBridge_inPlay(JNIEnv*, jclass)
{
    return Touch::InPlay() ? JNI_TRUE : JNI_FALSE;
}
#endif
