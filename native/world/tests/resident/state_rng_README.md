# Fixed-engine integer RNG primitive

This is a separate primitive milestone. It does not migrate personal combat, military combat, drills, bargaining, or any other stochastic gameplay. The first complete 1000-person journey did not draw from these streams and is not an RNG acceptance test.

## Source and engine identity

- Godot source commit: `ed1daf0bf001b61586d9930840f2f1394092c079` (4.7.2-stable).
- Actual executable: `Godot_v4.7.2-stable_linux.x86_64`, SHA256 `8d106cbe6144c2dc7e881d61d2429c1a8a76e6b22ef48bd5e48dcf934953f71e`.
- Primary source files, Git blob IDs and SHA256 values are recorded in `state_rng_sources.json`. The examined paths are `core/math/random_number_generator.h`, `core/math/random_pcg.{h,cpp}`, and `thirdparty/misc/pcg.{h,cpp}` at that exact commit.
- PCG attribution and Apache-2.0/Godot MIT license text are retained in `src/resident/model/common_rng_LICENSE.txt`.

`state_rng_vectors.gd` contains no native algorithm or simulated generator. It instantiates the actual engine's `RandomNumberGenerator`, sets its seed/state, invokes `randi_range`, and records output/next state. A separate engine generator advances via raw `randi` until the recorded next state is reached, documenting rejection counts independently of the C++ port.

The fixture has 952 draws, 11 seed→state rows, 87 cases consuming more than one raw draw, 136 equal-bound calls consuming zero draws, and up to eight raw draws in a rejection case. The five original WorldFactory seed→state pairs are explicitly checked by the capture script. All 64-bit seed/state values are decimal strings.

## Exact supported behavior

- PCG32 XSH-RR with fixed default sequence `1442695040888963407`; the effective increment is `(sequence << 1) | 1`.
- Unsigned64 state multiplication/addition wraps exactly; signed serialization uses bit-preserving copies, not an out-of-range signed conversion.
- Both endpoints must fit signed32, matching Godot's C++ `int` binding. Swapped endpoints are supported.
- Equal endpoints return the endpoint without advancing state.
- The full signed32 range uses one unbounded raw draw. Other ranges use the original rejection threshold and modulo.
- Restored state is used directly. Calling the primitive never reseeds, calls `randomize`, or advances another stream.
- Other endpoint widths or a backend other than `godot:4.7.2-stable (official)` return explicit unsupported-capability errors without mutation. A missing stream is an internal state error.
- Savepoint rollback restores seed/state/record order exactly. Clone and typed checkpoint continuation preserve subsequent draws.

No float RNG, weighted draw, or raw `randi` world API is added. The raw helper is only an implementation step and a focused exact-vector test seam.

## Reproduction

The ordinary native test reads the committed fixture; it does not require Godot or network access. `state_checks.py` includes it. For a narrow-only run, compile `state_rng.cpp` with `common_store.cpp`, `common_checkpoint.cpp` and `checkpoint/ordered_value.cpp`, the repository's include/src directories, and the existing pinned nlohmann include path; then pass `state_rng_vectors.json` as its sole argument.

To recapture, use the verified executable with an isolated empty Godot project and writable private HOME/XDG/TMP directories:

```sh
Godot_v4.7.2-stable_linux.x86_64 --headless --path /tmp/isolated-project \
  --script /absolute/path/state_rng_vectors.gd -- /absolute/path/vectors.json
```

The empty project needs a normal `project.godot`; it must not load the game. Compare the resulting file's SHA256 to `state_rng_sources.json` before replacing the committed fixture. A different engine/source mismatch is a blocker, not permission to approximate the algorithm.
