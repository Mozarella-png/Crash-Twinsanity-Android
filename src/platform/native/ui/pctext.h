#pragma once

// The PC wording's table (pctext.cpp), for the override and the self-test
#include "ui/nativeui.h"

#include "game/language.h"

#include <string>

namespace NativeUi
{
struct TextOverride
{
    u32 file;
    const char* language;
    u32 line;
    const char* original;
    const char* replacement;
};

constexpr u32 TextLanguages = 5;

struct NativeTextLine
{
    u32 text;
    const char* languages[TextLanguages];
};

extern const TextOverride g_TextOverrides[];
extern const u32 g_TextOverrideCount;
extern const NativeTextLine g_NativeTexts[];
extern const u32 g_NativeTextCount;
// The game's languages as its Language folders name them, in the native texts' order
extern const char* const g_TextLanguages[TextLanguages];

// A language's index in g_TextLanguages (-1: none)
s32 TextLanguageIndex(const char* language);
// UTF-8 to the game's Windows-1252: false when a character has no code there
bool ToGameText(const char* utf8, std::string* out);
}
