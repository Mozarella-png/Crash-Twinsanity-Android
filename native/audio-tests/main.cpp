// The sound's test harness (CMakeLists.txt): the game's music and sounds played from the disc through Platform::Audio and
// Platform::Stream the way src/game/sound.cpp asks for them, frame by frame (the PS2's 50 a second), the emulated SPU2's output
// written to a WAV file. Every call and value is the game's: the tracks' and sounds' entries are read from the disc's banks
//
//   audio-tests music TRACK SECONDS OUT.wav   a track of Crash6\Music (src/game/sound.cpp's StartMusic, PlayMusic)
//   audio-tests voice TRACK SECONDS OUT.wav   a track of the English voices' bank, played as music the same way
//   audio-tests sound - SECONDS OUT.wav       the front end's first sound (a sound bank's), as PlaySoundOnVoice plays it
//   audio-tests reverb - SECONDS OUT.wav      the same with core 0's reverb (hall) on
//   audio-tests movie NAME SECONDS OUT.wav    FMV\NAME.PSS played as the game's movie controller plays it
#include "audio/audio.h"
#include "audio/iop.h"
#include "audio/spu2.h"
#include "native.h"
#include "platform/audio.h"
#include "platform/movie.h"
#include "platform/stream.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" void NativeApplyDataFixups();

namespace
{
// The game's values (src/game/sound.cpp, filestream.cpp, the PS2 trace of its start-up)
constexpr s32 StreamChannels = 9;
constexpr u16 ReservedSounds = 0x100;
constexpr u32 MusicBufferSize = 0x8000;
constexpr u32 MusicStreamBuffer = 0x4000;
constexpr u32 PlayerCount = 3;
constexpr u32 MusicGroup = 3u << Platform::Audio::GroupShift;
constexpr s32 FramesPerSecond = 50;

struct Track
{
    s32 kind;
    u32 size;
    u32 offset;
    u32 rate;
    u32 value;
};

std::vector<u8> ReadDiscFile(const char* path)
{
    Native::Disc::Entry entry;
    if (!Native::Disc::Find(path, &entry))
    {
        Native::Fatal("no %s on the disc", path);
    }

    std::vector<u8> data(entry.size);
    Native::Disc::Read(entry.offset, data.data(), entry.size);
    return data;
}

void Frame()
{
    Platform::Stream::Update();
}

// A music player's start and play (StartMusic, PlayMusic, SetMusicVolume with the stereo setting on): its first stream (the
// first player's, channel 2) reads the bank, an interleaved track's second plays the other side from the second half of the
// player's buffer
int PlayTrack(const char* bankName, u32 number, u32 seconds)
{
    std::string header = std::string("CRASH6\\") + bankName + ".MH";
    std::string samples = std::string("CRASH6\\") + bankName + ".MB";
    std::vector<u8> mh = ReadDiscFile(header.c_str());
    u32 count;
    u32 blockSize;
    std::memcpy(&count, mh.data(), 4);
    std::memcpy(&blockSize, mh.data() + 4, 4);
    Track track;
    std::memcpy(&track, mh.data() + 8 + std::min(number, count - 1) * sizeof(Track), sizeof(Track));
    Native::Log("audio-tests: %s track %u: kind %d, %#x bytes at %#x, %u Hz, end %#x", bankName, number, track.kind, track.size,
                track.offset, track.rate, track.value);

    // ConstructMusic and ResetSound
    Platform::Stream::Initialise(StreamChannels, nullptr);
    Platform::Audio::Reset();
    for (u32 group = 1; group <= 4; group++)
    {
        Platform::Audio::SetGroupVolume(static_cast<s32>(group << Platform::Audio::GroupShift), Platform::Audio::FullGroupVolume,
                                        Platform::Audio::FullGroupVolume);
    }

    Platform::Audio::ReserveSounds(ReservedSounds);
    u32 buffers[PlayerCount];
    for (u32& buffer : buffers)
    {
        buffer = Platform::Audio::ReserveSoundMemory(MusicBufferSize);
    }

    // SetMusicBank's file and the first player's buffers (CreateMusicBuffers)
    s32 file = Platform::Stream::OpenFile(samples.c_str());
    constexpr s32 FirstChannel = 2;
    constexpr s32 SecondChannel = 3;
    Platform::Stream::AttachBuffer(FirstChannel, blockSize * 2 * 2, buffers[0], MusicStreamBuffer);
    Frame();

    // StartMusic: the free voices are the first core's first two
    bool interleaved = track.kind == 1;
    constexpr s32 FirstVoice = 0;
    constexpr s32 SecondVoice = 1;
    Platform::Audio::ReadMusic(file, track.offset, track.size);
    s32 pitch = Platform::Audio::PitchOfRate(static_cast<s32>(track.rate));
    Platform::Audio::StreamMusic(file, FirstChannel, FirstVoice | MusicGroup, static_cast<u16>(pitch), true);
    if (interleaved)
    {
        Platform::Audio::InterleaveMusic(FirstChannel, blockSize);
        Platform::Audio::AddMusicChannel(SecondChannel, FirstChannel, SecondVoice | MusicGroup, buffers[0] + MusicStreamBuffer);
        Platform::Audio::SetMusicEnd(FirstChannel, track.value);
    }

    Platform::Audio::PrepareMusic(FirstChannel);
    bool playing = false;
    s32 frames = static_cast<s32>(seconds) * FramesPerSecond;
    for (s32 frame = 0; frame < frames; frame++)
    {
        Frame();
        if (std::getenv("TWINSANITY_AUDIO_DEBUG") != nullptr && frame % 25 == 0)
        {
            Iop::LogState();
        }

        if (!playing && Platform::Audio::IsMusicReady(FirstChannel))
        {
            // PlayMusic at full volume, no fade: a side a voice for interleaved music
            constexpr s16 Full = Platform::Audio::MaxVolume;
            if (interleaved)
            {
                Platform::Audio::SetVoiceVolume(FirstVoice, Full, 0);
                Platform::Audio::SetVoiceVolume(SecondVoice, 0, Full);
            }
            else
            {
                Platform::Audio::SetVoiceVolume(FirstVoice, Full, Full);
            }

            Platform::Audio::PlayMusic(FirstChannel);
            playing = true;
            Native::Log("audio-tests: playing from frame %d", frame);
        }
        else if (playing && !Platform::Audio::IsMusicPlaying(FirstChannel))
        {
            Native::Log("audio-tests: the music ended at frame %d", frame);
            break;
        }
    }

    Platform::Audio::StopMusic(FirstChannel);
    Frame();
    return playing ? 0 : 1;
}
}

