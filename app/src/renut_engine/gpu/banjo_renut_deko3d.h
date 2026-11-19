// banjo-nx (2026-10-17): Renut's renderer on plume's deko3d back end (renut_gpu_backend = "deko3d",
// docs/deko3d-renderer.md). The game shaders' GLSL package in the NRO (tools/deko3d_shader_package.py) and the DKSH
// cache of compiled variants on the SD card.
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace plume {
struct RenderDevice;
struct RenderShader;
}  // namespace plume
struct ShaderCacheEntry;
struct ShaderVertexInput;
struct ShaderLiteral;

namespace banjo::renut_gpu::deko3d {

// Whether there are game shaders: the installer's package next to the NRO (deko3d_glsl.bin, made from the player's
// files by banjo-nx-installer) or the NRO's own (Switch builds made with one).
bool PackageAvailable();

// banjo-nx (2026-10-22): the shader table (shader_cache.h) of the shaders in the installer's package, offsets into
// inputs and literals; false without one. Renut's FindShaderEntry merges it with the NRO's own table.
bool InstalledShaderTable(std::vector<ShaderCacheEntry> &entries, std::vector<ShaderVertexInput> &inputs,
                          std::vector<ShaderLiteral> &literals);

// The deko3d shader (plume::Deko3DShaderBlob) of a game shader by its container hash, or null (logged once).
std::unique_ptr<plume::RenderShader> CreateGameShader(plume::RenderDevice *device, uint64_t hash);

// The boolean specialization constants (ids 1000..1255) a game shader has, as ShaderBoolSpecMask gives them.
bool BoolSpecMask(uint64_t hash, uint32_t mask[8]);

// The DKSH cache next to the NRO (deko3d_shaders.bin): what it holds is read at start; variants the console compiles
// are added to it (written by Save, and by a periodic save while the game runs). Installed into plume before the
// device is made.
void InstallShaderCache();
void SaveShaderCache();

// S4ep2-XBLA-nx (2026-11-19, shaders compiled up front): every variant listed in deko3d_variants.txt next to the NRO
// (tools/deko3d_variant_cache.py --export-variants: "<vs|ps>_<HASH> <id>=<value>,..."), made ready before the game
// draws: read from the DKSH cache, or compiled now when the cache lacks it (saved there for the next start). Blocks the
// calling thread; the progress is in ShaderPrep() for the screen that shows it. Does nothing off deko3d, without the
// file, or with s4_prepare_shaders off.
struct ShaderPrepProgress {
  std::atomic<bool> active{false};
  std::atomic<uint32_t> done{0}, total{0}, compiled{0}, failed{0};
};
ShaderPrepProgress &ShaderPrep();
void PrepareListedVariants();

// One line for the renderer's report: compiles, cache hits, memory.
std::string Report();

// sasrt-nx (2026-11-08, sasrt_deko3d_diag_trace; the second GPU round): at each scan-out; with the key, every 600th
// frame's command stream is recorded (plume::Deko3DTraceToken) and logged at the next scan-out.
void TraceFrameBoundary();

}  // namespace banjo::renut_gpu::deko3d
