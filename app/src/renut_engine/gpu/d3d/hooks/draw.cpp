/**
 * @file    gpu/d3d/hooks/draw.cpp
 * @brief   Draw, clear and resolve entry points, and the shader setters.
 *
 * The draws replace N&B's D3D entirely: the originals would flush the device's
 * dirty state into GPU packets, including binding shaders by reading their
 * microcode, which renderer-owned shader objects do not carry. Clear, Resolve
 * and tiling queue their host work and do the device work of N&B's D3D
 * natively (d3d_native.h): dirty masks, the tiling state, fences on the
 * destination.
 *
 * The host work runs on the render thread (queued.h); a device recording a
 * command buffer records its draws, clears and resolves (command_buffers.h)
 * instead. Neither takes State().mutex here.
 */
#include <algorithm>
#include <cstdlib>
#include <bit>
#include <cmath>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <rex/hook.h>
#include <rex/chrono/clock.h>
#include <rex/cvar.h>
#include "s4/s4_pacer.h"
#include <chrono>

#include "renut_engine/gpu/banjo_renut_select.h"
#include <rex/types.h>

#include "renut_engine/gpu/guest/command_buffers.h"
#include "renut_engine/gpu/d3d/d3d.h"
#include "renut_engine/gpu/d3d/d3d_native.h"
#include "renut_engine/gpu/render/draw.h"
#include "renut_engine/gpu/guest/guest_memory.h"
#include "renut_engine/gpu/guest/guest_records.h"
#include "renut_engine/gpu/log.h"
#include "renut_engine/gpu/backend/queued.h"
#include "renut_engine/gpu/backend/render_thread.h"
#include "renut_engine/gpu/render/surfaces.h"
#include "renut_engine/gpu/backend/video.h"
#include "profiler/profiler.h"  // katamari-nx: the frame boundary at Present
#include <atomic>
extern std::atomic<uint64_t> g_banjo_game_frames;  // katamari_keys.cpp: frames presented

using namespace renut::gpu;
#include "renut_engine/gpu/d3d/hooks/hooks_common.h"  // TFU-nx: the present diagnostic
#include "renut_engine/gpu/banjo_renut_capture.h"     // TFU-nx: CaptureFrame (the scan-out capture)
#include <rex/ui/presenter.h>

// A float argument also takes the GPR (or stack) slot of its position, which
// the marshaling does not skip: the u32 after each double in the hooks'
// parameters stands in for it.

