# Saving the Xbox way in the native build

The native build runs the game's own PS2 save manager on an emulated memory card that is always in (NATIVE.md, "Saves"). The
screens are still the PS2's: "checking memory card (ps2) in memory card slot 1", a 2 second check at boot, a "cancel save?"
question, a card polled every quarter of a second while autosave is on. The Xbox version (same studio, same code base) runs the
same save code on the console's hard disk instead. This document sets out both flows from the code, the differences, and the
smallest set of `TWIN_NATIVE` changes that gives the native build the Xbox's flow while the PS2 build stays the same executable.

Conventions:

- PS2 references are `file:line` in this repository (branch `macos-support`).
- Xbox references are `func.asm:line` (plus the function or label name), `data.asm:line`, `const.asm:line` or `libtwindata.c:line`
  in `OpenSanityNeo-main/twin/database/assembly/` (Xbox PAL build).
- **Verified** means read off the instructions. **Inferred** means a conclusion from verified facts that the listing doesn't show
  directly. **Uncertain** means it can't be settled without the XISO (section 4).
- Text IDs are lines of the `Language\Code\<language>.txt` file (blank lines don't count; `GameText`, `include/game/language.h:40`).
  PS2 English is quoted from the PS2 disc's `Code/English.txt`. Xbox wording is unknown until the XISO is available.

---

## 1. The PS2 flow (the decomp)

### 1.1 The parts

| Part | Where | What it does |
|---|---|---|
| Game controller | `src/game/gamecontroller.cpp` | Starts the save code's operations (`SaveManagerRequest`) and handles their results in `UpdateSaving` (512-629), every frame from `Update` (230) |
| Save manager | `src/game/savemanager.cpp`, `include/game/savemanager.h` | `SaveCode` on OLEG's screens: four OLEG screens (every widget hidden, message, choices, slots; `oleg.cpp:1462-1473`), three labels, two pages. Four bank files `Bank0.bin`..`Bank3.bin` (0xF400 bytes), folder file `BESLES-52568`, `icon.sys` and `StartUp\Crash.ico` (`Construct`, 210-241) |
| Save code | `src/game/savecode.cpp` | The state machine: `SaveManagerRequest` (564-607), `Frame` (273-395), `OperationDone` (105-233), `Waiting` (235-263), `TakeAnswer` (397-535), `MessageText` (542-560) |
| Save device | `src/game/savedevice.cpp`, `include/game/savedevice.h` | The memory card device. Operations 1-13 (`savedevice.h:254-269`): Check, Format, Measure, Create, WriteFolder, ReadFolder, Find, WriteFile, ReadFile, and the waits WaitFormatted, WaitSaved, WaitLoaded, WaitCancelled that keep an outcome message on screen |
| Pages | `src/game/savepages.cpp` | The choices page (8 items, 27-36) and the save slots' page items (`ShowSlotItems`, 126-163; widgets from `olegpages.cpp:1150-1194`) |
| Native card | `src/platform/native/memorycard.cpp` | Port 0's card always in, formatted, 8 MB, its files in `~/Library/Application Support/Crash Twinsanity/memorycard` |

`savecodeselection.cpp`'s `PadSaveCode` (a save code drawing its own text) is never constructed in retail. The Xbox build has the
same class (vtable `const.asm:11977`, constructor `FUN_00201A70` func.asm:614590), and nothing calls that constructor either.
Neither flow uses it.

Operations (`SaveCodeOperation`, `savemanager.h:23-32`): 1 CheckRoom (boot), 2 CheckInserted (autosave polling), 3 NewGameSave,
4 PauseSave, 5 Load, 6 Autosave.

Screens (`SaveCodeScreen`, `savemanager.h:38-57`): 0 Message, 1 NoCard, 2 NoRoom, 3 Create, 4 InsertCard, 5 Unformatted,
6 ConfirmFormat, 7 InsertSave, 8 InsertRoom, 9 Overwrite, 10 CancelSave, 11 SaveSlots, 12 LoadSlots, 13 FormatFailed, 14 SaveFailed,
15 LoadFailed.

### 1.2 Messages and text IDs

A screen's title is `g_ScreenMessages[screen]`. An operation's message is `g_OperationMessages[device operation]`. Both are save
message numbers. `g_SaveMessageTexts[message]` maps a message to a game text. `GameContext::LanguageChanged`
(`gamecontext.cpp:361-378`) fills it from `SaveMessageTexts` (`gamecontext.cpp:45-47`) for messages 1-37, and the English
`g_SaveMessages` (`asm/data/data.data.s:1618-1659`) are the fallback. In `MessageText`, `(x)` becomes the save's size in KB and
`(xxx)` becomes "crash twinsanity" (`savecode.cpp:542-560`).

The Xbox column is the Xbox's own message-to-text table, verified at func.asm:27005-27042 (`FUN_00021A90`, the PAL build). Section 2.5
covers it; the USA build's table differs from message 34 on (2.5: 34 → 0x44, 35 yes, 36 no, 37 → 0x45, no 0xD1).

| Msg | Used by | PS2 text | PS2 English (abridged) | Xbox text |
|---|---|---|---|---|
| 1 | op Check | 0x27 | checking memory card (ps2) in memory card slot 1. do not remove... | 0x2A |
| 2 | op Format | 0x28 | formatting memory card (ps2)... | 0x2B |
| 3 | op Measure | 0x29 | checking memory card (ps2)... | 0x2C |
| 4 | op Create | 0x2A | saving data. do not remove memory card (ps2)... | 0x2D |
| 5 | op WriteFolder | 0x2B | saving data... | 0x2E |
| 6 | op ReadFolder | 0x2C | loading data... | 0x2F |
| 7 | op Find | 0x2D | checking memory card (ps2)... | 0x30 |
| 8 | op WriteFile | 0x2E | saving data... | 0x31 |
| 9 | op ReadFile | 0x2F | loading data... | 0x32 |
| 10 | screen Create | 0x30 | no save file on the memory card (ps2) in memory card slot 1. would you like to create a (xxx) save file? | 0x33 |
| 11 | screen InsertCard | 0x31 | no memory card (ps2) in memory card slot 1. | 0x34 |
| 12 | screen NoCard | 0xB7 | no memory card (ps2) in memory card slot 1. (xxx) requires (x)kb of free space to save data. | 0xBC |
| 13 | screen Unformatted | 0x32 | memory card (ps2) in memory card slot 1 is unformatted. format memory card (ps2)? | 0x35 |
| 14 | screen ConfirmFormat | 0x5F | do you really wish to format memory card (ps2) in memory card slot 1? | 0x64 |
| 15 | screen InsertSave | 0x33 | no (xxx) save data present on memory card (ps2) in memory card slot 1. | 0x36 |
| 16 | screens NoRoom, InsertRoom | 0x34 | insufficient free space on memory card (ps2) in memory card slot 1. (xxx) requires (x)kb of free space to save data. | 0x37 |
| 17 | screen Overwrite | 0x35 | are you sure you wish to overwrite this save file? | 0x38 |
| 18 | screen SaveSlots | 0x36 | select save file to save | 0x39 |
| 19 | screen LoadSlots | 0x37 | select save file to load | 0x3A |
| 20 | screen CancelSave | 0xB8 | cancel save? | 0xBD |
| 21 | op WaitFormatted | 0xB4 | format successful | 0xB9 |
| 22 | op WaitSaved | 0xB5 | save successful | 0xBA |
| 23 | op WaitLoaded | 0xB6 | load successful | 0xBB |
| 24 | screen FormatFailed | 0x38 | format failed! check memory card (ps2)... | 0x3B |
| 25 | screen SaveFailed | 0x39 | save failed! check memory card (ps2)... | 0x3C |
| 26 | screen LoadFailed | 0x3A | load failed! check memory card (ps2)... | 0x3D |
| 27 | op WaitCancelled | 0xB9 | save cancelled | 0xBE |
| 28 | (slot label) | 0x3B | save file | 0x3E |
| 29 | empty slot | 0x3C | empty | 0x3F |
| 30 | item Cancel | 0x3D | cancel | 0x40 |
| 31 | item Format | 0x3E | format | 0x41 |
| 32 | item Continue | 0x05 | continue | 0x05 |
| 33 | item Leave | 0x3F | continue without saving | 0x42 |
| 34 | item Create | 0xCC | create a save file | 0xD1 |
| 35 | item Retry | 0x41 | retry | 0x44 |
| 36 | item Yes | 0x03 | yes | 0x03 |
| 37 | item No | 0x02 | no | 0x02 |
| 38 | Xbox only: item 8, "free blocks" (2.3.6) | (none) | (none) | 0x45 |

OLEG's own save-related labels (`oleg.cpp:55-75`, built at 1051-1068 and 1101): autosave enabled 0x23, autosave disabled 0x24,
"to enable autosave, return to the pause menu and re-save the game." 0x25, autosave warning 0x42 ("do not remove memory card
(ps2), in memory card slot 1 ... when the autosave indicator (below) is present."), autosaving 0x43 ("autosaving data. do not
remove memory card (ps2)..."), autosave failed 0xB3, disable autosave title 0x60 (`oleg.h:583`), and the pause menu's
"disable autosave" item 0x16 (`olegpages.cpp:106`).

### 1.3 The flows, by trigger

The device waits at least the request's time on each step (`savedevice.cpp:93-97`). Every PS2 request passes
`g_ClockUnitsPerSecond + g_ClockUnitsPerSecond`, so each message stays up at least **2 s** (`gamecontroller.cpp:1149, 1208, 1217,
1227, 1254`). Ask, ShowChoices and ShowSlots fade OLEG's screens over half a second (`savemanager.cpp:67-76`). The manager's
`Ask` brings up the message screen for operations 1, 3, 4 and 5. Autosave gets its message text but no screen switch, and
CheckInserted gets neither (`savemanager.cpp:124-152`).

**a. Boot check**: `StateCheckingCard` (dispatched at `gamecontroller.cpp:268-270`), `CheckingCard` (1248-1257).
1. On the first frame it hides the legal picture and calls `SaveManagerRequest(CheckRoom, 2 s)`, which asks the device for Check
   (`savecode.cpp:577-583`, no screen switch for the request itself).
2. `OperationDone` (109-131):
   - No card: NoCard screen.
   - Unformatted: done.
   - Otherwise it measures. The save fits: done. It doesn't fit: NoRoom screen.
3. A failed Check shows NoCard (`Frame` 327-337).
4. The state stays until the operation is none, then goes to `StateVivendiLogo`.
5. Answers: "continue" ends the operation (flagged). "retry" asks for Check again (`TakeAnswer` 409-412).

**b. New Game**: `NewGamePage::Entered` (`olegpages.cpp:281-284`) → `LoadForNewGame` (`gamecontroller.cpp:1204-1211`), saving step
`SavingNewGameLoaded`, operation NewGameSave.
1. `SaveManagerRequest` (591-606):
   - No card: InsertCard screen.
   - Formatted: it measures.
   - Unformatted: Unformatted screen.
2. Measure done (`OperationDone` 142-157):
   - Doesn't fit: **InsertRoom** screen.
   - A save is there: ReadFolder, then the SaveSlots screen.
   - No save: Create screen ("Create save", "continue without saving", "cancel").
3. Create → device Create (which clears the summaries and writes every file, `savedevice.cpp:442-460`) → SaveSlots.
4. Slot chosen (`TakeAnswer` 428-441):
   - It holds a save: Overwrite screen.
   - It's empty: the date is stamped and `WriteSlot`.
5. `WriteSlot` (72-85) writes every file (WriteFolder) if `results.fresh`, otherwise WriteFile. Then WaitSaved ("save
   successful"), then `Finish(1, 1)`.
6. `UpdateSaving` `SavingNewGameLoaded` (`gamecontroller.cpp:591-600`):
   - Flagged: `RequestNewGame(1)` (672-716) starts the game with autosave on (a save due, notice AutosaveOn).
   - Not flagged: back to the main menu.

**c. Load Game**: from the main menu (`LoadGamePage::Entered`, `olegpages.cpp:299-302`) or the pause menu (its `LoadGamePage` at
`olegpages.cpp:795-798`) → `LoadSavedGame` (`gamecontroller.cpp:1213-1220`), `SavingLoaded`, operation Load.
1. Measure:
   - A save: ReadFolder, then LoadSlots (only slots holding a save are shown, `savepages.cpp:131-141`).
   - No save: InsertSave screen ("no (xxx) save data present on memory card (ps2)...", "cancel").
2. A slot → ReadFile → WaitLoaded ("load successful") → `Finish(1, 1)` → `RequestSavedLevel(1)` (718-741), notice AutosaveOn.

**d. Save from the pause menu**: `SaveGamePage::Entered` (`olegpages.cpp:782-785`) → `SaveFromPause` (`gamecontroller.cpp:1222-1230`),
`SavingPaused`, operation PauseSave. Same as New Game, but "continue without saving" is hidden: the pages' `saving` argument is
`operation == NewGameSave` (`savemanager.cpp:162, 186`). A flagged result turns autosave on (`gamecontroller.cpp:602-623`).

**e. Checkpoint autosave**: a script command (`commandscharacters.cpp:1075`) calls `Autosave` (`gamecontroller.cpp:1137-1158`).
1. The checkpoint is always taken into the save controller.
2. Only with a save due (`bits.savingDue`): step `SavingSavedShown`, the autosaving icon and text 0x43 (`oleg.cpp:1829-1841`) on
   the first save, the dimmer, the chunks stopped, and `SaveManagerRequest(Autosave)`, which writes the last slot with no screens
   (`savecode.cpp:585-589`).
3. Result (`UpdateSaving` 557-575):
   - Flagged: `SavingWait`.
   - Not flagged: notice AutosaveFailed (text 0xB3).

**f. Polling the card while autosave is on**: `UpdateSaving` `SavingWait` (`gamecontroller.cpp:519-533`).
1. A quarter of a second after the wait begins, it asks `SaveManagerRequest(CheckInserted)` and goes to `SavingSaved`.
2. A failed Check ends the operation unflagged (`savecode.cpp:327-336`).
3. `SavingSaved` (545-556):
   - Flagged: back to `SavingWait`, so the card is polled all the time.
   - Not flagged: notice AutosaveOff ("autosave disabled" / 0x25) and the save due is dropped. Pulling the card turns autosave off.

**g. Disabling autosave**: the pause item (0x16) is shown while saving is Wait or Saved (`olegpages.cpp:826-839`), then
`DisableAutosavePage` (496-514) → `StopSaving` (`gamecontroller.cpp:1192-1202`).

**h. Notices**: `PauseReason` (`gamecontroller.cpp:2004-2070`) turns the notices into pause screens for ReasonAutosaveOff,
ReasonAutosaveOn and ReasonAutosaveFailed (labels at `oleg.cpp:1058-1068`).

### 1.4 Every screen: items and answers (PS2)

Items: `SaveChoicesPage::ShowItems`, `savepages.cpp:170-229`. Answers: `TakeAnswer`, `savecode.cpp:397-535`. "NG" means shown only
for the New Game save.

| Screen | Items | Item → effect |
|---|---|---|
| 1 NoCard, 2 NoRoom | continue, retry | continue → end, flagged. retry → Check |
| 3 Create | continue without saving (NG), cancel, create a save file | create → device Create. cancel → **CancelSave** (save ops). continue → end, flagged |
| 4 InsertCard, 7 InsertSave, 8 InsertRoom | continue without saving (NG), cancel | cancel → CancelSave (save ops) or end |
| 5 Unformatted | format, continue without saving (NG), cancel | format → ConfirmFormat |
| 6 ConfirmFormat | yes, no | yes → Format. no → Unformatted |
| 9 Overwrite | yes, no | yes → WriteSlot. no → SaveSlots |
| 10 CancelSave | yes, no | yes → WaitCancelled ("save cancelled") → end. no → measure again |
| 11 SaveSlots | 4 slots, continue without saving (NG), cancel | slot → Overwrite or write. cancel → CancelSave |
| 12 LoadSlots | slots with a save, cancel | slot → ReadFile. cancel → end |
| 13 FormatFailed, 15 LoadFailed | cancel, retry | retry → measure again (446-470). cancel → CancelSave or end |
| 14 SaveFailed | continue without saving (NG), cancel, retry | as above |

Device failures (`Frame` 325-394):

| Failed operation | Result |
|---|---|
| Check | NoCard (boot) or end |
| Format | FormatFailed |
| Measure | Load/SaveFailed, InsertCard without a card, or autosave ends |
| Create, WriteFolder, WriteFile, ReadFile, WaitFormatted, Find | LoadFailed or SaveFailed (autosave: end unflagged) |
| ReadFolder | save ops: **SaveSlots with `results.fresh = 1`**, so the next save rewrites every file. Load: LoadFailed |

---

## 2. The Xbox flow (the disassembly)

### 2.1 The functions, PS2 to Xbox

Xbox vtables start with Ask at offset 0: 0 Ask, 1 ShowChoices, 2 ShowSlots, 3 OperationDone, 4 Waiting, 5 Finish, 6 destructor,
7 Frame, 8 Draw. The PS2's GCC vtables number the same functions from 1.

| PS2 | Xbox | func.asm |
|---|---|---|
| `SaveManager::Construct` | `FUN_000B1660` | 178678 |
| `ConstructSaveCode` | `FUN_00201B10` | 614646 (messages set to -1: **0x27 = 39** of them, 614666; PS2 38) |
| `SaveManager::Ask` / `ShowChoices` / `ShowSlots` | `FUN_002031D0` / `FUN_002032C0` / `FUN_00203440` | 616759 / 616834 / 616966 (vtable `FUNPTR_0x0038d050`, const.asm:6366) |
| `SaveCode::OperationDone` | `FUN_00200BF0` | 613011-613259 |
| `SaveCode::Waiting` | `FUN_00200E20` | 613261 |
| `SaveCode::Finish` | `FUN_00200E70` | 613301 |
| `SaveCode::Frame` | `FUN_00202100` | 615232-615480 |
| `SaveCode::TakeAnswer` | `FUN_00200820` | 612653-613009 |
| `SaveCode::MessageText` | `FUN_00202330` | 615482-615547 |
| `SaveFits` / `WriteSlot` | `FUN_00200750` / `FUN_002007A0` | 612544 / 612592 |
| part of `Waiting` and `SaveManagerRequest` (measure, Unformatted or InsertSave) | `FUN_002007D0` | 612613 |
| `SaveManagerRequest` | `FUN_00200F30` | 613374-613454 |
| `SaveChoicesPageConstruct` | `FUN_002024A0` | 615619-615873 |
| `SaveChoicesPage::ShowItems` | `FUN_00201150` | 613594-613711 |
| `SaveCodePage::ShowSlotItems` | `FUN_00201050` | 613484-613592 |
| `GameContext::LanguageChanged` (save texts) | `FUN_00021A90` | 27005-27042 |
| save device (vtable `FUNPTR_0x00396190`, const.asm:11963) | constructor `FUN_002018F0`, `Request` `FUN_002029B0`, `Step` `FUN_00201EA0` | 614421, 616066-616435, 615002-615230 |
| `UpdateSaving` | `FUN_000BB920` | 189905-190237 |
| `Autosave` | `FUN_000B6C00` | 184256 |
| `LoadSavedGame` / `LoadForNewGame` / `SaveFromPause` | `FUN_000AFE90` / `FUN_000AFED0` / `FUN_000AFF10` | 176866 / 176886 / 176906 |
| `CheckingCard` | `FUN_000AF140` | 175722-175760 |
| `NewGamePage` / `LoadGamePage` / `SaveGamePage::Entered` | `FUN_000A70B0` / `FUN_000A70A0` / `FUN_000A70C0` | 165923-165940 |
| state dispatch in `Update` | `FUN_000BCF90` | 191960-192300 |
| `MainMenuPage::Construct` / `PausePage::Construct` | `FUN_000AA000` / `FUN_000AA100` | 170030 / 170126 |
| `OLEG::StartUp` labels | `FUN_000B8810` | 186054-186830 |

The English fallback tables are unchanged in content. `g_SaveMessages` is `ARRAY_0x003a40d8` (data.asm:9822-9824, still 38
entries), operation messages start at `NUM_0x003a4174` (data.asm:9829), and screen messages are `ARRAY_0x003a41b0`
(data.asm:9889). The values match the PS2's `D_002E80A8` and `D_002E80E0`. **Verified.**

### 2.2 The storage device (replaces the memory card)

All verified:

- **The card is always in and always formatted.** `HasCard` (`FUN_00201880`, func.asm:614378) and `CardFormatted` (`FUN_00201890`,
  614384) both return 1. The InsertCard, NoCard, Unformatted, ConfirmFormat and FormatFailed screens are unreachable.
- **Format does nothing.** Request case 0 jumps straight to the exit (`ARRAY_0x00202df4_CASE_0`, 616427).
- **Measure** calls `XFindFirstSaveGame("u:\", ...)` (616086). The root is `STR_0x0039618c` = "u:\", set at 614437.
  - If any save game of the title exists, it keeps its directory (+0x30) and name (+0x3C) and sets has-save (+0x13C) to 1. It takes
    the first found and walks the rest without using them.
  - Otherwise has-save is 0.
- **Create** (616117-616150) calls `XCreateSaveGame("u:\", name, OPEN_ALWAYS, 0, path, MAX_PATH)` only while no directory is known.
  - The name is a wide string copied from `STR_0x0038d570` at 178716 (`libtwindata.c:133` shows it as "Saved Games"; **uncertain**
    whether that decoding is right, see 4).
  - On success it keeps the path and sets has-save.
  - The step after Create still clears the summaries and writes every file, as on the PS2 (`FUN_00201D80` 614901, called at
    615070; `Step` case 12 at 615077).
- **Files.** The save holds `Header.bin` (0x800 bytes, the folder file, which replaces the PS2's `BESLES-52568`, 178707) and
  `Bank0.bin`..`Bank3.bin` (0xF400 each, 178727-178739).
  - Each file is 20 bytes of `XCalculateSignature` digest followed by the data (func.asm:616247-616302).
  - Reads check that the size is the data's plus 20 and check the digest (616362-616420).
- **No icon files.** The icon list is built with room 0 and no icon.sys (`FUN_00201740(0, 0)`, 178701). A save-image object named
  "saveimagewrong.xbx" is built (`FUN_00202960`, 616040, called at 178696) and never added to anything.
- **Free space** is `GetDiskFreeSpaceExA("u:\")`'s low 32 bits (`FUN_002019C0`, 614506-614522).
- **Needed space is counted in 16 KB Xbox blocks** (`FUN_00201970`, 614467-614504):
  `needed = 16384 * ((16384 * files + bytes + 0xFFFF) >> 14)`, where `files` counts the icon.sys (none here), the header and the
  four banks.
  - For this save that's 5 files and 0x3D800 bytes, which comes to **24 blocks**. This is computed from the formula, not observed.
- **The bytes a save takes equal the bytes needed** (`SavedBytes` `FUN_002018B0` jumps to `NeededBytes`, 614399). An existing save
  therefore always fits (`SaveFits`, 612544).
- **The device's update steps do nothing** (614411, 614416).

### 2.3 Save code differences (Xbox against PS2)

Everything not listed here matches the PS2 instruction for instruction in meaning. That covers `Waiting`, `Finish`, the Load and
Autosave branches of `OperationDone`, `SaveManagerRequest` apart from `WriteSlot`, `ShowSlotItems`, `Ask`, `ShowChoices` and
`ShowSlots`.

1. **Measure "doesn't fit" during a save shows NoRoom (2), not InsertRoom (8).** `OperationDone` LAB_0x00200CBF→LAB_0x00200CE3,
   func.asm:613105-613127, `ShowChoices(2)`. The title text is the same: both screens use message 16.
2. **`WriteSlot` always writes the slot's file.** `FUN_002007A0` (612592-612611) asks WriteFile(slot) and sets `results.slot`. The
   PS2's `results.fresh` branch (`savecode.cpp:74-78`) is gone. A WriteFolder result in a save goes straight to SaveSlots
   (LAB_0x00200CF0, 613128). The Autosave request also uses this `WriteSlot` (LAB_0x00200F8B, 613409).
3. **Device failures** (`Frame`, table `ARRAY_0x002022d0` at 615456):
   - Create failing shows **NoRoom** for every operation (LAB_0x002021A2, 615299). On the PS2 it's SaveFailed or LoadFailed.
   - ReadFolder failing is handled like WriteFile and the others: SaveFailed, LoadFailed, or the autosave ends (LAB_0x002021B0,
     615307). There's no `fresh = 1` and no slots.
   - The other failures and the "no card" handling in `Frame` (615391 on) match the PS2.
4. **"Back" never asks "cancel save?".** `TakeAnswer` back table `ARRAY_0x00200b9c` (612958) indexes by screen-2:
   - Create, InsertCard, Unformatted, InsertSave, InsertRoom, SaveSlots, LoadSlots and the three failures all go to `Finish(0, 0)`
     (LAB_0x002008BF, 612702).
   - ConfirmFormat → Unformatted, Overwrite → SaveSlots and CancelSave → measure again are kept.
   - **NoRoom's back runs `XLaunchNewImageA(NULL, LD_LAUNCH_DASHBOARD)`** (LAB_0x00200946, 612754-612766), with reason 2, context 0,
     parameter 1 = 0x55 ('U') and parameter 2 = needed bytes >> 14 (blocks). That is the Dashboard's free-space screen. The field
     meanings are XDK conventions; the values are verified.
   - Screen 10 (CancelSave) is shown by nothing on the Xbox, so it is unreachable.
5. **"Retry" on a failure screen asks for Create, not Measure.** In the first-choice table `ARRAY_0x00200bac` (612975), cases 12-14
   (screens 13-15) go to LAB_0x00200A00 (612816). With a card that is always in and formatted, that reaches LAB_0x00200A1C (612827),
   `Ask(4 Create, 0)`.
   - Only SaveFailed still shows a Retry item (point 6), so this only happens after a failed save.
   - Create clears every summary and then writes every file (2.2). A Retry after a failed save therefore shows four empty slots,
     and the save then writes all four banks. **Inferred:** this would wipe the other slots' saves. Section 3 explains why the
     native build shouldn't copy it.
   - The other first-choice cases match the PS2: Create → Create, Overwrite → WriteSlot, slots, CancelSave → WaitCancelled.
6. **The choices page has 9 items** (`FUN_002024A0`, 615619-615873). Items 0-7 are as on the PS2: Format, Continue, Leave, Create,
   Retry, Cancel, Yes, No, with the same messages and answers. **Item 8 is new: message 0x26 (Xbox text 0x45), answer Back.**
   `ShowItems` (`FUN_00201150`, 613594-613711) shows:

   | Mode | PS2 | Xbox |
   |---|---|---|
   | NoCard, NoRoom | continue, retry | **continue without saving** (always), **item 8 (NG only)** |
   | Create | continue without saving (NG), cancel, create a save file | continue without saving (NG), **yes, no** |
   | InsertCard, InsertSave, InsertRoom | continue without saving (NG), cancel | same |
   | Unformatted | format, continue without saving (NG), cancel | same |
   | ConfirmFormat, Overwrite, CancelSave | yes, no | same |
   | FormatFailed, LoadFailed | cancel, retry | **cancel** |
   | SaveFailed | continue without saving (NG), cancel, retry | same |

   "NG" is `operation == NewGameSave` on both (Xbox func.asm:616907). The byte index table for the "third" answer
   (`ARRAY_0x00200b78`, 612991) is printed for screens 1-8 only, all "end, flagged" as on the PS2. Screens 9-15 aren't in the
   listing (**uncertain**, see 4), but no Xbox screen past 8 offers a "continue" item except SaveSlots and SaveFailed with "continue
   without saving" for NG.
7. **`(x)` is the shortfall in blocks.** `MessageText` (`FUN_00202330`, 615482-615497) uses `(NeededBytes - FreeBytes) >> 14`, where
   the PS2 uses the needed size rounded up to KB (`savecode.cpp:544`).
8. **There are 39 save messages** (0x27 entries set to -1 at 614666), and message 38 maps to Xbox text 0x45 (27042). The English
   fallback table still has 38 entries, so message 38 needs its game text.

### 2.4 Game controller differences

1. **No check at boot.** The Xbox's `CheckingCard` (`FUN_000AF140`, 175722) asks `CheckRoom` with a 3 s wait and returns 3 once the
   operation is over. It is reached only through thunk `FUN_00018C6A` (func.asm:25473), which nothing references (the thunk's name
   appears nowhere else in func/const/data/decls). The dispatch's case 2 (LAB_0x000BD09D, func.asm:192041-192043) sets the next
   state to 3 without calling anything.
   - **Verified:** the boot check is dead code on the Xbox.
   - **Inferred:** Xbox state numbers run one below the PS2's from the check on. Xbox state 3 calls `FUN_000AF1D0`, the Vivendi logo
     movie (5320, 192047), and `UpdateSaving`'s main-menu test uses state 7 (func.asm:190067, `CMP EDX,07000h`), where the
     PS2's `StateMainMenu` is 8. That is consistent with the Xbox having no `StateWaitingForPad`. The full state list wasn't mapped.
2. **No card polling.** `UpdateSaving` (`FUN_000BB920`) returns 0 at once when the step is `SavingWait` (func.asm:189918,
   `CMP ECX,01h`), so it never asks CheckInserted. That is the only one of the PS2's six `SaveManagerRequest` call sites missing on
   the Xbox: the other five call `FUN_00200F30` at 175751, 176881, 176901, 176922 and 184305. After an autosave the step stays
   `SavingWait` until the next `Autosave` or `StopSaving`. The other steps (Saved, SavedShown, Loaded, NewGameLoaded, Paused,
   189932-190230) match the PS2.
3. **3 second minimum per message.** Every Xbox request passes clock units per second × `FLOAT_0x003865d0` (const.asm:107, 0x40400000
   = 3.0), for example func.asm:176874-176881. The PS2 uses 2 s. This matches the Xbox certification rule that save messages stay at
   least three seconds. That's a recollection about the rule, but the 3.0 is verified.
4. **Unchanged.** The main menu has new game, load game and options (`FUN_000AA000`). The pause menu has options, save game, load
   game, disable autosave, quit and resume (`FUN_000AA100`, 170126-170300, same texts and IDs as `olegpages.cpp:787-814`). `Autosave`
   (`FUN_000B6C00`) matches the PS2: save due, autosaving screen, dimmer, chunks stopped. The autosave notices' labels are still
   built (2.5). The four-slot UI is still there (`ShowSlotItems`).
5. **`PauseReason` (`FUN_000AF8F0`, 176379-176570) is restructured.** AutosaveFailed and AutosaveOff notices take priority even while
   paused, and a global `NUM_0x003e2d30` holds back a notice's reason while the pad (`FUN_0020A000`) is missing. This is not decoded
   further. It isn't part of the storage flow, so the native build should keep the PS2's.

### 2.5 Xbox text IDs

The listing (PAL build) gave the shift below; the USA disc's own `Language\Code\American.txt` (section 4, read 2026-10-06) checks
it line by line. **Verified against the USA file** unless a row says otherwise. Five lines are inserted and one is removed (205 PS2
lines, 209 Xbox lines).

| PS2 IDs | Xbox IDs | What |
|---|---|---|
| 0x00-0x20 | same | main and pause menu texts, "continue" 5, yes 3, no 2, the no-controller line 0x20 ("port 1") |
| (new) | 0x21-0x23 | "please reconnect the controller to port 2/3/4 and press start to continue": the other pads' no-controller notices |
| 0x21-0x40 | +3 | disc error 0x24/0x25, autosave on/off 0x26-0x28, the screen position hint's place 0x29 (a placeholder on the Xbox), save texts 0x2A-0x43 |
| 0x41 "retry" | (gone) | the Xbox has no retry text; its slot 0x44 is "create new save game" |
| (new) | 0x45 | "free more blocks" (save message 37, the Dashboard item) |
| 0x42-0x44 | +4 | autosave warning 0x46, autosaving 0x47, game over 0x48 |
| 0x45 | 0x49 | "quit game?" became a longer warning |
| (new) | 0x4A | "you will lose all unsaved progress by loading this game now..." |
| 0x46 on (to 0xCB) | +5 | press start 0x4B (the listing's "new 0x4B" was this), level names from 0x4C, format? 0x64, disable autosave title 0x65, next hint 0xB7, autosave failed 0xB8, no card 0xBC, cancel hint 0xC0 |
| 0xCC "create a save file" | (gone) | the file ends at 0xD0; the listing's 0xD1 doesn't exist on the USA disc |

The USA XBE's message table (the `mov [0x4A7144 + 4 * (message - 1)], text` run at VA 0x21B76) has **37** entries, not the PAL
listing's 38: messages 1-33 as in 1.2's Xbox column, then 34 → 0x44 (create new save game), 35 → 3 (yes), 36 → 2 (no), 37 → 0x45
(free more blocks). Whether the save code's message numbers themselves shift on the USA build (its call sites) isn't checked.

The Xbox's wording (USA, `~` a line break):

| Xbox ID | Text |
|---|---|
| 0x2A, 0x2C, 0x30 | checking hard disk.~~please don't turn~off your xbox console. |
| 0x2D, 0x2E, 0x31 | saving game.~~please don't turn~off your xbox console. |
| 0x2F, 0x32 | loading game.~~please don't turn~off your xbox console. |
| 0x33 | no saved games found on the~hard disk~~would you like to create a~(xxx)~save game? |
| 0x36 | no (xxx)~saved games found~on the hard disk. |
| 0x37 | your xbox doesn't have~enough free blocks~to save games.~~you need to free~(x) more blocks. (**blocks**: N8 is the Xbox's) |
| 0x38 | are you sure you wish to~overwrite this save game? |
| 0x39, 0x3A | select slot to save / select slot to load |
| 0x3C, 0x3D | the saved game is damaged~and cannot be used. (save failed and load failed) |
| 0x29, 0x2B, 0x34, 0x35, 0x3B, 0x64, 0xBC | placeholders: the screens the Xbox can't reach (no card, unformatted, format) |
| 0x3E-0x43 | save-file / empty / cancel / format / continue without saving / overwrite |
| 0x44, 0x45 | create new save game / free more blocks |
| 0x46 | please don't turn~off your xbox console~when the autosave indicator~(below) is present. |
| 0x47 | autosaving data.~~please don't turn~off your xbox console. |
| 0xB9-0xBB, 0xBD, 0xBE | format successful / save successful / load successful / cancel save? / save cancelled |

The native build's English save texts follow this wording with the computer for "your xbox console" and "the hard disk"
(native/PC_TEXT.md); the other languages keep the PS2's own sentences reworded (the USA disc has English only).

### 2.6 What the Xbox player sees (reachable screens only)

- **Boot**: no save screens at all.
- **New Game**:
  1. "checking..." (Xbox 0x2C), at least 3 s.
  2. If there's no save: "create a save?" (0x33) with continue without saving / yes / no. Yes → creating (0x2D) → slots (0x39).
     No → back to the main menu.
  3. If there's a save: loading header (0x2F) → slots.
  4. A slot: overwrite? (0x38, yes/no) or straight to saving (0x31) → "save successful" (0xBA) → the game starts, autosave on.
  5. Not enough space: "insufficient free space ... (x) [blocks]" (0x37) with continue without saving / item 8 (Dashboard).
- **Load** (main or pause menu): checking → no save: 0x36 with cancel. A save: loading → slots with saves → loading (0x32) →
  "load successful" (0xBB).
- **Pause save**: as New Game without "continue without saving" except on NoRoom, where it's the only choice. Cancel or back
  anywhere ends at once, with no "cancel save?".
- **Failures**: save failed (0x3C) with continue without saving (NG) / cancel / retry (retry → Create, 2.3.5). Load failed (0x3D)
  with cancel. Create failing gives the NoRoom screen.
- **Autosave**: as on the PS2: autosaving icon with Xbox text 0x47, failure notice 0xB8. There is no polling, and nothing to pull out.

---

## 3. Native changes (`#ifdef TWIN_NATIVE`, PS2 build unchanged)

**Done** (NATIVE.md, "Saves"): N1, N2, N4, N5, N6; N7 as a PC set (NoRoom: continue without saving for a new game, cancel, retry,
which measures again); N9 as the PC wording instead of the Xbox's strings (native/PC_TEXT.md, the XISO isn't available); N10, N11,
N13 and N14 as recommended (no Dashboard item, the PS2's retry, the PS2's folder rewrite, the PS2's layout). Not done: N3 (the PS2's
2 s stays: the Xbox's 3 s is its certification rule), N8 (KB stays), N12 (the 8 MB card's space). Added for a computer: no "create a
save file?" question (the save is made at once, then the slots), and a load from a save with no game in its slots shows "no save
data". `--selftest-ui` runs the game's save code through each of these.

The native card already makes `HasCard` and `CardFormatted` always true (`memorycard.cpp:1-5`). With the changes below, the reachable
screens are the Xbox's: Message, NoRoom, Create, InsertSave, Overwrite, the slots, SaveFailed and LoadFailed. Every change is a
`#ifdef TWIN_NATIVE` / `#else` around the existing line, so the PS2 object code doesn't move.

### 3.1 Changes that are the Xbox's behaviour

| # | Where (PS2) | Native change | Xbox evidence |
|---|---|---|---|
| N1 | `gamecontroller.cpp:268-270` (`case StateCheckingCard`) | `next = StateVivendiLogo;` and no `CheckingCard` call. The Vivendi state's first frame already hides every screen (`PlayMovie` 1429-1433), so the legal picture's fade is covered | 2.4.1 |
| N2 | `gamecontroller.cpp:519-533` (`SavingWait`) | `return 0;` before the timer: no CheckInserted, no `SavingSaved` | 2.4.2 |
| N3 | `gamecontroller.cpp:1149, 1209, 1218, 1228` (and 1254, moot after N1) | request time `static_cast<s32>(g_ClockUnitsPerSecond * 3.0f)` (one native constant) | 2.4.3 |
| N4 | `savecode.cpp:143-146` | `ShowChoicesScreen(this, SaveScreenNoRoom)` instead of InsertRoom | 2.3.1 |
| N5 | `savecode.cpp:356-375` (Create's failure) | split `case SaveDevice::OperationCreate:` off to `ShowChoicesScreen(this, SaveScreenNoRoom); return;` | 2.3.3 |
| N6 | `savecode.cpp:476-499` (back) | the screens listed there call `EndOperation(this, 0, 0)` for every operation, with no CancelSave | 2.3.4 |
| N7 | `savepages.cpp:170-229` (`ShowItems`) | Xbox sets: NoCard and NoRoom show `LeaveItem` always (not Retry or Continue). Create shows Leave (NG), Yes, No (not Create or Cancel). FormatFailed and LoadFailed show Cancel only | 2.3.6 |
| N8 | `savecode.cpp:542-560` (`MessageText`) | `(x)` = `(NeededBytes() - FreeBytes()) >> 14` blocks, but only if the Xbox text for message 16 says blocks (XISO). Otherwise keep KB. Either way it's only seen on NoRoom, which the 8 MB emulated card practically never reaches (see N12) | 2.3.7 |
| N9 | `gamecontext.cpp:361-378` + language loading | use the Xbox's strings for the save texts (3.3) | 2.5 |

A sketch of N6 (the other changes are one-line swaps):

```cpp
    if (answer == AnswerBack)
    {
        switch (screen)
        {
        case SaveScreenCreate:
        // ... the same list ...
        case SaveScreenLoadFailed:
#ifdef TWIN_NATIVE
            // The Xbox's: back ends the operation, no "cancel save?" (func.asm:612702)
            EndOperation(this, 0, 0);
#else
            if (operation == SaveOperationNewGameSave || operation == SaveOperationPauseSave)
            {
                ShowChoicesScreen(this, SaveScreenCancelSave);
            }
            else
            {
                EndOperation(this, 0, 0);
            }
#endif
            return;
```

N6's effect in play: cancelling a pause save goes back to the pause menu at once (`SavingPaused` unflagged,
`gamecontroller.cpp:612-623`). If autosave was on, it turns off with the "autosave disabled" notice, as it does after the PS2's
"cancel save? yes".

### 3.2 The Xbox's choices that a computer can't or shouldn't copy (each needs a decision)

| # | Xbox behaviour | Recommendation |
|---|---|---|
| N10 | Item 8 on NoRoom (NG): message 38 (Xbox text 0x45) opens the Dashboard's memory screen | There's no dashboard. Leave the page at 8 items. NoRoom then offers "continue without saving" only, which is the Xbox's own set for a pause save. If the user wants it anyway, a ninth item would need `SaveMessageCount` 39, a mapping for message 38 and an action. Opening the saves folder in Finder or a file manager is the nearest equivalent, but it is not the game's behaviour |
| N11 | Retry after "save failed" asks for Create (2.3.5), which clears every slot's summary and writes all four banks | **Keep the PS2's Retry (measure again).** Copying the Xbox's would show four empty slots and overwrite the other saves (inferred from `savedevice.cpp:442-460`, the same steps as Xbox `FUN_00201D80`/`Step`). This is a deliberate exception to the Xbox flow; note it in NATIVE.md |
| N12 | Free space from the disk (`GetDiskFreeSpaceExA`), need in 16 KB blocks | Optional. `memorycard.cpp` reports an 8 MB card minus its files. Reporting `std::filesystem::space(SettingsFolder())` instead (capped to a positive `s32` of clusters) would make NoRoom real. Without it, NoRoom (with the Xbox's choices) is effectively never shown |
| N13 | `WriteSlot` without the `fresh` path, and ReadFolder failure → SaveFailed (2.3.2, 2.3.3) | Optional, and only visible after a corrupt `BESLES-52568` folder file. The PS2's path (show the slots and rewrite every file) recovers. The Xbox's shows "save failed", and its Retry (N11) would recover by wiping. If copied for exactness: `savecode.cpp:74-78` and `161-176`, and `376-391` to the failure list. Recommendation: **keep the PS2's** |
| N14 | Save layout (an Xbox save game "u:\…\Header.bin", `Bank*.bin` with 20-byte signatures, no icons) | Keep the native card's PS2 layout (`BESLES-52568CRASH/…`). The storage format isn't visible to the player, and the PS2 layout keeps the round trip `--selftest-saves` checks and stays compatible with real PS2 saves copied in |

### 3.3 Getting the Xbox's strings

The native build reads the PS2 disc's `Code/<language>.txt`. The Xbox's lines are numbered differently (2.5), so they can't simply
replace the file. Two options:

**A. Recommended.** A native-only override at language load. After `ReadTextFile` for `CodeTexts`, if Xbox data is configured (a
`--xbox-data` option or local.json key, like `disc_image`), read the Xbox `Language\Code\<language>.txt` from it. Then set
`g_Texts[CodeTexts][ps2Id] = xboxLines[xboxId(ps2Id)]` for the IDs below, keeping the PS2 numbering that OLEG and
`SaveMessageTexts` use, so no game code constant changes. `xboxId` is the shift of 2.5: +0 up to 0x20, +3 for 0x21-0x41, +4 for
0x42-0x46, +5 from 0x47. Without Xbox data, the PS2 texts stay. That needs one entry in NATIVE.md's differences table.

**B.** Swap the IDs at their uses: `SaveMessageTexts` and OLEG's label constants under `TWIN_NATIVE`, and read the Xbox file as the
whole `Code` file. That's more invasive: every OLEG text ID shifts, and the level names, galleries and hints would all have to
follow.

The PS2 IDs to override with option A, and their Xbox lines:

- Save messages: 0x27-0x3A → 0x2A-0x3D, 0x3B-0x3F → 0x3E-0x42, 0x41 → 0x44, 0x5F → 0x64, 0xB4-0xB9 → 0xB9-0xBE, 0xB7 → 0xBC,
  0xCC → 0xD1.
- OLEG: 0x23-0x25 → 0x26-0x28 (autosave on/off/re-save), 0x42 → 0x46 (autosave warning), 0x43 → 0x47 (autosaving), 0xB3 → 0xB8
  (autosave failed), 0x60 → 0x65 (disable autosave title).
- Optionally 0x16 (the "disable autosave" item, unchanged ID) if its wording differs.

Check each pair against the Xbox file before trusting the shift. The shift is verified at the cited points and inferred between
them.

### 3.4 Afterwards

- NATIVE.md's "Saves" row: replace "the Xbox version's console-storage flow is to come" with the flow above, and list N10, N11 and
  N14 as deliberate differences.
- Check it the existing way (docs/DEVELOPMENT.md:39-44): the `--press` scripts for New Game into the third save and Load Game of the
  fourth. The Load script's `--press 140:cross` past the autosave notice still applies. New Game timings move by the extra second
  per message (N3) and the missing boot check (N1).
- Build the PS2 side with `tools/build.py` and compare against the last PS2 build: it must be identical.

---

## 4. The Xbox data (the USA disc, read 2026-10-06)

`Games Files/XBOX FILES/Crash Twinsanity (USA, Asia).xiso.iso` (an XDVDFS image, sha1 76b8ac89...). It's the USA build, the listing
the PAL one, so addresses differ (the text IDs and the save flow's shape match except where 2.5 says). Nothing of it is in the
repository; the native build reads its font at run time when the user names the image (NATIVE.md, "Button prompts").

1. **No BH/BD archive**: 782 loose files. One language, "American" (`Language\<file>\American.txt`); the texts are 2.5's.
   Message 16's wording is in blocks (N8).
2. **`default.xbe`**: title "Crash Twinsanity", title ID 0x56550036, region 7. The save game's name is L"Saved Games" (UTF-16) at
   VA 0x38D668 in this build (between "Bank"/".bin" and "Header.bin"), pushed once at VA 0xB1938; the PAL listing's 0x38D570 holds
   other data here. "u:\\" is ASCII at VA 0x396268. The bytes 8-14 of the "continue" answer table (the listing's
   `ARRAY_0x00200b78`) are **still unread** (the USA address wasn't found).
3. **Menus** (verified on the PAL listing, the USA texts agree): select is A (button 9), back is B (10), leave is start (4)
   (`FUN_000B4270`, func.asm 181594-181640); the footer texts are "select \\" (A's glyph) and "] back" (B's). A is jump and skips
   movies and cutscenes.
4. **Button glyphs**: the font `Startup\Fonts\Crash.psf` has them on the PS2's codes ('\\' A, ']' B, '[' X, '^' Y, '{' '}' the
   triggers, 0xA6 0xAC "L" and "R"); no white, black, start or back glyphs.
5. Still not available: a real Xbox save, only needed if N14 is reconsidered.
