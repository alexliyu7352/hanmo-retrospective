# terrain_march_v1 actual-Kernel seam gate

The fixture composes the existing military author generator through the production content pipeline, then stages one lightweight project. It adds explicit author profiles, two units in army A (one starting at fatigue 99), army B, two independent training camps, and normal/crawl/closure maps, a forest–road boundary map, and one real waypoint settlement. A one-round authored battle objective permits real post-battle action cleanup. Existing author packs are not edited. The two known tiny PNGs are test presentation only, not recovered art acceptance.

## Reproduce

Use a new output directory for the first preparation. Later `--refresh` only targets this helper's marked project and preserves its evidence directories. `--fixture-art` must contain the original hash-verified `pixel.png` and `portrait.png` from `tests/fixtures/content_packs/compose/new_packs/base/assets/art`.

```sh
python tests/fixtures/military_march_kernel/prepare_fixture.py \
  --output "$PWD/.local/military-march-evidence/project" \
  --fixture-art "$VERIFIED_TINY_ART"

export HOME="$PWD/.local/military-march-evidence/home"
export XDG_DATA_HOME="$PWD/.local/military-march-evidence/xdg-data"
export XDG_CONFIG_HOME="$PWD/.local/military-march-evidence/xdg-config"
export XDG_CACHE_HOME="$PWD/.local/military-march-evidence/xdg-cache"
export TMPDIR="$PWD/.local/military-march-evidence/tmp"
mkdir -p "$HOME" "$XDG_DATA_HOME" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$TMPDIR"
"$GODOT_4_7_2" --headless --path .local/military-march-evidence/project/game --editor --import
"$GODOT_4_7_2" --headless --path .local/military-march-evidence/project/game \
  --script res://tests/test_military_march_kernel.gd
```

The accepted engine is `4.7.2.stable.official.ed1daf0bf`. The restored workspace lacks `/usr/bin/python3.11`; this isolated preparation used the existing Python 3.12.14 interpreter without installing or changing system software. A zero Godot exit code alone is insufficient: inspect the log for script errors and require the final check/failure summary.

## Boundaries and evidence

- Real Kernel boot, office appointment, campaign acceptance, formation, travel, pause/resume/wait, training, Contact, military plan, and battle progression are used. No successful march fact or military profile is injected into `_state`.
- The closed-terrain case is explicitly a saved-state input boundary: only an existing boolean flag is changed, then both real Schema validation and Kernel cold restore must accept it. There is no claim of a gameplay command closing the road.
- The failure probe subclasses Kernel only in this test. After real `_apply` completes, it captures the candidate and corrupts one fatigue value. The unchanged production final Schema must reject it; the entire live state, including IDs, RNG, assets, facts, history, and request cache, must remain unchanged.
- Save coverage is in-memory Codec v2 plus a new Kernel's `restore_with_catalog`. Native/SaveFiles durability, bank selection, and physical save recovery are outside this gate.
- Optional `HM_MARCH_NOOPT_ORACLE` writes the exact no-opt snapshot and a genuine Codec v2 envelope at that absolute path and `.save.json`. Actual user-data directory and evidence paths are logged. `HM_MARCH_NOOPT_BASELINE` compares the final no-opt gameplay state against a prior snapshot, normalizing only `content_id`, `content_version`, and `content_digest`. `HM_MARCH_OLD_V6_SAVE` adds rejection of a genuine pre-hook v6 save; no labels or lock bytes are changed to force acceptance.
- `--expected-red` runs one valid opt-in move to demonstrate the missing pre-hook receipt. `--noopt-only` records the old behavior. `--fixture-smoke` checks only real contact setup and creation; `--training-smoke` checks mixed-course preparation with march assertions disabled. These preparation modes do not claim activated march acceptance.
- Each preparation emits a source/content/art identity receipt. The final gate must run only after the exact central hooks, field condition source/Training integration, and f10-combat-7 metadata are installed.

No GUI, full regression suite, SDK, original art, A4 history/performance, migration, automatic rest, battle/shortage fatigue, new damage model, strategic AI activation, or publication is claimed.

## Development-period focused risk mode

After the 425/0 original seam run, independent review found that recomputing a quantized Contact anchor as a new straight path could reject a legal full step. The focused fixture now reproduces a real planned `[419,281] → [421,320]` movement with frozen budget 860 and a rounded `[420,308]` Contact chord costing 865. Explicit zero-food formation leaves actual equipment load 3; authored army budget 964 yields 860 through the unchanged production formula. It also covers both armies moving, positive-time zero integer movement, and a genuine via-settlement route crossed before contact.

Use `--mixed-only` for those four cases and `--contact-cleanup-only` for actual battle completion, original action cleanup, and single-field source-proof rejection. These focused modes call real Kernel persistent-state validation, not Codec, Native/SaveFiles, old-save migration, or a full storage regression. The narrow invalid-state cases alter one saved-copy field at a time: missing/type/foreign-action/foreign-plan/endpoint/credit/retained-index/contact-index. The original successful state is never modified.

The fixture now pins its exact newly staged pack digest in a generated test-input selector and checks that identity against the real installed index; retained older content sets remain preserved. The original 425/0 and v6 evidence apply to their recorded older source/content identities. They are not relabeled as proof for the later Contact-source revision. Full storage and historical compatibility acceptance remain deferred under the latest development policy.
