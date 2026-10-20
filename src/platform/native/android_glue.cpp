// The Android activity's hand-off to the game: the disc image the player imported (a copy in the app's private folder) and
// the app's settings folder. The game reads both from the environment at start-up (system.cpp: TWINSANITY_ISO, and
// SettingsFolder's TWIN_SETTINGS_DIR on Android)
#if defined(__ANDROID__)
#include <jni.h>

#include <cstdlib>

extern "C" JNIEXPORT void JNICALL Java_com_giopixel_crashtwinsanity_NativeBridge_setPaths(JNIEnv* env, jclass, jstring disc,
                                                                                         jstring settings)
{
    if (disc != nullptr)
    {
        const char* text = env->GetStringUTFChars(disc, nullptr);
        setenv("TWINSANITY_ISO", text, 1);
        env->ReleaseStringUTFChars(disc, text);
    }
    if (settings != nullptr)
    {
        const char* text = env->GetStringUTFChars(settings, nullptr);
        setenv("TWIN_SETTINGS_DIR", text, 1);
        env->ReleaseStringUTFChars(settings, text);
    }
}
#endif
