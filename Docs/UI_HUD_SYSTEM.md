# CricketSim — UI & HUD System

> The simulation made legible. This layer **reads** gameplay state and draws it; it
> owns nothing and changes nothing. The ball is still the protagonist — the HUD is
> the scorer in the stand, not a player on the field.

This document is the design record for the `CricketUI` module: how the HUD is
architected, how data flows from the simulation into pixels, what the widgets and
debug overlays show, and how it is tested. Read `Docs/ARCHITECTURE.md` first for the
module graph and the physics-first philosophy this layer obeys.

---

## 1. The one hard rule

**The UI must never own or mutate gameplay state.** Every other decision follows
from this. The HUD consumes data from the Match Engine, ball physics, the
bowling/batting controllers and the replay system through *const reads only*, copies
what it needs into an immutable per-frame snapshot, and renders that. There is no
path by which a widget can write back into a simulation system — it is impossible by
construction, because the only thing a widget ever sees is a value-type copy.

---

## 2. Module & dependency placement

`CricketUI` is a new runtime module at the very top of the one-directional graph:

```
CricketUI        (HUD framework, widgets, debug overlays, view-models)   ← new
     │
CricketSim       (T20 Match Engine, GameMode)
     │
CricketGameplay  (ball physics / bowling / batting / replay components)
     │
CricketPhysics   (SI ball model, aero, bounce, replay & bowling/batting types)
```

It depends *down* on all three gameplay modules plus **UMG** and **Common UI**;
nothing depends up on it. The base game mode in `CricketSim` therefore stays unaware
of the UI — the HUD is attached by `ACricketHUDGameMode` (a `CricketUI` subclass that
sets `HUDClass`), selected as `GlobalDefaultGameMode` in `Config/DefaultEngine.ini`,
so the HUD appears in every scene with no level edits.

| Concern | Lives in |
|---|---|
| View-model contract (the data the UI shows) | `CricketHUDTypes.h` |
| Data binding (gameplay → view-model) | `UCricketHUDDataSource` |
| HUD framework (lifecycle, widgets, overlays) | `ACricketHUD` (`AHUD`) |
| Player-facing widgets (UMG) | `UCricketHUDLayout` + four panel widgets |
| Developer overlays (canvas) | `ACricketHUD::Draw*Overlay`, cvar-gated |
| Wiring | `ACricketHUDGameMode` + config |

---

## 3. UI architecture & data flow

```
 ┌───────────── authoritative simulation (owns ALL state) ─────────────┐
 │ UCricketMatchEngine   UCricketBallPhysicsComponent                   │
 │ UCricketBowlingComponent  UCricketBattingComponent  UCricketReplay…  │
 └───────────────┬─────────────────────────────────────────────────────┘
                 │  const reads only (ResolveSources + Build*)
                 ▼
        ┌───────────────────────┐   one immutable snapshot per frame
        │ UCricketHUDDataSource  │ ───────────────────────────────────►  FCricketHUDModel
        └───────────────────────┘                                          │
                 ▲ subscribes to OnBounce (tally)                          │ fan-out
                 │                                                         ▼
        ┌────────┴────────────────────────────────────────────────────────────────┐
        │ ACricketHUD (AHUD)                                                         │
        │   Tick:    DataSource->ResolveSources(); Model = BuildModel();            │
        │            Layout->UpdateFromModel(Model)         → UMG player HUD         │
        │   DrawHUD: Physics / Match / Dev canvas overlays  → developer overlays    │
        └───────────────────────────────────────────────────────────────────────────┘
```

The snapshot, `FCricketHUDModel`, is the entire contract. It aggregates five
view-models — `FCricketScoreboardVM`, `FCricketBattingVM`, `FCricketBowlingVM`,
`FCricketReplayVM`, `FCricketPhysicsVM` — each behind a `bHas*` relevance flag. A
panel whose source is absent simply hides itself, so the *same* HUD works in the
auto-playing match-runner scene (scoreboard only) and the interactive batting nets
scene (batting + bowling + replay + physics) with zero per-scene configuration.

The view-models also pre-format display strings (`"12.3"` overs, `"3.2-0-24-1"`
figures, `"0.25x"` speed) and convert SI to presentation units (km/h, rpm, degrees)
**once**, so widgets are pure layout and a future broadcast skin reuses the numbers.

---

## 4. Widget hierarchy (player-facing HUD)

Built on Common UI's `UCommonUserWidget` so the layer can grow toward full
presentation (input routing, activatable layers) without re-parenting. Every panel
is **asset-free**: it constructs its own titled, semi-transparent text layout in C++
(`RebuildWidget` → `BuildPanel`), matching the project's "drop an actor in, press
Play" ethos. A designer may still reskin any panel by subclassing it as a Widget
Blueprint — the `UpdateFromModel` data contract is unchanged, and the layout's panel
classes are `TSubclassOf` swap-in points.

```
UCricketHUDLayout (CanvasPanel, root)
 ├── UCricketScoreboardWidget  (top-centre)    score · wickets · overs · CRR/RRR · batters · bowler · ball count · chase/result
 ├── UCricketBowlingWidget     (top-left)      delivery type · line · length · swing · spin · predicted shape
 ├── UCricketBattingWidget     (bottom-left)   shot timing · footwork · direction · contact quality
 └── UCricketReplayWidget      (bottom-centre) transport · timeline scrubber · playback speed  (shown only during replay)

 base: UCricketHUDWidgetBase : UCommonUserWidget
   - UpdateFromModel(Model): IsRelevant ? RefreshFromModel : collapse
   - AddRow()/SetText(): programmatic titled text stack
```

