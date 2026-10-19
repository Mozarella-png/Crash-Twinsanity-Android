// A silent sound processor: what a PS2 with nothing plugged into its audio out would answer, for the time the native audio
// (src/platform/native/audio/, the SPU2's emulation) isn't in the build. Every function is weak, so the real ones take their
// place at link time. Nothing is faked about the game: banks are loaded (Platform::Stream reads them at once), music is ready
// when it's prepared and plays until it's stopped, no sound is heard
#include "native.h"
#include "platform/audio.h"

namespace
{
using namespace Platform::Audio;

constexpr s32 MusicChannels = 16;
bool g_MusicPlaying[MusicChannels];
// Where the sound banks' samples go in the sound processor's memory, past what the PS2's module keeps for itself
constexpr u32 FirstBankAddress = 0x5010;
bool g_BankAddressSet = false;

bool ValidVoice(s32 voice)
{
    return voice >= 0 && voice < Voices;
}
}

namespace Native
{
void SetSoundBankAddress(u32 address);
u32 SoundBankAddress();
}

#define SILENT __attribute__((weak))

SILENT void Platform::Audio::Reset()
{
    for (bool& playing : g_MusicPlaying)
    {
        playing = false;
    }
}

SILENT void Platform::Audio::PauseAll()
{
}

SILENT void Platform::Audio::ResumeAll()
{
}

SILENT s32 Platform::Audio::LendToMovie()
{
    return 0;
}

SILENT s32 Platform::Audio::ReclaimFromMovie()
{
    return 0;
}

SILENT void Platform::Audio::CompactSoundMemory()
{
}

SILENT void Platform::Audio::SetGroupVolume(s32, u32, u32)
{
}

SILENT s32 Platform::Audio::SetVoiceVolume(s32 voice, s16, s16)
{
    return ValidVoice(voice) ? 0 : -1;
}

SILENT s32 Platform::Audio::SetVoicePitch(s32 voice, u16)
{
    return ValidVoice(voice) ? 0 : -1;
}

SILENT void Platform::Audio::SetVoiceReverb(s32, bool)
{
}

SILENT void Platform::Audio::ReleaseVoice(s32, u32)
{
}

SILENT s32 Platform::Audio::IsVoiceFree(s32 voice)
{
    return ValidVoice(voice) ? 1 : -1;
}

SILENT void Platform::Audio::SetReverb(s32, s32, u16, u16, u16, u16)
{
}

SILENT void Platform::Audio::ClearReverb(s32)
{
}

SILENT void Platform::Audio::SetReverbVolume(s32, s16, s16)
{
}

SILENT u32 Platform::Audio::ReserveSoundMemory(u32 size)
{
    if (!g_BankAddressSet)
    {
        g_BankAddressSet = true;
        Native::SetSoundBankAddress(FirstBankAddress);
    }

    u32 location = Native::SoundBankAddress();
    Native::SetSoundBankAddress(location + size);
    return location;
}

SILENT void Platform::Audio::SoundBankLoaded(u16)
{
}

SILENT void Platform::Audio::ReserveSounds(u16)
{
}

SILENT void Platform::Audio::ReleaseSound(u32)
{
}

SILENT s32 Platform::Audio::PlaySound(u32, u32 voiceAndGroup, s16, s16, u16, u32, u32, u32 loops)
{
    if (!ValidVoice(static_cast<s32>(voiceAndGroup & 0xFFFF)))
    {
        return -1;
    }

    return loops > 0xFFFF ? -2 : 0;
}

// The SPU2's pitch for a sample rate: 0x1000 plays at 48 kHz
SILENT s32 Platform::Audio::PitchOfRate(s32 rate)
{
    return static_cast<s32>(static_cast<s64>(rate) * 0x1000 / 48000);
}

SILENT u32 Platform::Audio::ChannelBufferAddress(s32 channel)
{
    return 0x100000 + static_cast<u32>(channel) * 0x10000;
}

SILENT void Platform::Audio::ReadMusic(s32, u32, u32)
{
}

SILENT s32 Platform::Audio::StreamMusic(s32, s32, u32, u16, bool)
{
    return 0;
}

SILENT void Platform::Audio::InterleaveMusic(s32, u32)
{
}

SILENT void Platform::Audio::AddMusicChannel(s32, s32, u32, u32)
{
}

SILENT void Platform::Audio::SetMusicEnd(s32, u32)
{
}

SILENT void Platform::Audio::PrepareMusic(s32)
{
}

SILENT bool Platform::Audio::IsMusicReady(s32)
{
    return true;
}

SILENT s32 Platform::Audio::PlayMusic(s32 channel)
{
    if (channel >= 0 && channel < MusicChannels)
    {
        g_MusicPlaying[channel] = true;
    }

    return 0;
}

SILENT bool Platform::Audio::IsMusicPlaying(s32 channel)
{
    return channel >= 0 && channel < MusicChannels && g_MusicPlaying[channel];
}

SILENT void Platform::Audio::StopMusic(s32 channel)
{
    if (channel >= 0 && channel < MusicChannels)
    {
        g_MusicPlaying[channel] = false;
    }
}