namespace {

constexpr u32 kE_OUTOFMEMORY = 0x8007000Eu;
// D3DVIEWPORT9 {0, 0, 0xFFFF, 0xFFFF, 0.0, 1.0} in N&B's data (unk_82001744):
// the whole surface.
// S4ep1-nx: TFU's kFullViewport (0x8202CAEC: 0, 0, 0xFFFF, 0xFFFF, 0.0, 1.0) has no counterpart: Sonic 4's LTCG build
// inlines it (SetSurfaceInfo reads the rectangle from 0x8209BBF0, the Z range from float constants). The same values below.
// constexpr u32 kFullViewport = 0x8202CAEC;  // katamari-nx: 0x82001744->0x820FAEB4  // TFU-nx: D3D_SetSurfaceInfo sub_822D7B68, tools/renut_data_refs.py
// D3DRS_HIZWRITEENABLE's index in the render state table.
constexpr u32 kRenderStateHiZWriteEnable = 97;

// Guest memory Begin*Vertices hands out; the game writes one draw's vertices
// into it before its next D3D call. One per device, as devices record on
// different threads.
constexpr u32 kVerticesScratchSize = 4u << 20;
std::mutex g_vertices_scratch_mutex;  // devices call from their own threads
std::unordered_map<u32, u32> g_vertices_scratch;

u32 VerticesScratch(u32 device) {
  std::lock_guard lock(g_vertices_scratch_mutex);
  u32 &scratch = g_vertices_scratch[device];
  if (!scratch)
    scratch = records::AllocPhysicalBlock(kVerticesScratchSize);
  return scratch;
}

bool Active() {
  auto &s = State();
  return s.host_ready && !s.shutting_down.load(std::memory_order_acquire);
}

void StoreU32(u32 va, u32 value) {
  if (auto *p = GuestPtr<be_u32>(va))
    *p = value;
}

u32 LoadU32(u32 va) {
  const auto *p = GuestPtr<const be_u32>(va);
  return p ? u32(*p) : 0;
}

bool ReadRect(u32 va, ClearRect &rect) {
  const auto *p = GuestPtr<const be_u32>(va);
  if (!p)
    return false;
  rect = {i32(u32(p[0])), i32(u32(p[1])), i32(u32(p[2])), i32(u32(p[3]))};
  return true;
}

bool ReadVector4(u32 va, float out[4]) {
  const auto *p = GuestPtr<const be_u32>(va);
  if (!p)
    return false;
  for (int i = 0; i < 4; ++i) {
    const u32 bits = p[i];
    std::memcpy(&out[i], &bits, 4);
  }
  return true;
}

// void D3DDevice_DrawVertices(D3DDevice*, D3DPRIMITIVETYPE, UINT StartVertex,
//                             UINT VertexCount) [0x821FB880]
void D3DDevice_DrawVertices_hook(u32 device, u32 primitive, u32 start_vertex, u32 vertex_count) {
  if (!Active())
    return;
  DrawCall call;
  call.primitive = primitive;
  call.start_vertex = start_vertex;
  call.count = vertex_count;
  if (IsRecordingCommandBuffer(device))
    RecordDraw(device, call, 0, 0);
  else
    QueueDraw(device, call, 0, 0);
}

// void D3DDevice_DrawIndexedVertices(D3DDevice*, D3DPRIMITIVETYPE, INT BaseVertexIndex,
//                                    UINT StartIndex, UINT IndexCount) [0x8263F358]
void D3DDevice_DrawIndexedVertices_hook(u32 device, u32 primitive, u32 base_vertex,
                                        u32 start_index, u32 index_count) {
  if (!Active())
    return;
  DrawCall call;
  call.primitive = primitive;
  call.indexed = true;
  call.base_vertex = i32(base_vertex);
  call.start_index = start_index;
  call.count = index_count;
  if (IsRecordingCommandBuffer(device))
    RecordDraw(device, call, 0, 0);
  else
    QueueDraw(device, call, 0, 0);
}

// void D3DDevice_DrawIndexedTessellatedVertices(D3DDevice*, D3DTESSPRIMITIVETYPE,
//     INT BaseVertexIndex, UINT StartIndex, UINT IndexCount) [0x821FF958]
// Replaced like the other draws: the original's state flush binds the shader
// from its microcode, and bugchecks (0xF5) on a tessellation shader without
// the tessellation state the original SetVertexShader keeps. The draw is
// tessellated on the host (DrawCall::tessellation).
void D3DDevice_DrawIndexedTessellatedVertices_hook(u32 device, u32 primitive, u32 base_vertex,
                                                   u32 start_index, u32 index_count) {
  if (!Active())
    return;
  DrawCall call;
  call.primitive = primitive;
  call.tessellation = true;
  call.indexed = true;
  call.base_vertex = i32(base_vertex);
  call.start_index = start_index;
  call.count = index_count;
  if (IsRecordingCommandBuffer(device))
    RecordDraw(device, call, 0, 0);
  else
    QueueDraw(device, call, 0, 0);
}

// void D3DDevice_DrawVerticesUP(D3DDevice*, D3DPRIMITIVETYPE, UINT VertexCount,
//                               const void *pVertexStreamZeroData, UINT Stride) [0x821FD698]
void D3DDevice_DrawVerticesUP_hook(u32 device, u32 primitive, u32 vertex_count, u32 data,
                                   u32 stride) {
  if (!Active() || !data)
    return;
  DrawCall call;
  call.primitive = primitive;
  call.count = vertex_count;
  call.up_vertices = GuestPtr<const u8>(data);
  call.up_stride = stride;
  call.up_vertex_count = vertex_count;
  if (IsRecordingCommandBuffer(device))
    RecordDraw(device, call, vertex_count * stride, 0);
  else
    QueueDraw(device, call, vertex_count * stride, 0);
}

// void D3DDevice_DrawIndexedVerticesUP(D3DDevice*, D3DPRIMITIVETYPE, UINT MinVertexIndex,
//     UINT NumVertices, UINT IndexCount, const void *pIndexData, D3DFORMAT IndexDataFormat,
//     const void *pVertexStreamZeroData, UINT VertexStreamZeroStride) [0x8263F2B8]
void D3DDevice_DrawIndexedVerticesUP_hook(u32 device, u32 primitive, u32 min_vertex,
                                          u32 num_vertices, u32 index_count, u32 index_data,
                                          u32 index_format, u32 vertex_data, u32 stride) {
  if (!Active() || !index_data || !vertex_data)
    return;
  DrawCall call;
  call.primitive = primitive;
  call.indexed = true;
  call.count = index_count;
  call.up_indices = GuestPtr<const u8>(index_data);
  call.up_indices_32 = (index_format & 4) != 0;  // D3DFMT_INDEX32
  call.up_vertices = GuestPtr<const u8>(vertex_data);
  call.up_stride = stride;
  call.up_vertex_count = min_vertex + num_vertices;
  const u32 vertex_bytes = call.up_vertex_count * stride;
  const u32 index_bytes = index_count * (call.up_indices_32 ? 4 : 2);
  if (IsRecordingCommandBuffer(device))
    RecordDraw(device, call, vertex_bytes, index_bytes);
  else
    QueueDraw(device, call, vertex_bytes, index_bytes);
}

// void *D3DDevice_BeginVertices(D3DDevice*, D3DPRIMITIVETYPE, UINT VertexCount,
//                               UINT VertexStreamZeroStride) [0x821FBD30]
// Callers write the vertices, then end inline by restoring m_pRing from the
// device, so the draw's state is captured now and its vertices copied at the
// device's next hook (FlushDeferredVertices).
u32 D3DDevice_BeginVertices_hook(u32 device, u32 primitive, u32 vertex_count, u32 stride) {
  const u64 bytes = u64(vertex_count) * stride;
  if (!bytes || bytes > kVerticesScratchSize)
    return 0;
  // The inline end: m_pRing = [device + kDeviceOffsetVerticesRingEnd].
  StoreU32(device + d3d::kDeviceOffsetVerticesRingEnd,
           LoadU32(device + u32(offsetof(d3d::Device, m_pRing))));

  const u32 scratch = VerticesScratch(device);
  if (!scratch)
    return 0;
  DrawCall call;
  call.primitive = primitive;
  call.count = vertex_count;
  call.up_vertices = GuestPtr<const u8>(scratch);
  call.up_stride = stride;
  call.up_vertex_count = vertex_count;
  // Either way the device's previous Begin*Vertices data leaves the scratch
  // before the game writes this draw's over it.
  if (IsRecordingCommandBuffer(device))
    RecordDeferredDraw(device, call, call.up_vertices, u32(bytes));
  else if (Active())
    QueueDeferredDraw(device, call, call.up_vertices, u32(bytes));
  else
    FlushDeferredVertices(device);
  return scratch;
}

// A clear: the renderer's, recorded or queued, then N&B's device work.
// whole_target: BeginTiling's, of one tile's worth of EDRAM, which on the
// host is the whole tiled target.
void Clear(u32 device, u32 flags, const ClearRect *rect, const float color[4], float z,
           u32 stencil, bool whole_target) {
  const ClearRect *host_rect = whole_target ? nullptr : rect;
  if (IsRecordingCommandBuffer(device))
    RecordClear(device, flags, host_rect, color, z, stencil);
  else if (Active())
    QueueClear(device, flags, host_rect, color, z, stencil);
  const native::Rect native_rect = rect ? native::Rect{rect->x1, rect->y1, rect->x2, rect->y2}
                                        : native::Rect{};
  native::Clear(native::DeviceAt(device), flags, rect ? &native_rect : nullptr);
}

// A resolve: the renderer's copy (and clear), then N&B's device work.
void Resolve(u32 device, u32 flags, const ClearRect *rect, u32 dest_texture, i32 dest_x,
             i32 dest_y, u32 dest_level, u32 dest_slice, u32 color_va, float clear_z,
             u32 clear_stencil, u32 parameters) {
  float color[4] = {0, 0, 0, 0};
  const bool has_color = color_va && ReadVector4(color_va, color);
  if (IsRecordingCommandBuffer(device)) {
    RecordResolve(device, flags, rect, dest_texture, dest_x, dest_y, dest_level, dest_slice,
                  has_color ? color : nullptr, clear_z, clear_stencil);
  } else if (Active()) {
    QueueResolve(device, flags, rect, dest_texture, dest_x, dest_y, dest_level, dest_slice,
                 has_color ? color : nullptr, clear_z, clear_stencil);
  }
  native::Resolve(native::DeviceAt(device), flags, dest_texture, clear_z, clear_stencil,
                  parameters);
}

// void D3DDevice_ClearF(D3DDevice*, DWORD Flags, const D3DRECT *pRect,
//                       const D3DVECTOR4 *pColor, float Z, DWORD Stencil) [0x8224F110]
// Z takes r7's slot, so Stencil arrives in r8.
void D3DDevice_ClearF_hook(u32 device, u32 flags, u32 rect_va, u32 color_va, double z,
                           u32 /*z_slot*/, u32 stencil) {
  ClearRect rect;
  const bool has_rect = rect_va && ReadRect(rect_va, rect);
  float color[4] = {0, 0, 0, 0};  // unk_82001160 without one
  if (color_va)
    ReadVector4(color_va, color);
  Clear(device, flags, has_rect ? &rect : nullptr, color, float(z), stencil, false);
}

// void D3DDevice_Clear(D3DDevice*, DWORD Count, const D3DRECT *pRects, DWORD Flags,
//     D3DCOLOR Color, float Z, DWORD Stencil) [0x8224F238]: ClearF with the
// color as a vector (XMLoadColor: each byte as 1 + b / 2^23, scaled by
// 2^23 / 255 and offset), per rect. Z takes r8's slot.
void D3DDevice_Clear_hook(u32 device, u32 count, u32 rects_va, u32 flags, u32 color_argb,
                          double z, u32 /*z_slot*/, u32 stencil) {
  const float scale = std::bit_cast<float>(0x47008081u);  // aG, unk_82001150
  auto channel = [scale](u32 byte) {
    return std::fmaf(std::bit_cast<float>(0x3F800000u | (byte & 0xFF)), scale, -scale);
  };
  const float color[4] = {channel(color_argb >> 16), channel(color_argb >> 8),
                          channel(color_argb), channel(color_argb >> 24)};
  if (!rects_va) {
    Clear(device, flags, nullptr, color, float(z), stencil, false);
    return;
  }
  for (u32 i = 0; i < count; ++i) {
    ClearRect rect;
    if (ReadRect(rects_va + i * 16, rect))
      Clear(device, flags, &rect, color, float(z), stencil, false);
  }
}

// void D3DDevice_Resolve(D3DDevice*, DWORD Flags, const D3DRECT *pSourceRect,
//     D3DBaseTexture *pDestTexture, const D3DPOINT *pDestPoint, UINT DestLevel,
//     UINT DestSliceOrFace, const D3DVECTOR4 *pClearColor, float ClearZ,
//     DWORD ClearStencil, const D3DRESOLVE_PARAMETERS *pParameters) [0x821FC7A0]
// ClearZ is in f1 and takes the first stack slot; ClearStencil and pParameters
// are in the two after it.
// TFU-nx (2026-11-11, gameplay): with TFU_PRESENT_DIAG, every 1,800th Present (about a minute) starts a capture frame:
// each resolve until the next Present reads render target 0 back first (logs/tfu_<n>_resolveNN_<flags>_<dest>.ppm),
// and that Present the back buffer (tfu_<n>_back.ppm). The first level showed only its HUD with every draw drawn:
// these show which pass the picture is lost in.
static void TfuDiagReadBackBuffer(u32 device, u32 present_number, const std::string &tag);
static bool g_tfu_capture_frame = false;
static u32 g_tfu_capture_present = 0, g_tfu_capture_index = 0;

void D3DDevice_Resolve_hook(u32 device, u32 flags, u32 rect_va, u32 dest_texture, u32 point_va,
                            u32 dest_level, u32 dest_slice, u32 color_va, double clear_z,
                            u32 /*z_slot*/, u32 clear_stencil, u32 parameters) {
  TfuPresentDiagResolve(flags, dest_texture);  // TFU-nx (2026-11-11): the black-screen diagnostic (device.cpp)
  if (g_tfu_capture_frame && g_tfu_capture_index < 40)
    TfuDiagReadBackBuffer(device, g_tfu_capture_present,
                          fmt::format("resolve{:02}_{:X}_{:08X}", g_tfu_capture_index++, flags, dest_texture));
  ClearRect rect;
  const bool has_rect = rect_va && ReadRect(rect_va, rect);
  i32 dest_x = 0, dest_y = 0;
  if (const auto *p = GuestPtr<const be_u32>(point_va)) {
    dest_x = i32(u32(p[0]));
    dest_y = i32(u32(p[1]));
  }
  Resolve(device, flags, has_rect ? &rect : nullptr, dest_texture, dest_x, dest_y, dest_level,
          dest_slice, color_va, float(clear_z), clear_stencil, parameters);
}

// void D3DDevice_BeginTiling(D3DDevice*, DWORD Count, const D3DRECT *pTileRects,
//     const D3DVECTOR4 *pClearColor) [0x82215510]: TU3's build dropped the
// arguments BeginTiling never used (Flags, ClearZ, ClearStencil).
// The surfaces bound from here to EndTiling are backed by targets covering
// every tile (SetTilingExtentLocked). N&B's BeginTiling clears with Z 0.0 and
// stencil 0, unless the device records a one-pass Z pass (Flags3 0x20); its
// ring segment for the tiles is left out.
// katamari-nx (2026-10-30): XDK 2.0.5632's BeginTiling keeps the full public form, BeginTiling(Flags, Count,
// pTileRects, pClearColor, ClearZ, ClearStencil) (sub_824EFBC8: r4 Flags, r5 Count, r6 rects, r7 colour, f1 Z,
// r9 stencil). It clears with ClearZ / ClearStencil and keeps them at +0x3390 / +0x3394 for EndTiling (stfs f31 /
// stw r24 there), stores Flags at +0x337C, sets Flags3 0x20 (and 0x10 with Flags bit 2) when Flags has bit 1, and
// skips its first clear when Flags has bit 0 or Flags3 has 0x20. KatamariBeginTiling passes those through; N&B's
// update form (no Flags, Z 0.0, stencil 0) is the call with flags 0 and the default clear values.
static void BeginTilingImpl(u32 device, u32 count, u32 rects_va, u32 color_va, u32 flags, float clear_z,
                            u32 clear_stencil);
void D3DDevice_BeginTiling_hook(u32 device, u32 count, u32 rects_va, u32 color_va) {
  BeginTilingImpl(device, count, rects_va, color_va, 0, 0.0f, 0);
}
static void BeginTilingImpl(u32 device, u32 count, u32 rects_va, u32 color_va, u32 flags, float clear_z,
                            u32 clear_stencil) {
  native::Device d = native::DeviceAt(device);
  if (!d)
    return;
  if (flags & 2) {
    d.SetB(native::off::Flags3, d.B(native::off::Flags3) | 0x20 | ((flags & 4) ? 0x10 : 0));
  }
  std::vector<ClearRect> tiles;
  for (u32 i = 0; i < count; ++i) {
    ClearRect rect;
    if (!ReadRect(rects_va + i * 16, rect))
      break;
    tiles.push_back(rect);
  }
  count = u32(tiles.size());
  if (!IsRecordingCommandBuffer(device)) {
    FlushDeferredVertices(device);
    u32 width = 0, height = 0;
    for (const ClearRect &rect : tiles) {
      width = std::max(width, u32(rect.x2));
      height = std::max(height, u32(rect.y2));
    }
    render_thread::Queue([tiles, width, height] {
      SetTilingExtentLocked(width, height);
      SetTilesLocked(tiles.data(), u32(tiles.size()));
    });
  }

  d.SetW(native::off::TileCount, count);
  u32 extent_x = 0, extent_y = 0;
  for (u32 i = 0; i < count; ++i) {
    const ClearRect &rect = tiles[i];
    extent_x = std::max(extent_x, u32(rect.x2));
    extent_y = std::max(extent_y, u32(rect.y2));
    d.SetW(native::off::TileRects + i * 16, u32(rect.x1));
    d.SetW(native::off::TileRects + i * 16 + 4, u32(rect.y1));
    d.SetW(native::off::TileRects + i * 16 + 8, u32(rect.x2));
    d.SetW(native::off::TileRects + i * 16 + 12, u32(rect.y2));
    d.SetW(native::off::TileOrigins + i * 8, u32(rect.x1) & ~31u);
    d.SetW(native::off::TileOrigins + i * 8 + 4, u32(rect.y1) & ~31u);
  }
  d.SetW(native::off::TilingExtent, extent_x);
  d.SetW(native::off::TilingExtent + 4, extent_y);
  d.SetW(0x3434, flags);  // katamari-nx: 0x338C->0x337C; XDK 2.0.5632 keeps Flags there (N&B's update: 0) (TFU-nx: offset 0x337C->0x3384) (S4ep1-nx: offset 0x3384->0x3434)

  if (!(flags & 1) && !(d.B(native::off::Flags3) & 0x20)) {
    // The largest tile, from its origin: its clear covers a tile of EDRAM.
    u32 width = 0, height = 0;
    for (u32 i = 0; i < count; ++i) {
      width = std::max(width, u32(tiles[i].x2) - d.W(native::off::TileOrigins + i * 8));
      height = std::max(height, u32(tiles[i].y2) - d.W(native::off::TileOrigins + i * 8 + 4));
    }
    // The viewport (x, y, width, height, min z, max z) and scissor rect, for
    // after the clear.
    float viewport[6];
    for (u32 i = 0; i < 6; ++i)
      viewport[i] = d.F(0x3218 + i * 4);  // katamari-nx: 0x3170->0x3160 (TFU-nx: offset 0x3160->0x3168) (S4ep1-nx: offset 0x3168->0x3218)
    i32 scissor[4];
    for (u32 i = 0; i < 4; ++i)
      scissor[i] = i32(d.W(native::off::ScissorRect + i * 4));
    native::SetViewport(d, 0.0f, 0.0f, 65535.0f, 65535.0f, 0.0f, 1.0f);  // S4ep1-nx: TFU's full viewport
    native::SetScissorRect(d, 0, 0, 0xFFFF, 0xFFFF);  // dword_82001190
    const ClearRect rect{0, 0, i32(width), i32(height)};
    float color[4] = {0, 0, 0, 0};
    if (color_va)
      ReadVector4(color_va, color);
    Clear(device, 0x3F, &rect, color, clear_z, clear_stencil, true);
    native::SetViewport(d, viewport[0], viewport[1], viewport[2], viewport[3], viewport[4],
                        viewport[5]);
    native::SetScissorRect(d, scissor[0], scissor[1], scissor[2], scissor[3]);
  }
  if (const auto *color = color_va ? GuestPtr<const u8>(color_va) : nullptr)
    std::memcpy(d.bytes + 0x3440, color, 16);  // katamari-nx: 0x3390->0x3380 (TFU-nx: offset 0x3380->0x3390) (S4ep1-nx: offset 0x3390->0x3440)
  else
    std::memset(d.bytes + 0x3440, 0, 16);  // katamari-nx: 0x3390->0x3380 (TFU-nx: offset 0x3380->0x3390) (S4ep1-nx: offset 0x3390->0x3440)
  d.SetF(0x3450, clear_z);  // flt_82000D18 (katamari-nx: 0x33A0->0x3390; XDK 2.0.5632: the ClearZ argument) (TFU-nx: offset 0x3390->0x33A0) (S4ep1-nx: offset 0x33A0->0x3450)
  d.SetW(0x3454, clear_stencil);  // katamari-nx: 0x33A4->0x3394; XDK 2.0.5632: the ClearStencil argument (TFU-nx: offset 0x3394->0x33A4) (S4ep1-nx: offset 0x33A4->0x3454)

  const u32 target = d.W(native::off::RenderTargets);
  d.SetW(0x3264, target ? target : d.W(native::off::DepthStencil));  // katamari-nx: 0x31BC->0x31AC (TFU-nx: offset 0x31AC->0x31B4) (S4ep1-nx: offset 0x31B4->0x3264)
  for (u32 i = 0; i < 5; ++i)  // the 4 targets, then depth
    d.SetW(native::off::TilingSurfaces + i * 4, d.W(native::off::RenderTargets + i * 4));
  d.SetW(0x3260, 2);  // katamari-nx: 0x31B8->0x31A8 (TFU-nx: offset 0x31A8->0x31B0) (S4ep1-nx: offset 0x31B0->0x3260)
  d.SetB(native::off::Flags, d.B(native::off::Flags) | 0x20);
  d.SetW(native::off::PredicationSet, 0);
  native::UpdateTilePredicated(d);
  d.SetW(native::off::RunPredicationState, 0);
  d.SetW(native::off::Predication, ~0u);
  d.SetW(native::off::TilePredication, 0);
  d.SetW(native::off::RunPredication, 0);
  // The tiles' commands go into a ring segment of their own from a KickOff.
  native::KickOff(device);
  native::SetViewport(d, 0.0f, 0.0f, 65535.0f, 65535.0f, 0.0f, 1.0f);  // S4ep1-nx: TFU's full viewport
  native::ResetAllState(d);
}

// HRESULT D3DDevice_EndTiling(D3DDevice*, D3DBaseTexture *pDestTexture,
//     const D3DVECTOR4 *pClearColor) [0x821B17C0]: TU3's build dropped the
// arguments EndTiling never used (ResolveFlags, pResolveRects, ClearZ,
// ClearStencil, pParameters).
// N&B's EndTiling resolves each tile's rect (flags 0x300: color and depth
// cleared) with BeginTiling's clear values, the last one with pClearColor,
// Z 1.0 and stencil 0. The host resolve reads each rect from the full-size
// target. Left out: the tiles' ring segment, D3D's WorkerDpc callback that
// replays it per tile, and its fence's packets.
// katamari-nx (2026-10-30): XDK 2.0.5632's EndTiling keeps the full form, EndTiling(ResolveFlags, pResolveRects,
// pDestTexture, pClearColor, ClearZ, ClearStencil, pParameters) (sub_824F0150: r4, r5, r6, r7, f1, r9, r10). Every
// tile but the last resolves with ResolveFlags | 0x100 when a colour target is bound (+0x3090) | 0x200 when a depth
// surface is (+0x30A0), BeginTiling's clear values (+0x3380 colour, +0x3390 Z, +0x3394 stencil) and no parameters;
// the last with ResolveFlags, the caller's colour, Z, stencil and parameters. The dest point is each tile's origin
// in both. (N&B's update dropped these arguments: always 0x300, then Z 1.0, stencil 0.) The game passes no
// pResolveRects (all three calls at 0x8272F2E0..0x8272F384): a non-null one is logged and the tiles used.
static u32 EndTilingImpl(u32 device, bool katamari, u32 resolve_flags, u32 rects_va, u32 dest_texture, u32 color_va,
                         float clear_z, u32 clear_stencil, u32 params);
u32 D3DDevice_EndTiling_hook(u32 device, u32 dest_texture, u32 color_va) {
  return EndTilingImpl(device, false, 0x300, 0, dest_texture, color_va, 1.0f, 0, 0);
}
static u32 EndTilingImpl(u32 device, bool katamari, u32 resolve_flags, u32 rects_va, u32 dest_texture, u32 color_va,
                         float clear_z, u32 clear_stencil, u32 params) {
  native::Device d = native::DeviceAt(device);
  if (!d)
    return 0;
  if (rects_va) {
    static bool once = false;
    if (!once) {
      once = true;
      RGPU_WARN("[renut] EndTiling with pResolveRects 0x{:08X}: resolving the tiles' rects instead", rects_va);
    }
  }
  const u32 count = d.W(native::off::TileCount);
  if (dest_texture) {
    for (u32 i = 0; i < count; ++i) {
      const u32 tile = native::off::TileRects + i * 16;
      ClearRect rect{i32(d.W(tile)), i32(d.W(tile + 4)), i32(d.W(tile + 8)), i32(d.W(tile + 12))};
      // A tile at the surface's edge resolves to a multiple of 8.
      const u32 surface = d.W(0x3264);  // katamari-nx: 0x31BC->0x31AC (TFU-nx: offset 0x31AC->0x31B4) (S4ep1-nx: offset 0x31B4->0x3264)
      const auto *s = surface ? GuestPtr<const be_u32>(surface) : nullptr;
      const u32 size = s ? u32(s[9]) : 0;
      if (u32(rect.x2) == (size >> 18) + 1)
        rect.x2 = i32((u32(rect.x2) + 7) & ~7u);
      if (u32(rect.y2) == native::Extract(size, 3, 15) + 1)
        rect.y2 = i32((u32(rect.y2) + 7) & ~7u);
      native::SetPredication(d, 3u << (i * 2));
      native::SetRenderState(d, kRenderStateHiZWriteEnable,
                             (d.B(native::off::Flags3) & 0x20) ? 0 : 2);
      const i32 dest_x = i32(d.W(tile)), dest_y = i32(d.W(tile + 4));
      if (i + 1 < count) {
        u32 flags = resolve_flags;
        if (katamari) {
          if (d.W(native::off::RenderTargets))
            flags |= 0x100;
          if (d.W(native::off::DepthStencil))
            flags |= 0x200;
        }
        Resolve(device, flags, &rect, dest_texture, dest_x, dest_y, 0, 0, device + 0x3440,  // katamari-nx: 0x3390->0x3380 (TFU-nx: offset 0x3380->0x3390) (S4ep1-nx: offset 0x3390->0x3440)
                d.F(0x3450), d.W(0x3454), 0);  // katamari-nx: 0x33A0->0x3390, 0x33A4->0x3394 (TFU-nx: offset 0x3390->0x33A0, 0x3394->0x33A4) (S4ep1-nx: offset 0x33A0->0x3450, 0x33A4->0x3454)
      } else {
        Resolve(device, resolve_flags, &rect, dest_texture, dest_x, dest_y, 0, 0, color_va,
                clear_z /*N&B: flt_82000C1C*/, clear_stencil, params);
      }
    }
  }
  native::SetPredication(d, 0);
  native::SetRenderState(d, kRenderStateHiZWriteEnable, 2);
  if (count != 1) {
    // Around the WorkerDpc callback.
    native::SetPredication(d, 0x80000000u);
    native::SetPredication(d, 0);
  }
  native::KickOff(device);

  d.SetB(native::off::Flags3, d.B(native::off::Flags3) & 0xCF);
  d.SetB(native::off::Flags, d.B(native::off::Flags) & 0xDF);
  d.SetW(0x3264, 0);  // katamari-nx: 0x31BC->0x31AC (TFU-nx: offset 0x31AC->0x31B4) (S4ep1-nx: offset 0x31B4->0x3264)
  d.SetW(native::off::PredicationSet, 0);
  native::UpdateTilePredicated(d);
  d.SetW(native::off::RunPredicationState, 0);
  d.SetW(native::off::Predication, ~0u);
  d.SetW(native::off::TilePredication, 0);
  d.SetW(native::off::RunPredication, 0);
  // sub_821DF678: the segment's fence, unless the device's command memory ran
  // out (EndCommandBuffer marks a device so until the next Begin).
  const bool out_of_memory = (d.B(native::off::Flags1) & 0x20) != 0;
  if (out_of_memory) {
    d.SetB(native::off::Flags1, d.B(native::off::Flags1) & 0xDF);
    native::RewindRing(d);
  } else {
    native::SetFence(device);
  }
  native::SetViewport(d, 0.0f, 0.0f, 65535.0f, 65535.0f, 0.0f, 1.0f);  // S4ep1-nx: TFU's full viewport

  if (!IsRecordingCommandBuffer(device)) {
    render_thread::Queue([] {
      SetTilingExtentLocked(0, 0);
      SetTilesLocked(nullptr, 0);
    });
  }
  if (out_of_memory) {
    native::ResetAllState(d);
    return kE_OUTOFMEMORY;
  }
  return 0;
}

// void D3DDevice_BeginExport(D3DDevice*, DWORD Index, D3DResource*, DWORD Flags)
// [0x82237FE8]. The renderer finds a draw's memexport streams in its shaders.
void D3DDevice_BeginExport_hook(u32 device, u32 index, u32 resource, u32 flags) {
  native::BeginExport(native::DeviceAt(device), index, resource, flags);
}

// void D3DDevice_EndExport(D3DDevice*, DWORD Index, D3DResource*, DWORD Flags)
// [0x82237D80]: the resource is busy until the current fence, which completes
// once the render thread has written the stream's results.
void D3DDevice_EndExport_hook(u32 device, u32 index, u32 resource, u32 flags) {
  native::EndExport(native::DeviceAt(device), index, resource, flags);
}

// void D3DDevice_Present(D3DDevice*) [0x826352B8]: the automatic back buffer
// resolved into the automatic front buffer, then Swap. N&B creates its device
// without an automatic front buffer, so its callers (XUI, the threaded movie
// player) have nothing to resolve to.
// TFU-nx (2026-11-11): the black-window diagnostic, part 2. Renut boot 6 showed the frames reach the swap (the auto
// front buffer is a host texture the GPU wrote, 1280x720) and the window stayed black. At Present numbers 60, 300,
// 700 and 1000 the auto back buffer (render target 0, before the resolve to the front buffer) is read back: the log
// gets its average colour and how many pixels are not black, and logs/tfu_frame_<n>.ppm gets the picture.
// TFU-nx (2026-11-11, part 3): what the window is given. Boot 7's back buffers held the game's picture (the
// NaturalMotion logo, the title screen), yet the window stayed black. The SDK's frame capture runs the scan-out pass
// (the gamma ramp, the fit to the window) into a texture of its own and reads it back: a picture there and a black
// window means the swap chain; black there means the front buffer or the scan-out.
static void TfuDiagCaptureScanOut(u32 present_number) {
  rex::ui::RawImage out;
  if (!CaptureFrame(out, 3000) || !out.width || !out.height) {
    RGPU_INFO("[tfu diag] Present {}: scan-out capture failed (no front buffer image, or timed out)", present_number);
    return;
  }
  uint64_t sum[3] = {}, lit = 0;
  for (u32 y = 0; y < out.height; ++y) {
    const u8 *row = out.data.data() + size_t(y) * out.stride;
    for (u32 x = 0; x < out.width; ++x) {
      sum[0] += row[x * 4];
      sum[1] += row[x * 4 + 1];
      sum[2] += row[x * 4 + 2];
      lit += (row[x * 4] | row[x * 4 + 1] | row[x * 4 + 2]) > 8;
    }
  }
  const uint64_t n = uint64_t(out.width) * out.height;
  RGPU_INFO("[tfu diag] Present {}: scan-out {}x{}, average RGB {}/{}/{}, {} of {} pixels not black", present_number,
            out.width, out.height, sum[0] / n, sum[1] / n, sum[2] / n, lit, n);
  if (FILE *f = std::fopen(fmt::format("logs/tfu_scanout_{}.ppm", present_number).c_str(), "wb")) {
    std::fprintf(f, "P6\n%u %u\n255\n", out.width, out.height);
    std::vector<u8> row(size_t(out.width) * 3);
    for (u32 y = 0; y < out.height; ++y) {
      const u8 *src = out.data.data() + size_t(y) * out.stride;
      for (u32 x = 0; x < out.width; ++x) {
        row[x * 3] = src[x * 4];
        row[x * 3 + 1] = src[x * 4 + 1];
        row[x * 3 + 2] = src[x * 4 + 2];
      }
      std::fwrite(row.data(), 1, row.size(), f);
    }
    std::fclose(f);
  }
}

static void TfuDiagReadBackBuffer(u32 device, u32 present_number, const std::string &tag) {
  auto *device_bytes = GuestPtr<const u8>(device);
  if (!device_bytes || !State().host_ready || IsRecordingCommandBuffer(device))
    return;
  struct Readback {
    std::vector<u8> state;
    u32 width = 1280, height = 720;
    std::vector<u32> words;
    bool done = false;
  };
  auto readback = std::make_shared<Readback>();
  readback->state.assign(device_bytes, device_bytes + d3d::kDeviceSnapshotSize);
  render_thread::Queue([readback] {
    readback->done = ReadRenderTargetLocked(readback->state.data(), 0, readback->width, readback->height,
                                            readback->words);
  });
  render_thread::Drain();
  if (!readback->done || readback->words.empty()) {
    RGPU_INFO("[tfu diag] Present {} {}: render target 0 not read back", present_number, tag);
    return;
  }
  uint64_t sum[3] = {};
  uint64_t lit = 0;
  for (u32 w : readback->words) {
    // TFU-nx: the words come back R G B A on the PC (boot 7: every low byte FF), not FF R G B as draw.h says.
    const u32 r = (w >> 24) & 0xFF, g = (w >> 16) & 0xFF, b = (w >> 8) & 0xFF;
    sum[0] += r;
    sum[1] += g;
    sum[2] += b;
    lit += (r | g | b) > 8;
  }
  const uint64_t n = readback->words.size();
  RGPU_INFO("[tfu diag] Present {} {}: render target 0 {}x{}, average RGB {}/{}/{}, {} of {} pixels not black; first "
            "word {:08X}, middle word {:08X}",
            present_number, tag, readback->width, readback->height, sum[0] / n, sum[1] / n, sum[2] / n, lit, n,
            readback->words.front(), readback->words[n / 2 + readback->width / 2]);
  const std::string path = fmt::format("logs/tfu_{}_{}.ppm", present_number, tag);
  if (FILE *f = std::fopen(path.c_str(), "wb")) {
    std::fprintf(f, "P6\n%u %u\n255\n", readback->width, readback->height);
    std::vector<u8> row(size_t(readback->width) * 3);
    for (u32 y = 0; y < readback->height; ++y) {
      for (u32 x = 0; x < readback->width; ++x) {
        const u32 w = readback->words[size_t(y) * readback->width + x];
        row[x * 3] = u8(w >> 24);
        row[x * 3 + 1] = u8(w >> 16);
        row[x * 3 + 2] = u8(w >> 8);
      }
      std::fwrite(row.data(), 1, row.size(), f);
    }
    std::fclose(f);
  }
}

void D3DDevice_Present_hook(u32 device) {
  native::Device d = native::DeviceAt(device);
  if (!d)
    return;
  static u32 tfu_presents = 0;  // TFU-nx (2026-11-11)
  // The frame read-backs (a GPU wait each) only when asked: set TFU_PRESENT_DIAG=1 (tools/win/present_test.ps1 does).
  static const bool tfu_diag = [] {
    const char *v = std::getenv("TFU_PRESENT_DIAG");
    return v && *v && *v != '0';
  }();
  ++tfu_presents;
  FlushDeferredVertices(device);
  float viewport[6];
  for (u32 i = 0; i < 6; ++i)
    viewport[i] = d.F(0x3218 + i * 4);  // katamari-nx: 0x3170->0x3160 (TFU-nx: offset 0x3160->0x3168) (S4ep1-nx: offset 0x3168->0x3218)
  i32 scissor[4];
  for (u32 i = 0; i < 4; ++i)
    scissor[i] = i32(d.W(native::off::ScissorRect + i * 4));
  native::SetRenderTarget(d, 0, d.W(native::off::AutoBackBuffer));
  if (g_tfu_capture_frame) {  // TFU-nx (2026-11-11, gameplay): the capture frame's end
    TfuDiagReadBackBuffer(device, g_tfu_capture_present, "back");
    RGPU_INFO("[tfu diag] capture frame {}: {} resolves", g_tfu_capture_present, g_tfu_capture_index);
    g_tfu_capture_frame = false;
  }
  if (tfu_diag && tfu_presents % 1800 == 0) {
    g_tfu_capture_frame = true;
    g_tfu_capture_present = tfu_presents;
    g_tfu_capture_index = 0;
  }
  if (tfu_diag && (tfu_presents == 60 || tfu_presents == 300 || tfu_presents == 700 || tfu_presents == 1000 ||
      tfu_presents == 1300 || tfu_presents == 1600 || tfu_presents == 1900))
    TfuDiagReadBackBuffer(device, tfu_presents, "back");  // TFU-nx (2026-11-11)
  // SynchronizeToPresentationInterval (0x82258778): its swap callback and wait
  // go into the ring; the swap is pending.
  native::SetPredication(d, 0);
  d.SetB(native::off::Flags1, d.B(native::off::Flags1) | d3d::kSwapFlagPending);
  const u32 front_buffer = d.W(native::off::AutoFrontBuffer);
  TfuPresentDiagPresent(front_buffer, d.W(native::off::AutoBackBuffer));  // TFU-nx (2026-11-11)
  if (front_buffer)
    Resolve(device, 0, nullptr, front_buffer, 0, 0, 0, 0, 0, 1.0f, 0, 0);
  native::KickOff(device);
  native::SetViewport(d, viewport[0], viewport[1], viewport[2], viewport[3], viewport[4],
                      viewport[5]);
  native::SetScissorRect(d, scissor[0], scissor[1], scissor[2], scissor[3]);
  native::Swap(device, front_buffer, device + 0x361C);  // S4ep1-nx: offset 0x3548->0x361C
  if (tfu_diag && (tfu_presents == 60 || tfu_presents == 300 || tfu_presents == 700 || tfu_presents == 1000 ||
      tfu_presents == 1300 || tfu_presents == 1600 || tfu_presents == 1900))
    TfuDiagCaptureScanOut(tfu_presents);  // TFU-nx (2026-11-11)  // katamari-nx: 0x3548->0x3530 (TFU-nx: offset 0x3530->0x3548)
  // katamari-nx: one game frame ends at Present (N&B's ended at its appMainDraw hook, renut_engine/FPS.cpp): frame
  // time, the Switch profiler's frame marks, the stall watchdog.
  g_banjo_game_frames.fetch_add(1, std::memory_order_relaxed);
  banjo::profiler::FrameBoundary();
}

// void D3DDevice_SetVertexShader(D3DDevice*, D3DVertexShader*) [0x821F90D8]
void D3DDevice_SetVertexShader_hook(u32 device, u32 shader) {
  native::SetVertexShader(native::DeviceAt(device), shader);
}

// void D3DDevice_SetPixelShader(D3DDevice*, D3DPixelShader*) [0x821F58A8]
void D3DDevice_SetPixelShader_hook(u32 device, u32 shader) {
  native::SetPixelShader(native::DeviceAt(device), shader);
}

// S4ep1-nx (2026-11-15): Sonic 4's D3D is linked with LTCG, which drops arguments that are always the same at every
// call site and renumbers the rest. Checked in the generated code of each function (which registers it reads, and
// where each value goes): these adapters take Sonic 4's arguments and call Renut's hook with the dropped ones filled.

// DrawVertices sub_821BFE48: (device r3, primitive r4, vertex count r5); the start vertex is always 0 (r23 = li 0,
// stepped by the count of each packet).
void S4_DrawVertices_hook(u32 device, u32 primitive, u32 vertex_count) {
  D3DDevice_DrawVertices_hook(device, primitive, 0, vertex_count);
}

// DrawIndexedVertices sub_821C0238: (device r3, start index r4, index count r5); the primitive type is always 6
// (triangle strip: "ori r30, r7, 6" in the draw packet, and each split repeats 2 indices) and the base vertex 0.
void S4_DrawIndexedVertices_hook(u32 device, u32 start_index, u32 index_count) {
  D3DDevice_DrawIndexedVertices_hook(device, 6, 0, start_index, index_count);
}

// Resolve sub_821C1C80: (device r3, flags r4, rect r5, dest texture r6, point r7, clear colour r8, r9 unread,
// clear stencil r10, clear Z f1). r8 is loaded as a vector (lvx128, a constant when null); r10 goes to the depth clear
// (sub_821C1710 ORs it under Z << 8). Dest level, slice and the parameters are always 0, none read from the stack.
void S4_Resolve_hook(u32 device, u32 flags, u32 rect_va, u32 dest_texture, u32 point_va, u32 color_va,
                     u32 /*r9*/, u32 clear_stencil, double clear_z) {
  D3DDevice_Resolve_hook(device, flags, rect_va, dest_texture, point_va, 0, 0, color_va, clear_z, 0, clear_stencil, 0);
}

} // namespace

