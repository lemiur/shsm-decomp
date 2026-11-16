# Candidate translation units: Silent Hill: Shattered Memories (Wii)

> Caveat: MWCC emits a TU's `__sinit` at the end of that TU's `.text`, so a candidate that holds only a sinit function belongs to the functions before it (game/PadRumble: `__sinit_\PadRumble_cpp` closes 0x800067A0-0x80006C7C although the dtk asm split isolates it). Merge lone-sinit rows into the preceding candidate. A behaviour unit also emits its inline factory (address taken) and its two `@36@` this-adjust thunks after the sinit, so the unit ends after them: the cuts below that stop at a sinit are short by those functions (game-2: CHitEffectManager 0x80087614-0x800878D0, CAmbientTextMessage 0x800C2C48-0x800C2FF0, CAmbientVoicemail 0x800C2FF0-0x800C3398, CMapWaypointPurge 0x8018D98C-0x8018DBE8, CAmbientInterferencePoint 0x800C1E48-0x800C2C48 (the thunks at 0x800C1E38 and 0x800C1E40 belong to the vtable lbl_803EB24C of the unit before); the pad manager unit is 0x8000A028-0x8000C32C, EventHandler 0x801A92A8-0x801AA554). game-3: CMapWaypoint 0x8018D374-0x8018D98C holds two classes, CMapWaypoint and CRemoveMapWaypoint (one sinit registers both); the listener unk_800C8598 is a unit of its own, 0x800C8598-0x800C875C, so CColdTrigger is 0x800C875C-0x800C8A0C (the vtable order and the 8-byte `.data`/`.sdata` boundaries at 0x803EBEA8 and 0x80527510 show the cut); CAmbientInterferencePoint's `.data` starts at 0x803EB2B0 with its three save-data strings.

> Cut rule for behaviour units (game-7): a behaviour unit is its class functions, then its `__sinit`, then the inline factory and the two `@36@` thunks; rows that start a unit at its sinit (or end one before its factory) are one row late. The rows 800CA1F4-800CC4E4 (game-9), 800CD374-800CE988, 800CEEA4-800CF1AC and 800D0AB0-800D1934 (game-11) and 800CC4E4-800CCF10, 800CDD50-800CE684 (game-13) below are corrected to this rule; other rows are not yet re-cut.

Candidate TU boundaries for three address ranges of build R5WEA4: game code G1-G7, the Climax core, and the Climax XML/config/assets code. The evidence is the current analysis inputs only; nothing here is verified against a compiled split. A proposed name is a candidate only: under the game code layout ruling (AGENTS.md) a unit is named from class-name evidence, game code goes in `src/game/` and Climax engine code in `src/climax/`, and a file named without that evidence carries the "unit name unconfirmed" comment. Every proposed name here, a class token or a "(file)" asset stem alike, is an unconfirmed unit name; the "conf" column rates the cut positions, not the name. "linked" marks a candidate that is now a `Matching` unit in `configure.py`.

## Method

- Functions and string literals come from `build/tmp/game/fn_strings.tsv`; sizes there are hex. Two duplicate address rows are kept once; neither lies in these ranges.
- Cut source 1 (strict): each row of `ref/R5WEA4/tu_boundaries_strict.tsv` is a window of unanchored functions between two anchors (within these ranges the slack equals the number of functions between the two anchors). The cut is placed at the right anchor. Slack 0 is exact; slack s > 0 is one of s+1 positions and is imprecise.
- Cut source 2 (sinit): the 251 `.ctors` entries in `build/R5WEA4/asm` with `fn_` targets (static initialisers; the one non-`fn_` entry, `__init_cpp_exceptions`, is not used) each end their TU, so the next function starts a new one. A sinit inside a strict window gives that window its exact cut; two windows in G1-G7 hold two sinit functions each and both cuts are used. A sinit outside every strict window is a cut with no strict support and is imprecise (24 such cuts, all in G1-G7).
- Evidence strings: C-prefixed class tokens (`^C[A-Z][A-Za-z0-9]+` with a lowercase letter or a trailing `*`, or a `::` qualifier), `.xaml`/`.rws`/`.arc` names, `Msg`/`iMsg` names, `rwID_*` names, and "Fuse Binary Format". All-caps tokens without `*` are shown as evidence but not counted as classes. Other strings are not shown.
- Proposed name: the most frequent C-prefixed token in the TU; otherwise the stem of a file name, marked "(file)"; otherwise `unnamed`. Non-C `*` type names are evidence only.
- Confidence: high = both cut positions exact and (sinit at the end or a class token); medium = some evidence or a sinit, but not high; low = no sinit and no evidence string.
- Consecutive low-confidence candidates with no evidence and no sinit are merged into one row. The last TU of a range ends at the end of its last function. `fns` counts functions in `[start, end)`.
- The existing asm unit split (column 4 of `fn_strings.tsv`) isolates every G1-G7 sinit function as a one-function unit (218 of 218), so it is not independent evidence for these cuts and is not used as a cut source. Consequently 125 of the 218 G1-G7 sinit TUs consist of the sinit function alone (79 of them carry class names).

## Summary

- G1-G7 (800067A0-8018F8C4): 589 candidate TUs, 218 with sinit, 129 with class names (high 90, medium 171, low 328).
- Climax core (801A70B8-801B4354): 18 candidate TUs, 5 with sinit, 1 with class names (high 3, medium 9, low 6).
- Climax XML/config/assets (801CCD24-80209D54): 96 candidate TUs, 8 with sinit, 0 with class names (high 1, medium 33, low 62).

## G1-G7 (800067A0-8018F8C4)

