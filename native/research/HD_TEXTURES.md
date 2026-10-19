# HD textures for Crash Twinsanity (PS2): community packs and PCSX2's replacement format

Researched 2026-10-07. Nothing was downloaded except file listings and headers. Sizes come from the hosts' own metadata: the
GitHub API, archive.org's `/metadata` and in-archive listing, MEGA's public `g` API call, and MediaFire's page.

## TL;DR

- There are **five public Twinsanity packs**, and every one uses PCSX2's `textures/<SERIAL>/replacements/` format. Three are
  AI upscales of the whole game. One is a curated HD pack: official art, fonts and selected upscales. One is a recolour mod.
- **None of them has a licence.** They are fan derivatives of Traveller's Tales/Activision textures. The port should never
  bundle a pack. It should load whatever pack the user puts in a folder, the same way PCSX2 does.
- **A serial correction.** The NTSC-U serial is **SLUS-20909**, not SLUS-21059. In PCSX2's `GameIndex.yaml`,
  SLUS-21059 is *Tekken 5*. The Twinsanity entries are SLES-52568 (PAL-M5), SLUS-20909 (NTSC-U) and SLPM-65801
  (NTSC-J). The demo discs SLED-52574 and SLUS-29101 are "Crash Twinsanity & Spyro - A Hero's Tail [Demo]".
- **PAL and NTSC-U hashes are believed to be the same.** Zekion says to rename the PAL folder to SLUS-20909 and it works.
  CRASHARKI's single pack claims all three regions. A pack's file names therefore work for our PAL disc whichever serial
  folder they shipped under. Only region-specific text textures, such as level cards and legal screens, differ.
- **Our renderer can compute PCSX2's hashes exactly.** It emulates GS local memory in the real swizzled layouts
  (`native/GRAPHICS.md`, `gs/gsmemory.cpp`). The hash is XXH3-64 over the texture's raw GS blocks, or over its unswizzled
  texels. The full recipe is below.
- Twinsanity's textures in these packs are mostly **PSMT8** (8-bit indexed, PSM 0x13) with a 256-colour CLUT, so the
  file names carry a CLUT hash. A few are **PSMCT32** (PSM 0x00). The Twinsanity file names seen have no region (`-rWxH-`)
  and no `-mipN` part.

## The packs