// A sound of a bank as the game plays it (PlaySoundOnVoice): the bank's samples read into the sound processor's memory by
// STREAM.IRX (SoundBankReader), then played on core 0's first voice with the sound's own pitch and parameter, the game's release
// (0xD) and once. The sound is the front end's first (ID 0x8000: its record in Crash6\Crash.BD at 0x67A336, pitch 0x759,
// parameter 0x20, 0x1430 bytes of samples at 0x67A378). With reverb: core 0 in the hall mode at the depth and volume given
int PlayBankSound(u32 seconds, bool reverb)
{
    constexpr u32 SoundId = 0x8000;
    constexpr u32 SamplesOffset = 0x67A378;
    constexpr u32 SamplesSize = 0x1430;
    constexpr u16 Pitch = 0x759;
    constexpr u32 Parameter = 0x20;
    constexpr u32 ReleaseRate = 0xD;
    constexpr u32 EffectsGroup = 1u << Platform::Audio::GroupShift;
    constexpr s32 ReverbHall = 5;
    Platform::Stream::Initialise(StreamChannels, nullptr);
    Platform::Audio::Reset();
    for (u32 group = 1; group <= 4; group++)
    {
        Platform::Audio::SetGroupVolume(static_cast<s32>(group << Platform::Audio::GroupShift), Platform::Audio::FullGroupVolume,
                                        Platform::Audio::FullGroupVolume);
    }

    Platform::Audio::ReserveSounds(ReservedSounds);
    for (u32 i = 0; i < PlayerCount; i++)
    {
        Platform::Audio::ReserveSoundMemory(MusicBufferSize);
    }

    s32 file = Platform::Stream::OpenFile("CRASH6\\CRASH.BD");
    // The file stream's channel and its buffer in the IOP's memory (the game's first file stream: 0x20000 bytes)
    constexpr s32 Channel = 0;
    Platform::Stream::AttachBuffer(Channel, 0x20000, 0, 0);
    Platform::Stream::ReadSoundBank(Channel, SoundId, file, SamplesOffset, SamplesSize);
    Platform::Stream::Wait(Channel);
    Platform::Audio::SoundBankLoaded(SoundId);
    if (reverb)
    {
        Platform::Audio::SetReverbVolume(0, 0x7FFF, 0x7FFF);
        Platform::Audio::SetReverb(0, ReverbHall, 0x3FFF, 0x3FFF, 0, 0);
    }

    Frame();
    Platform::Audio::PlaySound(SoundId, 0 | EffectsGroup, Platform::Audio::MaxVolume, Platform::Audio::MaxVolume, Pitch, Parameter,
                               ReleaseRate, 1);
    if (reverb)
    {
        Platform::Audio::SetVoiceReverb(0, true);
    }

    s32 frames = static_cast<s32>(seconds) * FramesPerSecond;
    bool ended = false;
    for (s32 frame = 0; frame < frames; frame++)
    {
        Frame();
        if (std::getenv("TWINSANITY_AUDIO_DEBUG") != nullptr && frame < 3)
        {
            Spu2::LogVoice(0);
        }

        if (!ended && frame > 2 && Platform::Audio::IsVoiceFree(0) != 0)
        {
            Native::Log("audio-tests: the sound ended at frame %d", frame);
            ended = true;
        }
    }

    return ended ? 0 : 1;
}

