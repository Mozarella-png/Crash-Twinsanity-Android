# Menus, settings and controls in the modern Crash PC games, and a redesign for the native port

Researched 2026-10-07. The goal: replace the native build's menus, which feel clunky, with menus modelled on the modern
Crash and Spyro PC releases, drawn in Twinsanity's own font and colours.

How each claim is backed:

- **[S]** seen in an in-game screenshot I looked at. The images are on PCGamingWiki and the Game UI Database (linked in
  Sources).
- **[D]** documented in text by a source: PCGamingWiki's data tables, Activision support, reviews, guides.
- **[M]** from memory or inference, not verified. Check these in-game before relying on them.

## TL;DR

- **All three games use the same pattern.** Each menu page is a **vertical list**, with the label on the left and the value
  on the right of the same row. Choices are **left/right selectors**, shown `◀ VALUE ▶` with the arrows only on the
  focused row, and volumes are **0-10 sliders**. A page title sits at the top and a **prompt bar sits bottom-left**
  ("Ⓐ SELECT Ⓑ BACK"). The selected row gets a different colour, with a glow or pulse. **No tiles, no tabs.** The options
  page is a list of category buttons, and each category is its own page.
- **Rebinding is a per-action list.** It is never a list of PlayStation buttons. N. Sane shows **two keyboard columns**
  (primary and secondary, keyboard only). Crash 4 shows one key per action, with a **separate controller remap** screen
  and a pictured controller diagram. Spyro offers keyboard rebinding plus two fixed controller presets.
- **Button prompts follow the last device used.** In all three this makes prompts **flicker** with mixed input
  (PCGamingWiki notes it for every one of them). Crash 4 adds an **"Input Display Style"** override: Auto Detect or a
  fixed style.
- **The mouse works in every menu**, but never moves the camera in the two Crash games. Spyro alone has mouse camera
  and flight.
- **Accessibility leader: Crash 4.** It has subtitle size, style and background opacity, hint text size, a colourblind
  filter with strength, camera shake and motion blur toggles, Retro or Modern playstyle, inverted look and controller
  dead zone. N. Sane has almost nothing, not even subtitles.
- **CTR Nitro-Fueled has no PC version.** Its director said no PC version was ever planned. A 2025 leak claims a 2026 "Grand
  Prix Edition" for PC, but nothing was announced as of this date, so it is excluded.
- **Twinsanity's real control set is small**, read from the decomp in `src/game/controls.cpp`, `followrig.cpp`,
  `gamecontroller.cpp` and `oleg.cpp`:
  - Jump (✕)
  - Spin (□)
  - Crouch/Slide (○), which also gives body slam in the air and the Crash-throws-Cortex combo
  - Shoulder Left (L1 *or* L2) and Shoulder Right (R1 *or* R2): strafe while standing, hoverboard rise, pause-menu paging,
    camera turn rate
  - Status/HUD (△), which is also menu back and the restored cutscene skip
  - Pause (Start)
  - Move (left stick or D-pad), Camera look (right stick)

  Nina and Cortex use the **same** buttons with per-character meanings. The game reads L1 and L2 as one input, and R1
  and R2 as one, so the controls page should list **actions**, not the eight face and shoulder buttons it lists today.

## 1. What each game does

### 1.1 Crash Bandicoot N. Sane Trilogy (PC, 2018; Vicarious Visions, ported by Iron Galaxy)

**Structure**

- **Launch [D]:** one launcher for all three games. "After launching game, you will have the option to choose between"
  the three, and the announcer shouts each title as you scroll over it (N. Sane manual; Destructoid).
- **Pause [D]:** "Quit" ends the level. There is **no "Restart Level"** in normal play, only in Time Trials, which drew
  user complaints (GameFAQs). Other items such as Resume and Options are **[M]**.
- **Audio [D]:** the volume options appear only once you select a game and open its pause menu. Quote: *"You can only access
  them while playing and not from the main menu"* (Destructoid). This is a known pain point. Our port should not copy it.

