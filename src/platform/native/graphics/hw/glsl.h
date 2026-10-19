// The shaders are written once, as GLSL 330 core (desktop OpenGL 3.3), and adapted here for the context they run in: GLES 3.0
// on Android (the TWIN_GLES build), where the version line, the default precisions, the dual-source blend's extension and the
// few desktop-only qualifiers change. Adapt also says which shaders get the dual-source output (TWIN_DUAL_SOURCE).
#pragma once

#include <string>

namespace Glsl
{
// Whether the build is for GLES (Android) rather than desktop OpenGL
bool Gles();
// Whether the context has dual-source blending (a second fragment output, the blend factor): the main shader's outFactor
bool DualSource();
void SetDualSource(bool available);
// A shader's source for this context
std::string Adapt(const std::string& source);
// The context's attributes before its window is made (desktop 3.3 core, or GLES 3.0)
void SetContextAttributes();
}
