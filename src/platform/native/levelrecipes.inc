    {"mod/levels/beach.ops", GateSkips,
     R"(# Aku Aku crate cutscene (L01B) on N. Sanity Beach: skippable with Triangle
file Levels\Earth\Hub\beach.rm2
addbody 6367 7 572 1 8               # COM_CUTSCENE_L01B: while the camera sequence plays, Triangle (cond 572, player 1) -> state 8 = normal ending path
clearbodies 6365 5                   # director: drop the dead skip rule in orphan state 5
copybody 6371 1 1 0 2 51 207 0.2     # COM_AKU_CS_L01B: in state 1 also accept message 207 (sent by the ending) -> run SetObject(2,0) and go to state 2,
                                     # so the crate Aku finishes and hides even when his pop-out animation was cut short by a skip
)"},
    {"mod/levels/beach_training.ops", GateSkips,
     R"(# N. Sanity Beach training
file Levels\Earth\Hub\beach.rm2
copybody 6821 1 5 0 4 572 1 0   # COM_BEACH_TRAINING_CUTSCENE_DIRECTOR_ACTIVATED: state 1 plays the cutscene; add the developers' Triangle rule (cond 572, player 1) -> state 4
clearbodies 6821 5                  # drop the unreachable original in state 5
)"},
    {"mod/levels/bell_tower.ops", GateSkips,
     R"(# Bell tower, Madame Amberly (B03A) - a Cortex level: the rig has to arrive as Cortex (tools/rig/make_cortex_state.py).
# The scene ends by giving Cortex control and Madame Amberly starting the fight, so the skip has to run that ending.
# Like the core intro, it lives inside the scene script (COM_BELL_TOWER_CUTSCENE_B03A).
file Levels\school\Madame\amberly.rm2
# Triangle rule (cond 572, player 1) in every state the scene waits in -> 12. Not in its instant states: a polled
# rule there stops the state from stepping on.
addbody 6187 5 572 1 12
addbody 6187 7 572 1 12
addbody 6187 9 572 1 12
addbody 6187 11 572 1 12
# 12 (unused): the scene's last message step (state 4: Madame Amberly and Cortex) with 244 "skipped" -> 13
addbody 6187 12 5 0 13 0 0.01       # TimeInUnit: next frame
appendcmds 6187 12 0 6187 4 0
setarg 6187 12 0 0 0 66584820       # SendUserMessage 207 -> 244 (same packing, only the message byte changes)
setarg 6187 12 0 1 0 66781428
# 13 (unused): give the actors 0.3s to run their 244 handlers -> 1, the scene's own ending (its last messages)
addbody 6187 13 5 0 1 0 0.3
# COM_CORTEX_CUTSCENE_B03A: on 244, clear it and turn to his final facing (state 6's step) -> 2, which waits for the
# scene's last message and gives him control
copybody 6191 1 2 0 2 51 244 0.2
delcmd 6191 1 1 1
appendcmds 6191 1 1 6191 6 0
copybody 6191 4 2 0 2 51 244 0.2
delcmd 6191 4 1 1
appendcmds 6191 4 1 6191 6 0
copybody 6191 5 2 0 2 51 244 0.2
delcmd 6191 5 1 1
appendcmds 6191 5 1 6191 6 0
# COM_AMBERLEY_CUTSCENE_B03A: on 244, clear it and take her final object state (state 7's step) -> 3, which waits for
# the scene's last message before her last step (the message that starts the fight). Going straight to that step
# started the fight 1.5 s earlier than after the full scene.
copybody 6189 1 3 0 3 51 244 0.2
appendcmds 6189 1 2 6189 7 0
copybody 6189 4 3 0 3 51 244 0.2
appendcmds 6189 4 1 6189 7 0
copybody 6189 5 3 0 3 51 244 0.2
appendcmds 6189 5 1 6189 7 0
copybody 6189 6 3 0 3 51 244 0.2
appendcmds 6189 6 1 6189 7 0
copybody 6189 7 3 0 3 51 244 0.2
appendcmds 6189 7 1 6189 7 0
copybody 6189 10 3 0 3 51 244 0.2
appendcmds 6189 10 1 6189 7 0
)"},
    {"mod/levels/classroom.ops", GateSkips,
     R"(# Classroom Chaos (L08C) - the scene ends by handing control to Cortex, so the skip must do that too
file Levels\school\Crash\crgpa08.rm2
# COM_CLASSROOM_CHAOS_CUTSCENE_DIRECTOR_ACTIVATED: state 1 plays the cutscene; add the developers' Triangle rule (cond 572, player 1) -> state 5 -> 3 -> 2 (END)
copybody 6107 1 4 0 5 572 1 0
clearbodies 6107 4                  # drop the unreachable original
# state 5 = the skip: run the ending of COM_CLASSROOM_CHAOS_CUTSCENE_L08C (its states 2 and 3) with message 244 "skipped" instead of 207
appendcmds 6107 5 0 6109 2 0        # focus Cortex, message; focus Crash, message
appendcmds 6107 5 0 6109 3 0        # SwitchCharacter to Cortex
setarg 6107 5 0 1 0 66781428        # SendUserMessage 207 -> 244 (same packing as the game's own *_CUTSCENE_SKIP scripts)
setarg 6107 5 0 3 0 66781428
# COM_CRASH_CUTSCENE_L08C: on 244 from any waiting state, finish like the full scene (idle anim, dismissed, input handed over)
copybody 6111 3 5 0 1 51 244 0.2
appendcmds 6111 3 1 6111 2 0
copybody 6111 4 4 0 1 51 244 0.2    # mid-animation: also undo state 3's SetObject
appendcmds 6111 4 1 6111 5 0
appendcmds 6111 4 1 6111 2 0
copybody 6111 5 5 0 1 51 244 0.2
appendcmds 6111 5 1 6111 2 0
copybody 6111 2 2 0 1 51 244 0.2
# COM_CORTEX_CUTSCENE_L08C: on 244, give him input and draw the multitool (state 2). No warp to his end mark: once the
# scene is cut short the mark resolves outside the level, so after a skip he stays on his start mark (~6 units ahead).
copybody 6113 3 1 0 2 51 244 0.2
copybody 6113 4 1 0 2 51 244 0.2
copybody 6113 1 1 0 2 51 244 0.2
)"},
    {"mod/levels/coreintro.ops", GateSkips,
     R"(# Core intro (H04E) - Cortex and Nina leave at the end of the scene, so the skip has to tell them to.
# The skip lives inside the cutscene script (not the director) so the actors' end marks resolve through the running scene.
file Levels\AltEarth\Hub\coreent.rm2
# COM_COREINTRO_CS_DIR_H04E: Triangle rule (cond 572, player 1) in every state the scene waits in -> 11
addbody 6445 4 572 1 11
addbody 6445 2 572 1 11
addbody 6445 5 572 1 11
addbody 6445 7 572 1 11
addbody 6445 6 572 1 11
# 11 (unused): the scene's last two steps (state 7: reset counter, deactivate the linked object; state 6: message
# all three actors) with 244 "skipped" instead of 207 -> 12
addbody 6445 11 5 0 12 0 0.01       # TimeInUnit: next frame
appendcmds 6445 11 0 6445 7 0
appendcmds 6445 11 0 6445 6 0
setarg 6445 11 0 4 0 66191604       # SendUserMessage 207 -> 244 (same packing as the game's own *_CUTSCENE_SKIP scripts)
setarg 6445 11 0 5 0 66781428
setarg 6445 11 0 6 0 66584820
# 12 (unused): give the actors 0.3s to run their 244 handlers -> 9 (level music) -> 10 (end)
addbody 6445 12 5 0 9 0 0.3
# COM_CRASH_CS_H04E: on 244, warp to his end mark and get input back (states 2, 3 and 5 of the full scene)
copybody 6437 1 2 0 6 51 244 0
appendcmds 6437 1 1 6437 3 0
appendcmds 6437 1 1 6437 5 0
copybody 6437 2 2 0 6 51 244 0
appendcmds 6437 2 1 6437 3 0
appendcmds 6437 2 1 6437 5 0
copybody 6437 5 5 0 6 51 244 0
# COM_CORTEX_CS_H04E: on 244, go to his last step (state 8: warp away, hide); in state 7 it must precede the Else loop
addbody 6441 1 51 244 8
addbody 6441 2 51 244 8
addbody 6441 5 51 244 8
addbody 6441 7 51 244 8
movebody 6441 7 2 0
# COM_NINA_CS_H04E: same, her last step is state 6
addbody 6443 1 51 244 6
addbody 6443 2 51 244 6
addbody 6443 3 51 244 6
addbody 6443 7 51 244 6
movebody 6443 7 2 0
)"},
    {"mod/levels/dorm_room.ops", GateSkips,
     R"(# Dorm room (H03B) - plays as the level loads (Cortex spawns inside its trigger; test with tools/rig/arrival_test.py).
# The scene ends with Cortex asleep in bed and control switched to Nina, so the skip has to run that ending too. Like
# the core intro, it lives inside the scene script (COM_DORM_ROOM_CUTSCENE_H03B) so the actors' marks resolve.
file Levels\school\Cortex\cogpa08.rm2
# Triangle rule (cond 572, player 1) in every state the scene waits in (camera moves) -> 16. Not in its instant
# states: a polled rule there stops the state from stepping on.
addbody 5657 4 572 1 16
addbody 5657 6 572 1 16
addbody 5657 7 572 1 16
addbody 5657 9 572 1 16
addbody 5657 11 572 1 16
addbody 5657 12 572 1 16
addbody 5657 13 572 1 16
addbody 5657 15 572 1 16
addbody 5657 19 572 1 16
addbody 5657 21 572 1 16
# 16 (unused): the scene's last message steps (states 21 and 5: all four actors) with 244 "skipped" -> 17
addbody 5657 16 5 0 17 0 0.01       # TimeInUnit: next frame
appendcmds 5657 16 0 5657 21 0
appendcmds 5657 16 0 5657 5 0
setarg 5657 16 0 1 0 66781428       # SendUserMessage 207 -> 244 (same packing as the game's own *_CUTSCENE_SKIP scripts)
setarg 5657 16 0 3 0 66781428
setarg 5657 16 0 5 0 66781428
setarg 5657 16 0 7 0 66781428
# 17 (unused): give the actors 0.3s to run their 244 handlers -> 5, the scene's own ending (last messages, then
# state 1: fade, Nina takes over)
addbody 5657 17 5 0 5 0 0.3
# COM_CORTEX_CUTSCENE_H03B: on 244, warp to his bed mark (state 3's step) and take his final step at once (state 2's:
# sleep pose, attached, input) -> 1. The scene switches control to Nina right after its last message, which ends his
# cutscene script, so he can't be left to finish his climb-into-bed animations.
copybody 5659 3 3 0 1 51 244 0.2
appendcmds 5659 3 1 5659 2 0
copybody 5659 5 2 0 1 51 244 0.2
copybody 5659 8 2 0 1 51 244 0.2
# COM_NINA_CUTSCENE_H03B: on 244, state 3's step (agent, first mark) then state 6's warp to her last mark -> 2, which
# waits for the last message and hands her the controls
copybody 5661 3 3 0 2 51 244 0.2
appendcmds 5661 3 1 5661 6 0
copybody 5661 4 3 0 2 51 244 0.2
appendcmds 5661 4 1 5661 6 0
copybody 5661 5 3 0 2 51 244 0.2
appendcmds 5661 5 1 5661 6 0
copybody 5661 6 3 0 2 51 244 0.2
appendcmds 5661 6 1 5661 6 0
# COM_NINA_HEAD_CUTSCENE_H03B: on 244, its final object state (state 1's step) -> 3 (done)
copybody 5823 0 1 0 3 51 244 0.2
copybody 5823 1 1 0 3 51 244 0.2
)"},
    {"mod/levels/harmless_after_defeat.ops", GateFixes,
     R"(# Things that stay in the level after they are beaten but keep hurting Crash on contact. Contact damage is agent
# bit 6 (the object's creation-helper flags, bit 8); the game turns it off with SetAgent(64) in 700-odd places.

# Tiki Mon (Totem Hokum boss, Earth hub): the fight ends with BossModeExit and the beaten totem simply sits in the
# arena - still dealing a contact hit, so walking into it costs a mask, or a life and the whole fight again.
file Levels\Earth\Hub\hubd.rm2
appendcmds 4015 8 0 1511 7 0   # COM_TIKI_MON_ACTIVATED state 8, the last step after the defeat cutscene: SetAgent(64) (copied from COM_EARTH_WORM_START)
)"},
    {"mod/levels/icelab.ops", GateSkips,
     R"(# Iceberg Lab exterior (L04A); skip runs COM_ICELAB_CUTSCENE_SKIP
file Levels\Ice\Hub\labext.rm2
copybody 5495 1 9 0 4 572 1 0   # COM_ICELAB_CUTSCENE_DIRECTOR_ACTIVATED: state 1 plays the cutscene; add the developers' Triangle rule (cond 572, player 1) -> state 4
clearbodies 5495 9                  # drop the unreachable original in state 9
)"},
    {"mod/levels/lab_psychetron.ops", GateSkips,
     R"(# Lab psychetron room (H02M) - story progress 18-19 (set before the level loads; rig state psycho_p19).
# The scene's last step messages its linked objects, which starts the next scene, so a director-only skip broke the
# story chain. The skip lives inside the scene script (COM_LAB_PSYCHETRON_ROOM_CUTSCENE_H02M) and ends with the
# scene's own last steps.
file Levels\Ice\Hub\psycho.rm2
# Triangle rule (cond 572, player 1) in the scene's waiting states (camera moves) -> 11
addbody 6079 7 572 1 11
addbody 6079 9 572 1 11
# 11 (unused): the scene's last message pair (state 1's two sends, without its fade and camera) with 244 "skipped" -> 12
addbody 6079 11 5 0 12 0 0.01       # TimeInUnit: next frame
appendcmds 6079 11 0 6079 1 0
delcmd 6079 11 0 3                  # ToggleCutsceneCamera
delcmd 6079 11 0 2                  # FadeoutScreen
setarg 6079 11 0 0 0 66781428       # SendUserMessage 207 -> 244 (same packing, only the message byte changes)
setarg 6079 11 0 1 0 66584820
# 12 (unused): give the actors 0.3s to run their 244 handlers, then send the scene's last message pair (state 1's two
# sends, 207: input back) -> 3, the scene's last step (the message to its linked objects that plays the FMV and the
# next scene). State 1 itself is a slow camera hand-over, which made the skip barely shorter.
addbody 6079 12 5 0 3 0 0.3
appendcmds 6079 12 0 6079 1 0
delcmd 6079 12 0 3                  # ToggleCutsceneCamera
delcmd 6079 12 0 2                  # FadeoutScreen
# COM_CORTEX_CUTSCENE_H02M / COM_NINA_CUTSCENE_H02M: on 244, clear it and take their final object state and pose
# -> 2, which waits for the scene's last message (input back)
copybody 6081 1 2 0 2 51 244 0.2
delcmd 6081 1 1 1
appendcmds 6081 1 1 6081 4 0
appendcmds 6081 1 1 6081 5 0
copybody 6081 4 2 0 2 51 244 0.2
delcmd 6081 4 1 1
appendcmds 6081 4 1 6081 4 0
appendcmds 6081 4 1 6081 5 0
copybody 6083 1 2 0 2 51 244 0.2
delcmd 6083 1 1 1
appendcmds 6083 1 1 6083 6 0
appendcmds 6083 1 1 6083 5 0
copybody 6083 4 2 0 2 51 244 0.2
delcmd 6083 4 1 1
appendcmds 6083 4 1 6083 6 0
appendcmds 6083 4 1 6083 5 0
copybody 6083 6 2 0 2 51 244 0.2
delcmd 6083 6 1 1
appendcmds 6083 6 1 6083 6 0
appendcmds 6083 6 1 6083 5 0
)"},
    {"mod/levels/psychetron_h02j.ops", GateSkips,
     R"(# Psychetron room with Coco (H02J) - story progress 11-12 (set before the level loads; rig state psycho_p12).
# The scene hands Cortex and Coco between several senders, so instead of routing a "skipped" message, each actor
# watches for the Triangle itself (cond 572, the same rule the scene uses) and jumps to its own last step. The scene
# then runs its ending, whose last messages give the player control back.
file Levels\Ice\Hub\psycho.rm2
# COM_PSYCHETRON_ROOM_CUTSCENE_H02J: Triangle rule in every state the scene waits in (camera moves and dialogue
# waits) -> 2. Not in its instant states: a polled rule there stops the state from stepping on.
addbody 6353 8 572 1 2
addbody 6353 9 572 1 2
addbody 6353 11 572 1 2
addbody 6353 13 572 1 2
addbody 6353 15 572 1 2
addbody 6353 18 572 1 2
addbody 6353 19 572 1 2
addbody 6353 20 572 1 2
addbody 6353 21 572 1 2
addbody 6353 23 572 1 2
addbody 6353 25 572 1 2
addbody 6353 26 572 1 2
addbody 6353 28 572 1 2
addbody 6353 30 572 1 2
addbody 6353 32 572 1 2
addbody 6353 34 572 1 2
addbody 6353 36 572 1 2
addbody 6353 37 572 1 2
# 2 (unused): give the actors 0.3s to reach their last step -> 4, the scene's ending (cleanup, then the messages
# that hand control back)
addbody 6353 2 5 0 4 0 0.3
# COM_CORTEX_CUTSCENE_H02J: on Triangle, take his last step (state 10's: final mark and pose) -> 3, which waits for
# the scene's last message
copybody 6349 4 10 0 3 572 1 0.2 0.5
copybody 6349 5 10 0 3 572 1 0.2 0.5
copybody 6349 8 10 0 3 572 1 0.2 0.5
copybody 6349 10 10 0 3 572 1 0.2 0.5
# COM_COCO_CUTSCENE_H02J: on Triangle, run her step that messages the machine (state 9's) and then her last step
# (state 13's: final mark and pose) -> 2, which waits for the scene's last message
copybody 6351 6 9 0 2 572 1 0.2 0.5
appendcmds 6351 6 1 6351 13 0
copybody 6351 7 9 0 2 572 1 0.2 0.5
appendcmds 6351 7 1 6351 13 0
copybody 6351 8 9 0 2 572 1 0.2 0.5
appendcmds 6351 8 1 6351 13 0
copybody 6351 9 9 0 2 572 1 0.2 0.5
appendcmds 6351 9 1 6351 13 0
copybody 6351 12 9 0 2 572 1 0.2 0.5
appendcmds 6351 12 1 6351 13 0
copybody 6351 13 9 0 2 572 1 0.2 0.5
appendcmds 6351 13 1 6351 13 0
copybody 6351 17 9 0 2 572 1 0.2 0.5
appendcmds 6351 17 1 6351 13 0
copybody 6351 18 9 0 2 572 1 0.2 0.5
appendcmds 6351 18 1 6351 13 0
# COM_CRASH_CUTSCENE_H02J: on Triangle, take the step the scene's message would have run (state 3's: warp to his mark
# by the console, turn and pose) -> 4, which runs his last pose and then waits for the scene's last message
copybody 6463 3 3 0 4 572 1 0.2 0.5
)"},
    {"mod/levels/rockslide.ops", GateSkips,
     R"(# Rockslide Rumble intro (H04B) - the scene ends by warping Crash and Cortex to the top of the slide, starting the level
# music and mounting Crash on the Humiliskate, so the skip has to run that ending too. Like the core intro, the skip
# lives inside the scene script (COM_ALTEARTH_ROCKSLIDE_CUTSCENE_H04B) so the actors' end marks resolve.
file Levels\AltEarth\RockSlid\l10start.rm2
# Triangle rule (cond 572, player 1) in every state the scene waits in (camera moves and 'focus busy' waits) -> 29.
# Not in its instant states: a polled rule there stops the state from stepping on.
addbody 5977 5 572 1 29
addbody 5977 6 572 1 29
addbody 5977 9 572 1 29
addbody 5977 11 572 1 29
addbody 5977 12 572 1 29
addbody 5977 14 572 1 29
addbody 5977 16 572 1 29
addbody 5977 19 572 1 29
addbody 5977 21 572 1 29
addbody 5977 23 572 1 29
addbody 5977 25 572 1 29
addbody 5977 28 572 1 29
# 29 (unused): the scene's last message step (state 2: player, Cortex, Evil Crash, Nina) with 244 "skipped" -> 30
addbody 5977 29 5 0 30 0 0.01       # TimeInUnit: next frame
appendcmds 5977 29 0 5977 2 0
setarg 5977 29 0 1 0 66781428       # SendUserMessage 207 -> 244 (same packing as the game's own *_CUTSCENE_SKIP scripts)
setarg 5977 29 0 3 0 1912275188
setarg 5977 29 0 5 0 66781428
setarg 5977 29 0 7 0 66781428
# 30 (unused): give the actors 0.3s to run their 244 handlers -> 2, the scene's own ending (final messages, Humiliskate)
addbody 5977 30 5 0 2 0 0.3
# COM_CRASH_CUTSCENE_H04B: on 244 from any waiting state, warp to his end mark (state 15's step) -> 3 (waits for the
# scene's last message, then input back); also take state 11's SetObject so he ends holding what the full scene leaves
copybody 5979 2 15 0 3 51 244 0.2
appendcmds 5979 2 1 5979 11 0
copybody 5979 6 15 0 3 51 244 0.2
appendcmds 5979 6 1 5979 11 0
copybody 5979 7 15 0 3 51 244 0.2
appendcmds 5979 7 1 5979 11 0
copybody 5979 8 15 0 3 51 244 0.2
appendcmds 5979 8 1 5979 11 0
copybody 5979 12 15 0 3 51 244 0.2
copybody 5979 13 15 0 3 51 244 0.2
copybody 5979 14 15 0 3 51 244 0.2
copybody 5979 15 15 0 3 51 244 0.2
copybody 5979 16 15 0 3 51 244 0.2
# COM_CORTEX_CUTSCENE_H04B: on 244, warp to his end mark (state 10's step) -> 3 (waits for the last message, then
# input back and the level music)
copybody 5981 14 10 0 3 51 244 0.2
copybody 5981 8 10 0 3 51 244 0.2
copybody 5981 5 10 0 3 51 244 0.2
copybody 5981 9 10 0 3 51 244 0.2
copybody 5981 10 10 0 3 51 244 0.2
# COM_EVIL_CRASH_CUTSCENE_H04B / COM_NINA_CUTSCENE_H04B: on 244 before they have jumped off, go straight to their
# final warp (state 5's step) -> 3 (done)
copybody 5985 6 5 0 3 51 244 0.2
copybody 5987 6 5 0 3 51 244 0.2
)"},
    {"mod/levels/rooftop.ops", GateSkips,
     R"(# Rooftop Rampage (H03D / H03C)
# The director lives in two level files and both trigger it, so both need the skip. roof01 alone left the
# scene unskippable whenever it ran from roofcor2 - found by tools/skip_survey.py, which reads the built mod
# and lists every condition-572 rule the game still cannot reach.
file Levels\school\RoofTop\roof01.rm2
copybody 6375 1 5 0 4 572 1 0   # COM_ROOFTOP_CUTSCENE_DIRECTOR_ACTIVATED: state 1 plays the cutscene; add the developers' Triangle rule (cond 572, player 1) -> state 4
copybody 6375 7 5 0 4 572 1 0   # COM_ROOFTOP_CUTSCENE_DIRECTOR_ACTIVATED: state 7 plays the cutscene; add the developers' Triangle rule (cond 572, player 1) -> state 4
clearbodies 6375 5                  # drop the unreachable original in state 5
file Levels\school\RoofTop\roofcor2.rm2
copybody 6375 1 5 0 4 572 1 0   # the same scene, same script, same states - roofcor2's copy of the director
copybody 6375 7 5 0 4 572 1 0
clearbodies 6375 5
)"},
    {"mod/levels/school_hub.ops", GateSkips,
     R"(# Academy of Evil hub (H03A)
file Levels\school\Sch_Hub\sch_hub.rm2
copybody 5605 1 5 0 4 572 1 0   # COM_SCHOOL_HUB_CUTSCENE_DIRECTOR_ACTIVATED: state 1 plays the cutscene; add the developers' Triangle rule (cond 572, player 1) -> state 4
clearbodies 5605 5                  # drop the unreachable original in state 5
)"},
    {"mod/levels/skunk.ops", GateSkips,
     R"(# Angry skunk (Jungle Bungle high path)
file Levels\Earth\Hub\highpath.rm2
copybody 6147 1 8 0 4 572 1 0   # COM_ANGRY_SKUNK_CUTSCENE_DIRECTOR_ACTIVATED: state 1 plays the cutscene; add the developers' Triangle rule (cond 572, player 1) -> state 4
clearbodies 6147 8                  # drop the unreachable original in state 8
)"},
    {"mod/levels/slipslide.ops", GateSkips,
     R"(# Slip Slide Icecapades intro (H02G); skip runs COM_SLIPSLIDE_CUTSCENE_SKIP
file Levels\Ice\SlipSlide\l05start.rm2
copybody 5549 1 6 0 4 572 1 0   # COM_SLIPSLIDE_CUTSCENE_DIRECTOR_ACTIVATED: state 1 plays the cutscene; add the developers' Triangle rule (cond 572, player 1) -> state 4
clearbodies 5549 6                  # drop the unreachable original in state 6
copybody 5545 0 3 0 2 51 244 0.2   # COM_CRASH_CUTSCENE_H02G: reconnect the skip-message (244) handler - warp to the end mark, give control back
clearbodies 5545 3
copybody 5547 0 3 0 2 51 244 0.2   # COM_CORTEX_CUTSCENE_H02G: same for Cortex
clearbodies 5547 3
)"},
    {"mod/levels/treasure.ops", GateSkips,
     R"(# Treasure room (L12A)
file Levels\AltEarth\Core\treasure.rm2
copybody 6327 1 5 0 4 572 1 0   # COM_TREASUREROOM_CUTSCENE_DIRECTOR_ACTIVATED: state 1 plays the cutscene; add the developers' Triangle rule (cond 572, player 1) -> state 4
clearbodies 6327 5                  # drop the unreachable original in state 5
)"},
    {"mod/levels/walrus.ops", GateSkips,
     R"(# Walrus chase (L06E); skip runs the developers' COM_WALRUS_CUTSCENE_SKIP
# The scene plays the moment the level loads (the spawn is inside its trigger), and the chase starts as it ends.
file Levels\Ice\HighSeas\gpa10.rm2
copybody 6021 1 6 0 4 572 1 0   # COM_WALRUS_CUTSCENE_DIRECTOR_ACTIVATED: state 1 plays the cutscene; add the developers' Triangle rule (cond 572, player 1) -> state 4
clearbodies 6021 6                  # drop the unreachable original in state 6
appendcmds 6021 4 0 6023 17 0       # the skip never starts the chase music; run the full scene's BeginMusic (its state 17) when it ends
copybody 6025 0 3 0 1 51 244 0.2   # COM_CRASH_CUTSCENE_L06E: reconnect Crash's skip-message (244) handler, as COM_WALRUS_CS_L06E still has
clearbodies 6025 3
# COM_WALRUS_CS_L06E: its 244 handler warps the walrus to its current focus - Crash - so he died the moment the skip
# ended. The full scene never moves the walrus (it stays at its start, well behind Crash), so drop the warp.
delcmd 6033 0 1 1
)"},
    {"mod/skip_prompt.ops", GateSkips,
     R"(# 'Hold triangle to skip' hint (mod/text.txt line 24), drawn in the letterbox's bottom bar while a skippable cutscene plays.
# Applied after mod/levels/*.ops (and any --include recipes), so it also covers the skips those restore.
# skipprompt auto = every reachable state that runs a cutscene script and has a live Triangle rule (condition 572).

# cutscenes whose skip the executable patch alone restores
file Levels\Earth\DocAmok\docamok1.rm2
skipprompt auto
file Levels\Earth\DocAmok\docamok2.rm2
skipprompt auto
file Levels\Earth\DocAmok\docamok3.rm2
skipprompt auto
file Levels\Earth\Hub\highpath.rm2
skipprompt auto
file Levels\Earth\Hub\huba.rm2
skipprompt auto
file Levels\Earth\Hub\hubb.rm2
skipprompt auto
file Levels\Earth\Hub\hubd.rm2
skipprompt auto
file Levels\Earth\Totem\l03stock.rm2
skipprompt auto
file Levels\Ice\HighSeas\gpa01.rm2
skipprompt auto
file Levels\Ice\HighSeas\gpa04.rm2
skipprompt auto
file Levels\Ice\HighSeas\gpa11.rm2
skipprompt auto
file Levels\Ice\Hub\psycho.rm2
skipprompt auto
skipprompt 6077 1                   # lab psychetron room: the skip lives inside the scene, prompt while the director plays it
skipprompt 6347 3                   # psychetron room with Coco: same, its director plays the scene in state 3
file Levels\Ice\IceClimb\bergext.rm2
skipprompt auto
file Levels\Ice\IceClimb\ukafight.rm2
skipprompt auto
file Levels\school\Boiler\boiler_6.rm2
skipprompt auto

# cutscenes whose skip mod/levels restores
file Levels\Earth\Hub\beach.rm2
skipprompt auto
skipprompt 6365 1                   # Aku Aku crate scene: its skip lives inside the scene (COM_CUTSCENE_L01B), prompt while the director plays it
file Levels\Ice\Hub\labext.rm2
skipprompt auto
file Levels\Ice\SlipSlide\l05start.rm2
skipprompt auto
file Levels\school\Crash\crgpa08.rm2
skipprompt auto
file Levels\school\RoofTop\roof01.rm2
skipprompt auto
file Levels\school\RoofTop\roofcor2.rm2
skipprompt auto                     # the same scene in the other RoofTop file: it had the skip and no prompt
file Levels\school\Sch_Hub\sch_hub.rm2
skipprompt auto
file Levels\AltEarth\Core\treasure.rm2
skipprompt auto
file Levels\AltEarth\Hub\coreent.rm2
skipprompt 6435 7                   # the skip lives inside the scene: prompt while the director plays it (state 7)
file Levels\AltEarth\RockSlid\l10start.rm2
skipprompt 5975 1                   # the skip lives inside the scene: prompt while the director plays it (state 1)
file Levels\school\Cortex\cogpa08.rm2
skipprompt 5655 1                   # the skip lives inside the scene: prompt while the director plays it (state 1)
file Levels\Ice\HighSeas\gpa10.rm2
skipprompt auto
file Levels\school\Madame\amberly.rm2
skipprompt 6185 1                   # the skip lives inside the scene: prompt while the director plays it (state 1)

# levels with recipes in mod/levels-wip (their own skips are only built with --include)
file Levels\Earth\Hub\bossarea.rm2
skipprompt auto
file Levels\Ice\Hub\labint.rm2
skipprompt auto
)"},
