#pragma once

// The native build's optional bug fixes (native/NATIVE.md, "Bug fixes (optional)"): the community mod's gameplay fixes and the
// retail bugs of docs/RETAIL_BUGS.md whose intent is evident, each in the game's code under TWIN_NATIVE and asked for here. With
// the "bug fixes" option off (OptionRetailFixes, the Gameplay tab) the game plays as retail
#include "common.h"

struct ScriptGraph;

namespace NativeFixes
{
// Whether the bug fixes are on ($TWIN_RETAIL_FIXES=0 or 1 overrides the option, for tests)
bool RetailFixes();
// Whether the cutscene skip option is on (the level recipes that restore the scenes' cut skips go with it)
bool SkipCutscenes();

// The level recipes (levelrecipes.cpp): the community mod's edits of the levels' behaviour scripts, made on the scripts as they're
// read from the disc. A graph read (its ID and the item's bytes), and a graph let go of
void ScriptRead(ScriptGraph* graph, u32 id, const u8* data, u32 size);
void ScriptDestroyed(ScriptGraph* graph);
// --selftest-recipes (in the game, once it's started): every script of every level on the disc read back unchanged, and every
// recipe made on its level's scripts. 0 when it passed
int SelfTestRecipes();

// Once a frame (the pads' read): the checks of $TWIN_FIXES_TEST (fixestest.cpp)
void Frame();

// The 21:9 option (OptionUltrawide) and the game's widescreen setting both on: the widescreen picture is 21:9 (fixes.cpp)
bool Ultrawide();
// The widescreen TV's shape: 21:9 (the patch's 0x40155554) with Ultrawide, else 16:9 (WideAspect)
f32 WideTvAspect();

// A material item about to be read (its bytes, changed in place): opaque crates' shaders made shadow receivers
void MaterialRead(u8* data, u32 size);
}
