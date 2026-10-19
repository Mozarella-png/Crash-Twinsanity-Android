// The movies' pictures hashed: every .PSS file on the disc image demultiplexed by the native player's own pss.cpp and decoded
// by libavcodec fed exactly as movie.cpp feeds it (the MPEG video parser, then the MPEG-2 decoder, flushed at the end), every
// picture's Y, Cb and Cr planes hashed (FNV-1a, 64 bit). Two builds of this against two FFmpegs (native/tools/build_ffmpeg.sh's
// and the system's) giving the same lines means the game shows the same pictures with either. See native/AUDIO.md.
//
//   cmake -S native/movie-tests -B build/native/movie-tests -G Ninja [-DTWIN_FFMPEG_DIR=DIR] && cmake --build ...
//   build/native/movie-tests/movie-tests DISC.iso [NAME.PSS...] > hashes.txt
//
// One line a picture ("FMV/NAME.PSS 0123 <hash>"), then one a movie (its picture count and the hash of its pictures' hashes).
// The FFmpeg it runs with and its licence go to stderr.
#include "movie/pss.h"
#include "native.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
}

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

void Native::Log(const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    std::vfprintf(stderr, format, arguments);
    va_end(arguments);
    std::fputc('\n', stderr);
}

namespace
{
constexpr u64 FnvOffset = 0xCBF29CE484222325ull;
constexpr u64 FnvPrime = 0x100000001B3ull;

u64 Hash(u64 hash, const u8* bytes, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        hash = (hash ^ bytes[i]) * FnvPrime;
    }

    return hash;
}

u32 Le32(const u8* bytes)
{
    return static_cast<u32>(bytes[0]) | static_cast<u32>(bytes[1]) << 8 | static_cast<u32>(bytes[2]) << 16 |
           static_cast<u32>(bytes[3]) << 24;
}

// Every file under a directory whose name ends in .PSS (ISO 9660 records, as disc.cpp reads them)
void FindMovies(u32 sector, u32 size, const std::string& path, std::vector<std::string>* found)
{
    std::vector<u8> buffer(size);
    Native::Disc::Read(static_cast<u64>(sector) * 2048, buffer.data(), size);
    for (u32 at = 0; at < size;)
    {
        u8 length = buffer[at];
        if (length == 0)
        {
            at = (at / 2048 + 1) * 2048;
            continue;
        }

        const u8* record = buffer.data() + at;
        at += length;
        u8 nameLength = record[32];
        std::string name(reinterpret_cast<const char*>(record + 33), nameLength);
        if (nameLength == 1 && (name[0] == 0 || name[0] == 1))
        {
            continue;
        }

        name = name.substr(0, name.find(';'));
        if ((record[25] & 2) != 0)
        {
            FindMovies(Le32(record + 2), Le32(record + 10), path + name + "/", found);
        }
        else if (name.size() > 4 && strcasecmp(name.c_str() + name.size() - 4, ".PSS") == 0)
        {
            found->push_back(path + name);
        }
    }
}

struct Decoder
{
    AVCodecContext* codec = nullptr;
    AVCodecParserContext* parser = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
};

u64 HashFrame(const AVFrame* frame)
{
    u64 hash = FnvOffset;
    s32 header[3] = {frame->width, frame->height, frame->format};
    hash = Hash(hash, reinterpret_cast<const u8*>(header), sizeof(header));
    for (int plane = 0; plane < 3; plane++)
    {
        int width = plane == 0 ? frame->width : (frame->width + 1) / 2;
        int height = plane == 0 ? frame->height : (frame->height + 1) / 2;
        for (int y = 0; y < height; y++)
        {
            hash = Hash(hash, frame->data[plane] + static_cast<ptrdiff_t>(y) * frame->linesize[plane], static_cast<size_t>(width));
        }
    }

    return hash;
}

// One movie: its pictures' hashes printed, false when it couldn't be opened
bool HashMovie(const std::string& name, u32* pictures, u64* movieHash)
{
    NativeMovie::Pss pss;
    if (!pss.Open(name.c_str(), 0))
    {
        return false;
    }

    Decoder d;
    const AVCodec* decoder = avcodec_find_decoder(AV_CODEC_ID_MPEG2VIDEO);
    d.codec = avcodec_alloc_context3(decoder);
    d.parser = av_parser_init(AV_CODEC_ID_MPEG2VIDEO);
    d.packet = av_packet_alloc();
    d.frame = av_frame_alloc();
    if (decoder == nullptr || d.codec == nullptr || d.parser == nullptr || avcodec_open2(d.codec, decoder, nullptr) < 0)
    {
        return false;
    }

    *pictures = 0;
    *movieHash = FnvOffset;
    bool flushed = false;
    while (true)
    {
        int received = avcodec_receive_frame(d.codec, d.frame);
        if (received == 0)
        {
            u64 hash = HashFrame(d.frame);
            std::printf("%s %04u %016llx\n", name.c_str(), *pictures, static_cast<unsigned long long>(hash));
            *movieHash = Hash(*movieHash, reinterpret_cast<const u8*>(&hash), sizeof(hash));
            (*pictures)++;
            continue;
        }

        if (received == AVERROR_EOF)
        {
            break;
        }

        std::vector<u8> data;
        if (!pss.NextVideo(&data))
        {
            if (flushed)
            {
                break;
            }

            flushed = true;
            av_parser_parse2(d.parser, d.codec, &d.packet->data, &d.packet->size, nullptr, 0, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (d.packet->size > 0)
            {
                avcodec_send_packet(d.codec, d.packet);
            }

            avcodec_send_packet(d.codec, nullptr);
            continue;
        }

        const u8* at = data.data();
        int left = static_cast<int>(data.size());
        while (left > 0)
        {
            int used = av_parser_parse2(d.parser, d.codec, &d.packet->data, &d.packet->size, at, left, AV_NOPTS_VALUE,
                                        AV_NOPTS_VALUE, 0);
            if (used < 0)
            {
                break;
            }

            at += used;
            left -= used;
            if (d.packet->size > 0)
            {
                avcodec_send_packet(d.codec, d.packet);
            }
        }
    }

    avcodec_free_context(&d.codec);
    av_parser_close(d.parser);
    av_packet_free(&d.packet);
    av_frame_free(&d.frame);
    return true;
}
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: movie-tests DISC.iso [NAME.PSS...]\n");
        return 2;
    }

    Native::Log("FFmpeg %s: libavcodec %s, libavutil %s", av_version_info(), avcodec_license(), avutil_license());
    if (!Native::Disc::Open(argv[1]))
    {
        Native::Log("can't read %s as a disc image", argv[1]);
        return 1;
    }

    std::vector<std::string> movies(argv + 2, argv + argc);
    if (movies.empty())
    {
        u8 descriptor[2048];
        Native::Disc::Read(16 * 2048, descriptor, sizeof(descriptor));
        FindMovies(Le32(descriptor + 156 + 2), Le32(descriptor + 156 + 10), "", &movies);
    }

    int failed = 0;
    for (const std::string& movie : movies)
    {
        u32 pictures = 0;
        u64 hash = 0;
        if (!HashMovie(movie, &pictures, &hash))
        {
            Native::Log("%s: couldn't be decoded", movie.c_str());
            failed++;
            continue;
        }

        std::printf("%s pictures %u movie %016llx\n", movie.c_str(), pictures, static_cast<unsigned long long>(hash));
        Native::Log("%s: %u pictures", movie.c_str(), pictures);
    }

    return failed == 0 ? 0 : 1;
}