| start | end | fns | sinit | evidence strings | proposed name | conf |
|---|---|---|---|---|---|---|
| 800067A0 | 80006C7C | 10 | 80006A28 | `Msg*Rumble` names (`.sdata2` 8052B5E0) | `game/PadRumble` (linked; unit name unconfirmed) | high |
| 80006C7C | 8000A040 | 19 | - | none; 2 unnamed candidates | unnamed | low |
| 8000A040 | 8000C32C | 17 | 8000C2F4 | none | unnamed | medium |
| 8000C32C | 8000CFD8 | 21 | - | none; 3 unnamed candidates | unnamed | low |
| 8000CFD8 | 8000D038 | 1 | 8000CFD8 | CCollisionCapsule | CCollisionCapsule | medium |
| 8000D038 | 8001238C | 55 | - | none; 10 unnamed candidates | unnamed | low |
| 8001238C | 800129E0 | 16 | 800129B4 | none | unnamed | medium |
| 800129E0 | 80015234 | 37 | - | none; 4 unnamed candidates | unnamed | low |
| 80015234 | 800152C0 | 1 | 80015234 | none | unnamed | medium |
| 800152C0 | 80016A48 | 32 | - | none; 4 unnamed candidates | unnamed | low |
| 80016A48 | 80016C6C | 3 | 80016C5C | none | unnamed | medium |
| 80016C6C | 800174B8 | 5 | 80017480 | none | unnamed | high |
| 800174B8 | 80017570 | 19 | - | none | unnamed | low |
| 80017570 | 8001759C | 1 | 80017570 | CAnimBehaviour | CAnimBehaviour | medium |
| 8001759C | 8001A0A4 | 44 | - | s_Mount, rwID_HANIMANIMATION | unnamed | medium |
| 8001A0A4 | 8001A0D0 | 1 | 8001A0A4 | CAnimSkeleton | CAnimSkeleton | medium |
| 8001A0D0 | 800220EC | 121 | - | none; 4 unnamed candidates | unnamed | low |
| 800220EC | 80022178 | 1 | 800220EC | CBasicAnimatedObject | CBasicAnimatedObject | medium |
| 80022178 | 80024158 | 31 | - | none | unnamed | low |
| 80024158 | 80025434 | 32 | - | CMA | unnamed | medium |
| 80025434 | 80025460 | 1 | 80025434 | CSimpleAnimation | CSimpleAnimation | medium |
| 80025460 | 8002598C | 5 | - | none; 2 unnamed candidates | unnamed | low |
| 8002598C | 80027DEC | 33 | - | CZone | CZone | medium |
| 80027DEC | 80028734 | 13 | - | rwID_CBSP | unnamed | medium |
| 80028734 | 8002883C | 1 | 80028734 | CZone, CZoneCollision | CZone | medium |
| 8002883C | 80028FA4 | 16 | - | none; 3 unnamed candidates | unnamed | low |
| 80028FA4 | 80029004 | 1 | 80028FA4 | CPreCache | CPreCache | high |
| 80029004 | 8002D1A8 | 70 | - | none; 11 unnamed candidates | unnamed | low |
| 8002D1A8 | 8002D208 | 1 | 8002D1A8 | none | unnamed | medium |
| 8002D208 | 80031F1C | 83 | - | none | unnamed | low |
| 80031F1C | 80031F54 | 1 | 80031F1C | none | unnamed | medium |
| 80031F54 | 800324DC | 5 | - | none | unnamed | low |
| 800324DC | 80033684 | 15 | 8003356C | s_Mount, RwReal, EventSource | unnamed | medium |
| 80033684 | 80033D34 | 15 | - | none; 2 unnamed candidates | unnamed | low |
| 80033D34 | 80033D94 | 1 | 80033D34 | CAudioLocatorBehaviour2D | CAudioLocatorBehaviour2D | high |
| 80033D94 | 80034008 | 9 | - | none | unnamed | low |
| 80034008 | 800397A0 | 69 | - | RwReal, Audio.arc | Audio (file) | medium |
| 800397A0 | 800399A0 | 7 | - | none | unnamed | low |
| 800399A0 | 80039AEC | 1 | 800399A0 | none | unnamed | medium |
| 80039AEC | 80039DB8 | 9 | 80039D58 | none | unnamed | high |
| 80039DB8 | 8003BB84 | 32 | - | none; 2 unnamed candidates | unnamed | low |
| 8003BB84 | 8003BBE4 | 1 | 8003BB84 | CAudioReverb | CAudioReverb | medium |
| 8003BBE4 | 8003C324 | 10 | - | none; 2 unnamed candidates | unnamed | low |
| 8003C324 | 8003C384 | 1 | 8003C324 | CActorTest | CActorTest | high |
| 8003C384 | 80043658 | 96 | - | CZoneChangeInfo, SGrabInfo, rwID_FUSESTATE | CZoneChangeInfo | medium |
| 80043658 | 800436D8 | 7 | - | none | unnamed | low |
| 800436D8 | 80043780 | 1 | 800436D8 | CCharacter | CCharacter | medium |
| 80043780 | 80052E58 | 171 | - | none | unnamed | low |
| 80052E58 | 80052F3C | 1 | 80052E58 | CEnemy, CEnemySpawn | CEnemy | medium |
| 80052F3C | 80053834 | 37 | - | none | unnamed | low |
| 80053834 | 80053B50 | 11 | - | rwID_CLUMP | unnamed | medium |
| 80053B50 | 80053BB0 | 1 | 80053B50 | CGenericCharacter | CGenericCharacter | medium |
| 80053BB0 | 80055084 | 24 | - | none | unnamed | low |
| 80055084 | 800550E4 | 1 | 80055084 | CHeadTurnNPC | CHeadTurnNPC | medium |
| 800550E4 | 8005543C | 16 | - | none | unnamed | low |
| 8005543C | 80055464 | 1 | 8005543C | CNPC | unnamed | medium |
| 80055464 | 80073BDC | 216 | 80073554 | CPlayer, RwReal, rwID_HANIMANIMATION | CPlayer | medium |
| 80073BDC | 80073ED0 | 24 | - | none | unnamed | low |
| 80073ED0 | 80073F30 | 1 | 80073ED0 | CPlayerCostume | CPlayerCostume | medium |
| 80073F30 | 8007566C | 16 | - | rwID_CLUMP | unnamed | medium |
| 8007566C | 80077904 | 13 | - | rwID_SPLINE | unnamed | medium |
| 80077904 | 80077994 | 1 | - | none | unnamed | low |
| 80077994 | 800779F4 | 1 | 80077994 | CWalkNPC | CWalkNPC | high |
| 800779F4 | 80077B8C | 19 | 80077B04 | CCopCar | CCopCar | high |
| 80077B8C | 8007B9A8 | 100 | - | s_Mount | unnamed | medium |
| 8007B9A8 | 8007BA38 | 1 | 8007B9A8 | CGenericCar | CGenericCar | medium |
| 8007BA38 | 8007C548 | 22 | - | none | unnamed | low |
| 8007C548 | 8007C7FC | 1 | - | s_Mount | unnamed | medium |
| 8007C7FC | 8007C8A4 | 1 | 8007C7FC | CSUV | unnamed | high |
| 8007C8A4 | 8007D18C | 16 | - | CSUV | CSUV | medium |
| 8007D18C | 8007EF58 | 13 | - | none | unnamed | low |
| 8007EF58 | 8007EFE4 | 1 | 8007EF58 | none | unnamed | medium |
| 8007EFE4 | 80081928 | 29 | - | none; 2 unnamed candidates | unnamed | low |
| 80081928 | 80084D0C | 31 | 80084BC0 | s_Mount | unnamed | medium |
| 80084D0C | 80085C04 | 28 | - | none; 3 unnamed candidates | unnamed | low |
| 80085C04 | 80085C64 | 1 | 80085C04 | none | unnamed | high |
| 80085C64 | 80086504 | 12 | 800864A4 | none | unnamed | medium |
| 80086504 | 800865DC | 3 | - | none | unnamed | low |
| 800865DC | 80087564 | 13 | 800874BC | none | unnamed | medium |
| 80087564 | 80087820 | 10 | 800877C0 | CHitEffectManager | CHitEffectManager | high |
| 80087820 | 80087D88 | 17 | - | none | unnamed | low |
| 80087D88 | 80087E0C | 1 | 80087D88 | none | unnamed | medium |
| 80087E0C | 80088E58 | 23 | 80088DB0 | none | unnamed | high |
| 80088E58 | 80089F10 | 29 | - | none; 4 unnamed candidates | unnamed | low |
| 80089F10 | 8008C5E0 | 24 | - | CMA, rwID_RWS, rwID_HANIMANIMATION | unnamed | medium |
| 8008C5E0 | 8008C62C | 1 | 8008C5E0 | none | unnamed | medium |
| 8008C62C | 8008E4D0 | 13 | - | s_Mount | unnamed | medium |
| 8008E4D0 | 8008E570 | 1 | 8008E4D0 | none | unnamed | medium |
| 8008E570 | 8008EE7C | 15 | - | none; 2 unnamed candidates | unnamed | low |
| 8008EE7C | 8008EEDC | 1 | 8008EE7C | none | unnamed | high |
| 8008EEDC | 80090978 | 11 | - | none | unnamed | low |
| 80090978 | 80090A28 | 1 | 80090978 | none | unnamed | medium |
| 80090A28 | 80091398 | 10 | - | none; 2 unnamed candidates | unnamed | low |
| 80091398 | 800913F8 | 1 | 80091398 | none | unnamed | medium |
| 800913F8 | 80091B4C | 8 | - | none | unnamed | low |
| 80091B4C | 80091BA4 | 1 | 80091B4C | none | unnamed | high |
| 80091BA4 | 80092990 | 12 | - | none | unnamed | low |
| 80092990 | 800929F0 | 1 | 80092990 | CFogConfig | CFogConfig | medium |
| 800929F0 | 80095108 | 18 | - | none | unnamed | low |
| 80095108 | 80095154 | 1 | 80095108 | none | unnamed | medium |
| 80095154 | 800967D8 | 8 | - | none | unnamed | low |
| 800967D8 | 80098F2C | 5 | 80098EE0 | none | unnamed | high |
| 80098F2C | 80099278 | 4 | - | none | unnamed | low |
| 80099278 | 800999A8 | 7 | 8009996C | sUpdateData | unnamed | medium |
| 800999A8 | 8009CEEC | 71 | - | none; 4 unnamed candidates | unnamed | low |
| 8009CEEC | 8009CF18 | 1 | 8009CEEC | CBaseBehaviour | CBaseBehaviour | medium |
| 8009CF18 | 8009D190 | 4 | - | none | unnamed | low |
| 8009D190 | 8009D22C | 1 | - | s_Mount | unnamed | medium |
| 8009D22C | 8009D28C | 1 | 8009D22C | CBasicMount | CBasicMount | high |
| 8009D28C | 8009ECB4 | 30 | - | none | unnamed | low |
| 8009ECB4 | 8009ECE0 | 1 | 8009ECB4 | CClumpBehaviour | CClumpBehaviour | medium |
| 8009ECE0 | 8009EEC8 | 10 | - | rwID_CLUMP | unnamed | medium |
| 8009EEC8 | 8009EF28 | 1 | 8009EEC8 | CClumpSwap | CClumpSwap | medium |
| 8009EF28 | 800A2550 | 25 | 800A24A8 | CSceneGraphContainer | CSceneGraphContainer | high |
| 800A2550 | 800A292C | 14 | 800A2900 | CSystemCommands | CSystemCommands | high |
| 800A292C | 800A3EAC | 12 | 800A3E1C | CMultiTextureSequenceObject | CMultiTextureSequenceObject | high |
| 800A3EAC | 800A4E8C | 18 | - | rwID_CLUMP | unnamed | medium |
| 800A4E8C | 800A4EEC | 1 | 800A4E8C | CPhysicsObject | CPhysicsObject | medium |
| 800A4EEC | 800A5598 | 13 | - | none; 2 unnamed candidates | unnamed | low |
| 800A5598 | 800A55F8 | 1 | 800A5598 | CScalingObject | CScalingObject | high |
| 800A55F8 | 800A5860 | 11 | 800A5800 | CSceneObject | CSceneObject | medium |
| 800A5860 | 800A5928 | 6 | - | none | unnamed | low |
| 800A5928 | 800A6984 | 9 | - | s_Mount | unnamed | medium |
| 800A6984 | 800A69E4 | 1 | 800A6984 | CSequenceObject | CSequenceObject | high |
| 800A69E4 | 800A720C | 14 | - | none | unnamed | low |
| 800A720C | 800A726C | 1 | 800A720C | CSkyDome | CSkyDome | medium |
| 800A726C | 800A7BF8 | 23 | - | none | unnamed | low |
| 800A7BF8 | 800A7C58 | 1 | 800A7BF8 | CSmashableObject | CSmashableObject | high |
| 800A7C58 | 800A7FC4 | 15 | - | rwID_SPLINE | unnamed | medium |
| 800A7FC4 | 800A8024 | 1 | 800A7FC4 | CSplineHelper | CSplineHelper | high |
| 800A8024 | 800A82E4 | 5 | - | none | unnamed | low |
| 800A82E4 | 800A8644 | 2 | - | rwID_CLUMP, rwID_HANIMANIMATION | unnamed | medium |
| 800A8644 | 800A8F88 | 8 | 800A8ED8 | CIGCBorderControl | CIGCBorderControl | medium |
| 800A8F88 | 800A9064 | 3 | - | none | unnamed | low |
| 800A9064 | 800AAFF4 | 48 | - | rwID_CLUMP | unnamed | medium |
| 800AAFF4 | 800AB1B0 | 4 | - | none | unnamed | low |
| 800AB1B0 | 800AB210 | 1 | 800AB1B0 | MsgTrigger | unnamed | medium |
| 800AB210 | 800AB4B4 | 6 | 800AB434 | CIGCPlayerMover | CIGCPlayerMover | high |
| 800AB4B4 | 800AB5C4 | 4 | - | RwMatrix | unnamed | medium |
| 800AB5C4 | 800AB668 | 1 | - | none | unnamed | low |
| 800AB668 | 800AB6C8 | 1 | 800AB668 | none | unnamed | high |
| 800AB6C8 | 800AC0D8 | 13 | - | none | unnamed | low |
| 800AC0D8 | 800AEFE4 | 42 | 800AEF7C | none | unnamed | medium |
| 800AEFE4 | 800B05FC | 25 | - | none; 2 unnamed candidates | unnamed | low |
| 800B05FC | 800B0B80 | 2 | 800B0AD8 | none | unnamed | high |
| 800B0B80 | 800B0DEC | 7 | 800B0C64 | none | unnamed | high |
| 800B0DEC | 800B45E0 | 75 | - | none; 5 unnamed candidates | unnamed | low |
| 800B45E0 | 800B5E94 | 67 | 800B5D84 | s_Mount, EventMsgInfo | unnamed | medium |
| 800B5E94 | 800B60D4 | 13 | - | s_Mount | unnamed | medium |
| 800B60D4 | 800B70E0 | 45 | - | none; 4 unnamed candidates | unnamed | low |
| 800B70E0 | 800B710C | 1 | 800B70E0 | CBaseLight | CBaseLight | high |
| 800B710C | 800B789C | 7 | 800B783C | CColorLight, s_Mount | CColorLight | medium |
| 800B789C | 800B7950 | 3 | - | none | unnamed | low |
| 800B7950 | 800B8640 | 8 | - | s_Mount | unnamed | medium |
| 800B8640 | 800B86A0 | 1 | 800B8640 | CLightEffects | CLightEffects | medium |
| 800B86A0 | 800B8BDC | 10 | - | none | unnamed | low |
| 800B8BDC | 800B8E10 | 2 | 800B8D68 | none | unnamed | medium |
| 800B8E10 | 800B93D4 | 13 | 800B9374 | CPlayerLight | CPlayerLight | high |
| 800B93D4 | 800BB288 | 19 | 800BB1BC | s_Mount | unnamed | high |
| 800BB288 | 800BBDF8 | 13 | - | none | unnamed | low |
| 800BBDF8 | 800BC5D4 | 4 | 800BC588 | none | unnamed | medium |
| 800BC5D4 | 800BE8BC | 25 | - | none; 4 unnamed candidates | unnamed | low |
| 800BE8BC | 800C1E38 | 32 | 800C1DEC | none | unnamed | medium |
| 800C1E38 | 800C2B64 | 12 | - | none; 2 unnamed candidates | unnamed | low |
| 800C2B64 | 800C2BF0 | 1 | 800C2B64 | CAmbientInterferencePoint | CAmbientInterferencePoint | high |
| 800C2BF0 | 800C2F54 | 8 | 800C2EC8 | CAmbientTextMessage | CAmbientTextMessage | high |
| 800C2F54 | 800C32FC | 8 | 800C3270 | CAmbientVoicemail | CAmbientVoicemail | high |
| 800C32FC | 800C36B0 | 21 | - | none | unnamed | low |
| 800C36B0 | 800C4110 | 15 | - | CTraversalCamera | CTraversalCamera | medium |
| 800C4110 | 800C419C | 1 | 800C4110 | CHidingPlace | CHidingPlace | medium |
| 800C419C | 800C4EA4 | 25 | - | none | unnamed | low |
| 800C4EA4 | 800C4F04 | 1 | 800C4EA4 | CLookTrigger | CLookTrigger | medium |
| 800C4F04 | 800C5500 | 16 | - | none | unnamed | low |
| 800C5500 | 800C552C | 1 | 800C5500 | CPointOfInterest | CPointOfInterest | medium |
| 800C552C | 800C5FCC | 13 | - | none | `game/unk_PointOfInterestManager` (unit name unconfirmed; stubs) | medium |
| 800C5FCC | 800C6508 | 13 | 800C63D0 | CPointTrigger | `game/CPointTrigger` (NonMatching) | high |
| 800C6430 | 800C7688 | 19 | 800C75FC | CPsyPointOfInterest | CPsyPointOfInterest | high |
| 800C7688 | 800C8108 | 24 | 800C80D4 | CBaseSpawner | CBaseSpawner (header `game/CBaseSpawner.h`; unit not reconstructed) | high |
| 800C8108 | 800C8598 | 10 | 800C84E8 | CGenericSpawner | `game/CGenericSpawner` (linked) | high |
| 800C8598 | 800C875C | 3 | - | none | `game/unk_800C8598` (linked; unit name unconfirmed) | high |
| 800C875C | 800C8A0C | 7 | 800C8928 | CColdTrigger | `game/CColdTrigger` (linked) | high |
| 800C8A0C | 800C8CB0 | 7 | 800C8BCC | CCombatZoneSetEnemyCountTrigger | `game/CCombatZoneSetEnemyCountTrigger` (linked) | high |
| 800C8CB0 | 800C90DC | 7 | 800C8FD4 | CCounter | `game/CCounter` (linked) | high |
| 800C90DC | 800C9400 | 7 | 800C9318 | none | `game/DataTrigger` (linked; class registered as "DataTrigger") | high |
| 800C9400 | 800C984C | 7 | 800C9760 | CDesignerDebugMsg | `game/CDesignerDebugMsg` (parked NonMatching: `fn_8009CB1C` 87.7%) | high |
| 800C984C | 800C9CA8 | 7 | 800C9BA0 | CFadeTrigger | `game/CFadeTrigger` (linked) | high |
| 800C9CA8 | 800CA038 | 7 | 800C9F3C | CFlickerTrigger | `game/CFlickerTrigger` (linked) | high |
| 800CA038 | 800CA1F4 | 3 | - | none | unnamed | low |
| 800CA1F4 | 800CA304 | 4 | 800CA1F4 | none | unnamed (tail of a unit: sinit, factory 800CA280, two thunks; re-cut game-9) | medium |
| 800CA304 | 800CB1C8 | 13 | 800CB0DC | CGhostPhotoTrigger | CGhostPhotoTrigger (8 class functions, sinit, factory 800CB168, three thunks; re-cut game-9) | high |
| 800CB1C8 | 800CB4F8 | 7 | 800CB3E8 | CKeyHUDTrigger | CKeyHUDTrigger (game-9) | high |
| 800CB4F8 | 800CBEF8 | 9 | 800CBE40 | CMessageRelay | CMessageRelay (game-9) | high |
| 800CBEF8 | 800CC4E4 | 11 | 800CC3CC | CModalTutorialMessageTrigger | CModalTutorialMessageTrigger (game-9) | high |
| 800CC4E4 | 800CC914 | 9 | 800CC828 | MusicTrigger (registration and log strings, no C prefix) | `game/MusicTrigger` (linked; game-13) | high |
| 800CC914 | 800CCF10 | 9 | 800CCDDC | COnScreenPromptTrigger | `game/COnScreenPromptTrigger` (linked; game-13) | high |
| 800CCF10 | 800CD374 | 7 | 800CD26C | CPadRumbleTrigger | `game/CPadRumbleTrigger` (linked) | high |
| 800CD374 | 800CD998 | 9 | 800CD8B4 | CPhoneCallTrigger | `game/CPhoneCallTrigger` (linked) | high |
| 800CD998 | 800CDD50 | 7 | 800CDC30 | CPhoneMessageTrigger | `game/CPhoneMessageTrigger` (linked) | high |
| 800CDD50 | 800CE684 | 8 | 800CE568 | PlaneTrigger (registration and log strings, no C prefix) | `game/PlaneTrigger` (parked; game-13) | high |
| 800CE684 | 800CE988 | 7 | 800CE8A0 | CPlayerStateTrigger | `game/CPlayerStateTrigger` (linked) | high |
| 800CE988 | 800CEBF0 | 7 | 800CEB0C | CPsycologyZoneTrigger | `game/CPsycologyZoneTrigger` (linked) | high |
| 800CEBF0 | 800CEEA4 | 7 | 800CEDB8 | CPsyTrigger | `game/CPsyTrigger` (linked) | high |
| 800CEEA4 | 800CF1AC | 7 | 800CF0C0 | RespotZoneTrigger (registration strings, no C prefix) | `game/RespotZoneTrigger` (linked) | high |
| 800CF1AC | 800CF530 | 7 | 800CF43C | CScreenShakeTrigger | `game/CScreenShakeTrigger` (linked) | high |
| 800CF530 | 800CF95C | 7 | 800CF858 | CSlowMoTrigger | `game/CSlowMoTrigger` (linked) | high |
| 800CF95C | 800D0144 | 8 | 800D004C | CSplashScreenTrigger | `game/CSplashScreenTrigger` (linked) | high |
| 800D0144 | 800D02F4 | 3 | - | none | unnamed | low |
| 800D02F4 | 800D03EC | 4 | 800D02F4 | CStaticInit | CStaticInit | medium |
| 800D03EC | 800D06F8 | 7 | 800D05DC | CSwimStaminaTrigger | `game/CSwimStaminaTrigger` (linked) | high |
| 800D06F8 | 800D0AB0 | 7 | 800D0990 | CVoicemailMessageTrigger | `game/CVoicemailMessageTrigger` (linked) | high |
| 800D0AB0 | 800D1268 | 8 | 800D11B0 | AreaTriggerBase | AreaTriggerBase | high |
| 800D1268 | 800D1934 | 13 | 800D181C | AreaTriggerBox, AreaTriggerBoxLogic | `game/AreaTriggerBox` | high |
| 800D1934 | 800D2DE4 | 33 | 800D2DAC | none | unnamed | high |
| 800D2DE4 | 800D4064 | 15 | - | none; 2 unnamed candidates | unnamed | low |
| 800D4064 | 800D409C | 1 | 800D4064 | none | unnamed | medium |
| 800D409C | 800D5DA0 | 14 | - | none; 2 unnamed candidates | unnamed | low |
| 800D5DA0 | 800D5DD8 | 1 | 800D5DA0 | none | unnamed | medium |
| 800D5DD8 | 800D75A0 | 11 | 800D7510 | none | unnamed | high |
| 800D75A0 | 800D7C00 | 6 | 800D7B74 | COnScreenText | COnScreenText | medium |
| 800D7C00 | 800D7F80 | 6 | - | none | unnamed | low |
| 800D7F80 | 800D8E0C | 13 | 800D8DC4 | none | unnamed | medium |
| 800D8E0C | 800DA7A0 | 19 | 800DA768 | none | unnamed | high |
| 800DA7A0 | 800DAE60 | 9 | - | none | unnamed | low |
| 800DAE60 | 800DAE98 | 1 | 800DAE60 | none | unnamed | medium |
| 800DAE98 | 800DEB70 | 39 | 800DEB38 | none | unnamed | high |
| 800DEB70 | 800DF0C0 | 9 | - | none | unnamed | low |
| 800DF0C0 | 800DF0F8 | 1 | 800DF0C0 | none | unnamed | high |
| 800DF0F8 | 800E1074 | 10 | 800E103C | none | unnamed | high |
| 800E1074 | 800E2DB4 | 20 | 800E2D7C | none | unnamed | high |
| 800E2DB4 | 800E3150 | 9 | - | none; 2 unnamed candidates | unnamed | low |
| 800E3150 | 800E3188 | 1 | 800E3150 | none | unnamed | medium |
| 800E3188 | 800E6558 | 49 | - | none; 2 unnamed candidates | unnamed | low |
| 800E6558 | 800E6590 | 1 | 800E6558 | none | unnamed | high |
| 800E6590 | 800EF740 | 121 | 800EF61C | UI_MobileMainMenu.xaml | UI_MobileMainMenu (file) | high |
| 800EF740 | 800EFF74 | 15 | - | none; 2 unnamed candidates | unnamed | low |
| 800EFF74 | 800EFFAC | 1 | 800EFF74 | none | unnamed | medium |
| 800EFFAC | 800F0588 | 6 | - | none; 3 unnamed candidates | unnamed | low |
| 800F0588 | 800F05C0 | 1 | 800F0588 | none | unnamed | medium |
| 800F05C0 | 800F0C54 | 8 | - | none | unnamed | low |
| 800F0C54 | 800F0C8C | 1 | 800F0C54 | none | unnamed | medium |
| 800F0C8C | 800F21F0 | 17 | - | none | unnamed | low |
| 800F21F0 | 800F2228 | 1 | 800F21F0 | none | unnamed | medium |
| 800F2228 | 800F7960 | 82 | 800F7928 | none | unnamed | high |
| 800F7960 | 800F8AEC | 14 | - | none; 4 unnamed candidates | unnamed | low |
| 800F8AEC | 800F8B24 | 1 | 800F8AEC | none | unnamed | medium |
| 800F8B24 | 800F9994 | 18 | - | none | unnamed | low |
| 800F9994 | 800F99CC | 1 | 800F9994 | none | unnamed | medium |
| 800F99CC | 800FB410 | 23 | - | none | unnamed | low |
| 800FB410 | 800FB448 | 1 | 800FB410 | none | unnamed | medium |
| 800FB448 | 800FC510 | 9 | 800FC4D8 | none | unnamed | high |
| 800FC510 | 800FE7D0 | 19 | 800FE798 | none | unnamed | high |
| 800FE7D0 | 800FEAB4 | 7 | 800FEA54 | none | unnamed | high |
| 800FEAB4 | 800FEE70 | 10 | - | none | unnamed | low |
| 800FEE70 | 8010016C | 21 | 80100134 | none | unnamed | medium |
| 8010016C | 80100B40 | 13 | - | none; 2 unnamed candidates | unnamed | low |
| 80100B40 | 80100B78 | 1 | 80100B40 | none | unnamed | medium |
| 80100B78 | 80100C00 | 5 | 80100BC8 | none | unnamed | high |
| 80100C00 | 80100D70 | 9 | 80100D38 | none | unnamed | high |
| 80100D70 | 801020F0 | 36 | - | none; 4 unnamed candidates | unnamed | low |
| 801020F0 | 801021C8 | 1 | 801020F0 | none | unnamed | medium |
| 801021C8 | 80107098 | 9 | - | none; 2 unnamed candidates | unnamed | low |
| 80107098 | 801070D0 | 1 | 80107098 | none | unnamed | medium |
| 801070D0 | 80108CE8 | 27 | - | none; 3 unnamed candidates | unnamed | low |
| 80108CE8 | 80108D20 | 1 | 80108CE8 | none | unnamed | medium |
| 80108D20 | 8010AB48 | 16 | 8010AB10 | none | unnamed | high |
| 8010AB48 | 8010BEB4 | 9 | 8010BE88 | none | unnamed | high |
| 8010BEB4 | 8010EE1C | 54 | 8010EDE4 | none | unnamed | high |
| 8010EE1C | 80112BEC | 38 | - | none; 8 unnamed candidates | unnamed | low |
| 80112BEC | 80112C24 | 1 | 80112BEC | none | unnamed | medium |
| 80112C24 | 801136A0 | 13 | - | rwID_HANIMANIMATION | unnamed | medium |
| 801136A0 | 801136CC | 1 | 801136A0 | CAnimatedCamera | CAnimatedCamera | high |
| 801136CC | 80113DBC | 7 | - | rwID_HANIMANIMATION, rwID_CLUMP | unnamed | medium |
| 80113DBC | 801144B4 | 6 | - | s_Mount, rwID_HANIMANIMATION | unnamed | medium |
| 801144B4 | 80114540 | 1 | 801144B4 | none | unnamed | medium |
| 80114540 | 80114C08 | 13 | - | none; 2 unnamed candidates | unnamed | low |
| 80114C08 | 80114C34 | 1 | 80114C08 | CBaseCamera | CBaseCamera | medium |
| 80114C34 | 801175E4 | 25 | 801174A4 | none | unnamed | high |
| 801175E4 | 80118150 | 12 | - | SGrabInfo | unnamed | medium |
| 80118150 | 801181B0 | 1 | 80118150 | CGrabCamera | CGrabCamera | medium |
| 801181B0 | 8011B7C4 | 28 | - | SChangeCamModeArgs, SPhoneCamSettings | unnamed | medium |
| 8011B7C4 | 8011B86C | 1 | 8011B7C4 | CPlayerCamera | CPlayerCamera | medium |
| 8011B86C | 8011CC28 | 13 | - | none | unnamed | low |
| 8011CC28 | 8011CCD4 | 1 | 8011CC28 | CPOVCamera | CPOVCamera | medium |
| 8011CCD4 | 8011CDF0 | 7 | 8011CD90 | CSimpleStaticCamera | CSimpleStaticCamera | high |
| 8011CDF0 | 8011DBD4 | 11 | 8011DB74 | CStaticCamera | CStaticCamera | medium |
| 8011DBD4 | 8011E8D8 | 12 | - | none; 2 unnamed candidates | unnamed | low |
| 8011E8D8 | 8011E938 | 1 | 8011E8D8 | CSwimCamera | CSwimCamera | medium |
| 8011E938 | 8011FCC0 | 20 | - | none; 2 unnamed candidates | unnamed | low |
| 8011FCC0 | 8011FF90 | 2 | 8011FF30 | CAIFrozenLakeEnemyRespot | CAIFrozenLakeEnemyRespot | medium |
| 8011FF90 | 80124104 | 50 | - | none; 4 unnamed candidates | unnamed | low |
| 80124104 | 8012BD58 | 62 | 8012BC44 | CAIWorldBehaviour, rwID_ZONEINFO | CAIWorldBehaviour | medium |
| 8012BD58 | 8013A6B0 | 222 | - | none; 27 unnamed candidates | unnamed | low |
| 8013A6B0 | 8013B0D4 | 5 | 8013B0C0 | none | unnamed | high |
| 8013B0D4 | 801440B8 | 131 | - | none; 14 unnamed candidates | unnamed | low |
| 801440B8 | 801440E4 | 1 | 801440B8 | CAttractMarker | CAttractMarker | high |
| 801440E4 | 8014438C | 5 | - | none | unnamed | low |
| 8014438C | 80144ECC | 8 | 80144D2C | none | unnamed | medium |
| 80144ECC | 80145114 | 11 | - | CarSeat | unnamed | medium |
| 80145114 | 801455C8 | 3 | - | none; 2 unnamed candidates | unnamed | low |
| 801455C8 | 80148500 | 15 | 80148380 | CZone, CFlare, NVec3 | CZone | high |
| 80148500 | 8014882C | 12 | - | none | unnamed | low |
| 8014882C | 8014910C | 7 | 8014908C | CFlarePickup | CFlarePickup | medium |
| 8014910C | 80149290 | 6 | - | none | unnamed | low |
| 80149290 | 801499F4 | 12 | 8014994C | none | unnamed | medium |
| 801499F4 | 8014A998 | 17 | - | none; 2 unnamed candidates | unnamed | low |
| 8014A998 | 8014AEA4 | 4 | 8014ADEC | none | unnamed | medium |
| 8014AEA4 | 8014E56C | 30 | - | none | unnamed | low |
| 8014E56C | 8014E6F8 | 1 | 8014E56C | none | unnamed | high |
| 8014E6F8 | 8014EC98 | 11 | - | none; 2 unnamed candidates | unnamed | low |
| 8014EC98 | 8014F7DC | 4 | - | Psych.arc | Psych (file) | medium |
| 8014F7DC | 8015013C | 9 | - | none; 3 unnamed candidates | unnamed | low |
| 8015013C | 80151A64 | 13 | 80151980 | none | unnamed | medium |
| 80151A64 | 801540C0 | 24 | - | none; 2 unnamed candidates | unnamed | low |
| 801540C0 | 8015414C | 1 | 801540C0 | CSwimController | CSwimController | high |
| 8015414C | 80154F44 | 21 | 80154ED0 | none | unnamed | medium |
| 80154F44 | 80157AC4 | 21 | - | none; 2 unnamed candidates | unnamed | low |
| 80157AC4 | 80157C18 | 1 | 80157AC4 | none | unnamed | high |
| 80157C18 | 80158130 | 18 | - | none | unnamed | low |
| 80158130 | 8015A7B0 | 16 | 8015A724 | CWheelchair | CWheelchair | medium |
| 8015A7B0 | 8015AFB4 | 22 | 8015AF28 | CButtonPuzzle | CButtonPuzzle | medium |
| 8015AFB4 | 8015B704 | 13 | - | none; 5 unnamed candidates | unnamed | low |
| 8015B704 | 8015B764 | 1 | 8015B704 | CCanPuzzleController | CCanPuzzleController | high |
| 8015B764 | 8015BAC8 | 9 | - | none | unnamed | low |
| 8015BAC8 | 8015C984 | 14 | 8015C8B0 | none | unnamed | medium |
| 8015C984 | 8015DBE8 | 26 | - | none | unnamed | low |
| 8015DBE8 | 8015DC74 | 1 | 8015DBE8 | CCardObjectController | CCardObjectController | medium |
| 8015DC74 | 8015E24C | 14 | - | none; 2 unnamed candidates | unnamed | low |
| 8015E24C | 8015E2D8 | 1 | 8015E24C | CDeadSleepingPuzzle | CDeadSleepingPuzzle | medium |
| 8015E2D8 | 8015E94C | 12 | - | none; 2 unnamed candidates | unnamed | low |
| 8015E94C | 8015E9AC | 1 | 8015E94C | CFigurinePuzzle | CFigurinePuzzle | high |
| 8015E9AC | 80166040 | 79 | 80165EEC | none | unnamed | high |
| 80166040 | 80166474 | 21 | - | none; 2 unnamed candidates | unnamed | low |
| 80166474 | 80166500 | 1 | 80166474 | CLoveAndMarriage | CLoveAndMarriage | medium |
| 80166500 | 801676C8 | 21 | - | none; 5 unnamed candidates | unnamed | low |
| 801676C8 | 80167774 | 1 | 801676C8 | CMemento | CMemento | medium |
| 80167774 | 8016993C | 25 | - | none | unnamed | low |
| 8016993C | 801699C8 | 1 | 8016993C | CPasswordPuzzleController | CPasswordPuzzleController | medium |
| 801699C8 | 8016CCF4 | 34 | 8016CC20 | RwMatrix | unnamed | high |
| 8016CCF4 | 8016DBCC | 23 | 8016DB40 | CPickupObjectController | CPickupObjectController | high |
| 8016DBCC | 8016DE18 | 9 | - | none | unnamed | low |
| 8016DE18 | 8016E6F0 | 5 | 8016E664 | CPillBottlePuzzle | CPillBottlePuzzle | medium |
| 8016E6F0 | 8016EC10 | 8 | - | none; 4 unnamed candidates | unnamed | low |
| 8016EC10 | 8016EC9C | 1 | 8016EC10 | CPillPuzzleTrigger | CPillPuzzleTrigger | high |
| 8016EC9C | 8016FA44 | 10 | - | none | unnamed | low |
| 8016FA44 | 8016FAD0 | 1 | 8016FA44 | CProjectorPuzzle | CProjectorPuzzle | medium |
| 8016FAD0 | 8016FB50 | 7 | - | CProjectorPuzzle factory and thunks (belong to the 8016FA44 row) | CProjectorPuzzle | medium |
| 8016FB50 | 80170EE4 | 11 | 80170E2C | CSequencePuzzleLogic | CSequencePuzzleLogic (split, NonMatching) | high |
| 8017076C | 80170B14 | 1 | - | CSequencePuzzleLogic | CSequencePuzzleLogic | medium |
| 80170B14 | 80170E2C | 2 | - | none | unnamed | low |
| 80170E2C | 80170E8C | 1 | 80170E2C | CSequencePuzzleLogic | CSequencePuzzleLogic | medium |
| 80170E8C | 80171250 | 8 | - | none; 2 unnamed candidates | unnamed | low |
| 80171250 | 801712DC | 1 | 80171250 | CSexCardMagnet | CSexCardMagnet | medium |
| 801712DC | 80171368 | 2 | - | none | unnamed | low |
| 80171368 | 80173218 | 17 | 8017318C | CSplinePuzzle, rwID_SPLINE | CSplinePuzzle | medium |
| 80173218 | 80175948 | 24 | 80175838 | CSUV, s_Mount | CSUV | high |
| 80175948 | 80175E80 | 15 | - | none; 3 unnamed candidates | unnamed | low |
| 80175E80 | 80175F0C | 1 | 80175E80 | CTimeTablePuzzle | CTimeTablePuzzle | medium |
| 80175F0C | 80177760 | 17 | - | none | unnamed | low |
| 80177760 | 801777EC | 1 | 80177760 | CTwistPuzzle | CTwistPuzzle | medium |
| 801777EC | 8017A270 | 25 | 8017A1A4 | CDrawingTest, CCrayon | CDrawingTest | high |
| 8017A270 | 8017A3C0 | 16 | - | none | unnamed | low |
| 8017A3C0 | 8017ADA0 | 8 | 8017ACE8 | CPlayerResponse | CPlayerResponse | medium |
| 8017ADA0 | 8017CF48 | 21 | 8017CE38 | CQuestionnaire, EventMsgInfo | CQuestionnaire | medium |
| 8017CF48 | 8017D0C4 | 14 | - | none | unnamed | low |
| 8017D0C4 | 8017F718 | 28 | - | rwID_RWS | unnamed | medium |
| 8017F718 | 8017F7D0 | 1 | 8017F718 | CObstacle | CObstacle | high |
| 8017F7D0 | 8017FA78 | 25 | - | CShortCut | CShortCut | high |
| 8017FA78 | 8017FCDC | 6 | - | none; 3 unnamed candidates | unnamed | low |
| 8017FCDC | 8017FD3C | 1 | 8017FCDC | CShortCut | CShortCut | medium |
| 8017FD3C | 8018039C | 22 | - | none | unnamed | low |
| 8018039C | 8018041C | 1 | 8018039C | CShortCutView | CShortCutView | medium |
| 8018041C | 80180514 | 3 | - | none | unnamed | low |
| 80180514 | 80185890 | 31 | - | CTraversalTypeData | CTraversalTypeData | medium |
| 80185890 | 80185948 | 1 | 80185890 | CTerrainTraversal | CTerrainTraversal | medium |
| 80185948 | 801867D8 | 23 | - | none | unnamed | low |
| 801867D8 | 80186838 | 1 | 801867D8 | CTraversalCamera | CTraversalCamera | medium |
| 80186838 | 80186A70 | 6 | - | none | unnamed | low |
| 80186A70 | 80186E20 | 2 | - | CTraversalCamera | CTraversalCamera | medium |
| 80186E20 | 80186E80 | 1 | 80186E20 | CTraversalTypeData | CTraversalTypeData | medium |
| 80186E80 | 8018A964 | 31 | - | Version mismatch on Fuse Binary Format!! | unnamed | medium |
| 8018A964 | 8018AA80 | 1 | - | none | unnamed | low |
| 8018AA80 | 8018AAE0 | 1 | 8018AA80 | CMapAnchorPoint | CMapAnchorPoint | high |
| 8018AAE0 | 8018AD40 | 5 | - | none | unnamed | low |
| 8018AD40 | 8018ADA0 | 1 | 8018AD40 | CMapLayer | CMapLayer | medium |
| 8018ADA0 | 8018CB64 | 19 | - | none | unnamed | low |
| 8018CB64 | 8018CEC8 | 2 | - | UI_Map.rws | UI_Map (file) | medium |
| 8018CEC8 | 8018D33C | 3 | - | none; 3 unnamed candidates | unnamed | low |
| 8018D33C | 8018D374 | 1 | 8018D33C | none | unnamed | high |
| 8018D374 | 8018D854 | 7 | 8018D7AC | none | unnamed | high |
| 8018D854 | 8018DB6C | 10 | 8018DB0C | CMapWaypointPurge | CMapWaypointPurge | high |
| 8018DB6C | 8018E0F8 | 12 | - | none | unnamed | low |
| 8018E0F8 | 8018E1C0 | 2 | - | HomeMenu%s.arc | unnamed | medium |
| 8018E1C0 | 8018ED24 | 2 | 8018ED14 | none | unnamed | medium |
| 8018ED24 | 8018EF3C | 4 | - | homeBtn.arc, SpeakerSe.arc, HomeButtonSe.arc | homeBtn (file) | medium |
| 8018EF3C | 8018FC3C | 9 | - | none; 2 unnamed candidates | unnamed | low |

