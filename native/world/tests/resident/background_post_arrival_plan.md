# C next-profile closure: concrete boundary plan

Status: original rule map and two real GD next-step captures only. Current first-trip native profile is unchanged. This work is isolated on `work/resident-c-post-arrival`; shared request/receipt changes require the integrator's freeze before implementation.

## Observed original continuation

Input is the exact frozen 1000-person tick-4 state `e401369…`; strict parse and original `restore_with_catalog` preserved every input byte on re-export. No actor, resource, policy, seed, or map was edited.

- Tick 5 completes rider formation `action:2916` into `army:2925`. The commander accepts the real `campaign_depart` proposal for `campaign:2072` and starts `action:3168` / `journey:3169` toward `city.west`. Quote: mode army, duration 8, cost 64, budget 1636, load 59, payer `army_wallet:army:2586`. Rider physically moves to `east.entrance`.
- Tick 6 advances commander `(830,290) → (694,290)`, leaves credit 4 and remaining 7. Rider's `campaign_depart` targets its current `village.east`, so `Campaign.at_settlement` is already true: the campaign becomes marching but no journey/action is allocated.
- Both armies belong to `org.blackwater`; real military contacts are empty. The fixed `alpha_life` scenario has no military-condition-model field, so `March.enabled` is false. Enabling a different scenario must not silently skip its execution receipts.
- Consequence read from the formal rule, not yet captured: the next campaign step must mark the already-present rider operating and append a `campaign_arrival` fact. The commander's eventual arrival should reach the real siege-start capability boundary; no tick-number ceiling belongs in this extension.

The machine index, two command traces and two compressed full successor states are in `background_post_arrival_oracle/`. The original tick-4 state and catalog remain referenced by existing hashes, without duplicate resource copies. Timings in the GD trace are diagnostic only.

## Minimal interface/delta for central freeze

1. Move the existing C-owned `MilitaryJourneyRequest { army_id, destination_settlement_id }` into the common include dependency used by `MilitaryEffect`; add an optional `journey_request` to the typed effect and `campaign_id` to the departure quote. D marshals the original optional details. Do not add a generic command/JSON effect.
2. C implements original departure preview/apply and permits actual marching/operating campaign states. A typed `CampaignArrivalFact { id, campaign_id, army_id, actor_id, tick, stronghold_id }` is owned by C and registered once through A. Add its domain codec, reference checks and original ID allocation (`fact`, not `military`).
3. Central coordinates the optional request in the same candidate: if `at_settlement` is already true, do nothing; otherwise reuse B's exact plan plus existing begin/replace-plan path. Failure must discard the campaign status change. The NPC savepoint must back up both C military and B travel when a military command can start/replace travel; common ID/actor/resource writes stay in A's journal.
4. C replaces the blanket road-army rejection with the real eligibility/hostility boundary. Read active, fit, unengaged armies and actual commander motion; same-organization or otherwise non-hostile pairs prove an empty contact set. Actual hostile intersection must either be implemented with its full original contact/battle transaction or fail atomically before state publication. Never filter an army simply to make the phase empty.
5. Bind the original March scenario gate. The current Alpha model needs no march-condition receipt; a model-enabled source remains explicitly unsupported until that actual execution/receipt closure is implemented. B's existing movement still pays army-wallet travel cost and A/C still consume actual army rations.

## Bounded verification before enabling

- Reuse current exact first-trip and central atomicity gates without changing their source identities.
- Extend original oracle by the next real rider-arrival step before freezing arrival-fact semantics; then replay the captured tick-4→5→6→next boundary in native with full values/types/order and IDs.
- Assert commander and rider quotes, member action occupation, army food and wallet deltas, no same-city phantom action, and checkpoint restore-and-continue.
- Inject a B plan/begin failure after C changes marching state: whole candidate or actual NPC savepoint must restore military + travel + common state and allow the next ordinary proposal.
- Exercise the real unsupported hostile-contact / model-enabled boundary and verify complete atomic rollback. This is a capability boundary, not a successful empty hook.

No production implementation is included in this planning/oracle milestone.
