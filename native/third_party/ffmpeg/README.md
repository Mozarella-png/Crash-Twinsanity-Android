# FFmpeg

The native build's movies (`src/platform/native/movie/`) are decoded by FFmpeg's libavcodec (its MPEG-2 video decoder and MPEG
video parser) with libavutil. The app carries its own build of them, made by `native/tools/build_ffmpeg.sh`: LGPL 2.1 or later,
decode-only, nothing else in it.

| | |
|---|---|
| Version | FFmpeg 9.0.2, unmodified |
| Source | https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz (SHA-256 `8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e`, signed by FFmpeg's release key `FCF9 86EA 15E6 E293 A564 4F10 B432 2F04 D676 58D8`) |
| Licence | GNU Lesser General Public License, version 2.1 or later: [COPYING.LGPLv2.1](COPYING.LGPLv2.1) |
| Libraries | libavcodec 63 (`libavcodec.63.dylib`), libavutil 61 (`libavutil.61.dylib`); `.so.63` / `.so.61` on Linux |

The configuration (`build_ffmpeg.sh`; on macOS it adds `--extra-cflags`/`--extra-ldflags` with `-arch <arch>
-mmacosx-version-min=11.0`, and `--enable-cross-compile --arch=<arch> --target-os=darwin` when building the other architecture):

    ./configure --prefix=build/native/ffmpeg/<os>-<arch> --disable-everything --disable-gpl --disable-nonfree --disable-version3 \
        --disable-programs --disable-doc --disable-network --disable-autodetect --disable-avdevice --disable-avformat \
        --disable-avfilter --disable-swresample --disable-swscale --enable-decoder=mpeg2video --enable-parser=mpegvideo \
        --enable-shared --disable-static --enable-pic --enable-pthreads --cc=clang

`--disable-autodetect` keeps every external library out (and with it threads, which `--enable-pthreads` puts back: the system's).
Run against the libraries, `avcodec_configuration()` returns this line and `avcodec_license()` "LGPL version 2.1 or later"; the
app logs the version and licence at start-up (`movie: FFmpeg 9.0.2: ...`).

## The source

FFmpeg's source is at the address above (and at https://git.ffmpeg.org/ffmpeg.git, tag `n9.0.2`). `build_ffmpeg.sh` downloads
it into `native/third_party/ffmpeg-src/` (not in git), checks it and builds it; nothing in it is changed.

## Using your own build (the LGPL's relinking)

The app links the libraries dynamically, so they can be replaced without touching the game: build FFmpeg 9.x (libavcodec 63,
libavutil 61, any configuration that has the `mpeg2video` decoder and the `mpegvideo` parser) and put your
`libavcodec.63.dylib` and `libavutil.61.dylib` in place of the app's, in `Crash Twinsanity.app/Contents/Frameworks/`. Their
install names should be `@rpath/libavcodec.63.dylib` and `@rpath/libavutil.61.dylib` (`install_name_tool -id`), and libavcodec's
reference to libavutil `@rpath/libavutil.61.dylib` (`install_name_tool -change`). Then sign the app again:
`codesign --force --deep --sign - "Crash Twinsanity.app"`. On Linux, put the `.so` files where the app's loader finds them
(`LD_LIBRARY_PATH` works). To build the game itself against another FFmpeg, give CMake `-DTWIN_FFMPEG_DIR=<its prefix>` (a
folder with `lib/pkgconfig/libavcodec.pc`).

The pictures are the same with Homebrew's FFmpeg 9.0.2 and this build: `native/movie-tests` hashes every picture of every movie
on the disc (19,890 in 20 movies, PAL) and the two builds' hashes match.
