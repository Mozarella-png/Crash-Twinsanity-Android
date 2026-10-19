#include "glsl.h"

#include <SDL3/SDL.h>

namespace Glsl
{
namespace
{
#if defined(TWIN_GLES)
constexpr bool kGles = true;
#else
constexpr bool kGles = false;
#endif

bool g_DualSource = false;

constexpr const char* DesktopVersion = "#version 330 core";
}

bool Gles()
{
    return kGles;
}

bool DualSource()
{
    return g_DualSource;
}

void SetDualSource(bool available)
{
    g_DualSource = available;
}

std::string Adapt(const std::string& source)
{
    std::string out = source;
    size_t at = out.find(DesktopVersion);
    if (at != std::string::npos)
    {
        std::string header;
        if (kGles)
        {
            header = "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n"
                     "precision highp usampler2D;\nprecision highp isampler2D;";
        }
        else
        {
            header = DesktopVersion;
        }
        if (g_DualSource)
        {
            if (kGles)
            {
                header += "\n#extension GL_EXT_blend_func_extended : require";
            }
            header += "\n#define TWIN_DUAL_SOURCE 1";
        }
        out.replace(at, std::char_traits<char>::length(DesktopVersion), header);
    }
    if (kGles)
    {
        // Desktop-only qualifier: GLES 3.0 has no noperspective (the w of these quads is 1, so the interpolation is the same)
        for (size_t pos = out.find("noperspective "); pos != std::string::npos; pos = out.find("noperspective ", pos))
        {
            out.erase(pos, std::char_traits<char>::length("noperspective "));
        }
    }
    return out;
}

void SetContextAttributes()
{
    if (kGles)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    }
    else
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    }
}
}