| # | Name / author | Link(s) | Serial folder | Size | Format | Licence / terms | Status |
|---|---|---|---|---|---|---|---|
| 1 | **Crash Twinsanity Texture Pack**, CRASHARKI (`ctwin-tp`) | https://github.com/CRASHARKI/ctwin-tp (download the repo ZIP) | None. The files go in `textures/<your serial>/replacements/`. Claims SLUS-20909, SLES-52568 and SLPM-65801 | 638 PNGs, 132,652,381 bytes (~126.5 MiB) in the git tree; repo `size` is 161,245 KB | PNG, 4x the original (32x32 becomes 128x128, 64x64 becomes 256x256). Subfolders: Characters 18, Crates 26, Extras 204 (boss/enemy galleries), Font 9, Icons 41, Level Cards English 16 / Spanish 16 / Japanese 5, Level Icons 17, Screens 133, World 150, 2 at the root | **No LICENSE file**, and the README states no terms | Active: 18 commits, created 2023-10-23, last push 2025-03-11. The README says to **disable mipmapping** in PCSX2 |
| 2 | **Crash Twinsanity SLUS-20909 (NTSC-U) HD Texture Pack**, ReveriePass / "Reveriemasters" | First posted 2023-10-31 at https://gbatemp.net/threads/crash-twinsanity-slus-20909-ntsc-u-hd-texture-pack.642278/ (Wayback copy 2024-08-04). Download: https://mega.nz/file/kSsVTLiJ#Cagr14VW2-wM-nOWqWctfJzsyQXSNRSiTPqOhYDPwdU. Re-announced 2026-03-29 at https://www.youtube.com/watch?v=KKLCAK7iC64, with the link in the pinned comment | SLUS-20909 | **241,447,286 bytes (~230 MiB)** archive. MEGA's API still answered on 2026-10-07, so the file is alive | Upscaled with the **4xHDCube4** ESRGAN model by Venomalia. Character, level and boss portraits are taken from the artists' portfolios. The font is rebuilt from CRASHARKI's "Twelve Ton Goldfish Extended". Four button-prompt styles: PlayStation, Xbox, Nintendo, Steam Deck | None stated | Author "fixed the mipmapping issue" 2024-05-25, probably by adding DDS mips. Zekion later said the gbatemp thread was taken down |
| 3 | **Crash Twinsanity (USA) [SLUS-20909] HD Remaster**, mirrored by "Leviathan Storm" in the *PCSX2 HD Texture Packs* item | https://archive.org/details/cover_20231010, file `Crash Twinsanity (USA) [SLUS-20909] HD Remaster.rar` | SLUS-20909 | **115,476,205 bytes** RAR (md5 `339891fa411206abfd199d91cbe227ea`). Unpacked: **1,587 DDS files, 317,042,460 bytes** | DDS only. Folders `Environment` 1431, `Screens` 72, `Icons` 20, `Mini Icons` 16, `Levels` 12, `Intro` 9, `Bosses` 8, `Hubs` 4, `Buttons` 3. Files dated Aug to Oct 2023, which fits pack 2's first release (folder names like *Buttons* match its prompt styles). An **empty `dumps/`** folder is included | None. The archive.org item has no licence or rights fields | Uploaded 2023-10-10, RAR replaced 2023-11-06. File names use the **old** naming with bit 14 (TCC) set, e.g. `...-00005dd3.dds`. PCSX2 still loads these because it clears that bit when it parses the name |
| 4 | **Crash Twinsanity upscaled textures**, Zekion | https://drive.google.com/drive/folders/1gmfvHQtRqnNYexlLy4tMeulsWkfqs7LU (folder "Crash Twinsanity", modified 2023-02-26 in Drive's listing). Video and notes: https://www.youtube.com/watch?v=gkZ_sCpsQ4A (2023-02-25), https://crashynews.wordpress.com/2023/02/25/zekion-crash-twinsanity-upscaled-textures-for-pcsx2/ | **SLES-52568** (PAL). The author says renaming the folder to SLUS-20909 works | Not shown in Drive's public view | Topaz Gigapixel AI v6.3.3, with PNGs converted to **DDS**. **Includes the full `dumps/` folder** of the originals, collected by 100%-ing the game | None stated | The author notes Crash's own textures look broken in this pack and points people to pack 1 |
| 5 | **Crash Twinsanity Definitive Edition**, "Modder Espartano" | MediaFire: https://www.mediafire.com/file/dzkboh0fuanymxq/Crash_Twinsanity_Definitive_Edition_%28Modesp%29.7z/file. Google Drive: https://drive.google.com/file/d/1vfwS-4eE2c0-MChxnYj8ZP5GUkxp8DTu/view (both from https://www.youtube.com/watch?v=mnVzJw0g5aw). Author's video: https://www.youtube.com/watch?v=AWO7fFeOPr0 (2025-12-29), which points to a Discord, https://discord.gg/dmgWJtZDsB | SLUS-20909 (CRC B318AA3C per the video) | **2.47 GB** 7z (MediaFire) | "4K" textures, packaged as `Crash_Twinsanity_Definitive_Edition_(Modesp).7z` | None stated. The author asks for donations | Released Dec 2025. A showcase notes *"the game has an issue with flickering objects"* and the player has no shadow. Work-in-progress video: https://www.youtube.com/watch?v=2h5-vs-MHkQ |
| 6 | **Crash Twinsanity EUR [SLES-52568] Texture Pack**, yamijpg | https://gbatemp.net/threads/crash-twinsanity-eur-sles-52568-texture-pack.669875/ (2025-04-16). The download link is hidden from guests. Its "You need access" error points to Google Drive | SLES-52568 | **2.07 GB** (stated as "2,07 GB") | Unknown | None stated | The only pack built for our PAL disc's serial. Link not verified |
| 7 | **Better Twinsanity Textures**, FreezyTheDragon | https://www.deviantart.com/freezythedragon/art/Better-Twinsanity-Textures-Only-For-PCSX2-1073004460 (2024-07-09) | Not stated | Not shown (needs a login) | PCSX2 replacements | DeviantArt default | **A recolour, not an HD pack.** It changes character colours: Crash's shoes red, N. Gin and N. Brio "correct" colours, and so on |