BANJO_RENUT_HOOK(sub_821BFE48, S4_DrawVertices_hook);  // TFU-nx: Katamari sub_824D9D08, d3d_match 1.00 (next 0.71);  // D3DDevice_DrawVertices  // katamari-nx: N&B sub_821FB880, signature 0.90 (next 0.77)  // S4ep1-nx: TFU sub_8281E2B8, seed los1 d3d_match 0.98 (same XDK) D3DDevice_DrawVertices
BANJO_RENUT_HOOK(sub_821C0238, S4_DrawIndexedVertices_hook);  // TFU-nx: Katamari sub_824DA0F0, d3d_match 1.00 (next 0.72);  // D3DDevice_DrawIndexedVertices  // katamari-nx: N&B sub_8263F358, signature 0.87 (next 0.78)  // S4ep1-nx: TFU sub_8281E6A0, seed los1 d3d_match 0.90 (same XDK) D3DDevice_DrawIndexedVertices
BANJO_RENUT_UNHOOKED_HOOK(sub_8281EAF8, D3DDevice_DrawIndexedTessellatedVertices_hook);  // TFU-nx: Katamari sub_824DA9C0, d3d_match 0.99 (next 0.66);  // D3DDevice_DrawIndexedTessellatedVertices  // katamari-nx: N&B sub_821FF958, signature 0.86 (next 0.65)  // S4ep1-nx: no counterpart found in Sonic 4 (docs/renut-map.md, still open)
BANJO_RENUT_HOOK(sub_821BFE00, D3DDevice_DrawVerticesUP_hook);  // TFU-nx: no counterpart in TFU (docs/renut-map.md);  // D3DDevice_DrawVerticesUP  // katamari-nx: N&B sub_821FD698, D3DDevice_DrawVerticesUP: BeginVertices-style reserve, reads VerticesRingEnd +13428 (N&B +13444) and the flags +10940; sig 0.69/0.55  // S4ep1-nx: was unhooked in TFU (sub_824D9758); D3DDevice_DrawVerticesUP: LoS1 d3d_match 1.00 (same XDK)
BANJO_RENUT_UNHOOKED_HOOK(sub_8263F2B8, D3DDevice_DrawIndexedVerticesUP_hook);  // D3DDevice_DrawIndexedVerticesUP  // katamari-nx: not hooked (D3DDevice_DrawIndexedVerticesUP: no counterpart found (sig 0.32))  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_HOOK(sub_821BF968, D3DDevice_BeginVertices_hook);  // TFU-nx: no counterpart in TFU (docs/renut-map.md);  // D3DDevice_BeginVertices  // katamari-nx: N&B sub_821FBD30, signature 0.87 (next 0.70)  // S4ep1-nx: was unhooked in TFU (sub_824D92B8); D3DDevice_BeginVertices: LoS1 d3d_match 1.00 (same XDK)
BANJO_RENUT_HOOK(sub_821B2FD0, D3DDevice_ClearF_hook);  // TFU-nx: Katamari sub_824F3248, d3d_match 1.00 (next 0.41);  // D3DDevice_ClearF  // katamari-nx: N&B sub_8224F110, D3DDevice_ClearF: sig 0.71/0.40; reads the targets +12432.. and tiling surfaces +12720.. as N&B +12448 / +12736  // S4ep1-nx: TFU sub_822F67F0, seed los1 d3d_match 1.00 (same XDK) D3DDevice_ClearF
BANJO_RENUT_UNHOOKED_HOOK(sub_822F6918, D3DDevice_Clear_hook);  // TFU-nx: Katamari sub_824F3370, d3d_match 1.00 (next 0.37);  // D3DDevice_Clear  // katamari-nx: N&B sub_8224F238, signature 0.84 (next 0.36)  // S4ep1-nx: no counterpart found in Sonic 4 (docs/renut-map.md, still open)
BANJO_RENUT_HOOK(sub_821C1C80, S4_Resolve_hook);  // TFU-nx: Katamari sub_824F12B8, by hand: 0.82, between ClearF/Clear and SetPredication as in Katamari;  // D3DDevice_Resolve  // katamari-nx: N&B sub_821FC7A0, signature 0.78 (next 0.33)  // S4ep1-nx: TFU sub_822F47A8, seed Resolve: called by Present in both; TFU d3d_match top 0.48
BANJO_RENUT_UNHOOKED_HOOK_SHARED(sub_82246798, D3DDevice_BeginTiling_hook);  // D3DDevice_BeginTiling: N&B's update form, see below  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_UNHOOKED_HOOK(sub_821E17C0, D3DDevice_EndTiling_hook);  // D3DDevice_EndTiling: N&B's update form, see below  // S4ep1-nx: unhooked as in TFU
// katamari-nx (2026-10-30): XDK 2.0.5632's BeginTiling (sub_824EFBC8: it stores the count at TileCount +0x31C4 and
// the targets at TilingSurfaces +0x31B0, where the zones put N&B's fields; the game calls it at 0x8271E62C) and
// EndTiling (sub_824F0150: sig 0.66/0.36, resolves through Resolve sub_824F12B8; called at 0x8272F2E0..0x8272F384),
// with their full arguments read from the registers (the float argument takes r8's place: ClearStencil is r9).
BANJO_RENUT_UNHOOKED_HOOK_RAW(sub_822F36E0) {  // TFU-nx: BeginTiling: d3d_match 0.69 (next 0.17), just before EndTiling; same argument registers r4-r7, r9  // S4ep1-nx: no counterpart found in Sonic 4 (docs/renut-map.md, still open)
  {  // TFU-nx (2026-11-11): the first calls' arguments, to check the 0.69 pairing in play
    static int logged = 0;
    if (logged < 6) {
      ++logged;
      const auto *rects = GuestPtr<const be_u32>(ctx.r6.u32);
      RGPU_INFO("[tfu diag] BeginTiling: flags {:X}, count {}, rects {:08X} (first {} {} {} {}), colour {:08X}, Z {}, "
                "stencil {}", ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, rects ? u32(rects[0]) : 0, rects ? u32(rects[1]) : 0,
                rects ? u32(rects[2]) : 0, rects ? u32(rects[3]) : 0, ctx.r7.u32, ctx.f1.f64, ctx.r9.u32);
    }
  }
  BeginTilingImpl(ctx.r3.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r4.u32, float(ctx.f1.f64), ctx.r9.u32);
}
BANJO_RENUT_UNHOOKED_HOOK_RAW(sub_822F3B78) {  // TFU-nx: EndTiling: d3d_match 1.00  // S4ep1-nx: no counterpart found in Sonic 4 (docs/renut-map.md, still open)
  if (g_tfu_capture_frame && g_tfu_capture_index < 40)  // TFU-nx (2026-11-11): the tiled target before its resolves
    TfuDiagReadBackBuffer(ctx.r3.u32, g_tfu_capture_present,
                          fmt::format("endtiling{:02}_{:X}_{:08X}", g_tfu_capture_index++, ctx.r4.u32, ctx.r6.u32));
  ctx.r3.u64 = EndTilingImpl(ctx.r3.u32, true, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, float(ctx.f1.f64),
                             ctx.r9.u32, ctx.r10.u32);
}
BANJO_RENUT_UNHOOKED_HOOK_SHARED(sub_82237FE8, D3DDevice_BeginExport_hook);  // D3DDevice_BeginExport  // katamari-nx: not hooked (D3DDevice_BeginExport: XDK 2.0.5632 has no memexport functions (as feedingfrenzy2-nx found for 6534))  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_UNHOOKED_HOOK_SHARED(sub_82237D80, D3DDevice_EndExport_hook);  // D3DDevice_EndExport  // katamari-nx: not hooked (D3DDevice_EndExport: as above)  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_UNHOOKED_HOOK(sub_8249E2F8_whole, D3DDevice_Present_hook);  // S4ep1-nx: replaced by the raw hook at the end of the file; was  // TFU-nx: Katamari sub_824E9428, d3d_match 0.92 (next 0.34);  // D3DDevice_Present  // katamari-nx: N&B sub_826352B8, D3DDevice_Present: SynchronizeToPresentationInterval sub_824E89E8, Resolve sub_824F12B8, KickOff sub_824E0298, then Swap sub_824E8E90; sig 0.43/0.26  // S4ep1-nx: TFU sub_822E98D8, seed Present: the same callees as TFU's (SetRenderTarget, KickOff, Swap, SynchronizeToPresentationInterval, Resolve, SetScissorRect, sub_822C0450's pair); outside the D3D block (LTCG)
BANJO_RENUT_HOOK(sub_821B9EC8, D3DDevice_SetVertexShader_hook);  // TFU-nx: Katamari sub_824D4930, d3d_match 0.99 (next 0.90);  // D3DDevice_SetVertexShader  // katamari-nx: N&B sub_821F90D8, signature 0.87 (next 0.75)  // S4ep1-nx: TFU sub_822E3E80, D3DDevice_SetVertexShader: reads and writes the device's vertex shader +0x3248 (LoS1 12872); TFU match top
BANJO_RENUT_HOOK(sub_821B9B20, D3DDevice_SetPixelShader_hook);  // TFU-nx: Katamari sub_824D4630, d3d_match 0.96 (next 0.88);  // D3DDevice_SetPixelShader  // katamari-nx: N&B sub_821F58A8, signature 0.85 (next 0.73)  // S4ep1-nx: TFU sub_822E3B80, D3DDevice_SetPixelShader: reads and writes the device's pixel shader +0x3244 (LoS1 12868); TFU match top