**Settings layout [S]:** a wooden-plank background. Labels are right-aligned in yellow with an outline, values sit in a
right column as `◀ VALUE ▶`, and the focused row's label turns white.

- **Graphics page [S]:** Display Mode (Fullscreen/…), Resolution, Max FPS, VSync, Motion Blur, Preset (Custom/…),
  Anti-Aliasing (FXAA Low/Medium/High), Shadow Resolution, Ambient Occlusion, Bloom, Depth of Field, Fur Blur. A
  GPU/VRAM line sits at the bottom. Every item is a `◀ ▶` selector. There are no dropdowns or sliders.
- **Game/audio page [S]:** Invert Vehicle Controls, Invert Bazooka Aiming, Music Level, Speech Level, SFX Level (0-10
  selectors), **Reset to Defaults**.
- **Apply behaviour:** no Apply button appears in the screenshots, so changes probably apply when chosen **[M]**.
  PCGamingWiki notes that "Max FPS" and "VSync" are misnomers that only set the sync interval.

**Keyboard controls [S]** (PCGamingWiki screenshot; the list title is in the table, not in the image)

| Action | Primary | Secondary |
|---|---|---|
| Move Left | A | Left |
| Move Right | D | Right |
| Move Backward | S | Down |
| Move Forward | W | Up |
| Jump | Space | None |
| Attack | Left Click | None |
| Slide/Crouch | Right Click | None |
| Status/Inventory | Tab | None |
| Equip Bazooka | Q | None |
| Speed Shoes | Shift | None |

- The list has **two key columns: primary and secondary**. Mouse buttons can be bound. Pause and menu keys are not in the
  list, so they are probably fixed to Esc/Enter **[M]**.
- **Controller remap: no [D]** (PCGamingWiki; Destructoid: "you cannot rebind any gamepad options").
- **Prompts [D]:** Xbox and DualShock 4 glyphs, plus keyboard and mouse prompts. There is no override, and with
  simultaneous input the prompts "rapidly change".
- **Mouse in menus: yes [D]. Mouse camera: no.** Haptics are always on, for XInput only.
- **Accessibility [D]:** separate volumes. **No subtitles**, even though the Steam page claimed them.

### 1.2 Crash Bandicoot 4: It's About Time (PC, 2021; Toys for Bob, PC by Vicarious Visions, Unreal Engine 4)

**Title and main menu [S]** (Game UI Database screenshot)

- The logo sits in the centre. **Three stacked buttons sit bottom-right:** START GAME, BANDICOOT BATTLE, OPTIONS.
- The selected button turns orange with sparkles. The others are blue slanted plates.
- The prompt bar sits bottom-left: **Ⓐ SELECT · Ⓧ SAVE SLOTS · Ⓨ CHANGE USER · ☰ CREDITS**. Secondary functions live
  on face buttons shown in the bar, not as list items.

**Options root [S]:** the title "OPTIONS", then a centred vertical list: **GAMEPLAY, CONTROLS, VIDEO, SOUND, SUBTITLES,
PRIVACY & ACCOUNT**. Prompt bar: Ⓐ SELECT, Ⓑ CANCEL.

**Pages** ([S] = screenshot, [D] = Activision accessibility article)

| Page | Items |
|---|---|
| Gameplay [S] | Playstyle (Modern/Retro), Time Trial Ghost (On/Off), Enhanced Shadows (On/Off) |
| Video [S] | Camera Shake, Motion Blur, VSync, Windowed Mode (Fullscreen/Windowed/Windowed Fullscreen), Screen Resolution (a **dropdown**: the value plus a ⌄ chevron), **ADVANCED VIDEO** (a sub-page button), **SAVE SETTINGS** (an explicit commit button) |
| Advanced Video [D] (PCGamingWiki) | TAA (Off/1x/2x/4x/8x), a frame cap (30, 60, 75, 90, 120, 144, 165, 240, unlimited) and more quality items. There is also a built-in benchmark |
| Controls [S] | A **pictured controller** with every default labelled, then Controller Vibration, **CHANGE BUTTON MAPPING**. The PC page also has Controller Dead Zone (a **slider**, shown at 25), Horizontal Look and Vertical Look (Normal/Inverted), and **Input Display Style** (Auto Detect, or a fixed Xbox, PlayStation or Switch style) |
| Sound [S] | Music Volume, SFX Volume, Voice Volume: **sliders 0-10** with the number in a bubble at the end of the track |
| Subtitles [D] | Subtitles (default On), Text Size, Text Style (3 fonts), Background Opacity (slider, 0 means none), Hint Text Size (Medium/Large) |
| Colour vision [D] | Colorblind Mode (Off, Deuteranopia, Tritanopia, Protanopia), Filter Strength (0-10) |