Always-true save condition (game-10). Five functions test a third key after `fn_80206348( a, b, "SaveData" )` with a
dead compare and a constant true (`lwz rK, key; li rT, 1; cmpwi rK, 0; cmpwi rT, 0; beq`): CMessageRelay `fn_8009CB1C`
(0x800CB804, key 0xC4), CPointTrigger `fn_800C62B0` (0x800C62DC, key 0x88; the class name is from its log string
"CPointTrigger[%s] sends %s & %s"), AreaTriggerBox (0x800D1468 and 0x800D1524, key 0xE8, the enter and exit handlers of the
800D1268 row) and a class of the 8016FAD0 row (0x8016FF64, key 0xDC; its "SaveData" pointer is followed by a "CSeq..."
string). CPointTrigger's unit (800C552C row) also keeps both checks out of line, `fn_800C63BC` (`return fn_80206348( 0x8C,
0x90, "SaveData" )`, a tail call) and `fn_800C63AC` (`lwz r0, 0x88(r3); li r3, 1; cmpwi r0, 0; blr`), both called by
`fn_80148BD0` in the 8014882C row (CFlarePickup), which first calls `fn_800C62B0` as a base method (`fn_80148B70` calls
`fn_800C63BC` alone). The check is a
member function of each class that passes its key to the shared inline `CheckSaveDataKey` (`include/game/SaveData.h`),
a debug check compiled out in release (docs/exceptions.md, "Game code rulings (game-11)").

## Climax core (801A70B8-801B4354)

| start | end | fns | sinit | evidence strings | proposed name | conf |
|---|---|---|---|---|---|---|
| 801A70B8 | 801A9310 | 38 | - | none | unnamed | low |
| 801A9310 | 801AA550 | 20 | 801AA434 | CEventHandler, CAttributeHandler | CEventHandler | medium |
| 801AA550 | 801AABD8 | 7 | 801AABA0 | none | unnamed | high |
| 801AABD8 | 801AB758 | 14 | - | rwID_RWS | unnamed | medium |
| 801AB758 | 801AB838 | 1 | - | rwID_ATOMIC, rwID_CLUMP | unnamed | medium |
| 801AB838 | 801AB86C | 1 | - | rwID_CLUMP | unnamed | medium |
| 801AB86C | 801AB878 | 1 | - | rwID_CLUMP | unnamed | medium |
| 801AB878 | 801ABB88 | 5 | - | none | unnamed | low |
| 801ABB88 | 801AC1C4 | 8 | 801AC18C | rwID_RWS | unnamed | medium |
| 801AC1C4 | 801B0120 | 87 | 801B008C | rwID_TEXDICTIONARY | unnamed | high |
| 801B0120 | 801B1374 | 14 | - | none | unnamed | low |
| 801B1374 | 801B2740 | 15 | - | Async.Arc | Async (file) | medium |
| 801B2740 | 801B274C | 1 | - | DATA.ARC | DATA (file) | medium |
| 801B274C | 801B2DC4 | 7 | - | IGC.ARC | IGC (file) | medium |
| 801B2DC4 | 801B365C | 11 | - | none; 3 unnamed candidates | unnamed | low |
| 801B365C | 801B438C | 12 | 801B4354 | none | unnamed | high |

## Climax XML/config/assets (801CCD24-80209D54)

| start | end | fns | sinit | evidence strings | proposed name | conf |
|---|---|---|---|---|---|---|
| 801CCD24 | 801CE708 | 12 | - | none; 2 unnamed candidates | unnamed | low |
| 801CE708 | 801D18A4 | 27 | 801D186C | none | unnamed | medium |
| 801D18A4 | 801D26E0 | 12 | - | none; 3 unnamed candidates | unnamed | low |
| 801D26E0 | 801D2894 | 2 | 801D285C | CONFIG | unnamed | medium |
| 801D2894 | 801D7A0C | 77 | - | none; 7 unnamed candidates | unnamed | low |
| 801D7A0C | 801D7D54 | 3 | 801D7CD4 | none | unnamed | medium |
| 801D7D54 | 801E2CE8 | 185 | - | none; 14 unnamed candidates | unnamed | low |
| 801E2CE8 | 801E3658 | 12 | 801E3620 | none | unnamed | medium |
| 801E3658 | 801F8A5C | 183 | - | none; 18 unnamed candidates | unnamed | low |
| 801F8A5C | 801FA470 | 41 | 801FA388 | none | unnamed | medium |
| 801FA470 | 80202B9C | 181 | - | none; 6 unnamed candidates | unnamed | low |
| 80202B9C | 80202E0C | 6 | 80202CCC | sUpdateData | unnamed | high |
| 80202E0C | 80203724 | 15 | - | iMsgStopSystem | unnamed | medium |
| 80203724 | 80203818 | 1 | 80203724 | none | unnamed | medium |
| 80203818 | 802044A8 | 11 | - | iMsgPreRender | unnamed | medium |
| 802044A8 | 802045CC | 1 | 802044A8 | none | unnamed | medium |
| 802045CC | 80209044 | 99 | - | none; 4 unnamed candidates | unnamed | low |
| 80209044 | 8020907C | 2 | - | rwID_ASSETGROUPTAG | unnamed | medium |
| 8020907C | 802091E8 | 4 | - | none; 2 unnamed candidates | unnamed | low |
| 802091E8 | 8020920C | 4 | - | rwID_ASSETGROUPTAG | unnamed | medium |
| 8020920C | 80209220 | 2 | - | rwID_AUDIODATA | unnamed | medium |
| 80209220 | 80209234 | 2 | - | rwID_AUDIOCUES | unnamed | medium |
| 80209234 | 80209248 | 2 | - | rwID_AINAVMESH | unnamed | medium |
| 80209248 | 8020925C | 2 | - | rwID_RWS | unnamed | medium |
| 8020925C | 80209270 | 2 | - | none | unnamed | low |
| 80209270 | 80209284 | 2 | - | rwID_SPLINE | unnamed | medium |
| 80209284 | 80209298 | 2 | - | rwID_COMBATCOLLISION | unnamed | medium |
| 80209298 | 802092AC | 2 | - | rwID_FUSESTATE | unnamed | medium |
| 802092AC | 802092BC | 2 | - | none | unnamed | low |
| 802092BC | 802092CC | 2 | - | CMA | unnamed | medium |
| 802092CC | 802092DC | 2 | - | none | unnamed | low |
| 802092DC | 802092F0 | 2 | - | rwID_CBSP | unnamed | medium |
| 802092F0 | 80209304 | 2 | - | rwID_POLYAREA | unnamed | medium |
| 80209304 | 8020932C | 4 | - | none; 2 unnamed candidates | unnamed | low |
| 8020932C | 80209340 | 2 | - | rwID_MTEFFECTDICT | unnamed | medium |
| 80209340 | 80209354 | 2 | - | rwID_TEXDICTIONARY | unnamed | medium |
| 80209354 | 80209368 | 2 | - | rwID_ZONEINFO | unnamed | medium |
| 80209368 | 8020937C | 2 | - | rwID_KFONT | unnamed | medium |
| 8020937C | 80209390 | 2 | - | rwID_DMORPHANMSTREAM | unnamed | medium |
| 80209390 | 802093A4 | 2 | - | rwID_DMORPHANIMATION | unnamed | medium |
| 802093A4 | 802093B8 | 2 | - | rwID_HANIMANIMATION | unnamed | medium |
| 802093B8 | 802093CC | 2 | - | rwID_CLUMP | unnamed | medium |
| 802093CC | 802093E0 | 2 | - | rwID_ATOMIC | unnamed | medium |
| 802093E0 | 80209B88 | 30 | - | rwID_WORLD | unnamed | medium |
| 80209B88 | 80209C38 | 1 | - | rwID_CLUMP | unnamed | medium |
| 80209C38 | 80209CD4 | 1 | - | rwID_HANIMANIMATION | unnamed | medium |
| 80209CD4 | 80209D74 | 2 | - | none | unnamed | low |

## Class-name evidence

Distinct C-prefixed class-name strings in the three ranges, with the lowest function address that references each. GR = the class also appears as a qualifier in `ref/ghostrider_ps2/classes.txt`.

- `CActorTest` 8003C324
- `CAIFrozenLakeEnemyRespot` 8011FF30
- `CAIWorldBehaviour` 8012BC44
- `CAmbientInterferencePoint` 800C2B64
- `CAmbientTextMessage` 800C2EC8
- `CAmbientVoicemail` 800C3270
- `CAnimatedCamera` 801136A0
- `CAnimBehaviour` 80017570 GR
- `CAnimSkeleton` 8001A0A4
- `CAttractMarker` 801440B8
- `CAttributeHandler*` 801A9310
- `CAudioLocatorBehaviour2D` 80033D34
- `CAudioReverb` 8003BB84
- `CBaseBehaviour` 8009CEEC GR
- `CBaseCamera` 80114C08 GR
- `CBaseLight` 800B70E0 GR
- `CBaseSpawner` 800C80D4 GR
- `CBasicAnimatedObject` 800220EC
- `CBasicMount` 8009D22C
- `CButtonPuzzle` 8015AF28
- `CCanPuzzleController` 8015B704
- `CCardObjectController` 8015DBE8
- `CCharacter` 800436D8
- `CClumpBehaviour` 8009ECB4 GR
- `CClumpSwap` 8009EEC8
- `CColdTrigger` 800C8928
- `CCollisionCapsule` 8000CFD8 GR
- `CColorLight` 800B783C GR
- `CCombatZoneSetEnemyCountTrigger` 800C8BCC
- `CCopCar` 80077B04
- `CCounter` 800C8FD4 GR
- `CCrayon` 8017A1A4
- `CDeadSleepingPuzzle` 8015E24C
- `CDesignerDebugMsg` 800C9760
- `CDrawingTest` 8017A1A4
- `CEnemy` 80052E58
- `CEnemySpawn` 80052E58
- `CEventHandler*` 801A9310
- `CFadeTrigger` 800C9BA0
- `CFigurinePuzzle` 8015E94C
- `CFlare` 80148380
- `CFlarePickup` 8014908C
- `CFlickerTrigger` 800C9F3C
- `CFogConfig` 80092990
- `CGenericCar` 8007B9A8
- `CGenericCharacter` 80053B50
- `CGenericSpawner` 800C84E8 GR
- `CGenericSpawner::%s has data attached` 800C8108 GR
- `CGhostPhotoTrigger` 800CB0DC
- `CGrabCamera` 80118150
- `CHeadTurnNPC` 80055084
- `CHidingPlace` 800C4110
- `CHitEffectManager` 800877C0 GR
- `CIGCBorderControl` 800A8ED8
- `CIGCPlayerMover` 800AB434
- `CKeyHUDTrigger` 800CB3E8
- `CLightEffects` 800B8640
- `CLookTrigger` 800C4EA4
- `CLoveAndMarriage` 80166474
- `CMapAnchorPoint` 8018AA80
- `CMapLayer` 8018AD40
- `CMapWaypoint` 8018D7AC (with `CRemoveMapWaypoint`, same sinit)
- `CMapWaypointPurge` 8018DB0C
- `CMemento` 801676C8
- `CMessageRelay` 800CBE40 GR
- `CModalTutorialMessageTrigger` 800CC3CC
- `CMultiTextureSequenceObject` 800A3E1C
- `CObstacle` 8017F718
- `COnScreenPromptTrigger` 800CCDDC
- `COnScreenText` 800D7B74
- `CPadRumbleTrigger` 800CD26C GR
- `CPasswordPuzzleController` 8016993C
- `CPhoneCallTrigger` 800CD8B4
- `CPhoneMessageTrigger` 800CDC30
- `CPhysicsObject` 800A4E8C GR
- `CPickupObjectController` 8016DB40
- `CPillBottlePuzzle` 8016E664
- `CPillPuzzleTrigger` 8016EC10
- `CPlayer` 80073554
- `CPlayerCamera` 8011B7C4
- `CPlayerCostume` 80073ED0
- `CPlayerLight` 800B9374
- `CPlayerResponse` 8017ACE8
- `CPlayerStateTrigger` 800CE8A0
- `CPointOfInterest` 800C5500
- `CPointTrigger` 800C63D0
- `CPOVCamera` 8011CC28
- `CPreCache` 80028FA4
- `CProjectorPuzzle` 8016FA44
- `CPsycologyZoneTrigger` 800CEB0C
- `CPsyPointOfInterest` 800C75FC
- `CPsyTrigger` 800CEDB8
- `CQuestionnaire*` 8017B2F4
- `CScalingObject` 800A5598
- `CSceneGraphContainer*` 8009F4B4
- `CSceneObject` 800A5800 GR
- `CScreenShakeTrigger` 800CF43C GR
- `CSequenceObject` 800A6984 GR
- `CSequencePuzzleLogic` 8017076C
- `CSexCardMagnet` 80171250
- `CShortCut` 8017FA28
- `CShortCutView` 8018039C
- `CSimpleAnimation` 80025434 GR
- `CSimpleStaticCamera` 8011CD90
- `CSkyDome` 800A720C GR
- `CSlowMoTrigger` 800CF858 GR
- `CSmashableObject` 800A7BF8 GR
- `CSplashScreenTrigger` 800D004C
- `CSplineHelper` 800A7FC4
- `CSplinePuzzle` 8017318C
- `CStaticCamera` 8011DB74 GR
- `CStaticInit` 800D02F4 GR
- `CSUV*` 8007C8A4
- `CSwimCamera` 8011E8D8
- `CSwimController` 801540C0
- `CSwimStaminaTrigger` 800D05DC
- `CSystemCommands` 800A2900 GR
- `CTerrainTraversal` 80185890
- `CTimeTablePuzzle` 80175E80
- `CTraversalCamera` 801867D8
- `CTraversalCamera::TraversalCameraMsg*` 800C3AF8
- `CTraversalTypeData` 80186E20
- `CTraversalTypeData::TraversalTypeDataMsg*` 801814F0
- `CTwistPuzzle` 80177760
- `CVoicemailMessageTrigger` 800D0990
- `CWalkNPC` 80077994
- `CWheelchair` 8015A724
- `CZone` 80028734
- `CZone*` 8002598C
- `CZoneChangeInfo*` 8003C454
- `CZoneCollision` 80028734 GR

Not counted as class names (all-caps, no `*`): `CMA` 8002450C, `CNPC` 8005543C, `CONFIG` 801D26E0, `CSUV` 8007C7FC.