// S4ep1-nx (2026-11-15): Sonic 4's Present (sub_8249E2F8) takes no arguments: the game's NN layer wraps it around
// the inlined D3D Present. Its parts, from the generated code:
//  1. once: if [0x824F13CC] is 0, sub_82488698(0x824F1330);
//  2. if [0x8252FA48] is 0, D3D Present on the device at [0x82586168] (Renut's hook); otherwise the game flips its own
//     two front buffers: SynchronizeToPresentationInterval(device, 0), Swap(device, [0x82505BC0 + 4 * [0x82586180]],
//     0), sub_82493890 (flips the index);
//  3. if [0x82586118] is set: sub_82223E98([obj + 8]), then sub_82488738(obj, the frame time in ms from three float
//     constants).
REXCVAR_DEFINE_INT32(s4_diag_capture_present, 0, "S4ep1-nx",
                     "Diagnostic: the Present whose frame is read back before each resolve and EndTiling (0 = off)");
REXCVAR_DEFINE_BOOL(s4_pacer_fix, true, "S4ep1-nx",
                    "Sonic 4's frame thread: a Present that comes after a deadline step does not wake it again, so "
                    "the game steps 60 times a second when frames are late (off: the game's own rule, which ran up "
                    "to 1.45x in the GPU dips). draw.cpp, sub_8249E2F8's hook.");
