# CricketSim — Audio System

> Sound is the echo of the simulation, never its cause. Every thock, edge, appeal and
> roar is *triggered by* a physics or match event and shaped by that event's data —
> and nothing a speaker does ever changes a run, a wicket or a trajectory.

Design record for the `CricketAudio` module: the reactive architecture, the event
system, the data models, routing, crowd logic, debug tooling and tests. Read
`Docs/ARCHITECTURE.md` (module graph, physics-first rule) and `Docs/UI_HUD_SYSTEM.md`
(the sibling presentation layer it mirrors) first.

---

## 1. The one hard rule

**Audio reacts to the simulation; it never determines an outcome.** The data flows
strictly one way — gameplay/physics → audio — and the audio module depends *down* on
the gameplay modules, never the reverse, so a feedback path is impossible to compile.
The only randomness audio uses (picking which take of a sound to play) selects a
*sample*, never a result.

---

## 2. Module placement

```
CricketAudio     (reactive sound layer)        CricketUI     (HUD)
        └───────────────┬───────────────────────────┘
                  CricketSim  (Match Engine)
                        │
                  CricketGameplay (ball/bowling/batting/fielder/replay components)
                        │
                  CricketPhysics  (SI reports: bat impact, bounce, release, fielding)
```

`CricketAudio` is a runtime module at the top of the graph, a sibling of `CricketUI`.
It uses only engine-portable audio (`UGameplayStatics` / `UAudioComponent`), so it
builds and behaves identically on macOS and Windows.

---

## 3. Audio architecture & event flow

The pipeline turns a typed gameplay event into a played sound through four stages,
the first two of which are pure data and unit-tested:

```
 gameplay/physics event
   (OnBatImpact, OnBounce, OnWicket, OnBallApplied, fielder state, replay rate, …)
            │
            ▼  FCricketAudioSelector / Crowd & Environment controllers   ← pure, testable
   FCricketAudioCue            { event, category, priority, gain, pitch, variation, loop }
            │
            ▼  + world location / source
   FCricketAudioRequest        { cue, worldLocationCm, bSpatial, source }
            │
            ▼  UCricketAudioSubsystem  (the Audio Manager)
   route (bus gain + voice budget + priority) → pick a take from the data bank → play
            │                                                                   │
            ▼ spatialised at the source (ball/player/fielder)                   ▼ 2D (crowd/ambience)
        UAudioComponent                                                     UAudioComponent
```

| Deliverable | Where |
|---|---|
| **Audio Manager** | `UCricketAudioSubsystem` (a `UTickableWorldSubsystem` — auto-created, no level placement) |
| **Event-Based Audio System** | the subsystem's source discovery + delegate bindings + fielder polling |
| **Audio Data Assets** | `UCricketAudioBankAsset` (event → `FCricketSoundSet`) |
| **Sound Routing System** | `FCricketAudioRouting` (master/bus gain, voice budget, priority preemption) |
| **Crowd Audio Controller** | `FCricketCrowdController` (excitement model) |
| **Environmental Audio Controller** | `FCricketEnvironmentController` (stadium/day/night beds) |
| **Decision core** | `FCricketAudioSelector` (sim data → cue) |

---

## 4. Event system — what drives what