Searches of Nexus Mods, GameBanana and Reddit found no Twinsanity packs. Other related material:

- https://textures.spriters-resource.com/playstation_2/crashtwinsanity/ has texture rips, not a replacement pack.
- Someguy14201 and SomberShroud mention an upscale and its dumps in the gbatemp thread for pack 2. Another poster reports
  ~100 more textures in a fuller dump.
- The gbatemp thread for pack 2 notes that PCSX2's naming change in 2024 broke old dumps for anyone mixing them with fresh
  ones. That is the region naming change below.

**Recommendation.** Use **pack 1 (CRASHARKI)** as the first test target. It is on GitHub, is PNG and has no mips, which
makes it easy to inspect. It is region-agnostic and the README is explicit. Use **pack 3** (DDS, 1,587 files, old-style
names) as the second target to exercise the DDS path, the bit-14 masking, and coverage of nearly every environment
texture. Use **pack 4's `dumps/`** (PAL) to check our hashes against real PCSX2 output without having to run PCSX2.

## PCSX2's replacement format, exactly

Sources, PCSX2 `master` @ `9fffbdbd59b962d63a2259b150f419ad3773e7b4` (2026-10-07):

- `pcsx2/GS/Renderers/HW/GSTextureReplacements.cpp/.h`: file names, parsing, loading
- `pcsx2/GS/Renderers/HW/GSTextureCache.cpp/.h`: `HashCacheKey::Create`, `HashTextureLevel`, `SourceRegion`, `LookupHashCache`, `PaletteKeyHash`
- `pcsx2/GS/Renderers/HW/GSTextureReplacementLoaders.cpp`: PNG and DDS loaders
- `pcsx2/GS/GSXXH.h/.cpp`: XXH3 wrappers
- `pcsx2/GS/GSLocalMemory.cpp`: `psm.bs`, `psm.fmsk`, `psm.pal`, `psm.fmt`
- `pcsx2/GS/GSClut.cpp`: the CLUT buffer that gets hashed
- `pcsx2/GS/Renderers/HW/GSRendererHW.cpp`: when mip levels join the hash

All of it is GPL-3.0+. Below is a description of the algorithm, not copied code. Our port's own implementation should
be written from this description.

### Folder layout

```
<textures root>/<SERIAL>/replacements/**/<name>.(png|dds)   # searched recursively, so subfolders are fine
<textures root>/<SERIAL>/dumps/<name>.png                   # where PCSX2 writes dumps
```

- `<SERIAL>` is `VMManager::GetDiscSerial()`, e.g. `SLES-52568`. The match is case-sensitive on case-sensitive file
  systems. PCSX2 only *warns* about a wrong-case `SLES-xxxxx` or `Replacements` folder, so our loader should match those
  folder names case-insensitively.
- The extension is matched case-insensitively against `png` and `dds`. Nothing else is loaded.
- Hidden files are included. A file name that doesn't parse is ignored.

### File names

All the hex is lower case, from `printf`'s `%x` / `PRIx64`, **without zero padding** on the 64-bit hashes. Note
`c54b03227130988-...` in pack 3, which has 15 digits. The parser uses `sscanf("%llx")`, so padded and unpadded names
both parse.

| Kind | Pattern |
|---|---|
| direct colour, whole texture | `<TEX0Hash>-<bits:08x>.ext` |
| paletted, whole texture | `<TEX0Hash>-<CLUTHash>-<bits:08x>.ext` |
| direct colour, region | `<TEX0Hash>-r<W>x<H>-<bits:08x>.ext` (W and H in decimal) |
| paletted, region | `<TEX0Hash>-<CLUTHash>-r<W>x<H>-<bits:08x>.ext` |
| old region forms, still parsed | `<TEX0Hash>-r<regionbits:hex64>-<bits>.ext` and `<TEX0Hash>-<CLUTHash>-r<regionbits:hex64>-<bits>.ext`. W and H come from the packed SourceRegion: x min/max in bits 0-15 and 16-31, y min/max in 32-47 and 48-63 |
| dumped mip level N (dumps only) | the same name plus `-mip<N>` before `.png` |