REX_EXTERN(__imp__sub_8249E2F8);
REX_EXTERN(sub_82488698);
REX_EXTERN(sub_821C0920);
REX_EXTERN(sub_821C0E58);
REX_EXTERN(sub_82493890);
REX_EXTERN(sub_82223E98);
REX_EXTERN(sub_82488738);
BANJO_RENUT_HOOK_RAW(sub_8249E2F8) {
  auto load_u32 = [](u32 va) -> u32 {
    const auto *p = GuestPtr<const rex::be_u32>(va);
    return p ? u32(*p) : 0;
  };
  auto load_f32 = [&](u32 va) -> float { return std::bit_cast<float>(load_u32(va)); };

  if (load_u32(0x824F13CC) == 0) {
    ctx.r3.u64 = 0x824F1330;
    sub_82488698(ctx, base);
  }
  const u32 device = load_u32(0x82586168);
  {  // S4ep1-nx (2026-11-15, the title's white sky): TFU-nx's frame capture for the game's own flips (which skip
     // D3DDevice_Present_hook): s4_diag_capture_present = N reads render target 0 back before each resolve and
     // EndTiling of Present N's frame, and the back buffer at its end (logs/tfu_<N>_*.ppm).
    static const int32_t capture_at = REXCVAR_GET(s4_diag_capture_present);
    static u32 s4_presents = 0;
    ++s4_presents;
    if (capture_at > 0) {
      if (g_tfu_capture_frame) {
        TfuDiagReadBackBuffer(device, g_tfu_capture_present, "back");
        RGPU_INFO("[s4 diag] capture frame {}: {} resolves", g_tfu_capture_present, g_tfu_capture_index);
        g_tfu_capture_frame = false;
      }
      if (s4_presents == u32(capture_at)) {
        g_tfu_capture_frame = true;
        g_tfu_capture_present = s4_presents;
        g_tfu_capture_index = 0;
      }
    }
  }
  if (load_u32(0x8252FA48) == 0) {
    D3DDevice_Present_hook(device);
  } else {
    ctx.r3.u64 = device;
    ctx.r4.u64 = 0;
    sub_821C0920(ctx, base);
    ctx.r3.u64 = device;
    ctx.r4.u64 = load_u32(0x82505BC0 + 4 * load_u32(0x82586180));
    ctx.r5.u64 = 0;
    sub_821C0E58(ctx, base);
    sub_82493890(ctx, base);
  }
  // S4ep1-nx (2026-11-15, s4_pacer_fix): the frame thread (sub_82488B10) steps when this Present sets its event OR
  // when its deadline (the last Present plus 16,667 us) passes first. On a 360 the Presents land on refreshes (16.7 or
  // 33.3 ms apart), so a late frame gives one deadline step and one Present step in 33.3 ms: 60 steps a second. With
  // Renut a Present can come at any time: a 20 ms frame gave a deadline step at 16.7 ms and a Present step at 20 ms,
  // two in 20 ms: the game ran up to 1.45x in the console's GPU dips (Switch test 2: 77-87 steps a second at 45-50
  // fps, 60 at 60 fps). With the fix a Present that comes after a deadline step does not wake the frame thread and
  // leaves its deadline on the 16.7 ms grid; one that comes before it (a frame on time) works as before.
  static u32 s4_entries_at_last = 0;
  static bool s4_last_woke = false;
  const u32 s4_entries = s4::g_pacer_entries.load(std::memory_order_acquire);
  const u32 s4_since = s4_entries - s4_entries_at_last;
  const bool s4_hold = REXCVAR_GET(s4_pacer_fix) && s4_since > (s4_last_woke ? 1u : 0u);
  s4_entries_at_last = s4_entries;
  const u32 obj = load_u32(0x82586118);
  s4_last_woke = obj && !s4_hold;
  if (obj && s4_hold) {
    s4::g_present_skips.fetch_add(1, std::memory_order_relaxed);
  } else if (obj) {
    s4::g_present_wakes.fetch_add(1, std::memory_order_relaxed);
    ctx.r3.u64 = load_u32(obj + 8);
    sub_82223E98(ctx, base);
    const float ms = float(std::fma(double(load_f32(0x824D9B14)), double(load_f32(0x82094ADC)),
                                    double(load_f32(0x8207C954))));
    ctx.r3.u64 = load_u32(0x82586118);
    ctx.r4.u64 = u32(rex::ppc::fctiwz(double(ms)));
    sub_82488738(ctx, base);
  }
}

