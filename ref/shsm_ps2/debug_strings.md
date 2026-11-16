# PS2-only debug strings and their Wii counterparts

Evidence for `ref/shsm_ps2/debug_strings.txt` (19 strings). Advisory, never gate input.

## How the PS2 side was read

- The PS2 executable is `SLUS_218.99` in `orig/_iso/` (one RWX `PT_LOAD`, vaddr `0x100000`, file offset `0x1000`).
  `witness.exe ps2 <address>` lists a function and annotates its strings, but offers no string-to-function
  search, so references were found by scanning the code for `lui rt,hi` + `addiu|ori|daddiu rd,rt,lo` pairs (window
  60 instructions, same register, no intervening `lui`), for `$gp`-relative pairs, and every 4-byte word of the file
  for a pointer to the string. The scan reproduces what `witness.exe ps2` annotates (for example `"SaveZoneInfo"`
  at `0x127FB8`).
- **None of the 19 strings is referenced**: no code pair, no `$gp` offset, no data pointer. The same holds for the
  other attribute names around them (`Rendering`, `Lighting`, `Effects`, `Q Clipper`, `Beam Fogging`, `Angular
  Attenuation`, `World Shadowing`, ...). They are literals the compiler pooled and the code never loads: the attribute
  handlers do not look attributes up by name. A PS2 handler iterates `unk_AttributeCommandIterator` (ctor
  `0x281EB0`, Wii `__ct__28unk_AttributeCommandIteratorFRC19unk_AttributePacketUl`) and dispatches on numeric
  command ids; the names are editor metadata that survived only as pool strings.
- So the strings were attributed by their **position in the string pool**. The pool is emitted per translation
  unit in first-use order; within a unit it is not in code-address order (compile order differs from link order),
  but a unit's strings stay together and a group of strings first used in a handler sits between the strings
  referenced by the functions compiled just before and after it. Each group below is bounded by the nearest
  referenced strings on both sides. This is indirect evidence: a confidence word says how tight the bound is.
- Pairing to Wii used `witness.exe of` / `order` (tier named) and, independently, strings shared by the PS2
  neighbours and the Wii DOL (`query.exe find-string`). Where both agree, the row says so.
- PS2 string addresses are vaddrs in the executable. Handler addresses are PS2 function starts.

## Table

Pool groups (one handler or unit each) are separated by blank rows.