The parser tries the patterns in this order: region+CLUT, region, old region+CLUT, old region, CLUT, plain. It accepts a
pattern only if `.` follows the last field.

The `<bits>` field is a 32-bit packed struct, LSB first:

| Bits | Field | Notes |
|---|---|---|
| 0-5 | TEX0.PSM | 0x00 PSMCT32, 0x01 CT24, 0x02 CT16, 0x0A CT16S, 0x13 PSMT8, 0x14 PSMT4, 0x1B T8H, 0x24 T4HL, 0x2C T4HH, ... |
| 6-9 | TEX0.TW | log2 width |
| 10-13 | TEX0.TH | log2 height |
| 14 | unused (was TEX0.TCC) | **Cleared when the name is parsed.** Older dumps, including pack 3, have it set (`0x4000`). Mask it before comparing |
| 15-22 | TEXA.TA0 | zero unless the format is direct colour and not 32-bit |
| 23 | TEXA.AEM | same |
| 24-31 | TEXA.TA1 | same |

Worked examples from the packs:

- `00001553`: PSM 0x13 (PSMT8), TW 5, TH 5, so 32x32.
- `00001993`: PSMT8 64x64.
- `000021d3`: PSMT8 128x256.
- `00002200`: PSMCT32 256x256.
- `00005dd3`: PSMT8 128x128 with the old bit 14 set.

The lookup key is the whole name tuple: TEX0Hash, CLUTHash (0 for non-paletted formats), region W and H (0 if none),
bits, and mip level 0. A replacement has to match all of it.

### TEX0Hash: XXH3-64 over the texture data

The hash is **XXH3_64bits with seed 0 and the default secret**, computed through the streaming API
(`XXH3_64bits_reset`, then `_update` calls, then `_digest`). The result equals one-shot `XXH3_64bits` over the
concatenation of all the update calls. PCSX2's multi-ISA wrappers don't change the result. Any XXH3 implementation, such
as xxhash ≥0.8, gives identical hashes.

For each level hashed (the base level, plus mips when enabled, as described further down), take:

- `bs` = the PSM's block size in texels: PSMCT32/24, PSMZ32/24, T8H, T4HL, T4HH 8x8; PSMCT16/16S, PSMZ16/16S 16x8;
  **PSMT8 16x16**; PSMT4 32x16.
- `tw, th` = the region's width and height if the region is set on that axis, otherwise `1<<TW` and `1<<TH`.
- `rect` = the texel rectangle (the region's, or `0,0,tw,th`). `block_rect` = `rect` aligned outward to `bs`.
- `fmsk` = 0xFFFFFFFF for every format except: CT24/Z24 0x00FFFFFF, CT16/CT16S/Z16/Z16S 0x80F8F8F8, T8H 0xFF000000,
  T4HL 0x0F000000, T4HH 0xF0000000.

Then use one of two paths:

1. **Raw-block path.** This applies when `tw >= bs.x && th >= bs.y && fmsk == 0xFFFFFFFF && region.maxX <= 0 &&
   region.minY <= 0`. Most of Twinsanity's PSMT8 and CT32 textures take this path. Walk the GS blocks covering
   `block_rect`, row of blocks by row of blocks (top to bottom), each row left to right. For each block, feed the
   **256 raw bytes of that block exactly as stored in GS local memory**, with the in-block column/byte swizzle intact.
   The block address comes from (TBP0, TBW, PSM) through the PSM's page/block layout (`GSOffset::bnMulti`, then
   `BlockPtr`). Since `native/graphics/gs/gsmemory.cpp` already keeps local memory in the real layouts, this is a direct
   read of its 256-byte blocks.
2. **Expanded path.** This applies in every other case: smaller than a block, a masked format, or a region. Unswizzle
   `block_rect` into a linear buffer:
   - **Paletted formats** (T8, T4, T8H, T4HL, T4HH): one byte per texel holding the **index** (`rtxP`, so T4 is
     widened to a byte per texel).
   - **Direct formats**: 4 bytes per texel of RGBA8 (R in the low byte), expanded with **TEXA** for 24 and 16-bit
     (`rtx`).

   Then feed `th` rows of `row_size` bytes each, where `row_size` is `tw` for paletted and `tw*4` for direct. The first
   row starts at `buffer + pitch*(rect.top - block_rect.top) + (rect.left - block_rect.left)`. **Watch the x offset:
   PCSX2 adds it in bytes, not multiplied by 4, even for 32-bit texels.** This only matters for direct-colour regions
   whose left edge isn't block-aligned. To match PCSX2's names we have to copy that quirk. When the buffer's pitch equals
   `row_size`, the rows are fed in one call, which gives the same bytes.

