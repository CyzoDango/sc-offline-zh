# zh.13: current-pilot controls and native flight investigation

This is an implementation status document, not a claim of in-game acceptance.
No game was launched for this work. Runtime verification belongs to the user.

## Implemented

- Shared M-menu settings popups are centered and constrained to the native menu
  viewport. They no longer depend on the mouse's opening position.
- The vehicle affiliation dropdown contains enemy, neutral/civilian and friendly.
  UI indices 0/1/2 still map to backend relation IDs 1/2/3; default is friendly.
  Existing explicitly labeled original-template fallback buttons are retained for
  classes without a generated fleet recipe. Such fallback retains its own faction.
- Current-ship actions require an unambiguous named primary Pilot seat whose
  decoded occupant is the local player. CoPilot, Co_Pilot, Co-Pilot, secondary and
  auxiliary seats do not count. Generic seat priority alone does not count.
- This lookup does not change the Crew page's pinned target or scratch seat list.
- Heat, power-supply and AI-ammunition overrides remain scene-wide rules. They
  are gated by current-pilot confirmation and restore owned snapshots on leaving
  the seat or changing ship/seat. They have not become per-ship component edits.
- Ship ammunition uses the existing game.creative service, with ownership taken
  only from an initially disabled state. A preexisting external toggle is not
  taken over. Restore failures retain ownership for retry.
- The current-ship power action addresses the confirmed pilot dashboard directly.
  It does not use the Crew target and does not synthesize an R key fallback.
- Crew settings provide a real request to spawn an NPC into an empty, uniquely
  identified primary pilot seat. Existing crew jobs check the actual occupant and
  handle seating failures. This action does not implement destination navigation.

## Not implemented: adjustable normal-flight speed in metres per second

The current SDK exposes creative fly/noclip speed, but not a ship IFCS velocity
limit/acceleration service. Noclip speed is not a substitute for ship propulsion.

Static inspection of the user's local game binary and extracted DataCore records
found these genuine parameters:

| Identifier | Evidence/meaning | Why it is insufficient alone |
| --- | --- | --- |
| `v_ifcs2.new_model_thrusters_linear_overwrite_time_to_full_speed` | Native help: overwrites thruster time to full speed | Unit is time, not m/s; depends on native acceleration/model |
| `v_ifcs2.new_model_linear_time_goal_overwrite` | Native help: overwrites time to max speed when positive | Also time, not a per-ship m/s value |
| `v_ifcs2.new_model_thrusters_linear_jerk_overwriteTimeToFullAcceleration` | Native help: time to full acceleration in seconds | Controls ramp, not terminal speed |
| `v_ifcs2.new_model_rotation_modifier_override_linear_speed` | Rotation-modifier override, adjacent boolean control | Must not be written as a requested m/s speed |
| `v_ifcs2.limiters_linear_velocity_type` | Enum selecting limiter geometry | Not a speed value |
| `p_max_velocity` | Native help: clamps physicalized objects' velocities | Global physics ceiling does not raise a ship's IFCS limit |
| `Controller_Flight_RSI_Bengal` / `Components[4].maxSpeed` | Extracted record contains 900.0 | A static record value does not establish a writable live controller or new-model behavior |

None of these candidates were written to the game. No unconnected speed slider
or coordinate-teleport substitute is shipped. Stock flight speed and acceleration
remain unchanged by this release.

Proposed behavior after a verified native interface is available:

1. Default mode leaves every original ship value alone.
2. Custom mode adjusts the native maximum forward speed; it does not press the
   throttle or instantly assign a velocity. Pilot steering and braking remain.
3. A candidate full-throttle ramp uses a one-second acceleration build-up, then
   constant acceleration: `T = 8 + 0.0025 * V`, `A = V / (T - 0.5)` for a start
   from rest in vacuum. At 1,000/5,000/10,000 m/s this gives approximately
   10.5/20.5/33 seconds and 100/250/308 m/s^2. These are design candidates only,
   not applied values or promised game performance.
4. Ship change, leaving the primary seat, unloading or manual default restore
   must restore native values, with readback. Native atmosphere, master-mode,
   physics clamps and braking behavior still need to be reconciled.

Next technical dependency: identify and validate the live IFCS controller and
its per-entity limit/acceleration interfaces, then compare HUD velocity with
native telemetry during a user-controlled game test.

## Not implemented: NPC destination navigation

An NPC seated in a Pilot seat is not evidence that flight AI has accepted control.
The existing seat/spawn API has no exposed destination/path/flight-task call.
The remaining dependency is a native vehicle AI controller, goal/task submission
and completion/failure feedback. Planetary routing, quantum travel and obstacle
avoidance are separate capabilities. No destination button is shipped without a
connected navigation implementation.

## Verification scope

- Native Release build and no-window policy/restore checks.
- Offscreen rendering of real menu draw functions with explicitly isolated test
  fixtures; no HWND, swap chain, OS input, injected game or running game reads.
- Popup geometry checks at 100%, 150% and 200% DPI.
- In-game pilot detection, restoration and NPC seating remain pending user tests.
