# A lane narrow state gates

These tests exercise the real typed common/social/estate implementations. They are not the complete resident journey acceptance gate and do not run a world-phase matrix.

Run from any directory using absolute paths:

```sh
python native/world/tests/resident/state_checks.py \
  --json-root /path/to/locked/json \
  --oracle /path/to/resident-journey-oracle-baseline-20261004/journey1000-20261005 \
  --build /tmp/hanmo-state-checks
```

`--json-root` is the existing, verified dependency directory containing `single_include`; no download is performed. The oracle files are read in place, not copied or rewritten. Assertions remain enabled in optimized test builds.

Coverage:

- `state_rng`: 952 exact vectors from the fixed Godot 4.7.2 executable, including 11 seeded states, rejection sampling, signed32 extremes/reversed/equal bounds, stream isolation, clone/checkpoint continuation and savepoint rollback. `state_rng_README.md` records source and engine identities.
- `state_resources`: owner-order lot splits and transfers, original lot/ID/revision behavior, zero money reservations, failed-recipe atomicity, unrelated release versions, clone independence.
- `state_savepoint`: touched-only NPC undo for actor/action/resource/fact/role/ID/read-version changes; repeated action removal/reinsertion restores original positions and checkpoint bytes. Nested/stale tokens fail. Direct mutable `state()` writes are outside this journal; dispatcher clock/request/history/result writes must remain outside NPC savepoints.
- `state_membership`: active army priority, prisoner exclusion, party following, absent IDs, clone rebind, explicit missing-stream rejection with no mutation.
- `state_seed`: actual 1000 actors/1057 inventories, independent common validation, exact exported common values and dictionary order, typed checkpoint, no restored execution epoch, damaged counters/types/weapon/date rejection.
- `state_receipts`: native successful typed-cache envelope admission rejects null, failed, malformed and counter-inconsistent receipt shells while preserving request sequence gaps and cache trimming. Original GD Schema itself checked field types and the global instance upper bound; no new cache-key/history matching rule is claimed or added here.
- `state_social`: exact first social phase from the real 1000-person oracle, including all 163 emitted events, relations, memories and scale-actor bags. Prior phases' next ID is explicitly supplied as a phase fixture; this is not a full advance result. Replay and typed checkpoint are checked.
- `state_estate`: exact first production state/fact/inventory, replay, real storage-full rejection, withdraw/fund behavior, transfer-provenance rejection, typed checkpoint.
- `state_record_roundtrip`: A-owned records at all eight actual 1000-person journey boundaries, including final social/estate history. The fixture deliberately excludes central cached receipts and B/C action/fact payloads; it never claims complete-world equality.

Current finite-profile capability boundaries are explicit failures: RNG endpoints outside signed32 or an unverified RNG backend, dynamic actor provenance, inherited appearance provenance, public estate transfer sources, estate build/muster/return, and nonempty estate detachments. The frozen journey inputs do not require these capabilities. Five RNG states remain unchanged in all four recovered oracle cases and in the 1000-person oracle.

Root checkpoint field order and action/fact record order must be applied by the central composer after all owner payloads are assembled. Common export only owns its typed fields; it is not a complete world export. `CommonResultCodec` supplies the single central receipt schema for decode/export/checkpoint, including nonempty histories.