**Mip levels.** `lod` is only non-null when PCSX2's **Hardware Mipmapping** is on, or tri-filtering is Forced. In that
case the levels `1..(lod.y - lod.x)` are appended to the same hash state. Each level is `GetTex0Layer(basemip+i)`, with
the region scaled by `>> i`. With mipmapping off, only the base level is hashed. CRASHARKI's README says to disable
mipmapping, which means its names are base-level-only hashes. Our renderer should therefore compute the
**base-level-only** hash (`lod = null`). It can optionally try the full-chain hash as a second key for textures with
MXL > 0, if a pack turns out to need it.

### CLUTHash

The CLUT hash only applies to paletted formats (`psm.pal` > 0). It is
**`XXH3_64bits(clut, pal * 4)`**: 16 entries (64 bytes) for 4-bit formats, 256 entries (1,024 bytes) for 8-bit formats.

`clut` is `GSClut::m_buff32`: the palette the GS will apply, in **index order**, as 32-bit RGBA8 with R in the low byte.

- **CPSM CT32:** the entries as stored. CSA picks a 16-entry window (`(CSA & 15) << 4`), and the CSM1 layout is undone
  into logical order.
- **CPSM CT16/CT16S:** each entry expanded with TEXA. RGB5 is shifted left by 3. Alpha is TA1 if the A bit is set,
  otherwise TA0 (or 0 when AEM is set and RGB is 0).
- Alpha stays in the PS2's range, where 0x80 is opaque.

PCSX2 computes the CLUT hash whenever dumping or replacing is on, even with GPU palette conversion. A dumped or replaced
texture is therefore keyed per palette. The same indices under two palettes give two files, which is why packs often
contain many variants of the same TEX0Hash.

### Region (CLAMP)

`SourceRegion::Create(TEX0, CLAMP)` derives the region from CLAMP. It sets X if WMS is REGION_CLAMP with
`MAXU >= MINU`, and either the clamp width `MAXU-MINU+1 < 1<<TW` or `MAXU >= 1<<TW`. X then runs from MINU to MAXU+1.
It also sets X for REGION_REPEAT with `MINU != 0` (the "Lupin" case, with X from MAXU to `(MINU|MAXU)+1`). The same rules
apply to Y with WMT, MINV and MAXV. A later pass can also shrink the region to the part a previous transfer actually
wrote. The file name's `rWxH` is the region's width and height, which are also stored as u16 in the key. No Twinsanity
pack name seen uses a region, but the loader should support them.

### Loading and alpha

- **PNG:** the base image only. When PCSX2's mipmapping is on, it generates mips for uncompressed textures.
- **DDS:** A8R8G8B8, X8R8G8B8, A8B8G8R8, X8B8G8R8 and R8G8B8, or BC1 (DXT1), BC2 (DXT2/3), BC3 (DXT4/5) and the DX10
  header's BC1/2/3/7 formats. Mips are read from the file. Compressed DDS textures without mips switch mipmapping off
  for that texture, with an on-screen warning.
- **Size:** a replacement can be any size. PCSX2 samples it with normalised coordinates in place of the original
  (`1<<TW` x `1<<TH`, or the region).
- **Alpha:** dumps are the expanded texture written verbatim (with the palette applied for indexed formats), so **PS2
  alpha is preserved: 0x80 means opaque**. Packs keep that range. PCSX2 uploads the replacement into the same RGBA8
  texture format and computes its alpha min/max (`SetReplacementTextureAlphaMinMax`) for its blend and alpha-test
  shortcuts. Our renderer has to treat replacement alpha exactly as it treats the PS2 texel's alpha: 0..0x80 maps to 0..1,
  and values above 0x80 are valid.