// S4ep1-nx (2026-11-15): Sonic 4 draws 1920x1080 with predicated tiling (the colour and depth targets do not fit in
// EDRAM at once), and its BeginTiling and EndTiling were not mapped: the game's own ran, Renut never knew of the
// tiles, and only the first tile reached the screen (Mark's screenshot: the bottom from row 544 down black).
// Found by the device's TileCount (+0x327C): BeginTiling sub_821C2D30 stores it, EndTiling sub_821C30D0 loops over it
// calling Resolve. Their LTCG arguments, from the generated code:
//  - BeginTiling (device r3, count r4, rects r5, clear colour r6, clear Z f1, clear stencil r8); r7 unread; the
//    flags are always 0 (stored as 0 at +0x3434).
//  - EndTiling (device r3, resolve flags r4, dest texture r5, clear colour r6, clear stencil r8, clear Z f1); r7
//    unread; no resolve rects (the tiles' own); each tile but the last resolves with flags | 0x100 (colour bound,
//    +0x3148) | 0x200 (depth bound, +0x3158) and BeginTiling's clear values, the last with the caller's.
BANJO_RENUT_HOOK_RAW(sub_821C2D30) {
  static int logged = 0;
  if (logged < 4) {
    ++logged;
    const auto *rects = GuestPtr<const rex::be_u32>(ctx.r5.u32);
    RGPU_INFO("[s4] BeginTiling: count {}, rects {:08X} (first {} {} {} {}, second {} {} {} {}), colour {:08X}, Z {}, "
              "stencil {}", ctx.r4.u32, ctx.r5.u32, rects ? u32(rects[0]) : 0, rects ? u32(rects[1]) : 0,
              rects ? u32(rects[2]) : 0, rects ? u32(rects[3]) : 0, rects && ctx.r4.u32 > 1 ? u32(rects[4]) : 0,
              rects && ctx.r4.u32 > 1 ? u32(rects[5]) : 0, rects && ctx.r4.u32 > 1 ? u32(rects[6]) : 0,
              rects && ctx.r4.u32 > 1 ? u32(rects[7]) : 0, ctx.r6.u32, ctx.f1.f64, ctx.r8.u32);
  }
  BeginTilingImpl(ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, 0, float(ctx.f1.f64), ctx.r8.u32);
}
BANJO_RENUT_HOOK_RAW(sub_821C30D0) {
  static int logged = 0;
  if (logged < 4) {
    ++logged;
    RGPU_INFO("[s4] EndTiling: flags {:X}, dest {:08X}, colour {:08X}, Z {}, stencil {}", ctx.r4.u32, ctx.r5.u32,
              ctx.r6.u32, ctx.f1.f64, ctx.r8.u32);
  }
  if (g_tfu_capture_frame && g_tfu_capture_index < 40)  // S4ep1-nx: the tiled target before its resolves
    TfuDiagReadBackBuffer(ctx.r3.u32, g_tfu_capture_present,
                          fmt::format("endtiling{:02}_{:X}_{:08X}", g_tfu_capture_index++, ctx.r4.u32, ctx.r5.u32));
  ctx.r3.u64 = EndTilingImpl(ctx.r3.u32, true, ctx.r4.u32, 0, ctx.r5.u32, ctx.r6.u32, float(ctx.f1.f64), ctx.r8.u32,
                             0);
}