**Widgets [S]**

- Each row is a slanted blue plate, with the label left and the value in a dark inset right.
- The focused row is orange, with **big ◀ ▶ chevrons outside the row** that can be clicked.
- Values are ON/OFF selectors, sliders, or a dropdown for resolution.

**Applying changes [S]:** the Video page has an explicit **Save Settings** button. There is no revert timer anywhere.

**Keyboard bindings [S]** (PCGamingWiki screenshot, **one key per action**)

| Action | Key |
|---|---|
| Jump | Space Bar |
| Primary (spin) | J |
| Secondary (crouch/slide) | K |
| Mask Power | I |
| Special | L |
| Show HUD | Tab |
| Walk | Left Ctrl |
| Move Forward / Backward / Left / Right | W / S / A / D |
| Camera Up / Down / Left / Right | Up / Down / Left / Right |

**Controller defaults [S]** (Xbox layout on the Control Options screen)

| Input | Action |
|---|---|
| A | Jump |
| X | Primary (spin) |
| B | Secondary (crouch/slide) |
| Y | Mask Power |
| RB | Secondary |
| RT | Special/Mask Power |
| LT | Show HUD |
| L stick, D-pad | Move |
| R stick | Camera |
| View | Level Stats |
| Menu | Pause |

The PS4 layout is the same with PlayStation glyphs.

- **Remappable actions [D]:** Move, Jump, Primary Action, Secondary Action, Mask Power, Special/Mask Power, Show HUD.
  Controller remap is **yes** (PCGamingWiki), with separate DS4, Xbox and Switch remap screens.
- **Prompts [D]:** Xbox One, DualShock 4 and Switch glyphs, overridable in game. They flicker when the controller and the
  keyboard are both in use.
- **Mouse in menus: yes. Mouse camera: no [D].** Inverted controller Y is supported, plus haptics.

**Post-level menu [S]:** two big arrow buttons, ⟵ REPLAY and CONTINUE ⟶, with the prompt bar Ⓐ SELECT Ⓑ BACK.

**Pause [D]:** you can **warp to any level from the pause menu** and **restart the level** (crashmania.net analysis).
The exact item names are **[M]**.

### 1.3 Spyro Reignited Trilogy (PC, 2019; Toys for Bob / Iron Galaxy, Unreal Engine 4)

**Layout [S]:** a page title at the top ("CONTROLS", "CAMERA", "VOLUMES") over an ornate purple panel. Items are a centred
list in the form **"LABEL: VALUE"** inline, and the focused row gets a magenta band with fleur-de-lis end caps.

**Pages [S]**

| Page | Items |
|---|---|
| Controls | Vibration: On, Control Scheme (presets Reignited/Retro), Keyboard Controls (sub-page), Move List, Use Mouse for Flying: On, Invert Mouse Flying Y: Off |
| Camera | Camera: Passive/Active, Invert Camera X, Invert Camera Y, Mouse Sensitivity (slider, 25) |
| Volumes | Music, Effects, Voice (gold sliders 0-10) |
| Video | Display Mode, Resolution, Max FPS (Unlimited…), VSync, Motion Blur, Vignette, Graphics Quality (preset), Bloom, Anti-Aliasing (High TAA…), SSR, Ambient Occlusion, Shadow Quality, Foliage Quality, View Distance Quality |