The subsystem discovers the live systems each tick (like the HUD's data source) and
binds once. Single-instance sources are event-driven; fielders are polled (a dynamic
delegate hides which fielder fired, and polling also yields the fielder's location and
catch difficulty for free).

| Audio | Source signal | Data used to shape it |
|---|---|---|
| Ball release | `UCricketBowlingComponent::OnDelivery` | release speed → gain |
| Ball flight (loop) | `UCricketBallPhysicsComponent::IsBallInFlight` | speed → gain & pitch; attached to the ball |
| Pitch bounce | `OnBounce(FCricketBounceReport)` | approach speed → gain; pace retained → pitch |
| Bat / edge impact | `OnBatImpact(FCricketBatImpactReport)` | **region, edge factor, quality, exit speed, launch angle** → event + gain + pitch |
| Pad / stump / direct hit | `UCricketMatchEngine::OnWicket(ECricketDismissal)` | dismissal type → which impact |
| Four / six / run call | `OnBallApplied(FCricketDeliveryOutcome)` | boundary + runs |
| Catch / dive / pickup / throw | `UCricketFielderComponent` state + `GetIntercept().Difficulty` | difficulty grades a catch into a dive |
| Appeal / celebration / effort | wicket, big strike, dive | — |
| Crowd reactions | four/six/wicket/edge → `FCricketCrowdController` | excitement weight |
| Replay slow-mo | `UCricketReplayComponent` rate/paused | rate → global SFX pitch scale |

### Bat contact — the headline example

A single `FCricketBatImpactReport` produces five audibly different cues, decided in
priority order (edge → block → loft → middle):

| Contact | Detected from | Cue | Character |
|---|---|---|---|
| Thin edge | `ThinEdge` region / edge factor > 0.7 | `BatThinEdge` | quiet, **brighter pitch** (a tick) |
| Thick edge | `ThickEdge`/toe/top region | `BatThickEdge` | meatier, duller |
| Defensive block | clean but low exit speed (< 9 m/s) | `BatDefensiveBlock` | soft, low pitch |
| Lofted strike | clean, launch > 22°, exit > 16 m/s | `BatLoftedStrike` | full, bright |
| Sweet spot | clean middle (default) | `BatSweetSpot` | loud crisp "thock" |

Loudness rides the **impact speed**; the thin edge is provably quieter and brighter
than a middled drive (asserted in tests). The geometry already differs, so the sound
differs — audio reads it, never invents it.

---

## 5. Data models

- **`UCricketAudioBankAsset`** — the entire palette as data. Rows of
  `event → FCricketSoundSet { Variations[], BaseVolume, BasePitch, Attenuation,
  Concurrency }`. A sound designer authors one and assigns it (`SetBank`). The code
  references *events*, never assets, so banks are swappable (stadium pack, training
  pack). **The system is fully functional with an empty/partial bank**: a missing
  event simply plays nothing and is reported by the overlay as `no-asset` — gameplay
  never depends on audio existing.
- **`FCricketAudioCue`** — the abstract decision (event, category, priority, gain,
  pitch, variation, loop). Asset-free, so it is unit-testable.
- **`FCricketAudioRequest`** — a cue plus where/who (world location, `bSpatial`,
  source name).

---

## 6. Sound routing design

`FCricketAudioRouting` is policy-as-data:

- **`ResolveGain(cue)`** = `MasterGain × CategoryGain[category] × cue.Gain` (a bus at
  gain 0 mutes its whole category).
- **`ShouldPlay(cue, activeInCategory)`** — under the per-category voice budget,
  always; over budget, only `High`/`Critical` cues preempt; `Critical` (stumps,
  wickets) is never dropped.

Spatialisation follows the category: Ball/Bat/Impact/Fielding/Player play at their
world source with attenuation; Crowd/Environment play 2D. During replay, a global
**pitch scale** (= playback rate, ~0 when paused) is applied to live spatial SFX, so
a 0.25× slow-motion replay drops the world's sounds to a quarter pitch while the
crowd bed stays steady.

---

## 7. Crowd audio design (MVP)

Deliberately simple: `FCricketCrowdController` holds one **excitement** value in
[0,1]. Big moments bump it (six +0.6, wicket +0.7, four +0.4, close chance +0.3); it
**decays** over time. Excitement (a) swells the ambient bed gain
(`BaseBedGain + boost × excitement`) so the stadium lifts after a big moment, and
(b) gates the one-shot reactions (four / six / wicket / close chance). No behaviour
model, no per-spectator logic — just a believable rise-and-settle.

Environment: `FCricketEnvironmentController` layers a constant stadium bed under a
day **or** night ambience (reusing the stadium system's `ECricketTimeOfDay`), swapped
by `SetTimeOfDay`.

---

## 8. Debug tooling

`cricket.Audio.Debug 1` draws an on-screen overlay (the project's cvar convention):

- master gain, replay pitch scale, total active voices, crowd excitement & bed gain;
- **active voices per category** (Ball/Bat/Impact/Fielding/Player/Crowd/Environment);
- a rolling log of recent events — **event · category · priority · source · gain ·
  result**, where result is `PLAYED` / `no-asset` / `dropped` (voice budget) /
  `invalid`, colour-coded.

This visualises exactly what the brief asks: active sound events, their sources,
categories and priorities — and *why* a sound did or didn't play.

---

## 9. Testing strategy

The decision core is pure, so it is fully covered headlessly. Suite
**`CricketSim.Audio`** (`Source/CricketAudio/Private/CricketAudioTests.cpp`):

| Test | Asserts |
|---|---|
| `BatImpacts` | sweet spot / lofted / block are distinct cues; harder = louder; no contact = no cue |
| `Edges` | thin vs thick are different events; a thin edge is quieter **and** brighter than the middle |
| `Catches` | fielder states → catch/pickup/throw; difficulty grades a catch into a dive; idle is silent |
| `Wickets` | bowled→stump (Critical), LBW→pad, run-out→direct hit, caught→catch, not-out→silent |
| `CrowdReactions` | a six excites more than a four; the bed swells then decays; the bed isn't a "reaction"; reactions are 2D |
| `ReplayPlayback` | slow-mo scales pitch by the rate, paused ≈ frozen; routing resolves gain, gates by priority, mutes a zeroed bus |

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$PWD/CricketSim.uproject" \
  -ExecCmds="Automation RunTests CricketSim.Audio; Quit" -unattended -nullrhi -nosplash
```

All six pass. Playback itself (the subsystem) is a thin wrapper over engine audio and
is exercised in-editor, not unit-tested.

---

## 10. Files

```
Source/CricketAudio/
  CricketAudio.Build.cs
  Public/
    CricketAudioTypes.h            events / categories / priorities / cue / request + helpers
    CricketAudioBankAsset.h        data-driven sound bank (event -> sound set)
    CricketAudioSelector.h         pure sim-data -> cue decisions
    CricketAudioRouting.h          bus gain + voice budget + priority gate
    CricketCrowdController.h       excitement model + bed/reaction cues
    CricketEnvironmentController.h stadium/day/night ambience selection
    CricketAudioSubsystem.h        the Audio Manager (discovery, binding, playback, debug)
  Private/
    <each .cpp> + CricketAudioModule.cpp + CricketAudioTests.cpp
```

Wiring: `CricketSim.uproject` (module entry). The subsystem auto-creates in game/PIE
worlds; a designer assigns a `UCricketAudioBankAsset` to give it voice.

---

## 11. Scope

Delivered: ball/bat/fielding/player/crowd/environment audio, the manager + event
system, data-driven bank, routing, crowd & environment controllers, spatialisation,
replay slow-motion, debug overlay and tests. Out of scope (per the brief): commentary,
career mode, multiplayer, advanced broadcast presentation. The architecture scales
toward those — new events extend the enum and bank; the routing and crowd policy are
already data — without disturbing the one-way, outcome-safe contract.