- **Loading options:** "Load Textures" on its own loads synchronously. **"Asynchronous Texture Loading"** uses the
  original texture until the replacement arrives. **"Precache Textures"** loads every file at boot. All three packs ask
  for async plus precache.

### Making it work in our renderer (notes)

1. **Where to hook.** Hook where the GS samples a texture for a primitive, in `gs/gsdraw.cpp` or the scaled shadow path.
   Key on (TBP0, TBW, PSM, TW, TH, CLAMP region, TEXA if direct and not 32-bit, CBP/CPSM/CSA/CSM, the CLUT contents).
   Cache the computed (TEX0Hash, CLUTHash) per key, and invalidate it when a write touches the GS pages under that
   texture or its CLUT. Our GS already tracks page writes for the shadow buffers. Hashing every draw would be far too slow.
2. **Where it matters.** Replacements only pay off in the **internal-scale > 1** shadow render. Sample the replacement with
   normalised UVs `(u/tw, v/th)` against the replacement's own size. The 1x local-memory draw stays as it is, so effects
   that read local memory are unaffected.
3. **Load-time indexing.** Scan `<textures>/<SERIAL>/replacements/` recursively once and parse the names with the six
   patterns. Mask bit 14 of `bits`. Build a map from the 32-byte name tuple to the path. Also keep a map with the CLUT
   hash zeroed, as PCSX2 does, so palette variants can be found. Decode files lazily on a worker thread. PNG via stb or
   libpng and DDS BC1-3/7 via a small decoder, or upload BC data straight to GL where `EXT_texture_compression_s3tc` or
   BPTC exists. That is mostly the case on Linux. Apple's GL 4.1 has no BPTC, so BC7 has to be decoded on the CPU there.
4. **Serial.** Use SLES-52568 for our PAL disc. Also look in SLUS-20909 (and SLPM-65801) folders, because every public pack
   but one ships under the NTSC-U serial and the hashes are reported to be the same.
5. **Validation.** Dump with our own implementation in PCSX2's naming. Then diff our names against pack 4's
   `dumps/` (PAL) and against a fresh PCSX2 dump made with the same mipmapping setting.
6. **Licensing.** No pack has a licence, and all are derivatives of copyrighted game art. **Don't redistribute any of
   them.** Document "drop a PCSX2 pack here" instead.

## Sources

- CRASHARKI/ctwin-tp README and tree, via the GitHub API: https://github.com/CRASHARKI/ctwin-tp
- archive.org item metadata: https://archive.org/metadata/cover_20231010
- In-RAR listing: https://archive.org/download/cover_20231010/Crash%20Twinsanity%20%28USA%29%20%5BSLUS-20909%5D%20HD%20Remaster.rar/
- gbatemp pack 2 thread (Wayback 2024-08-04): https://web.archive.org/web/20240804052218/https://gbatemp.net/threads/crash-twinsanity-slus-20909-ntsc-u-hd-texture-pack.642278/
- gbatemp pack 6 thread (Wayback 2025-05-05): https://web.archive.org/web/20250505223831/https://gbatemp.net/threads/crash-twinsanity-eur-sles-52568-texture-pack.669875/
- Zekion: https://www.youtube.com/watch?v=gkZ_sCpsQ4A and https://crashynews.wordpress.com/2023/02/25/zekion-crash-twinsanity-upscaled-textures-for-pcsx2/
- Reveriemasters 2026: https://www.youtube.com/watch?v=KKLCAK7iC64
- Espartano: https://www.youtube.com/watch?v=AWO7fFeOPr0, https://www.youtube.com/watch?v=mnVzJw0g5aw, https://www.youtube.com/watch?v=eD5NHeExQck, https://www.youtube.com/watch?v=2h5-vs-MHkQ
- FreezyTheDragon: https://www.deviantart.com/freezythedragon/art/Better-Twinsanity-Textures-Only-For-PCSX2-1073004460
- PCSX2 GameIndex: https://github.com/PCSX2/pcsx2/blob/master/bin/resources/GameIndex.yaml
- PCSX2 source files listed above, at https://github.com/PCSX2/pcsx2/tree/master/pcsx2/GS
- Community install tutorial (folder layout, DDS mips): https://sites.google.com/view/pcsx2-hd-textures-project/tutorial
