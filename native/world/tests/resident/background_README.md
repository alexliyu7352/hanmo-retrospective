# Resident lane C: supported profile and evidence

The first-profile sections below are pinned to `aa8649d` / `5139e6b`. The separate `work/resident-c-post-arrival` branch adds the continuation documented at the end; it does not relabel earlier evidence.

These tests cover C-owned typed military, encounter, interaction, conditions, and read-only family roots. They do not establish complete resident-world, Bridge, Main, or pressure-gate acceptance.

## Supported actual paths

- Ordinary military office acceptance, campaign acceptance, garrison/army rations, delayed army formation with real shared resource reservations and transfers
- Typed NPC military proposals through office → campaign → formation → local movement
- One encounter installation entry, player talk/leave, real dialogue say/choice/branch handling, resolved encounter retention of 128 by created tick then lexical ID
- Flag, party, actor-location, task-status and relation conditions via real owner read ports; entry events and queued-event reconciliation
- Explicit domain codecs and binary checkpoints preserve table, array and record-local dictionary order

C does not own the clock, dispatcher, action head release, common-resource journal, or final world publication. Common resource/actor writes must occur in the central candidate. Ordinary NPC application failures require rolling back both the common savepoint and C domain state; unsupported/internal errors require discarding the entire world candidate.

## Explicit capability boundaries

There is no tick-number ceiling. A valid next `campaign_depart` reaches `UnsupportedNativeCapability`. Real military road contacts, campaign marching/results/return, battle/siege/custody/occupation/training, completed waiting-dialogue business actions, and nonempty family history are not claimed supported. Each applicable phase checks actual domain objects/requests before returning empty-domain success. Known malformed military terms retain their original GD errors.

Unregistered command parameter fields are rejected as `UnsupportedNativeCapability` before mutation and during seed admission. This bounded-profile restriction was accepted by root and the integrator. It is not arbitrary-extra-parameter GD equivalence; unknown values are neither silently dropped nor stored as opaque hot JSON.

## Narrow build

Run `background_build.sh REPO SOURCE_ROOT LOCKED_JSON_INCLUDE ORACLE1000_DIRECTORY` with a unified source tree containing the frozen A/B/C/D dependencies. In C's isolated worktree, `.local/test-src` is a small source-only overlay assembled from the existing lane files, avoiding duplicate `#pragma once` identities from mixed worktrees. It does not copy game assets or oracle resources. The script rebuilds objects when dependent headers change.

- `background_domains`: all eight actual 1000-person oracle boundaries, exact C values/types/dictionary orders, domain validators, binary roundtrip, army:2586/action:2916, float32 anchor edge, capability boundary
- `background_runtime`: original GD office/form quotes; real office completion, military supply, reserved formation and resulting shared money/lots; repeated-finish idempotence; encounter leave/dialogue progression; 160-install trimming; activity-owner/provider semantics
- `background_recovery CATALOG STATE...`: fresh and restored physical-state C-domain roundtrips, including dialogue snapshots; not native end-to-end restore-and-continue

The runtime test retains the independent review's malformed-state regressions: simultaneously emptying formation goods in action/fact, request.money mismatch, wallet escrow mismatch, reserved quantity mismatch, and supply organization mismatch. Admission/finish must reject with original codes before any ID/resource/domain mutation. It also checks the empty-ID errors and explicit unsupported extra-field restriction.

The formation test is intentionally a domain slice: it runs real military supply, then uses the oracle's documented post-estate allocation ID to isolate formation. It must never be cited as proof of whole-world ID sequencing or full half-day replay. Those are the central integration gate's responsibility.

## Actual central NPC atomicity regression

`background_central_atomicity.cpp` links the real central resident library and directly invokes `try_npc_command`, the function used by the phase tail. It validates the original oracle slice first, then injects one deliberately illegal prospective action-ID collision with an owned typed payload solely so complete exports remain comparable. A separate direct call of the same central apply verifies that the collision occurs after actual lot reservations and mobilizing-unit writes. The real NPC transaction must roll those writes back, preserving complete exported values/types/order, typed checkpoint bytes, next ID and all common resource read versions. The following real commander `move_local` must still be adopted in the same candidate.

This regression passed against the archived diagnostic libraries reported as build commit `1c5a6d066f48c7c02b94c2e0822ada3ee4622d22`. The primary immutable artifact identities are in `background_central_atomicity_result.json`. The local archive snapshot and executable are retained under `.local/central-atomicity-evidence-20261005/`; no compiled files are committed. It does not establish a complete pressure-gate result or claim naturally duplicated IDs are legal.

## Separate next-profile continuation

The isolated post-arrival branch adds real `campaign_depart`, C-owned `CampaignArrivalFact`, exact `army_at_settlement`, marching/operating state and the original contact eligibility/hostility/geometric-hit boundary. It binds `decode_military_catalog(data, scenario_id)` to the actual source scenario. Model-enabled positive March execution and actual hostile military contact are explicit unsupported errors; first-profile binary/source artifacts are unchanged.

`background_departure` uses the independently captured actual tick4/5/6/7 states. It compares the real B army quote, actual army movement/fatigue/wallet debit after real C ration consumption, and the original rider arrival fact `fact:3664`. It also checks the arrival checkpoint/reference validator and synthetic pure-contact eligibility/hostility cases. Its one manual global-ID selection isolates the campaign phase at the original observed allocation point; this is not full-world sequencing proof. Central still owns full-chain integration, combined C+B NPC rollback and next-profile checkpoint-version changes.