// A movie as the game's movie controller plays it (src/game/movie.cpp: Start lends the sound and a music stream's buffer, then
// a step, a draw and a wait a frame), its sound written to the WAV
alignas(16) u8 g_PlayerMemory[Platform::Movie::PlayerSize];
Platform::Movie::Player* MoviePlayer()
{
    return reinterpret_cast<Platform::Movie::Player*>(g_PlayerMemory);
}

int PlayMovie(const char* name, u32 seconds)
{
    Platform::Stream::Initialise(StreamChannels, nullptr);
    Platform::Audio::Reset();
    Platform::Audio::ReserveSounds(ReservedSounds);
    u32 buffer = Platform::Audio::ReserveSoundMemory(MusicBufferSize);
    // A music player's stream buffer (CreateMusicBuffers: two of the music's blocks of 0x10000, double buffered)
    constexpr s32 MusicChannel = 2;
    Platform::Stream::AttachBuffer(MusicChannel, 0x10000 * 2 * 2, buffer, MusicStreamBuffer);
    Frame();
    u32 lent = Platform::Audio::ChannelBufferAddress(MusicChannel);
    Platform::Movie::Construct(MoviePlayer());
    Platform::Movie::BeginPresenting(MoviePlayer(), [] { Platform::Movie::QueuePicture(MoviePlayer()); });
    Platform::Audio::LendToMovie();
    std::string file = std::string("\\FMV\\") + name + ".PSS;1";
    if (!Platform::Movie::Open(MoviePlayer(), file.c_str(), 0, 640, reinterpret_cast<void*>(static_cast<uiptr>(lent))))
    {
        Native::Log("audio-tests: %s didn't open", file.c_str());
        return 1;
    }

    Platform::Movie::StartSound(MoviePlayer(), 1.0f);
    s32 frames = 0;
    while (Platform::Movie::Step(MoviePlayer()) && frames < static_cast<s32>(seconds) * FramesPerSecond)
    {
        // No renderer here: the picture isn't drawn (TWINSANITY_MOVIE_DUMP writes them)
        Platform::Movie::WaitFrame(MoviePlayer());
        frames++;
    }

    Native::Log("audio-tests: %s: %d pictures", name, frames);
    Platform::Movie::Close(MoviePlayer());
    Platform::Audio::ReclaimFromMovie();
    Frame();
    return 0;
}

int main(int argc, char** argv)
{
    // Frame-locked and silent unless asked otherwise
    setenv("TWINSANITY_AUDIO", "frames", 0);
    Native::ParseArguments(argc, argv);
    Native::OpenDiscImage();
    NativeApplyDataFixups();
    if (argc < 5)
    {
        std::fprintf(stderr, "audio-tests music|voice TRACK SECONDS OUT.wav, audio-tests movie NAME SECONDS OUT.wav\n");
        return 2;
    }

    setenv("TWINSANITY_AUDIO_WAV", argv[4], 1);
    const char* image = Native::GetSettings().discImage;
    if (image == nullptr || !Native::Disc::Open(image))
    {
        Native::Fatal("no disc image");
    }

    u32 track = static_cast<u32>(std::atoi(argv[2]));
    u32 seconds = static_cast<u32>(std::atoi(argv[3]));
    int result = 1;
    if (std::strcmp(argv[1], "music") == 0)
    {
        result = PlayTrack("MUSIC", track, seconds);
    }
    else if (std::strcmp(argv[1], "voice") == 0)
    {
        result = PlayTrack("ENGLISH", track, seconds);
    }
    else if (std::strcmp(argv[1], "sound") == 0 || std::strcmp(argv[1], "reverb") == 0)
    {
        result = PlayBankSound(seconds, std::strcmp(argv[1], "reverb") == 0);
    }
    else if (std::strcmp(argv[1], "movie") == 0)
    {
        result = PlayMovie(argv[2], seconds);
    }

    std::exit(result);
}
