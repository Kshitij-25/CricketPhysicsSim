# Cricket AI System

The cricket-intelligence layer: five AIs — **Batter, Bowler, Captain, Fielding,
Team Strategy** — capable of playing complete T20 matches. It lives in the
`CricketAI` runtime module and is a **driver** of the existing gameplay/physics
systems, never a parallel simulation.

> **The one rule: the AI never cheats.** Every decision is expressed as the *same
> intent a human supplies* — a bowling intent fed to `UCricketBowlingComponent`, a
> shot + footwork fed to `UCricketBattingComponent`, a chaser nomination fed to
> `UCricketFielderComponent`, a `SetBowler` on the Match Engine. The *outcome* then
> emerges from the identical physics and laws the player is subject to. Difficulty
> tunes **decision quality**, not reaction speed, omniscience, or the result.

## Where it sits

```
CricketAI -> CricketSim -> CricketGameplay -> CricketPhysics -> Engine/Core
```

It reads the Match Engine for awareness and drives the gameplay controllers. UI and
Audio remain pure consumers and never depend on AI. The reasoning cores are pure and
UWorld-free, so the whole layer is deterministic and unit-tested headlessly.

## Architecture

### 1. Data models (`Public/`)
| Type | Purpose |
|---|---|
| `FCricketAIDifficultyParams` | The four tiers (Easy/Medium/Hard/Simulation) resolved to quality dials: selection temperature, execution noise, situational awareness, mistake rate. |
| `FCricketBattingTendencies` / `FCricketBowlingTendencies` | Per-player cricketing personality (aggression, technique, weaknesses; pace, control, movement, death skill). |
| `FCricketTacticalPreferences` / `FCricketTeamStrategy` | Captaincy leanings + phase-by-phase approach and difficulty. |
| `FCricketAIPlayerProfile` | A player's full AI identity. `Derive(FCricketPlayer)` builds one from the existing roster rating, so the shipped India/Australia teams get rich AI for free. |
| `UCricketAIProfileDataAsset` | Optional authored personalities + strategy per team. |
| `FCricketMatchSituation` | **Match Awareness System** — one read-only snapshot (phase, score, rates, pressure) built from the Match Engine. The AI sees exactly the public scoreboard. |

### 2. Decision brains (pure static cores)
The "intelligence". Each scores cricket options and selects one through the shared,
difficulty-aware selector; each returns a concrete intent **plus a decision trace**.

- **`FCricketTacticalEvaluator`** — the Tactical Evaluation System: shared math
  (batting aggression target, bowling pressure) and the `SelectOption` core where a
  hot temperature spreads the softmax (a weaker AI misses the best play) and
  `MistakeRate` injects lapses.
- **`FCricketBatterBrain`** — weighs **Leave / Defend / Rotate / Attack / Boundary**
  from phase aggression, the required rate, the matchup and the field; maps the choice
  to shot + footwork + aim + power.
- **`FCricketBowlerBrain`** — picks a delivery (line, length, movement, pace, swing,
  spin) from a style-specific repertoire on phase fit, the batter's weakness, control
  risk and unpredictability; sets dismissals up across balls via a short memory.
- **`FCricketCaptainBrain`** — bowling changes (phase- and form-aware, legal under the
  over caps), field placement (legal under the powerplay restriction), powerplay
  management.
- **`FCricketFieldingBrain`** — chaser selection from physics-true intercepts, the
  dive/commit decision, and throw selection (run-out vs the safe ball to the keeper).

### 3. Controller components (drive the existing gameplay)
| Component | Drives |
|---|---|
| `UCricketBatterAIController` | `UCricketBattingComponent` — reads the live ball, decides, pushes the shot, and times the swing/defence from the ball itself. |
| `UCricketBowlerAIController` | `UCricketBowlingComponent` — pushes the chosen line/length/pace/swing/spin and `BowlNow()`; execution scatter flows through the same `HumanScatter` channel a human is subject to. |
| `UCricketCaptainAIController` | The Match Engine — nominates each over's bowler via `SetBowler`, sets fields. |
| `UCricketFieldingAICoordinator` | The `UCricketFielderComponent`s — designates the active chaser each tick. |

### 4. Contest model + match simulator (the believability engine)
The live game resolves each ball through the full physics rollout (needs a ticking
world). For the **headless, deterministic** path:

- **`FCricketContestModel`** stands in for the rollout: given the two brains'
  decisions and the players' abilities it produces the same raw facts
  (`FCricketBallResult`) the physics would — modelling the same *factors* (length
  suitability, the matchup, timing risk, the field), not a flat dice.
- **`FCricketAIMatchSimulator`** wires the brains to the **real** Match Engine: the
  captain rotates bowlers, the bowler brain chooses each ball, the batter brain
  responds, the contest resolves it, the **existing** `FCricketOutcomeInterpreter`
  classifies it and the engine applies the laws. It emits telemetry (run rate, wickets,
  bowling patterns, shot distribution, dismissal modes).

  This is exactly the path `ACricketMatchRunner` needs to replace its placeholder
  ball generator with — same interpreter, same engine, now fed by cricket intelligence.

### 5. Debug tooling
`UCricketAIDebugComponent` + console variable `cricket.AI.Debug 1` renders the live
reasoning of any wired controller: current state and tactical intent, the scored
options behind each decision (chosen one + selection probability), the fielding
chaser decision, and the batter's shot-selection logic.

## Difficulty — quality, not cheating

| Tier | Temperature | Exec. noise | Awareness | Mistakes |
|---|---|---|---|---|
| Easy | 1.30 | 0.55 | 0.35 | 0.22 |
| Medium | 0.65 | 0.30 | 0.65 | 0.10 |
| Hard | 0.28 | 0.14 | 0.88 | 0.035 |
| Simulation | 0.08 | 0.05 | 1.00 | 0.00 |

A weaker AI picks worse options, reads the situation less, and executes less
precisely (through legitimate input channels). It never reacts faster than physics
allows or sees hidden information.

## Testing

Suite **`CricketSim.AI`** (10 tests, all pass). Run:

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "$PWD/CricketSim.uproject" \
  -ExecCmds="Automation RunTests CricketSim.AI; Quit" -unattended -nullrhi -nosplash
```

Validates the difficulty model and selector, match awareness, each brain's
behaviour (a steep RRR forces aggression; a disciplined batter rarely attacks the
wide ball; yorkers spike at the death; an aware bowler targets the weakness; the
captain never picks an illegal bowler) and the **full match end-to-end**: a complete
T20 plays to a result with believable numbers — averaged first innings ≈ **140–160
runs at ~7.3 RPO for ~7 wickets**, good length the stock ball, positive intents
outweighing leaves — and the simulation is deterministic given its seed.