| string | PS2 addr | PS2 function(s) | kind | referenced neighbours (W = also in Wii DOL) | Wii counterpart (how) | confidence | class / system | what the Wii function lacks |
|---|---|---|---|---|---|---|---|---|
| `ZoneDebug` | 0x53DFF0 | none reference it | pool literal; owning code compiled out | before: `CPlayerCostume.Ring` (W, `lbl_803E1A90`), `CH_HM_Phone01(_JPN)`, `On Screen Display`; after: `START`, `IGC_1` (W only as `IGC_1_Crash.IGCStream`), `LC_Clinic`, `IT_CrashIntro`, `%s-%s.TexStream` (W), `IT_Frozen_StartAlley`, `SaveZoneInfo`, `ZoneName`, `ZoneFilter`, `ZoneLinkId`, `SavedPlayerPos`, `SavedPlayerAt` | unit `auto_03_8002D208_text`, class `unk_RespotManager` (Wii 0x8002D384..0x800332B4). `witness of` fn_80030314 (`unk_RespotManager::fn_80030314`, 0x80030314) <-> PS2 0x129998, tier string (`%s-%s.TexStream`, `%s.TexStream`); `order` agrees (fn_8002D384 <-> P128E48, fn_8002D4BC <-> P128FA0, both Wii users of `CPlayerCostume.Ring`) | medium (unit level; no function) | the zone/respot unit: `unk_RespotManager` (name invented in the project) | the whole zone-info debug display: attribute `ZoneDebug` and the print routine that uses `  ZoneInfo`, `   %s (%d+%d/%d) %s`, ` - Clean`, `- Dirty`; the PS2 save/restore functions `SaveZoneInfo` (0x127F80) and its loader (0x1282F8) have no paired Wii function either |
| `ZoneLinkDebug` | 0x53E000 | none reference it | pool literal | as above; its print formats are `  ZoneLinks`, `   %s::%s` | as above | medium (unit level) | as above | the link dump routine and attribute `ZoneLinkDebug` |
| | | | | | | | | |
| `(Debug) Disable Lighting Engine` | 0x53E598 | none; first used in the unit after `SavedPlayerAt` and its anim-name block (see note A) | pool literal in the same unit pool as the zone strings | own group: `Rendering`, `Lighting`, `Effects`, `Q Clipper`, `Beam Fogging` (none in Wii) | candidate handler PS2 0x12B5C8 (the only `unk_AttributeCommandIterator` user in the unit's code, `Wii: none`); Wii candidates are the unit's Wii handlers `fn_800324DC`, `fn_80032F44`, `fn_80033368` (callers of the iterator ctor), not paired by witness | low | a zone-level render-settings attribute set (category `Rendering`, groups `Lighting`, `Effects`); `unk_RespotManager` unit or a class compiled in it | the debug cases `(Debug) Disable Lighting Engine`, `Enable Non Black Fog`, `World Alpha Mode`, `Disable Light Engine for non focus secondary zones`, `Render Shadow Meshes`, and the non-debug `Q Clipper`, `Beam Fogging` |
| `(Debug) Enable Non Black Fog` | 0x53E5B8 | none | same | same | same | low | same | its case |
| `(Debug) World Alpha Mode` | 0x53E5D8 | none | same | same | same | low | same | its case |
| `(Debug) Disable Light Engine for non focus secondary zones` | 0x53E5F8 | none | same | same; wording ("secondary zones", "focus") ties it to the zone system | same | low | same | its case |
| `(Debug) Render Shadow Meshes` | 0x53E638 | none | same | same | same | low | same | its case |
| | | | | | | | | |
| `(debug) Reflection Markers` | 0x549AF8 | none reference it; sits in the pool of PS2 handler 0x1778B0 | pool literal of an attribute handler | `Rendering`, `Effects`, then `s_Mount*` (W; referenced at 0x177A5C inside 0x1778B0); class names `CSUV` (W, 0x176FD0), `CSUV*` (W), `SUV_PreSendMounts` (W) | PS2 0x1778B0 (0x398 bytes) <-> Wii fn_8007D18C (0x8007D18C, 0x334 bytes), witness tier callees (6 mapped callees on both sides); class registration PS2 0x176FD0 <-> Wii fn_8007C7FC by string `CSUV`: both methods agree on the class | medium-high | `CSUV` (the SUV vehicle; derives from `CGenericCar`, Wii `fn_801A73A8__11CGenericCar...` is its base handler) | cases for attributes `(debug) Reflection Markers`, `(debug) Reflection Highlight` (and the categories `Rendering`/`Effects`); in PS2 0x1778B0 they would sit near the `s_Mount*` event |
| `(debug) Reflection Highlight` | 0x549B18 | none | same | same | same | medium-high | `CSUV` | its case |
| | | | | | | | | |
| `(Debug) Show Shadow Buffer` | 0x554138 | none; pool group first used after `s_Mount*` (0x1B090C, in handler 0x1B0898) and before `CSpotLight` (0x1B1A74) | pool literal | `CPlayerLight` (W, PS2 reg 0x1AFB10), `s_Mount*` (W), then `Rendering`, `Lighting`, `World Shadowing`, `Shadow Buffer Resolution`, `(PS2) Vertex Light Multiplier` (none in Wii) | handler PS2 0x1B0898 <-> Wii fn_800B9E54 (0x800B9E54, 0x768 bytes), tier callees; next handler PS2 0x1B1308 <-> Wii fn_800BAB10 is the alternative. Class registration PS2 0x1AFB10 <-> Wii fn_800B9374 by string `CPlayerLight`; the Wii order of handlers (fn_800B9374 then fn_800B9E54) matches | medium for class (`CPlayerLight`), low-medium for which of the two handlers | `CPlayerLight`, the light that follows the player (shadow settings: world shadowing, shadow buffer resolution) | the case for `(Debug) Show Shadow Buffer`, and the PS2-specific `(PS2) Vertex Light Multiplier` case |
| | | | | | | | | |
| `(Debug) Display Buffer` | 0x555060 | none | pool literal | group: `Lighting`, `Q Clipper`, `Angular Attenuation`, `(Debug) Spotlight Show LProj`, `(Debug) Ramp Scale`, `(Debug) Beam Confinement Length`, `No Shadowing in Non Focus Player Zones`, `(Debug) Enable`, `Disable for secondary zones` (none in Wii). After the group: `%s%s` (0x1B62DC), `%s%s_active`, `CAmbientInterferencePoint` | candidate handler PS2 0x1B6098 (0x1A4 bytes, nearest iterator handler before 0x1B62DC, `Wii: none`); Wii spotlight code: `CSpotLightSurface::MakeNew` 0x80033D94 exists as a name but its pair is unproven; the Wii lighting range pairs end at fn_800BE690 / fn_800BE748 (P1B5638 / P1B56D8) | low | the spotlight: angular attenuation, ramp scale, beam confinement, projector buffer (`LProj`), shadow exclusion in non-focus zones | the five `(Debug)` cases of this group (Display Buffer, Spotlight Show LProj, Ramp Scale, Beam Confinement Length, Enable) |
| `(Debug) Spotlight Show LProj` | 0x555090 | none | same | same | same | low | spotlight | its case |
| `(Debug) Ramp Scale` | 0x5550B0 | none | same | same | same | low | spotlight | its case |
| `(Debug) Beam Confinement Length` | 0x5550C8 | none | same | same | same | low | spotlight | its case |
| `(Debug) Enable` | 0x555110 | none | same | same; follows `No Shadowing in Non Focus Player Zones` | same | low | spotlight (shadow-exclusion option) | its case |
| | | | | | | | | |
| `(Debug) Tension Level` | 0x574128 | none; bounded by `Tension` (referenced 0x24434C, function P2441D8/P244380) and `TensionBehaviour[%s]: Recieve %s` (0x244688, function P244650) | pool literal of the handler at PS2 0x244558 (the only iterator user between them) | `Game`, `Tension` (W), `Current Tension Level`, `Current Threat  Level`; then `TensionBehaviour[%s]: Recieve %s` (W), `TensionBehaviour`, `TensionManager`, `(%0.1f)` (W), `No Tension Manager` (W), `No VHS effect object` (W) | no paired Wii function for 0x244558 (`Wii: none`). Same-class Wii functions: fn_80154384 <-> P2441D8 (string/call tier, `Tension` in both), fn_8015463C <-> P244380, fn_80154AF0 <-> P244650 (receive handler, string `TensionBehaviour[%s]: Recieve %s` in both), fn_80154D78 <-> P244918 (`No Tension Manager`, string tier), fn_80154D4C <-> P244A38 (`No VHS effect object`, string tier): both methods agree on the unit | medium-high for the class, low for the handler pairing | the tension system: `TensionBehaviour` and `TensionManager` (class-name strings PS2 only) | the cases for `Current Tension Level`, `Current Threat Level`, `(Debug) Tension Level`, `(Debug) Interpolation Time`, `(Debug) Overide Combat Level` (attribute "Game" category) |
| `(Debug) Interpolation Time` | 0x574140 | none | same | same | same | medium-high (class), low (handler) | tension | its case |
| `(Debug) Overide Combat Level` | 0x574160 | none | same | same | same | medium-high (class), low (handler) | tension / combat-level override | its case |
| | | | | | | | | |
| `{DebugOutput} %s` | 0x5857F8 (the literal is `{DebugOutput} %s\n`; the tool's search must not require a NUL after `%s`) | none | pool literal | `PAL8`, `PAL4`, `Unkn`, `1555`, `565 `, `4444`, `LUM8`, `8888`, `888 `, `16  `, `24  `, `555 ` (raster-format name table), and the per-unit `rwID_*` chunk-name table; none referenced | none: the Wii DOL has no `PAL8`/`PAL4`/`Unkn` pool, only RenderWare's `rwRASTERFORMATLUM8 not yet supported` messages (fn_802AB6EC, `_rwDlPixelToRGB`) | unresolved | Climax asset/RenderWare debug dump (raster formats and chunk ids), a PS2 unit with no Wii equivalent | the whole debug printing routine |

Note A. The zone strings and this group lie in one pool segment: `SavedPlayerAt` (0x53E170), an anim-name block
(`bad_alloc` at 0x53E1F8 through `HDDirSync` at 0x53E528), then the group; the next unit's pool restarts at `bad_alloc`
(0x53E688). Read as one unit with the zone strings; the attribution to handler 0x12B5C8 is a position argument only.

## What this gives the matcher

- **Classes identified**
  - `CSUV` (PS2 handler 0x1778B0 <-> Wii fn_8007D18C; registration PS2 0x176FD0 <-> Wii fn_8007C7FC): the Wii handler
    omits two reflection debug cases.
  - `CPlayerLight` (registration PS2 0x1AFB10 <-> Wii fn_800B9374; handler PS2 0x1B0898 <-> Wii fn_800B9E54): shadow
    attributes; the Wii handler omits `(Debug) Show Shadow Buffer` and the PS2-only `(PS2) Vertex Light Multiplier`.
  - Tension system (`TensionBehaviour`, `TensionManager`): Wii fn_80154384, fn_8015463C, fn_80154AF0, fn_80154D4C,
    fn_80154D78 pair with P2441D8, P244380, P244650, P244A38, P244918; the debug attributes belong to the PS2 handler
    0x244558, which has no Wii pair.
  - The zone unit `unk_RespotManager` (Wii 0x8002D208 range) owns the zone debug display.
  - The spotlight attributes sit in a unit the pairing cannot name from the Wii side; keep them as "spotlight".
- **Name proposals** (none applied)
  - `unk_RespotManager`: the PS2 neighbours (`SaveZoneInfo`, `ZoneName`, `ZoneFilter`, `ZoneLinkId`, `SavedPlayerPos`,
    `SavedPlayerAt`, the on-screen `ZoneInfo`/`ZoneLinks` dump) show a zone and player-position manager; `ZoneManager`
    is a candidate if no `Respot` string appears. The existing name rests on `%s-%s.TexStream` only.
  - Wii handler fn_8007D18C: `CSUV` attribute handler; fn_800B9E54: `CPlayerLight` attribute handler (name the class
    from the registration strings `CSUV` and `CPlayerLight`, which the Wii DOL holds).
  - `TensionBehaviour` and `TensionManager` as class names for the tension unit (strings PS2 only; Wii has `Tension`
    twice and `TensionBehaviour[%s]: Recieve %s`).
- **Reading the PS2 witness for these handlers.** The PS2 handler is a longer `switch` over command ids from
  `unk_AttributeCommandIterator`. The debug attributes (`(Debug)`, `(debug)`, `(PS2)`) are cases whose body the Wii
  build dropped, so a PS2 call or memory access that has no Wii twin inside a handler is most likely such a case:
  ignore it when matching the Wii body, and do not add a case or a member for it. The strings themselves never occur
  in the code, so a PS2 call-set difference is the only trace; the Wii handler's own case list is what retail shows.

## Not resolved

- No string resolved to a referencing PS2 function (none is referenced). All 19 resolved to a pool group; 8 of them
  (reflection 2, shadow 1, tension 3, zone 2) are tied to a class or unit with a Wii counterpart and an agreeing
  second method; the rendering group (5 strings) and the spotlight group (5 strings) have only a position argument.
- `{DebugOutput} %s` has no identifiable function or Wii counterpart.
- The two light groups' exact handler (0x1B0898 versus 0x1B1308 for the shadow group, 0x1B6098 for the spotlight
  group) rests on pool position, not on a reference.
- Whether the PS2 attribute names were compiled out of the Wii build or never existed there cannot be told from the
  Wii DOL; only the absence is certain.