Each panel maps one-to-one to a brief requirement:

| Panel | Shows (from the view-model) |
|---|---|
| Scoreboard | Score, wickets, overs, current & required run rate, striker & non-striker, current bowler + figures, ball count, target/runs required/balls remaining, match result |
| Batting | Shot timing verdict (+ ms error, colour-coded), footwork, shot direction, contact region + edge + quality + exit speed/angle, live swing phase & bat speed |
| Bowling | Delivery type (movement), line, length, swing amount, spin amount, pace/revs, physics-predicted length/line/free-flight swing + swing regime |
| Replay | Play/pause, `0.25x` speed, normalized timeline scrubber, frame/event counts, control hints |

---

## 5. Data-binding strategy

`UCricketHUDDataSource` is the only object that reaches into gameplay.

- **Discovery** (`ResolveSources`) is cheap and idempotent: it caches weak pointers
  to each system and only searches for missing ones. The Match Engine comes from the
  `ACricketMatchRunner`; the controllers and ball physics are found by component
  class on any actor, so the HUD is agnostic to which pawn carries them.
- **Snapshot** (`BuildModel`) calls the static `Build*` builders, each of which takes
  a `const` source and returns a value-type view-model. Purity here is what makes the
  binding unit-testable headlessly (§7).
- **Events**: the one retained piece of state is a rolling bounce tally; the data
  source subscribes to the ball's `OnBounce` delegate and records the last
  `FCricketBounceReport` for the physics overlay. It re-binds automatically if the
  live ball changes.

Because the builders are pure and static, the test feeds them a configured engine /
a driven replay clip / a released ball and asserts the exact fields the player would
see — no widgets, no world, no RHI.

---

## 6. Debug overlay design

Three optional developer overlays are drawn on the HUD canvas in `DrawHUD`, each
gated by its own console variable (mirroring the project's existing
`cricket.<system>.Debug.*` convention). They render from the **same** `FCricketHUDModel`
as the widgets, so an overlay can never disagree with the player HUD.

| Console variable | Default | Overlay | Contents |
|---|---|---|---|
| `cricket.UI.Widgets` | `1` | (player HUD master) | shows/hides the whole UMG layer |
| `cricket.UI.Debug.Physics` | `0` | **Physics Overlay** | ball speed, rpm, spin axis, seam normal & stability, drag/Magnus/swing force, reverse regime, Reynolds, last-bounce angle/apex/turn/seam/pace-retained |
| `cricket.UI.Debug.Match` | `0` | **Match State Overlay** | state-machine state, innings phase, score/overs/extras/CRR, chase maths, result |
| `cricket.UI.Debug.Dev` | `0` | **Developer HUD** | which sources resolved, ball summary, replay transport summary |

Toggle at runtime from the console, e.g. `cricket.UI.Debug.Physics 1`.

---

## 7. Testing strategy

Widgets are pure layout, so the tests target the part with logic — the binding
layer. Suite **`CricketSim.UI`** (`Source/CricketUI/Private/CricketHUDTests.cpp`),
headless under `-nullrhi`:

| Test | Asserts |
|---|---|
| `ScoreUpdates` | runs/wickets/overs strings, `ScoreText`, ball number and CRR mirror the engine after a scoring over |
| `WicketUpdates` | a dismissal bumps team wickets *and* credits the current bowler |
| `OverTransitions` | six legal balls roll `0.4 → 1.0`; the next ball reads `1.1` |
| `ReplayControls` | speed (`1.00x → 0.25x`), pause, and timeline cursor (advance + frame-step) flow into the VM |
| `PhysicsOverlayAccuracy` | the physics VM reproduces a released ball's speed (km/h), rpm, spin axis and seam normal exactly |

Run:

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$PWD/CricketSim.uproject" \
  -ExecCmds="Automation RunTests CricketSim.UI; Quit" -unattended -nullrhi -nosplash
```

Results land in `~/Library/Logs/Unreal Engine/CricketSimEditor/CricketSim.log`
(`Test Completed. Result={Success}`). All five pass.

---

## 8. Files

```
Source/CricketUI/
  CricketUI.Build.cs
  Public/
    CricketHUDTypes.h          view-model contract (FCricket*VM, FCricketHUDModel)
    CricketHUDDataSource.h     read-only binding: discovery + pure builders
    CricketHUD.h               AHUD framework + canvas overlays
    CricketHUDGameMode.h       HUDClass wiring (ACricketGameMode subclass)
    Widgets/
      CricketHUDWidgetBase.h   UCommonUserWidget base; programmatic titled panel
      CricketHUDLayout.h       root CanvasPanel; owns + places the four panels
      CricketScoreboardWidget.h / CricketBattingWidget.h /
      CricketBowlingWidget.h   / CricketReplayWidget.h
  Private/
    <each .cpp> + CricketUIModule.cpp + CricketHUDTests.cpp
```

Wiring: `CricketSim.uproject` (module + CommonUI plugin) and
`Config/DefaultEngine.ini` (`GlobalDefaultGameMode = CricketHUDGameMode`).

---

## 9. Scope & future work

In scope and delivered: gameplay HUD, batting/bowling/replay/match-information
panels, the three developer overlays, the data-driven view-model architecture, and
tests. Explicitly **out** of scope this milestone (per the brief): commentary,
broadcast presentation, career mode, multiplayer, monetization.

The architecture is built to scale into those later: the view-model already carries
clean, pre-formatted presentation data, so a broadcast skin is new widgets over the
same `FCricketHUDModel`; the Common UI base classes leave room for activatable HUD
layers and gamepad routing without touching the binding layer.
