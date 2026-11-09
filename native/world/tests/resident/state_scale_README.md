# Actual 100/500 common-boundary checks

The completed runs are preserved in `state_scale_evidence.json`. They used exact Git-object headers at `fcfc252ca2808b1adda8a91a1fe2d9a4d741dcc5` and the frozen `libhanmo_resident.a` SHA256 `2c1eb52ebfbc0ec707849a22541433d541e54d057aa76239251d4ae2b7013e05`.

- 100 actors: 16 real committed boundaries, 120 full cached receipts, 50 original/restored army-member queries, two actual armies.
- 500 actors: 4 real committed boundaries, 6 full cached receipts; this shorter trip has no formed army.
- Both exited zero. Common, social and estate records retained exact values, types, arrays and dictionary field order; their typed checkpoint roundtrips passed.
- Actual army wallets/inventories, ownership, spendable money, carried weight and member/leader resolution were checked through A common reads bound to C's real `MilitaryMembership`.
- Cached receipts were not removed. The test used the central `ResidentResultCodec`, including nested military details, and required successful non-null immutable shells through restore.

This is a cold domain-boundary test. It does not execute `advance`, rerun the GD oracle, measure performance, replace the complete world validator, claim B/C payload coverage or add admission rules. Root action/fact order metadata is checked, while their complete domain payload composition remains centrally owned.

The test executable takes an expanded catalog path, the expected actor count, and each actual state path. Link it against matching frozen `hanmo_resident`, `hanmo_world` and `hanmo_flecs` archives and exact frozen include/src headers. Do not mix live working-tree headers with an older archive. Inputs are read in place and their manifest hashes are retained in the evidence; no game resource copies are required.

No additional execution is needed merely to preserve these already-terminal results. A later implementation change affecting this contract can justify a new narrowly scoped run with a new evidence identity.