**Pause [D]:** includes Options and "Exit Level" (at the bottom). "Quit Game" is mentioned as highlighted
(speedrun.com forum, search snippet). Inventory and Guidebook as items are **[M]**.

**Keyboard defaults [D]** (KosGames / Magic Game World guides)

| Action | Key |
|---|---|
| Move | W/A/S/D |
| Jump | Space |
| Charge | Left Shift |
| Flame | Left Click |
| Free look / Headbash / Hover | F |
| Roll left / right | Q / E |
| Guidebook | Tab |
| Point to treasure | P |
| Camera rotate left / centre / right | 1 / 2 / 3 |
| Skip cutscenes | Space |

- **Rebinding [D]:** keyboard yes. Controller presets only.
- **Mouse [D]:** works in the pause and start menus. Mouse camera with a sensitivity slider. Optional mouse flying.
- **Prompts [D]:** they flicker with mixed input, and the switch **plays a UI sound**.
- **Accessibility [D]:** subtitles, which always follow the audio language.

### 1.4 Patterns worth copying, and their failures

| Pattern | N. Sane | Crash 4 | Spyro | Take for Twinsanity |
|---|---|---|---|---|
| Vertical list, label left and value right | ✓ | ✓ | inline "LABEL: VALUE" | ✓ but keep **Twinsanity's centred list** for menus that only hold links |
| `◀ value ▶` selectors, arrows on the focused row only | ✓ | ✓ (outside the row, clickable) | – | ✓ |
| 0-10 volume sliders | selector | slider | slider | ✓ slider, value shown as a number |
| Category buttons, each opening a page (no tabs) | – | ✓ | ✓ | ✓ (works with the game's page/parent model) |
| Bottom-left prompt bar | – | ✓ | ✓ | ✓ (clickable) |
| Explicit "Save Settings" | – | Video only | – | ✗. Apply live and save on leaving, with a revert timer only for display mode/resolution (our own addition, not from these games) |
| Two keyboard columns | ✓ | ✗ | ✗ | ✓ (our bindings already have `keys[2]`) |
| Controller remap | ✗ | ✓ | presets | ✓ |
| Prompt style override | ✗ | ✓ | ✗ | ✓ |
| Prompt switching with no hysteresis (flicker) | ✗ | ✗ | ✗ | **Fix it:** debounce |
| Audio settings only reachable in game | ✗ (bad) | – | – | **Avoid:** the same Options from the title screen and the pause menu |
| Restart from pause | Time Trial only (complained about) | ✓ | – | ✓ ("restart checkpoint" already exists natively) |

## 2. Twinsanity's own controls (from the decomp)

| PS2 input | What it does in the game | Where |
|---|---|---|
| Left stick / D-pad | Move (all characters, vehicles, the hoverboard) | `controls.cpp` axis bindings |
| Right stick | Camera look: the follow rig's yaw and pitch input | `followrig.cpp` (`AxisLookX/Y`) |
| ✕ Cross | Jump. In menus: select | `ActionCross`; `MenuInput::ActionSelect` |
| □ Square | Spin. Nina and Cortex use it for their own attacks | `ActionSquare` |
| ○ Circle | Crouch and slide. In the air it gives the body slam. Jump then ○ throws Cortex in the Crash+Cortex sections | `ActionCircle` |
| L1 **or** L2 | Shoulder Left: strafe while standing, hoverboard rise, the follow camera's turn rate, pause menu page left | `ActionL` = L1 \| L2; `PausePage` |
| R1 **or** R2 | Shoulder Right: the same, page right. Some scripts read R1 alone (`IsR1Pressed`) | `ActionR` = R1 \| R2 |
| △ Triangle | Shows the HUD/status in normal play. In menus: back. The native option makes it skip cutscenes | `gamecontroller.cpp:407`; `oleg.cpp:1479`; `conditionchecks.cpp` |
| Start | Pause. In menus: "leave" | `ActionStart`; `MenuInput::ActionLeave` |
| Select, L3, R3 | Not used in play | – |

The same buttons mean different things for each character, so the bindings page should show **one row per action**.
A help or move-list page can then give the per-character meaning, the way Spyro's "Move List" does.

## 3. Recommended design for the native port

### 3.1 Look

- **Font:** the game's own menu font, which `oleg.cpp` passes to `SetItemStyles` as `menuFont`, and its title font. Keep
  the game's **lowercase** text and its button glyph codes (`\ ^ { } ¦ ¬ < > [ ]`) for PlayStation prompts.
- **Colours:** the game's own menu styles. The selected item is `ColourWhite` with the titles' breathing pulse
  (`g_BreathingScale`). Unselected items are `ColourLightGrey`, disabled ones `ColourDarkGrey`. Colours are on the
  game's scale, where 192 counts as full. These replace Crash 4's orange-and-blue plates.
  - Any new accent, such as the selector arrows, the slider fill or the prompt-bar glyph rings, should be **sampled from
    the game's own art**: the logo's orange and yellow, or the save-slot ring panels' colours. Don't invent new hues.
- **Layout:**
  - Title at the top, at `TitleOffset`. Pages that only hold links keep the game's centred list (`ListStyle`).
  - **Settings pages** use the N. Sane and Crash 4 two-column row: label right-aligned in a left column, value in a right
    column, `◀ value ▶` arrows drawn only on the focused row.
  - **Prompt bar** at bottom-left, as glyph plus label pairs.

### 3.2 Menu tree

```
Title ("press any button")
└─ Main menu (centred list)
   ├─ continue              (loads the last save; hidden when there is none)
   ├─ new game
   ├─ load game             (the game's save slots page)
   ├─ options ──────────────► Options (shared, see 3.3)
   ├─ extras                (galleries/movies, when unlocked)
   └─ quit to desktop       (PC must-have; confirm yes/no)
   prompt bar: ✕ select · ○ back · (keyboard: enter select · esc back)

Pause (in play)  – the game's paged pause (L/R pages: pause · levels · extras) kept
   ├─ resume
   ├─ restart checkpoint    (native text 0x133)
   ├─ level select          (native text 0x134; jumps to the levels page)
   ├─ save game / disable autosave   (game's own, as now)
   ├─ options ──────────────► the same Options pages
   └─ quit game             (confirm; to title)
   prompt bar: ✕ select · ○ back · L1/R1 ◀ page ▶ (keyboard Q/E)

Options (category list, centred)
   ├─ gameplay
   ├─ display
   ├─ audio
   ├─ controls ─┬─ keyboard bindings
   │            └─ controller bindings
   ├─ accessibility
   └─ back
```

Options must be **identical from the title and from pause**. N. Sane's audio, reachable only in game, is the anti-pattern.

### 3.3 Settings per page

Widgets: **[sel]** is a `◀ ▶` selector, **[tog]** an on/off selector, **[sld]** a 0-10 slider (dead zone 0-50%),
**[act]** an action row, **[sub]** a sub-page.

| Page | Item | Widget | Values (default first) | Status |
|---|---|---|---|---|
| **Gameplay** | vibration | tog | on/off | moved from the game's "game options" (`VibrationItem`) |
| | skip cutscenes (hold △) | tog | off/on | existing `OptionSkipCutscenes` |
| | fast loading | tog | on/off | existing |
| | bug fixes (retail fixes) | tog | on/off | existing `OptionRetailFixes` |
| | auto pause when inactive | tog | on/off | existing `OptionPauseOnFocusLoss` |
| | language | sel | en/fr/de/es/it | new (PAL M5 has all five) [M: check the game supports switching at run time] |
| **Display** | display mode | sel | windowed / borderless / fullscreen | split out of today's combined "display" choice |
| | window size | sel | 960…1920, widescreen variants | existing choices, windowed only |
| | render scale | sel | 1x–4x | existing (`ResolutionItem`) |
| | widescreen | tog | off/on | the game's own (`WidescreenItem`) |
| | refresh rate | sel | 50 hz / 60 hz | existing |
| | texture filtering | sel | smooth / sharp | existing |
| | crt filter | tog | off/on | existing |
| | hd textures | tog | off/on (greyed out when no pack is found) | new: see `HD_TEXTURES.md` |
| | screen position | act | opens the game's centring screen | the game's own |
| **Audio** | music | sld | 10 | the game's own |
| | sound effects | sld | 10 | the game's own |
| | voices | sld | 10 | existing native |
| | output | sel | stereo / mono / pro logic ii | the game's own |
| | mute when inactive | tog | on/off | new (N. Sane has it) |
| **Controls** | keyboard bindings | sub | – | 3.4 |
| | controller bindings | sub | – | 3.4 |
| | invert camera x / invert camera y | tog | off | new (Crash 4 "Horizontal/Vertical Look"), on the follow rig's look axes |
| | camera speed | sld | 5 | new [M: scale `AxisLookX/Y`] |
| | stick dead zone | sld | 25% | new (Crash 4) |
| | button prompts | sel | auto / keyboard / xbox / playstation / nintendo | existing prompt style, exposed as Crash 4's "Input Display Style" |
| | nintendo layout | sel | positional / by label | existing `OptionNintendoLayout` |
| | mouse in menus | tog | on/off | existing `OptionMouse` |
| | reset controls | act | confirm yes/no | existing text 0x11A |
| **Accessibility** | colour-blind filter | sel | off / deuteranopia / protanopia / tritanopia | new (Crash 4). A post filter on the presenter (`NativeGraphicsSetPostFilter` already exists) |
| | filter strength | sld | 10 | new |
| | camera shake | tog | on/off | [M: only if the game's shake can be scaled] |
| | hold to repeat spin | tog | off/on | [M: optional; reduces mashing] |

### 3.4 Bindings pages

Each page is an action list. One row per action.

| Action (row) | Keyboard primary | Keyboard secondary | Controller (SDL position) | Game input |
|---|---|---|---|---|
| move forward | W | – | left stick / D-pad (fixed) | left stick up |
| move back | S | – | (fixed) | left stick down |
| move left | A | – | (fixed) | left stick left |
| move right | D | – | (fixed) | left stick right |
| camera up | Up | – | right stick (fixed) | right stick up |
| camera down | Down | – | (fixed) | right stick down |
| camera left | Left | – | (fixed) | right stick left |
| camera right | Right | – | (fixed) | right stick right |
| jump | Space | – | South (✕ / A) | ✕ |
| spin | J | Mouse Left | West (□ / X) | □ |
| crouch / slide / body slam | K | Mouse Right | East (○ / B) | ○ |
| shoulder left (strafe ◀, hover, page ◀) | Q | – | LB **and** LT | L1 \| L2 |
| shoulder right (strafe ▶, hover, page ▶) | E | – | RB **and** RT | R1 \| R2 |
| status / hud | Tab | I | North (△ / Y) | △ |
| pause | Esc (fixed) | Enter | Start/Menu | Start |

**Why these defaults.** They follow Crash 4, the most recent first-party PC Crash scheme:

- **Camera on the arrow keys**, because Twinsanity, like Crash 4, has a real look camera on the right stick.
- **Spin on J, Crouch on K, Status/HUD on Tab, Jump on Space.**
- **Mouse Left and Mouse Right** as secondary keys for Spin and Crouch, following N. Sane's Attack (Left Click) and
  Slide/Crouch (Right Click).

**Alternative.** An N. Sane-style scheme puts arrows on movement as a secondary set and camera look on IJKL. It is
worse here because it gives up Crash 4's J/K muscle memory.

**What changes from today.** The current defaults in `bindings.cpp` are already close: Space and K for ✕, J for □, L
for ○, I for △, Q and E for L1 and R1, and 1 and 3 for L2 and R2. The work is to:

- relabel the rows as actions
- merge L1/L2 into one action and R1/R2 into another
- move ○ from L to K, and △ from I to Tab, with I kept as △'s secondary. K is ✕'s secondary today, so it comes off ✕
- add camera keys. Today the keyboard has **no camera look** at all.

The controller page lists the same actions with one button each. Sticks and the D-pad are fixed, as in Crash 4. The
title bar shows a picture of the pad with the defaults labelled, Crash 4's Control Options screen.

**Interaction rules for rebinding**

1. Select a row and the value cell shows "press a key" (native text 0x11B). The next key, mouse button, or pad button
   (on the controller page) binds. **Esc cancels.** Esc and the system keys cannot be bound. **Backspace or Delete
   clears** the focused cell.
2. **Conflict: swap.** If the input already belongs to another action, the two actions swap, and a one-line notice shows
   "spin ↔ crouch". The current `BindButton` already swaps. Change `BindKey` from "take it" to "swap" as well, so no
   action is ever left unbound by accident. Allow duplicates only between primary and secondary of the *same* action.
3. **Context sets.** Gameplay bindings are one set. Menu navigation is fixed:
   - confirm: Enter, Space, Mouse Left, South
   - back: Esc, Backspace, Mouse Right, East, **and △/North as an alias**, so original players' muscle memory still
     works
   - move: arrows, WASD, D-pad, stick

   A gameplay rebind therefore never strands the menus.
4. **Reset to defaults** per page (keyboard or controller), with a confirm yes/no.
5. **Changes apply instantly and save on leaving the page.** There is no Apply button.

### 3.5 Navigation, confirm/back, and mouse

| Rule | Detail |
|---|---|
| Move focus | Up/down: D-pad, left stick (repeat after 400 ms, then every 80 ms), arrows, W/S, mouse hover (already in `mouse.cpp`). **Wrap around** at the ends |
| Change a value | Left/right: D-pad, stick, arrows, A/D, a click on the ◀/▶ arrows, the mouse wheel over the row, or dragging a slider (new) |
| Confirm | South (✕/A, or the Nintendo positional bottom button), Enter, Space, left click |
| Back | East (○/B), △/North as an alias, Esc, Backspace, right click. Back from a settings page **saves**. Back from Options returns to wherever it was opened from |
| Display changes | Apply at once. A **"keep these settings? (15)"** countdown reverts **display mode** and **window size** only. This is a PC convention, not something these Crash games do |
| Prompt bar | Bottom-left, built per page from the actual bindings, e.g. `[✕] select [○] back [□] reset`. Each pair is clickable |
| Prompt switching | Change style only on a **button or key press, or a stick past 50%**. Require **≥ 500 ms** on the new device before switching (hysteresis), and **no sound** on switching. This fixes the flicker all three games have |
| Prompt override | The "button prompts" setting: auto / keyboard / xbox / playstation / nintendo |
| Cursor | Shows on mouse movement and hides after 3 s or when playing (already in `mouse.cpp`) |
| Sounds | The game's own menu sounds (move, select, back) as now |

### 3.6 What changes in the current code

- **`ui/bindings.*`.** Today the functions are PS2 buttons (`BindCross` … `BindLeftStickRight`). Add an **action
  layer**:
  - `jump`, `spin`, `crouch`, `shoulderL`, `shoulderR`, `status`, `pause`, `move*`, `camera*`
  - each action maps to a report bit set, e.g. `shoulderL` → `PadBitL1|PadBitL2`, and `camera*` → right stick
    `rightX`/`rightY`, which nothing on the keyboard feeds today
  - `Binding{keys[2], button}` stays as it is
  - `BindingName` returns action names: native texts 0x125 jump, 0x126 crouch, 0x127 status and 0x128 spin already
    exist. Add "camera", "shoulder left/right" and "pause"
- **`ui/controls.cpp`.** Split into keyboard and controller pages. Draw a two-column `◀ key ▶` row style. Add swap
  notices and clear-with-Backspace.
- **`ui/options.cpp` and `optionitems.cpp`.** Regroup the existing native options into the pages in 3.3. Add the prompt
  bar, the selector arrows, and sliders for volumes and the dead zone.
- **`olegpages.cpp`.**
  - Main menu: add continue, extras and quit to desktop.
  - Options: replace the game's graphic/sound/game pages with the new categories, keeping the game's own items
    (widescreen, centring, volumes, output, vibration) in them.
  - Pause: add restart checkpoint and level select, both with existing texts.
- **`oleg.cpp:1479` menu input.** Add East (○/B) as `MenuInput::ActionBack` next to △.

## Sources

**PCGamingWiki** (wikitext through the MediaWiki API; screenshots through the Wayback Machine)

- https://www.pcgamingwiki.com/wiki/Crash_Bandicoot_N._Sane_Trilogy, with screenshots
  `Crash_Bandicoot_N._Sane_Trilogy_keyboard.png`, `…_graphics.png` and `…_game.png`
- https://www.pcgamingwiki.com/wiki/Crash_Bandicoot_4:_It%27s_About_Time, with screenshots
  `Crash_4_Keyboard_Bindings.png`, `Crash_4_Video_and_Graphics_Settings.png`, `Crash_4_General_Settings.png` and
  `Crash_4_Gamepad_and_Camera_Settings.png`
- https://www.pcgamingwiki.com/wiki/Spyro_Reignited_Trilogy, with screenshots `Spyro_Reignited_Trilogy_controls.png`,
  `…_camera.png`, `…_video.png` and `…_audio.png`

**Game UI Database, Crash 4** (https://www.gameuidatabase.com/gameData.php?id=525): main menu, options, sound options,
control options and post-level screenshots, files `uploads/Crash-Bandicoot-4-Its-About-Time01172021-115152-66603.jpg`,
`…115152-8673.jpg`, `…115154-43103.jpg`, `…115153-33545.jpg` and `…115201-17419.jpg`

**Crash 4**

- Activision, Crash 4 accessibility: https://support.activision.com/crash-bandicoot-4/articles/crash-bandicoot-4-its-about-time-accessibility
- Crash 4 controller layout: https://spottis.com/games/crash-bandicoot-4-controls/
- Crash 4 pause warp and restart: https://www.crashmania.net/en/blog/crash-bandicoot-4-analysis-content/

**N. Sane Trilogy**

- Destructoid, *PC Port Report: N. Sane Trilogy*: https://www.destructoid.com/pc-port-report-crash-bandicoot-n-sane-trilogy/
- N. Sane Trilogy online manual (PS4): https://m.media-amazon.com/images/I/D1oCBd-qMeS.pdf
- N. Sane controls (gamepressure): https://www.gamepressure.com/crash-bandicoot-n-sane-trilogy/controls/z49ef0
- GameFAQs, restart-level complaint: https://gamefaqs.gamespot.com/boards/191634-crash-bandicoot-n-sane-trilogy/75560490

**Spyro Reignited Trilogy**

- Keyboard defaults: https://kosgames.com/spyr-reignited-trilogy-keyboard-controls-bindings-31275/,
  https://www.magicgameworld.com/spyro-reignited-trilogy-pc-keyboard-controls/,
  https://steamcommunity.com/sharedfiles/filedetails/?id=2861763786
- Pause menu: https://www.speedrun.com/spyrortconsoletimetrials/forums/aknnj

**CTR Nitro-Fueled on PC**

- https://en.wikipedia.org/wiki/Crash_Team_Racing_Nitro-Fueled
- https://twistedvoxel.com/crash-team-racing-nitro-fueled-director-reveals-reasons-behind-no-pc-port/
- Leak: https://gamedaily.com/features/crash-team-racing-nitro-fueled-pc-port-coming-in-2026

**Twinsanity's controls (this repo)**

- `src/game/controls.cpp`, `followrig.cpp`, `characterframe.cpp:491-497`, `gamecontroller.cpp:407, 1964-1972`,
  `oleg.cpp:313-335, 1479`, `olegpages.cpp`, `conditionchecks.cpp:775-1516`
- `src/platform/native/ui/bindings.*`, `controls.cpp`, `mouse.cpp`, `prompts.cpp`
- `native/PC_TEXT.md` (native texts 0x10F-0x134)
