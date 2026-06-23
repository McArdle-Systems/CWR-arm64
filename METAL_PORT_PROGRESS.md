# Metal Port — Progress Notes (WIP)

Status as of this commit. Written so a future session (or another contributor)
can pick this up without re-deriving context.

## Where things stand

**The game boots and runs its real per-frame loop on native Metal, against
real retail game data, on Apple Silicon.** Confirmed end to end:

```
config → Metal device init (Apple M-series) → display/graphics config
  → asset banks mounted (packages/Remaster) → landscape loaded
  → fonts initialized → scene preloader initialized
  → enters World::Simulate / RenderFrame
  → crashes on the first real Landscape::Draw() call
```

The crash is expected and well understood (see "Where it stops" below) — it
is not a boot/integration bug, it's the deliberate boundary of how far this
milestone was scoped to go.

## Milestones completed

### Milestone 0 — bare Metal pipeline (`engine/PoseidonMTL/EngineMTLBootstrap.*`)
Standalone proof that SDL3 → `CAMetalLayer` → `MTLDevice` → clear → present
works at all. Exercised by `apps/tools/MetalSmokeTest`. No Poseidon engine
dependency — deliberately isolated because metal-cpp's `Foundation.hpp` and
Poseidon's core headers can't be included in the same translation unit (see
below).

### Milestone 1 — real `Poseidon::Engine` backend (`engine/PoseidonMTL/EngineMTL.*`)
`EngineMTL` implements the full `IGraphicsEngine`/`Engine` virtual contract
(~38 methods) and registers as backend `"mtl"` via
`GraphicsEngineFactory`/`RegisterMetalGraphicsBackend()`, selectable with
`--render mtl` (macOS-only CLI option, added in `AppConfig.cpp`). Window
creation, display-mode switching, and event handling reuse the same
`WindowPlacement` resolver and `SDLEventWindow` helper GL33 uses — only the
device/swapchain plumbing is Metal-specific, and that's delegated through
`EngineMTLBootstrap` (extended with `AttachToWindow()`, `GetRendererName()`,
a `clear` flag on `RenderClearAndPresent()`).

**All `Draw*`/`Mesh*`/`PrepareTriangle`/etc. methods are still no-op stubs**
(mirroring `GraphicsEngineDummy`'s pattern). `TextBank()` returns a
`TextBankDummy`. This was intentional — the goal was proving the *lifecycle*
integration (config → window → display settings → world/landscape load),
not rendering.

### Milestone 1.5 — macOS/ARM64 portability fixes to Poseidon core
None of this is Metal-specific; it was blocking `Poseidon` (the engine core
library) from compiling on Apple Silicon **at all**, regardless of graphics
backend:

- `finite()` → `isfinite()`/`__builtin_isfinite` (macOS libc++ doesn't have
  the BSD/glibc `finite()` compat function). `platform.hpp`, `MathOpt.cpp`.
- x86 SSE/MMX intrinsics have no ARM64 equivalent. Vendored
  [sse2neon](https://github.com/DLTcollab/sse2neon) (`thirdparty/sse2neon/`)
  and added `engine/Poseidon/Foundation/Common/X86IntrinsicsCompat.hpp` as
  the single redirect point + the handful of legacy MMX intrinsics
  (`_mm_packs_pi32`, `_mm_packs_pu16`, `_mm_set1_pi8`, `_mm_cmpgt_pi8`,
  `_mm_and_si64`/`_mm_or_si64`/`_mm_andnot_si64`) sse2neon doesn't cover,
  hand-implemented against NEON matching Intel's documented semantics. Most
  of the 11 files that referenced x86 intrinsic headers turned out to have
  the actual SIMD code already dead-gated behind legacy compiler checks
  (`_COMPILER_CAN_PIII`, `OPTIMIZE_FOR_MMX`, `__ICL`) that are false on any
  Clang build — only `Math3DK.hpp`, `Quatrix.hpp`, `ColorsK.hpp`, and
  `V3QuadsP3.cpp`'s top-of-file include actually needed the redirect.
- Linux-only headers (`link.h`, `linux/sysinfo.h`, `malloc.h`) gated/replaced:
  `CrashHandler.cpp`'s build-ID capture (ELF `NT_GNU_BUILD_ID` via
  `dl_iterate_phdr`) is Linux-only — macOS branch is a stub (no Mach-O
  `LC_UUID` equivalent implemented yet, diagnostic-only). `MemGrow.cpp`'s
  out-of-memory error message now uses `sysctlbyname("vm.swapusage", ...)`
  on macOS instead of `sysinfo()`.
- `PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP` is a glibc-only static-initializer
  macro; `PoCritical.cpp` now builds the same recursive mutex portably via
  `pthread_mutexattr_settype(PTHREAD_MUTEX_RECURSIVE)` on all platforms.
- `apps/cwr/Game/CMakeLists.txt`: Apple's linker has no GNU
  `--start-group`/`--end-group` equivalent — needed its own `elseif(APPLE)`
  branch instead of falling into the Linux `else()`.

### Real-data validation
Tested against the user's actual retail `packages/Remaster` install (DTA/BIN/
AddOns/Missions/Worlds PBOs — Windows-only DLLs and the 99MB DirectX/OpenAL
installer redistributables stripped out, since they're dead weight on
macOS). One genuine data gap found and fixed: the retail copy was missing
`fonts/{cwr_title,cwr_body,cwr_mono,cwr_serif,cwr_hand}.ttf` (loose files at
the content root, not packed in a PBO) that the newer Remastered build
requires — these came from the official Remaster Demo zip and were copied in
locally. `packages/` is gitignored; nothing data-related is in this repo.

