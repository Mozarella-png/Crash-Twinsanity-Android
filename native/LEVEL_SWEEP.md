# The level sweep

Every level chunk on the PAL disc started headless and played for 30 s with scripted input (`native/tools/level_sweep.py`, the
game started in the chunk by `ui/sweep.cpp`'s `$TWIN_SWEEP_LEVEL`, as the game's own command-line start does), its exit, states,
fatal signals and stray frees recorded. Run: `python native/tools/level_sweep.py OUT --jobs 2` (`--resume` to go on, `--report`
for this table). The raw results are `native/sweep/results.json`.

**Status: partial (22 of 131 chunks so far; the run was stopped to fix the save/load crash). The rest is still to run.**

- OK: the chunk loads and plays 30 s with no fatal signal.
- Entered from a neighbour: the chunk has no instance of the played character (a hub's part, a tunnel, a corridor): play started
  there alone restarts a null character's camera rig (`FollowCameraRig::Restart` from `StartingPlay`). The game only enters these
  from a neighbouring chunk, whose links bring the character; not a native bug.

Bugs fixed (NATIVE.md, "Native bugs found by the level sweep"): VU0 programs called with another microcode set loaded (77 of the
first run's chunks aborted while loading), the texture pack switch's race with the renderer.

| World | Area | Chunk | Status | Load (s) | Frames in 30 s | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| AltEarth | Core | afttreas | OK | 4.4 | 210 |  |
| AltEarth | Core | corea | OK | 7.2 | 189 |  |
| AltEarth | Core | coreb | OK | 7.3 | 173 |  |
| AltEarth | Core | corec | OK | 7.4 | 175 |  |
| AltEarth | Core | cored | OK | 7.1 | 228 |  |
| AltEarth | Core | pretreas | OK | 7.2 | 205 |  |
| AltEarth | Core | throne | OK | 8.8 | 234 |  |
| AltEarth | Core | treasure | OK | 5.7 | 202 |  |
| AltEarth | Hub | alta | OK | 5.8 | 135 |  |
| AltEarth | Hub | altdoc | OK | 8.0 | 156 |  |
| AltEarth | Hub | altdoc_b | OK | 7.7 | 139 |  |
| AltEarth | Hub | altdoc_c | OK | 7.9 | 158 |  |
| AltEarth | Hub | alttunl | Entered from a neighbour | - | - |  |
| AltEarth | Hub | coreent | OK | 7.5 | 172 |  |
| AltEarth | Hub | slipjoin | OK | 7.9 | 164 |  |
| AltEarth | Lab | altlabin | OK | 6.9 | 142 |  |
| AltEarth | Lab | labext | OK | 4.6 | 176 |  |
| AltEarth | Lab | psycho | OK | 6.2 | 206 |  |
| AltEarth | Lab | ptcorr | Entered from a neighbour | - | - |  |
| AltEarth | Lab | ptexit | Entered from a neighbour | - | - |  |
| AltEarth | RockSlid | l10chasa | OK | 6.4 | 170 |  |
| AltEarth | RockSlid | l10chasb | OK | 6.3 | 181 |  |