## Where it stops

First real draw call (`Landscape::Draw()` → `EngineMTL::PrepareTriangle` /
`BeginMesh` / etc.) crashes with SIGSEGV at a near-null offset (`0x28`) —
some Landscape-side state that a real `BeginMesh`/`PrepareMesh` would have
set up, dereferenced by the next call. This is the expected limit of
no-op stubs meeting real usage, not a lifecycle bug.

## Next milestone (not started)

Implement the real Metal draw path in `EngineMTL`:
- Vertex buffers (the two existing GL33 layouts: `TLVertex` screen-space,
  `SVertex` 3D mesh — both simple, low attribute count)
- MSL shader equivalents of the 9 GLSL sources in
  `engine/PoseidonGL33/EngineGL33_Shaders.cpp` (40 linked-program
  permutations there; worth spiking whether SPIRV-Cross can semi-automate
  GLSL→SPIR-V→MSL before hand-porting)
- Map the existing 3 UBOs (VS constants, PS constants, WorldInstances) onto
  Metal's buffer-index binding model — should be the easiest part, Metal's
  explicit indices line up closely with GL33's existing scheme
- Render pipeline state objects, replacing GL33's `ApplyPipeline`/bind-cache
  state-dedup pattern with Metal's PSO model
- Shadow maps / SSAA / instancing are later still — see the original M0 plan
  roadmap for the full GL33 feature surface still to match

## Key files

| File | Purpose |
|------|---------|
| `engine/PoseidonMTL/EngineMTL.{hpp,cpp}` | The real `Poseidon::Engine` backend |
| `engine/PoseidonMTL/EngineMTLBootstrap.{hpp,cpp}` | Metal device/layer/queue wrapper (Engine-agnostic, metal-cpp-isolated) |
| `engine/PoseidonMTL/GraphicsBackendMTL.cpp` | Factory registration (`"mtl"`) |
| `engine/PoseidonMTL/MetalCppImpl.cpp` | metal-cpp's `*_PRIVATE_IMPLEMENTATION` macros (one definition per binary) |
| `engine/Poseidon/Foundation/Common/X86IntrinsicsCompat.hpp` | x86 SIMD → sse2neon redirect + MMX shims |
| `apps/tools/MetalSmokeTest/` | Milestone-0 standalone smoke test |
