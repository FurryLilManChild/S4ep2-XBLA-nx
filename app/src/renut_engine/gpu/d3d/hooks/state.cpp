/**
 * @file    gpu/d3d/hooks/state.cpp
 * @brief   N&B's D3D state calls, hooked with the native ports (d3d_native.h).
 *
 * The render and sampler state setters are the ones N&B's state tables name
 * (D3D::InitializeApiState puts them into every device's dispatch table,
 * which the game's inline SetRenderState / SetSamplerState call through).
 * The rest are D3DDevice_Set* and the CDevice internals that write the ring.
 */
#include <cstring>

#include <rex/hook.h>

#include "renut_engine/gpu/banjo_renut_select.h"
#include <rex/types.h>

#include "renut_engine/gpu/d3d/d3d_native.h"
#include "renut_engine/gpu/guest/guest_memory.h"

using namespace renut::gpu;

namespace {

template <u32 State>
void SetRenderStateHook(u32 device, u32 value) {
  native::SetRenderState(native::DeviceAt(device), State, value);
}

template <u32 State>
void SetSamplerStateHook(u32 device, u32 sampler, u32 value) {
  native::SetSamplerState(native::DeviceAt(device), sampler, State, value);
}

template <u32 State>
u32 GetRenderStateHook(u32 device) {
  return native::GetRenderState(native::DeviceAt(device), State);
}

template <u32 State>
u32 GetSamplerStateHook(u32 device, u32 sampler) {
  return native::GetSamplerState(native::DeviceAt(device), sampler, State);
}

// The device's AddRef of a resource it hands out (D3DResource_AddRef,
// resource.cpp).
u32 AddRefResource(u32 resource) {
  return native::AddRefResource(resource);
}

// D3DSurface *GetRenderTarget(D3DDevice*, DWORD Index) [0x82636ED0]
u32 D3DDevice_GetRenderTarget_hook(u32 device, u32 index) {
  const native::Device d = native::DeviceAt(device);
  const u32 surface = d && index < 4 ? d.W(native::off::RenderTargets + index * 4) : 0;
  if (surface)
    AddRefResource(surface);
  return surface;
}

// D3DSurface *GetDepthStencilSurface(D3DDevice*) [0x82636F10]
u32 D3DDevice_GetDepthStencilSurface_hook(u32 device) {
  const native::Device d = native::DeviceAt(device);
  const u32 surface = d ? d.W(native::off::DepthStencil) : 0;
  if (surface)
    AddRefResource(surface);
  return surface;
}

// D3DVertexBuffer *GetStreamSource(D3DDevice*, UINT StreamNumber,
//     UINT *pOffsetInBytes, UINT *pStride) [0x82636E40]: the offset is the
// stream's fetch constant address less the buffer's.
u32 D3DDevice_GetStreamSource_hook(u32 device, u32 stream, mapped_u32 offset, mapped_u32 stride) {
  const native::Device d = native::DeviceAt(device);
  if (!d || stream >= 16)
    return 0;
  const u32 buffer = d.W(native::off::StreamSources + stream * 4);
  u32 bytes = 0;
  if (buffer) {
    const u32 fetch = d.W(native::off::TextureFetch + (95 - stream) * 8);
    bytes = fetch - native::GpuAddress(GuestPtr<const be_u32>(buffer)[6]);
    AddRefResource(buffer);
  }
  if (offset)
    *offset = bytes;
  if (stride)
    *stride = u32(d.B(native::off::StreamStrides + stream)) << 2;
  return buffer;
}

// void D3DDevice_GetViewport(D3DDevice*, D3DVIEWPORT9*) [0x82244B18]
void D3DDevice_GetViewport_hook(u32 device, mapped_u32 viewport) {
  const native::Device d = native::DeviceAt(device);
  if (!d || !viewport)
    return;
  for (u32 i = 0; i < 4; ++i)  // fctidz, the low word
    viewport[i] = u32(native::TruncateToI64(d.F(0x3218 + i * 4)));  // katamari-nx: 0x3170->0x3160 (TFU-nx: offset 0x3160->0x3168) (S4ep1-nx: offset 0x3168->0x3218)
  viewport[4] = d.W(0x3228);  // katamari-nx: 0x3180->0x3170 (TFU-nx: offset 0x3170->0x3178) (S4ep1-nx: offset 0x3178->0x3228)
  viewport[5] = d.W(0x322C);  // katamari-nx: 0x3184->0x3174 (TFU-nx: offset 0x3174->0x317C) (S4ep1-nx: offset 0x317C->0x322C)
}

// void D3DDevice_SetClipPlane(D3DDevice*, DWORD Index, const float *pPlane) [0x826370E8]
void D3DDevice_SetClipPlane_hook(u32 device, u32 index, mapped_u32 plane) {
  native::Device d = native::DeviceAt(device);
  if (!d || !plane || index >= 6)
    return;
  for (u32 i = 0; i < 4; ++i)
    d.SetW(0x2820 + index * 16 + i * 4, plane[i]);
  d.Dirty(4, u64(1) << (54 - index));
}

// void D3DDevice_SetVertexFetchConstant(D3DDevice*, UINT VertexFetchRegister,
//     D3DVertexBuffer*, UINT Offset) [0x82637138]: a vertex fetch slot of the
// 3 in a fetch constant, made a vertex fetch constant first.
void D3DDevice_SetVertexFetchConstant_hook(u32 device, u32 reg, u32 vertex_buffer, u32 offset) {
  native::Device d = native::DeviceAt(device);
  const auto *vb = vertex_buffer ? GuestPtr<const be_u32>(vertex_buffer) : nullptr;
  if (!d || !vb || reg > 0x5F)
    return;
  const u32 slot = (0x5F - reg) / 3;
  const u32 part = (0x5F - reg) - slot * 3;
  const u32 constant = native::off::TextureFetch + slot * 24;
  const u32 type = d.W(constant) & 3;
  if (type == 0 || type == 2)
    std::memset(d.bytes + constant, 0, 24);
  d.TouchRange(constant, 24);  // banjo-nx (2026-10-26, banjo_renut_tags)
  d.SetW(constant, native::Insert(d.W(constant), 1, 0, 2));
  d.Dirty(3, u64(1) << (31 - slot));
  d.SetW(constant + part * 8, native::GpuAddress(u32(vb[6]) + offset));
  d.SetW(constant + part * 8 + 4, native::Insert(u32(vb[7]) - offset, 1, 28, 2));
}

// The boolean and integer shader constants (D3DDevice_Set{Vertex,Pixel}
// ShaderConstant{B,I}): bits from 0x2780 (vertex) / 0x2790 (pixel), a word
// of x, y, z bytes each from 0x27A0 / 0x27E0.
void SetShaderConstantsB(u32 device, u32 base, u32 start, mapped_u32 data, u32 count) {
  native::Device d = native::DeviceAt(device);
  if (!d || !data)
    return;
  for (u32 i = 0; i < count; ++i, ++start) {
    const u32 word = base + (start >> 5) * 4;
    const u32 bit = 1u << (start & 31);
    d.SetW(word, (d.W(word) & ~bit) | ((u32(data[i]) & 1) << (start & 31)));
  }
  d.Dirty(4, u64(1) << 56);
}

void SetShaderConstantsI(u32 device, u32 base, u32 start, mapped_u32 data, u32 count) {
  native::Device d = native::DeviceAt(device);
  if (!d || !data)
    return;
  for (u32 i = 0; i < count; ++i) {
    const u32 x = u32(data[i * 4]) & 0xFF, y = u32(data[i * 4 + 1]) & 0xFF;
    const u32 z = u32(data[i * 4 + 2]) & 0xFF;
    d.SetW(base + (start + i) * 4, (z << 16) | (y << 8) | x);
  }
  d.Dirty(4, u64(1) << 56);
}

// void D3DDevice_SetVertexShaderConstantB(D3DDevice*, UINT StartRegister,
//     const BOOL*, UINT BoolCount) [0x821F0B40], and the others alike.
void D3DDevice_SetVertexShaderConstantB_hook(u32 device, u32 start, mapped_u32 data, u32 count) {
  SetShaderConstantsB(device, 0x2780, start, data, count);
}
void D3DDevice_SetPixelShaderConstantB_hook(u32 device, u32 start, mapped_u32 data, u32 count) {
  SetShaderConstantsB(device, 0x2790, start, data, count);
}
void D3DDevice_SetVertexShaderConstantI_hook(u32 device, u32 start, mapped_u32 data, u32 count) {
  SetShaderConstantsI(device, 0x27A0, start, data, count);
}
void D3DDevice_SetPixelShaderConstantI_hook(u32 device, u32 start, mapped_u32 data, u32 count) {
  SetShaderConstantsI(device, 0x27E0, start, data, count);
}

// void D3DDevice_EndIndexedVertices(D3DDevice*) [0x8263ED90]: the inline end
// of BeginIndexedVertices, back to the ring pointer it saved.
void D3DDevice_EndIndexedVertices_hook(u32 device) {
  native::Device d = native::DeviceAt(device);
  if (d)
    d.SetW(native::off::Ring, d.W(0x3554));  // katamari-nx: 0x3484->0x3474 (TFU-nx: offset 0x3474->0x3484) (S4ep1-nx: offset 0x3484->0x3554)
}

// void D3DDevice_SetTexture(D3DDevice*, DWORD Sampler, D3DBaseTexture*,
//     UINT64 PendingMask3) [0x821FE558]; the mask is the sampler's fetch
// constant bit, which the port derives from Sampler.
void D3DDevice_SetTexture_hook(u32 device, u32 sampler, u32 texture) {
  native::SetTexture(native::DeviceAt(device), sampler, texture);
}

// void D3DDevice_SetStreamSource(D3DDevice*, UINT StreamNumber, D3DVertexBuffer*,
//     UINT OffsetInBytes, UINT Stride, UINT64 PendingMask3) [0x821F9438]
void D3DDevice_SetStreamSource_hook(u32 device, u32 stream, u32 vertex_buffer, u32 offset,
                                    u32 stride) {
  native::SetStreamSource(native::DeviceAt(device), stream, vertex_buffer, offset, stride);
}

// void D3DDevice_SetIndices(D3DDevice*, D3DIndexBuffer*) [0x821DE438]
void D3DDevice_SetIndices_hook(u32 device, u32 index_buffer) {
  native::SetIndices(native::DeviceAt(device), index_buffer);
}

// void D3DDevice_SetVertexDeclaration(D3DDevice*, D3DVertexDeclaration*) [0x821D5060]
void D3DDevice_SetVertexDeclaration_hook(u32 device, u32 declaration) {
  native::SetVertexDeclaration(native::DeviceAt(device), declaration);
}

// void D3DDevice_SetViewport(D3DDevice*, const D3DVIEWPORT9*) [0x821F2090]
void D3DDevice_SetViewport_hook(u32 device, u32 viewport) {
  native::SetViewport(native::DeviceAt(device), viewport);
}

// void D3D::SetViewport(D3DDevice*, float X, float Y, float Width, float Height,
//     float MinZ, float MaxZ) [0x821F2110]
void D3D_SetViewport_hook(u32 device, float x, float y, float width, float height, float min_z,
                          float max_z) {
  native::SetViewport(native::DeviceAt(device), x, y, width, height, min_z, max_z);
}

// void D3DDevice_SetScissorRect(D3DDevice*, const RECT*) [0x821F2368]
void D3DDevice_SetScissorRect_hook(u32 device, mapped_u32 rect) {
  if (!rect)
    return;
  native::SetScissorRect(native::DeviceAt(device), i32(u32(rect[0])), i32(u32(rect[1])),
                         i32(u32(rect[2])), i32(u32(rect[3])));
}

// void D3D::SetSurfaceClip(CDevice*, int, int, int, int) [0x821F2460]: the
// window scissor and per-tile clip packets, nothing else.
void D3D_SetSurfaceClip_hook(u32 /*device*/, u32, u32, u32, u32) {}

// void D3DDevice_SetRenderTarget(D3DDevice*, DWORD Index, D3DSurface*) [0x82248BD0]
void D3DDevice_SetRenderTarget_hook(u32 device, u32 index, u32 surface) {
  native::SetRenderTarget(native::DeviceAt(device), index, surface);
}

// void D3DDevice_SetDepthStencilSurface(D3DDevice*, D3DSurface*) [0x82248FB0]
void D3DDevice_SetDepthStencilSurface_hook(u32 device, u32 surface) {
  native::SetDepthStencilSurface(native::DeviceAt(device), surface);
}

// void D3D::SetSurfaceInfo(CDevice*, D3DSurface*) [0x82248F20]
void D3D_SetSurfaceInfo_hook(u32 device, u32 surface) {
  native::SetSurfaceInfo(native::DeviceAt(device), surface);
}

// void D3DDevice_SetPredication(D3DDevice*, DWORD PredicationMask) [0x821F9A48]
void D3DDevice_SetPredication_hook(u32 device, u32 mask) {
  native::SetPredication(native::DeviceAt(device), mask);
}

// void D3DDevice_SetCommandBufferPredication(D3DDevice*, DWORD TilePredication,
//     DWORD RunPredication) [0x826411C0]
void D3DDevice_SetCommandBufferPredication_hook(u32 device, u32 tile_predication,
                                                u32 run_predication) {
  native::SetCommandBufferPredication(native::DeviceAt(device), tile_predication,
                                      run_predication);
}

// void D3DDevice_SetShaderGPRAllocation(D3DDevice*, DWORD Flags,
//     DWORD VertexShaderCount, DWORD PixelShaderCount) [0x821EFD38]
void D3DDevice_SetShaderGPRAllocation_hook(u32 device, u32 flags, u32 vertex_count,
                                           u32 pixel_count) {
  native::SetShaderGPRAllocation(native::DeviceAt(device), flags, vertex_count, pixel_count);
}

// void D3D::CleanupGprAllocationSetup(CDevice*) [0x82637208]
void D3D_CleanupGprAllocationSetup_hook(u32 device) {
  native::CleanupGprAllocationSetup(native::DeviceAt(device));
}

// void D3DDevice_SetScreenExtentQueryMode(D3DDevice*, D3DSCREENEXTENTQUERYMODE)
// [0x826414C0]
void D3DDevice_SetScreenExtentQueryMode_hook(u32 device, u32 mode) {
  native::SetScreenExtentQueryMode(native::DeviceAt(device), mode);
}

// void D3DDevice_UnsetAll(D3DDevice*) [0x82634C10]
void D3DDevice_UnsetAll_hook(u32 device) {
  native::UnsetAll(native::DeviceAt(device));
}

// void D3D::ResetAllState(CDevice*, BOOL) [0x82255C98]
void D3D_ResetAllState_hook(u32 device, u32 /*skip_packets*/) {
  native::ResetAllState(native::DeviceAt(device));
}

// void D3D::InvalidateConstants(D3DDevice*) [0x82634B90]
void D3D_InvalidateConstants_hook(u32 device) {
  native::InvalidateConstants(native::DeviceAt(device));
}

// void D3DDevice_BlockUntilIdle(D3DDevice*) [0x82640010] (IDA: the second
// D3D::InvalidateConstants): waits for the device's current fence (N&B calls
// it before releasing a device).
void D3DDevice_BlockUntilIdle_hook(u32 device) {
  native::InvalidateConstantsAndWait(native::DeviceAt(device));
}

} // namespace

// Render states, by their index in N&B's table (off_82E506F0).
BANJO_RENUT_HOOK(sub_821B3F68, SetRenderStateHook<10>);  // TFU-nx: Katamari sub_824DC2F8, render state 10 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_ZEnable  // katamari-nx: N&B sub_82635740, render state 10 setter (state tables, this game index 10)  // S4ep1-nx: TFU sub_822D55C8, state table row 10 col 1
BANJO_RENUT_HOOK(sub_821B3FD8, SetRenderStateHook<11>);  // TFU-nx: Katamari sub_824DC368, render state 11 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_ZFunc  // katamari-nx: N&B sub_821CB040, render state 11 setter (state tables, this game index 11)  // S4ep1-nx: TFU sub_822D5638, state table row 11 col 1
BANJO_RENUT_HOOK(sub_821B3FA8, SetRenderStateHook<12>);  // TFU-nx: Katamari sub_824DC338, render state 12 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_ZWriteEnable  // katamari-nx: N&B sub_82635780, render state 12 setter (state tables, this game index 12)  // S4ep1-nx: TFU sub_822D5608, state table row 12 col 1
BANJO_RENUT_HOOK(sub_821B3850, SetRenderStateHook<13>);  // TFU-nx: Katamari sub_824DBBD8, render state 13 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_FillMode  // katamari-nx: N&B sub_8225D148, render state 13 setter (state tables, this game index 13)  // S4ep1-nx: TFU sub_822D4EA8, state table row 13 col 1
BANJO_RENUT_HOOK(sub_821B3820, SetRenderStateHook<14>);  // TFU-nx: Katamari sub_824DBBA8, render state 14 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_CullMode  // katamari-nx: N&B sub_8225D168, render state 14 setter (state tables, this game index 14)  // S4ep1-nx: TFU sub_822D4E78, state table row 14 col 1
BANJO_RENUT_HOOK(sub_821B38B8, SetRenderStateHook<15>);  // TFU-nx: Katamari sub_824DBC40, render state 15 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_AlphaBlendEnable  // katamari-nx: N&B sub_821F9748, render state 15 setter (state tables, this game index 15)  // S4ep1-nx: TFU sub_822D4F10, state table row 15 col 1
BANJO_RENUT_HOOK(sub_821B3C48, SetRenderStateHook<16>);  // TFU-nx: Katamari sub_824DBFD0, render state 16 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_SeparateAlphaBlendEnable  // katamari-nx: N&B sub_821F96C0, render state 16 setter (state tables, this game index 16)  // S4ep1-nx: TFU sub_822D52A0, state table row 16 col 1
BANJO_RENUT_HOOK(sub_821B3D78, SetRenderStateHook<17>);  // TFU-nx: Katamari sub_824DC100, render state 17 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_BlendFactor  // katamari-nx: N&B sub_8223D118, render state 17 setter (state tables, this game index 17)  // S4ep1-nx: TFU sub_822D53D0, state table row 17 col 1
BANJO_RENUT_HOOK(sub_821B39D8, SetRenderStateHook<18>);  // TFU-nx: Katamari sub_824DBD60, render state 18 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_SrcBlend  // katamari-nx: N&B sub_821FF1D0, render state 18 setter (state tables, this game index 18)  // S4ep1-nx: TFU sub_822D5030, state table row 18 col 1
BANJO_RENUT_HOOK(sub_821B3A68, SetRenderStateHook<19>);  // TFU-nx: Katamari sub_824DBDF0, render state 19 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_DestBlend  // katamari-nx: N&B sub_821FF8D8, render state 19 setter (state tables, this game index 19)  // S4ep1-nx: TFU sub_822D50C0, state table row 19 col 1
BANJO_RENUT_HOOK(sub_821B3948, SetRenderStateHook<20>);  // TFU-nx: Katamari sub_824DBCD0, render state 20 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_BlendOp  // katamari-nx: N&B sub_821F54F0, render state 20 setter (state tables, this game index 20)  // S4ep1-nx: TFU sub_822D4FA0, state table row 20 col 1
BANJO_RENUT_HOOK(sub_821B3B68, SetRenderStateHook<21>);  // TFU-nx: Katamari sub_824DBEF0, render state 21 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_SrcBlendAlpha  // katamari-nx: N&B sub_82235CD8, render state 21 setter (state tables, this game index 21)  // S4ep1-nx: TFU sub_822D51C0, state table row 21 col 1
BANJO_RENUT_HOOK(sub_821B3BD8, SetRenderStateHook<22>);  // TFU-nx: Katamari sub_824DBF60, render state 22 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_DestBlendAlpha  // katamari-nx: N&B sub_821EFB00, render state 22 setter (state tables, this game index 22)  // S4ep1-nx: TFU sub_822D5230, state table row 22 col 1
BANJO_RENUT_HOOK(sub_821B3AF8, SetRenderStateHook<23>);  // TFU-nx: Katamari sub_824DBE80, render state 23 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_BlendOpAlpha  // katamari-nx: N&B sub_821EE830, render state 23 setter (state tables, this game index 23)  // S4ep1-nx: TFU sub_822D5150, state table row 23 col 1
BANJO_RENUT_HOOK(sub_821B3880, SetRenderStateHook<24>);  // TFU-nx: Katamari sub_824DBC08, render state 24 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_AlphaTestEnable  // katamari-nx: N&B sub_82635498, render state 24 setter (state tables, this game index 24)  // S4ep1-nx: TFU sub_822D4ED8, state table row 24 col 1
BANJO_RENUT_HOOK(sub_821B3CE0, SetRenderStateHook<25>);  // TFU-nx: Katamari sub_824DC068, render state 25 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_AlphaRef  // katamari-nx: N&B sub_82635550, render state 25 setter (state tables, this game index 25)  // S4ep1-nx: TFU sub_822D5338, state table row 25 col 1
BANJO_RENUT_HOOK(sub_821B3D48, SetRenderStateHook<26>);  // TFU-nx: Katamari sub_824DC0D0, render state 26 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_AlphaFunc  // katamari-nx: N&B sub_826355B8, render state 26 setter (state tables, this game index 26)  // S4ep1-nx: TFU sub_822D53A0, state table row 26 col 1
BANJO_RENUT_HOOK(sub_821B4010, SetRenderStateHook<27>);  // TFU-nx: Katamari sub_824DC3A0, render state 27 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_StencilEnable  // katamari-nx: N&B sub_826357C0, render state 27 setter (state tables, this game index 27)  // S4ep1-nx: TFU sub_822D5670, state table row 27 col 1
BANJO_RENUT_HOOK(sub_821B4050, SetRenderStateHook<28>);  // TFU-nx: Katamari sub_824DC3E0, render state 28 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_TwoSidedStencilMode  // katamari-nx: N&B sub_82635800, render state 28 setter (state tables, this game index 28)  // S4ep1-nx: TFU sub_822D56B0, state table row 28 col 1
BANJO_RENUT_HOOK(sub_821B40B8, SetRenderStateHook<29>);  // TFU-nx: Katamari sub_824DC448, render state 29 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_StencilFail  // katamari-nx: N&B sub_82635868, render state 29 setter (state tables, this game index 29)  // S4ep1-nx: TFU sub_822D5718, state table row 29 col 1
BANJO_RENUT_HOOK(sub_821B40F0, SetRenderStateHook<30>);  // TFU-nx: Katamari sub_824DC480, render state 30 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_StencilZFail  // katamari-nx: N&B sub_826358A0, render state 30 setter (state tables, this game index 30)  // S4ep1-nx: TFU sub_822D5750, state table row 30 col 1
BANJO_RENUT_HOOK(sub_821B4128, SetRenderStateHook<31>);  // TFU-nx: Katamari sub_824DC4B8, render state 31 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_StencilPass  // katamari-nx: N&B sub_826358D8, render state 31 setter (state tables, this game index 31)  // S4ep1-nx: TFU sub_822D5788, state table row 31 col 1
BANJO_RENUT_HOOK(sub_821B4088, SetRenderStateHook<32>);  // TFU-nx: Katamari sub_824DC418, render state 32 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_StencilFunc  // katamari-nx: N&B sub_82635838, render state 32 setter (state tables, this game index 32)  // S4ep1-nx: TFU sub_822D56E8, state table row 32 col 1
BANJO_RENUT_HOOK(sub_821B4228, SetRenderStateHook<33>);  // TFU-nx: Katamari sub_824DC5B8, render state 33 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_StencilRef  // katamari-nx: N&B sub_826359D8, render state 33 setter (state tables, this game index 33)  // S4ep1-nx: TFU sub_822D5888, state table row 33 col 1
BANJO_RENUT_HOOK(sub_821B4248, SetRenderStateHook<34>);  // TFU-nx: Katamari sub_824DC5D8, render state 34 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_StencilMask  // katamari-nx: N&B sub_826359F8, render state 34 setter (state tables, this game index 34)  // S4ep1-nx: TFU sub_822D58A8, state table row 34 col 1
BANJO_RENUT_HOOK(sub_821B4268, SetRenderStateHook<35>);  // TFU-nx: Katamari sub_824DC5F8, render state 35 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_StencilWriteMask  // katamari-nx: N&B sub_82635A18, render state 35 setter (state tables, this game index 35)  // S4ep1-nx: TFU sub_822D58C8, state table row 35 col 1
BANJO_RENUT_HOOK(sub_821B4188, SetRenderStateHook<36>);  // TFU-nx: Katamari sub_824DC518, render state 36 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_CCWStencilFail  // katamari-nx: N&B sub_82635938, render state 36 setter (state tables, this game index 36)  // S4ep1-nx: TFU sub_822D57E8, state table row 36 col 1
BANJO_RENUT_HOOK(sub_821B41C0, SetRenderStateHook<37>);  // TFU-nx: Katamari sub_824DC550, render state 37 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_CCWStencilZFail  // katamari-nx: N&B sub_82635970, render state 37 setter (state tables, this game index 37)  // S4ep1-nx: TFU sub_822D5820, state table row 37 col 1
BANJO_RENUT_HOOK(sub_821B41F8, SetRenderStateHook<38>);  // TFU-nx: Katamari sub_824DC588, render state 38 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_CCWStencilPass  // katamari-nx: N&B sub_826359A8, render state 38 setter (state tables, this game index 38)  // S4ep1-nx: TFU sub_822D5858, state table row 38 col 1
BANJO_RENUT_HOOK(sub_821B4158, SetRenderStateHook<39>);  // TFU-nx: Katamari sub_824DC4E8, render state 39 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_CCWStencilFunc  // katamari-nx: N&B sub_82635908, render state 39 setter (state tables, this game index 39)  // S4ep1-nx: TFU sub_822D57B8, state table row 39 col 1
BANJO_RENUT_HOOK(sub_821B4288, SetRenderStateHook<40>);  // TFU-nx: Katamari sub_824DC618, render state 40 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_CCWStencilRef  // katamari-nx: N&B sub_82635A38, render state 40 setter (state tables, this game index 40)  // S4ep1-nx: TFU sub_822D58E8, state table row 40 col 1
BANJO_RENUT_HOOK(sub_821B42A8, SetRenderStateHook<41>);  // TFU-nx: Katamari sub_824DC638, render state 41 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_CCWStencilMask  // katamari-nx: N&B sub_82635A58, render state 41 setter (state tables, this game index 41)  // S4ep1-nx: TFU sub_822D5908, state table row 41 col 1
BANJO_RENUT_HOOK(sub_821B42C8, SetRenderStateHook<42>);  // TFU-nx: Katamari sub_824DC658, render state 42 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_CCWStencilWriteMask  // katamari-nx: N&B sub_82635A78, render state 42 setter (state tables, this game index 42)  // S4ep1-nx: TFU sub_822D5928, state table row 42 col 1
BANJO_RENUT_HOOK(sub_821B42E8, SetRenderStateHook<43>);  // TFU-nx: Katamari sub_824DC678, render state 43 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_ClipPlaneEnable  // katamari-nx: N&B sub_8225D188, render state 43 setter (state tables, this game index 43)  // S4ep1-nx: TFU sub_822D5948, state table row 43 col 1
BANJO_RENUT_HOOK(sub_821B4628, SetRenderStateHook<44>);  // TFU-nx: Katamari sub_824DC9A0, render state 44 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_PointSize  // katamari-nx: N&B sub_82635CC0, render state 44 setter (state tables, this game index 44)  // S4ep1-nx: TFU sub_822D5C70, state table row 44 col 1
BANJO_RENUT_HOOK(sub_821B4688, SetRenderStateHook<45>);  // TFU-nx: Katamari sub_824DCA00, render state 45 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_PointSizeMin  // katamari-nx: N&B sub_82635D20, render state 45 setter (state tables, this game index 45)  // S4ep1-nx: TFU sub_822D5CD0, state table row 45 col 1
BANJO_RENUT_HOOK(sub_821B4618, SetRenderStateHook<46>);  // TFU-nx: Katamari sub_824DC990, render state 46 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_PointSpriteEnable  // katamari-nx: N&B sub_82635CB0, render state 46 setter (state tables, this game index 46)  // S4ep1-nx: TFU sub_822D5C60, state table row 46 col 1
BANJO_RENUT_HOOK(sub_821B46D8, SetRenderStateHook<47>);  // TFU-nx: Katamari sub_824DCA58, render state 47 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_PointSizeMax  // katamari-nx: N&B sub_822371C0, render state 47 setter (state tables, this game index 47)  // S4ep1-nx: TFU sub_822D5D28, state table row 47 col 1
BANJO_RENUT_HOOK(sub_821B44C8, SetRenderStateHook<48>);  // TFU-nx: Katamari sub_824DC840, render state 48 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_MultiSampleAntiAlias  // katamari-nx: N&B sub_82635B98, render state 48 setter (state tables, this game index 48)  // S4ep1-nx: TFU sub_822D5B10, state table row 48 col 1
BANJO_RENUT_HOOK(sub_821B44F8, SetRenderStateHook<49>);  // TFU-nx: Katamari sub_824DC870, render state 49 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_MultiSampleMask  // katamari-nx: N&B sub_82635BC8, render state 49 setter (state tables, this game index 49)  // S4ep1-nx: TFU sub_822D5B40, state table row 49 col 1
BANJO_RENUT_HOOK(sub_821B4340, SetRenderStateHook<50>);  // TFU-nx: Katamari sub_824DE720, render state 50 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_ScissorTestEnable  // katamari-nx: N&B sub_82635AA8, render state 50 setter (state tables, this game index 50)  // S4ep1-nx: TFU sub_822D7AB8, state table row 50 col 1
BANJO_RENUT_HOOK(sub_821B4358, SetRenderStateHook<51>);  // TFU-nx: Katamari sub_824DC6D0, render state 51 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_SlopeScaleDepthBias  // katamari-nx: N&B sub_82635AC0, render state 51 setter (state tables, this game index 51)  // S4ep1-nx: TFU sub_822D59A0, state table row 51 col 1
BANJO_RENUT_HOOK(sub_821B4420, SetRenderStateHook<52>);  // TFU-nx: Katamari sub_824DC798, render state 52 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_DepthBias  // katamari-nx: N&B sub_821CAFA8, render state 52 setter (state tables, this game index 52)  // S4ep1-nx: TFU sub_822D5A68, state table row 52 col 1
BANJO_RENUT_HOOK(sub_821B4518, SetRenderStateHook<53>);  // TFU-nx: Katamari sub_824DC890, render state 53 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_ColorWriteEnable  // katamari-nx: N&B sub_821CAF70, render state 53 setter (state tables, this game index 53)  // S4ep1-nx: TFU sub_822D5B60, state table row 53 col 1
BANJO_RENUT_HOOK(sub_821B4558, SetRenderStateHook<54>);  // TFU-nx: Katamari sub_824DC8D0, render state 54 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_ColorWriteEnable1  // katamari-nx: N&B sub_82635BF0, render state 54 setter (state tables, this game index 54)  // S4ep1-nx: TFU sub_822D5BA0, state table row 54 col 1
BANJO_RENUT_HOOK(sub_821B4598, SetRenderStateHook<55>);  // TFU-nx: Katamari sub_824DC910, render state 55 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_ColorWriteEnable2  // katamari-nx: N&B sub_82635C30, render state 55 setter (state tables, this game index 55)  // S4ep1-nx: TFU sub_822D5BE0, state table row 55 col 1
BANJO_RENUT_HOOK(sub_821B45D8, SetRenderStateHook<56>);  // TFU-nx: Katamari sub_824DC950, render state 56 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_ColorWriteEnable3  // katamari-nx: N&B sub_82635C70, render state 56 setter (state tables, this game index 56)  // S4ep1-nx: TFU sub_822D5C20, state table row 56 col 1
BANJO_RENUT_HOOK(sub_821B4DA8, SetRenderStateHook<57>);  // TFU-nx: Katamari sub_824DD190, render state 57 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_TessellationMode  // katamari-nx: N&B sub_826363C8, render state 57 setter (state tables, this game index 57)  // S4ep1-nx: TFU sub_822D6460, state table row 57 col 1
BANJO_RENUT_HOOK(sub_821B4D38, SetRenderStateHook<58>);  // TFU-nx: Katamari sub_824DD120, render state 58 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_MinTessellationLevel  // katamari-nx: N&B sub_82636358, render state 58 setter (state tables, this game index 58)  // S4ep1-nx: TFU sub_822D63F0, state table row 58 col 1
BANJO_RENUT_HOOK(sub_821B4D70, SetRenderStateHook<59>);  // TFU-nx: Katamari sub_824DD158, render state 59 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_MaxTessellationLevel  // katamari-nx: N&B sub_82636390, render state 59 setter (state tables, this game index 59)  // S4ep1-nx: TFU sub_822D6428, state table row 59 col 1
BANJO_RENUT_HOOK(sub_821B4728, SetRenderStateHook<60>);  // TFU-nx: Katamari sub_824DCAB0, render state 60 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap0  // katamari-nx: N&B sub_82635D88, render state 60 setter (state tables, this game index 60)  // S4ep1-nx: TFU sub_822D5D80, state table row 60 col 1
BANJO_RENUT_HOOK(sub_821B4748, SetRenderStateHook<61>);  // TFU-nx: Katamari sub_824DCAD0, render state 61 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap1  // katamari-nx: N&B sub_82635DA8, render state 61 setter (state tables, this game index 61)  // S4ep1-nx: TFU sub_822D5DA0, state table row 61 col 1
BANJO_RENUT_HOOK(sub_821B4770, SetRenderStateHook<62>);  // TFU-nx: Katamari sub_824DCAF8, render state 62 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap2  // katamari-nx: N&B sub_82635DD0, render state 62 setter (state tables, this game index 62)  // S4ep1-nx: TFU sub_822D5DC8, state table row 62 col 1
BANJO_RENUT_HOOK(sub_821B4798, SetRenderStateHook<63>);  // TFU-nx: Katamari sub_824DCB20, render state 63 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap3  // katamari-nx: N&B sub_82635DF8, render state 63 setter (state tables, this game index 63)  // S4ep1-nx: TFU sub_822D5DF0, state table row 63 col 1
BANJO_RENUT_HOOK(sub_821B47C0, SetRenderStateHook<64>);  // TFU-nx: Katamari sub_824DCB48, render state 64 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap4  // katamari-nx: N&B sub_82635E20, render state 64 setter (state tables, this game index 64)  // S4ep1-nx: TFU sub_822D5E18, state table row 64 col 1
BANJO_RENUT_HOOK(sub_821B47E8, SetRenderStateHook<65>);  // TFU-nx: Katamari sub_824DCB70, render state 65 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap5  // katamari-nx: N&B sub_82635E48, render state 65 setter (state tables, this game index 65)  // S4ep1-nx: TFU sub_822D5E40, state table row 65 col 1
BANJO_RENUT_HOOK(sub_821B4810, SetRenderStateHook<66>);  // TFU-nx: Katamari sub_824DCB98, render state 66 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap6  // katamari-nx: N&B sub_82635E70, render state 66 setter (state tables, this game index 66)  // S4ep1-nx: TFU sub_822D5E68, state table row 66 col 1
BANJO_RENUT_HOOK(sub_821B4838, SetRenderStateHook<67>);  // TFU-nx: Katamari sub_824DCBC0, render state 67 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap7  // katamari-nx: N&B sub_82635E98, render state 67 setter (state tables, this game index 67)  // S4ep1-nx: TFU sub_822D5E90, state table row 67 col 1
BANJO_RENUT_HOOK(sub_821B4858, SetRenderStateHook<68>);  // TFU-nx: Katamari sub_824DCBE0, render state 68 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap8  // katamari-nx: N&B sub_82635EB8, render state 68 setter (state tables, this game index 68)  // S4ep1-nx: TFU sub_822D5EB0, state table row 68 col 1
BANJO_RENUT_HOOK(sub_821B4878, SetRenderStateHook<69>);  // TFU-nx: Katamari sub_824DCC00, render state 69 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap9  // katamari-nx: N&B sub_82635ED8, render state 69 setter (state tables, this game index 69)  // S4ep1-nx: TFU sub_822D5ED0, state table row 69 col 1
BANJO_RENUT_HOOK(sub_821B48A0, SetRenderStateHook<70>);  // TFU-nx: Katamari sub_824DCC28, render state 70 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap10  // katamari-nx: N&B sub_82635F00, render state 70 setter (state tables, this game index 70)  // S4ep1-nx: TFU sub_822D5EF8, state table row 70 col 1
BANJO_RENUT_HOOK(sub_821B48C8, SetRenderStateHook<71>);  // TFU-nx: Katamari sub_824DCC50, render state 71 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap11  // katamari-nx: N&B sub_82635F28, render state 71 setter (state tables, this game index 71)  // S4ep1-nx: TFU sub_822D5F20, state table row 71 col 1
BANJO_RENUT_HOOK(sub_821B48F0, SetRenderStateHook<72>);  // TFU-nx: Katamari sub_824DCC78, render state 72 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap12  // katamari-nx: N&B sub_82635F50, render state 72 setter (state tables, this game index 72)  // S4ep1-nx: TFU sub_822D5F48, state table row 72 col 1
BANJO_RENUT_HOOK(sub_821B4918, SetRenderStateHook<73>);  // TFU-nx: Katamari sub_824DCCA0, render state 73 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap13  // katamari-nx: N&B sub_82635F78, render state 73 setter (state tables, this game index 73)  // S4ep1-nx: TFU sub_822D5F70, state table row 73 col 1
BANJO_RENUT_HOOK(sub_821B4940, SetRenderStateHook<74>);  // TFU-nx: Katamari sub_824DCCC8, render state 74 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap14  // katamari-nx: N&B sub_82635FA0, render state 74 setter (state tables, this game index 74)  // S4ep1-nx: TFU sub_822D5F98, state table row 74 col 1
BANJO_RENUT_HOOK(sub_821B4968, SetRenderStateHook<75>);  // TFU-nx: Katamari sub_824DCCF0, render state 75 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Wrap15  // katamari-nx: N&B sub_82635FC8, render state 75 setter (state tables, this game index 75)  // S4ep1-nx: TFU sub_822D5FC0, state table row 75 col 1
BANJO_RENUT_HOOK(sub_821B4A88, SetRenderStateHook<76>);  // TFU-nx: Katamari sub_824DCE10, render state 76 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_ViewportEnable  // katamari-nx: N&B sub_821D53E8, render state 76 setter (state tables, this game index 76)  // S4ep1-nx: TFU sub_822D60E0, state table row 76 col 1
BANJO_RENUT_HOOK(sub_821B4AD8, SetRenderStateHook<77>);  // TFU-nx: Katamari sub_824DCE60, render state 77 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_HighPrecisionBlendEnable  // katamari-nx: N&B sub_826360F8, render state 77 setter (state tables, this game index 77)  // S4ep1-nx: TFU sub_822D6130, state table row 77 col 1
BANJO_RENUT_HOOK(sub_821B4B70, SetRenderStateHook<78>);  // TFU-nx: Katamari sub_824DCF10, render state 78 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_HighPrecisionBlendEnable1  // katamari-nx: N&B sub_82636190, render state 78 setter (state tables, this game index 78)  // S4ep1-nx: TFU sub_822D61E0, state table row 78 col 1
BANJO_RENUT_HOOK(sub_821B4C08, SetRenderStateHook<79>);  // TFU-nx: Katamari sub_824DCFC0, render state 79 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_HighPrecisionBlendEnable2  // katamari-nx: N&B sub_82636228, render state 79 setter (state tables, this game index 79)  // S4ep1-nx: TFU sub_822D6290, state table row 79 col 1
BANJO_RENUT_HOOK(sub_821B4CA0, SetRenderStateHook<80>);  // TFU-nx: Katamari sub_824DD070, render state 80 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_HighPrecisionBlendEnable3  // katamari-nx: N&B sub_826362C0, render state 80 setter (state tables, this game index 80)  // S4ep1-nx: TFU sub_822D6340, state table row 80 col 1
BANJO_RENUT_HOOK(sub_821B4DE0, SetRenderStateHook<81>);  // TFU-nx: Katamari sub_824DD1C8, render state 81 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_HalfPixelOffset  // katamari-nx: N&B sub_82636400, render state 81 setter (state tables, this game index 81)  // S4ep1-nx: TFU sub_822D6498, state table row 81 col 1
BANJO_RENUT_HOOK(sub_821B4E18, SetRenderStateHook<82>);  // TFU-nx: Katamari sub_824DD200, render state 82 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_PrimitiveResetEnable  // katamari-nx: N&B sub_82636438, render state 82 setter (state tables, this game index 82)  // S4ep1-nx: TFU sub_822D64D0, state table row 82 col 1
BANJO_RENUT_HOOK(sub_821B4E48, SetRenderStateHook<83>);  // TFU-nx: Katamari sub_824DD230, render state 83 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_PrimitiveResetIndex  // katamari-nx: N&B sub_82636468, render state 83 setter (state tables, this game index 83)  // S4ep1-nx: TFU sub_822D6500, state table row 83 col 1
BANJO_RENUT_HOOK(sub_821B4E70, SetRenderStateHook<84>);  // TFU-nx: Katamari sub_824DD258, render state 84 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_AlphaToMaskEnable  // katamari-nx: N&B sub_82636490, render state 84 setter (state tables, this game index 84)  // S4ep1-nx: TFU sub_822D6528, state table row 84 col 1
BANJO_RENUT_HOOK(sub_821B4EA0, SetRenderStateHook<85>);  // TFU-nx: Katamari sub_824DD288, render state 85 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_AlphaToMaskOffsets  // katamari-nx: N&B sub_826364C0, render state 85 setter (state tables, this game index 85)  // S4ep1-nx: TFU sub_822D6558, state table row 85 col 1
BANJO_RENUT_HOOK(sub_821B4EC8, SetRenderStateHook<86>);  // TFU-nx: Katamari sub_824DD2C0, render state 86 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_GuardBand_X  // katamari-nx: N&B sub_826364E8, render state 86 setter (state tables, this game index 86)  // S4ep1-nx: TFU sub_822D6590, state table row 86 col 1
BANJO_RENUT_HOOK(sub_821B4EF8, SetRenderStateHook<87>);  // TFU-nx: Katamari sub_824DD2F0, render state 87 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_GuardBand_Y  // katamari-nx: N&B sub_82636518, render state 87 setter (state tables, this game index 87)  // S4ep1-nx: TFU sub_822D65C0, state table row 87 col 1
BANJO_RENUT_HOOK(sub_821B4F28, SetRenderStateHook<88>);  // TFU-nx: Katamari sub_824DD320, render state 88 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_DiscardBand_X  // katamari-nx: N&B sub_82636548, render state 88 setter (state tables, this game index 88)  // S4ep1-nx: TFU sub_822D65F0, state table row 88 col 1
BANJO_RENUT_HOOK(sub_821B4F50, SetRenderStateHook<89>);  // TFU-nx: Katamari sub_824DD348, render state 89 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_DiscardBand_Y  // katamari-nx: N&B sub_82636570, render state 89 setter (state tables, this game index 89)  // S4ep1-nx: TFU sub_822D6618, state table row 89 col 1
BANJO_RENUT_HOOK(sub_821B5018, SetRenderStateHook<90>);  // TFU-nx: Katamari sub_824DD410, render state 90 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_HiStencilEnable  // katamari-nx: N&B sub_826365E0, render state 90 setter (state tables, this game index 90)  // S4ep1-nx: TFU sub_822D66E0, state table row 90 col 1
BANJO_RENUT_HOOK(sub_821B5048, SetRenderStateHook<91>);  // TFU-nx: Katamari sub_824DD440, render state 91 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_HiStencilWriteEnable  // katamari-nx: N&B sub_82636610, render state 91 setter (state tables, this game index 91)  // S4ep1-nx: TFU sub_822D6710, state table row 91 col 1
BANJO_RENUT_HOOK(sub_821B5078, SetRenderStateHook<92>);  // TFU-nx: Katamari sub_824DD470, render state 92 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_HiStencilFunc  // katamari-nx: N&B sub_82636640, render state 92 setter (state tables, this game index 92)  // S4ep1-nx: TFU sub_822D6740, state table row 92 col 1
BANJO_RENUT_HOOK(sub_821B50A8, SetRenderStateHook<93>);  // TFU-nx: Katamari sub_824DD4A0, render state 93 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_HiStencilRef  // katamari-nx: N&B sub_82636670, render state 93 setter (state tables, this game index 93)  // S4ep1-nx: TFU sub_822D6770, state table row 93 col 1
BANJO_RENUT_HOOK(sub_821B50C8, SetRenderStateHook<94>);  // TFU-nx: Katamari sub_824DD4C0, render state 94 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_PresentInterval  // katamari-nx: N&B sub_82636690, render state 94 setter (state tables, this game index 94)  // S4ep1-nx: TFU sub_822D6790, state table row 94 col 1
BANJO_RENUT_HOOK(sub_821B50D8, SetRenderStateHook<95>);  // TFU-nx: Katamari sub_824DD4D0, render state 95 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_PresentImmediateThreshold  // katamari-nx: N&B sub_826366A0, render state 95 setter (state tables, this game index 95)  // S4ep1-nx: TFU sub_822D67A0, state table row 95 col 1
BANJO_RENUT_HOOK(sub_821B4F80, SetRenderStateHook<96>);  // TFU-nx: Katamari sub_824DD378, render state 96 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_HiZEnable  // katamari-nx: N&B sub_826365A0, render state 96 setter (state tables, this game index 96)  // S4ep1-nx: TFU sub_822D6648, state table row 96 col 1
BANJO_RENUT_HOOK(sub_821B4FB0, SetRenderStateHook<97>);  // TFU-nx: Katamari sub_824DD3A8, render state 97 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_HiZWriteEnable  // katamari-nx: N&B sub_82257938, render state 97 setter (state tables, this game index 97)  // S4ep1-nx: TFU sub_822D6678, state table row 97 col 1
BANJO_RENUT_HOOK(sub_821B3E88, SetRenderStateHook<98>);  // TFU-nx: Katamari sub_824DC220, render state 98 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_LastPixel  // katamari-nx: N&B sub_82635660, render state 98 setter (state tables, this game index 98)  // S4ep1-nx: TFU sub_822D54F0, state table row 98 col 1
BANJO_RENUT_HOOK(sub_821B3EC0, SetRenderStateHook<99>);  // TFU-nx: Katamari sub_824DC258, render state 99 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_LineWidth  // katamari-nx: N&B sub_82635698, render state 99 setter (state tables, this game index 99)  // S4ep1-nx: TFU sub_822D5528, state table row 99 col 1
BANJO_RENUT_HOOK(sub_821B3F38, SetRenderStateHook<100>);  // TFU-nx: Katamari sub_824DC2C8, render state 100 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetRenderState_Buffer2Frames  // katamari-nx: N&B sub_82635710, render state 100 setter (state tables, this game index 100)  // S4ep1-nx: TFU sub_822D5598, state table row 100 col 1

// Sampler states, by their index in N&B's table (off_82E50BD0).
BANJO_RENUT_HOOK(sub_821B5878, SetSamplerStateHook<0>);  // TFU-nx: Katamari sub_824DDC30, sampler state 0 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_AddressU  // katamari-nx: N&B sub_8224B2C0, sampler state 0 setter (state tables, this game index 0)  // S4ep1-nx: TFU sub_822D6F00, state table row 0 col 1
BANJO_RENUT_HOOK(sub_821B58C8, SetSamplerStateHook<1>);  // TFU-nx: Katamari sub_824DDC80, sampler state 1 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_AddressV  // katamari-nx: N&B sub_821DB8C8, sampler state 1 setter (state tables, this game index 1)  // S4ep1-nx: TFU sub_822D6F50, state table row 1 col 1
BANJO_RENUT_HOOK(sub_821B5918, SetSamplerStateHook<2>);  // TFU-nx: Katamari sub_824DDCD0, sampler state 2 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_AddressW  // katamari-nx: N&B sub_82636C30, sampler state 2 setter (state tables, this game index 2)  // S4ep1-nx: TFU sub_822D6FA0, state table row 2 col 1
BANJO_RENUT_HOOK(sub_821B5808, SetSamplerStateHook<3>);  // TFU-nx: Katamari sub_824DDBC0, sampler state 3 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_BorderColor  // katamari-nx: N&B sub_82636B90, sampler state 3 setter (state tables, this game index 3)  // S4ep1-nx: TFU sub_822D6E90, state table row 3 col 1
BANJO_RENUT_HOOK(sub_821B52A0, SetSamplerStateHook<4>);  // TFU-nx: Katamari sub_824DD680, sampler state 4 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_MagFilter  // katamari-nx: N&B sub_821FE308, sampler state 4 setter (state tables, this game index 4)  // S4ep1-nx: TFU sub_822D6950, state table row 4 col 1
BANJO_RENUT_HOOK(sub_821B50F8, SetSamplerStateHook<5>);  // TFU-nx: Katamari sub_824DD4F0, sampler state 5 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_MinFilter  // katamari-nx: N&B sub_821FE3D0, sampler state 5 setter (state tables, this game index 5)  // S4ep1-nx: TFU sub_822D67C0, state table row 5 col 1
BANJO_RENUT_HOOK(sub_821B5440, SetSamplerStateHook<6>);  // TFU-nx: Katamari sub_824DD810, sampler state 6 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_MipFilter  // katamari-nx: N&B sub_8225D108, sampler state 6 setter (state tables, this game index 6)  // S4ep1-nx: TFU sub_822D6AE0, state table row 6 col 1
BANJO_RENUT_HOOK(sub_821B5660, SetSamplerStateHook<7>);  // TFU-nx: Katamari sub_824DDA20, sampler state 7 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_MipMapLodBias  // katamari-nx: N&B sub_82636A58, sampler state 7 setter (state tables, this game index 7)  // S4ep1-nx: TFU sub_822D6CF0, state table row 7 col 1
BANJO_RENUT_HOOK(sub_821B5708, SetSamplerStateHook<8>);  // TFU-nx: Katamari sub_824DDAC0, sampler state 8 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_MaxMipLevel  // katamari-nx: N&B sub_821FE6D8, sampler state 8 setter (state tables, this game index 8)  // S4ep1-nx: TFU sub_822D6D90, state table row 8 col 1
BANJO_RENUT_HOOK(sub_821B5540, SetSamplerStateHook<9>);  // TFU-nx: Katamari sub_824DD900, sampler state 9 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_MaxAnisotropy  // katamari-nx: N&B sub_82636938, sampler state 9 setter (state tables, this game index 9)  // S4ep1-nx: TFU sub_822D6BD0, state table row 9 col 1
BANJO_RENUT_HOOK(sub_821B53A0, SetSamplerStateHook<10>);  // TFU-nx: Katamari sub_824DD778, sampler state 10 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_MagFilterZ  // katamari-nx: N&B sub_826367D8, sampler state 10 setter (state tables, this game index 10)  // S4ep1-nx: TFU sub_822D6A48, state table row 10 col 1
BANJO_RENUT_HOOK(sub_821B51F8, SetSamplerStateHook<11>);  // TFU-nx: Katamari sub_824DD5E8, sampler state 11 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_MinFilterZ  // katamari-nx: N&B sub_826366F8, sampler state 11 setter (state tables, this game index 11)  // S4ep1-nx: TFU sub_822D68B8, state table row 11 col 1
BANJO_RENUT_HOOK(sub_821B5498, SetSamplerStateHook<12>);  // TFU-nx: Katamari sub_824DD868, sampler state 12 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_SeparateZFilterEnable  // katamari-nx: N&B sub_82636890, sampler state 12 setter (state tables, this game index 12)  // S4ep1-nx: TFU sub_822D6B38, state table row 12 col 1
BANJO_RENUT_HOOK(sub_821B5788, SetSamplerStateHook<13>);  // TFU-nx: Katamari sub_824DDB40, sampler state 13 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_MinMipLevel  // katamari-nx: N&B sub_82636B10, sampler state 13 setter (state tables, this game index 13)  // S4ep1-nx: TFU sub_822D6E10, state table row 13 col 1
BANJO_RENUT_HOOK(sub_821B5968, SetSamplerStateHook<14>);  // TFU-nx: Katamari sub_824DDD20, sampler state 14 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_TrilinearThreshold  // katamari-nx: N&B sub_82636C80, sampler state 14 setter (state tables, this game index 14)  // S4ep1-nx: TFU sub_822D6FF0, state table row 14 col 1
BANJO_RENUT_HOOK(sub_821B55B8, SetSamplerStateHook<15>);  // TFU-nx: Katamari sub_824DD978, sampler state 15 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_AnisotropyBias  // katamari-nx: N&B sub_826369B0, sampler state 15 setter (state tables, this game index 15)  // S4ep1-nx: TFU sub_822D6C48, state table row 15 col 1
BANJO_RENUT_HOOK(sub_821B59C0, SetSamplerStateHook<16>);  // TFU-nx: Katamari sub_824DDD78, sampler state 16 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_HGradientExpBias  // katamari-nx: N&B sub_82636CD8, sampler state 16 setter (state tables, this game index 16)  // S4ep1-nx: TFU sub_822D7048, state table row 16 col 1
BANJO_RENUT_HOOK(sub_821B5A18, SetSamplerStateHook<17>);  // TFU-nx: Katamari sub_824DDDD0, sampler state 17 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_VGradientExpBias  // katamari-nx: N&B sub_82636D30, sampler state 17 setter (state tables, this game index 17)  // S4ep1-nx: TFU sub_822D70A0, state table row 17 col 1
BANJO_RENUT_HOOK(sub_821B5A70, SetSamplerStateHook<18>);  // TFU-nx: Katamari sub_824DDE28, sampler state 18 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_WhiteBorderColorW  // katamari-nx: N&B sub_82636D88, sampler state 18 setter (state tables, this game index 18)  // S4ep1-nx: TFU sub_822D70F8, state table row 18 col 1
BANJO_RENUT_HOOK(sub_821B5AC8, SetSamplerStateHook<19>);  // TFU-nx: Katamari sub_824DDE80, sampler state 19 set (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_SetSamplerState_PointBorderEnable  // katamari-nx: N&B sub_82636DE0, sampler state 19 setter (state tables, this game index 19)  // S4ep1-nx: TFU sub_822D7150, state table row 19 col 1

BANJO_RENUT_HOOK(sub_821B8B48, D3DDevice_SetTexture_hook);  // TFU-nx: Katamari sub_824D2640, by hand: Katamari slot right after LockBox (822DC358); shape 0.45 (XDK change);  // D3DDevice_SetTexture  // katamari-nx: N&B sub_821FE558, signature 0.82 (next 0.39)  // S4ep1-nx: TFU sub_822DC378, call at +0xfc of 822E1218
BANJO_RENUT_UNHOOKED_HOOK(sub_824A3D40, D3DDevice_SetTexture_hook);  // TFU-nx: no counterpart in TFU (docs/renut-map.md);  // D3DDevice_SetTexture_Inline  // katamari-nx: N&B sub_82BF2578, signature 1.00 (next 0.71)  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_UNHOOKED_HOOK(sub_822D72B0, D3DDevice_SetStreamSource_hook);  // TFU-nx: Katamari sub_824DDFE0, d3d_match 0.99 (next 0.28);  // D3DDevice_SetStreamSource  // katamari-nx: N&B sub_821F9438, signature 0.83 (next 0.38)  // S4ep1-nx: no counterpart found in Sonic 4 (docs/renut-map.md, still open)
BANJO_RENUT_UNHOOKED_HOOK(sub_822D7458, D3DDevice_SetIndices_hook);  // TFU-nx: Katamari sub_824DE188, d3d_match 0.97 (next 0.31);  // D3DDevice_SetIndices  // katamari-nx: N&B sub_821DE438, signature 0.76 (next 0.34)  // S4ep1-nx: no counterpart found in Sonic 4 (docs/renut-map.md, still open)
BANJO_RENUT_UNHOOKED_HOOK(sub_822E4098, D3DDevice_SetVertexDeclaration_hook);  // TFU-nx: Katamari sub_824D4B48, d3d_match 1.00 (next 0.83);  // D3DDevice_SetVertexDeclaration  // katamari-nx: N&B sub_821D5060, D3DDevice_SetVertexDeclaration: 5 instructions, VertexDeclaration +11812 (N&B +11824); sig 0.67/0.43  // S4ep1-nx: no counterpart found in Sonic 4 (docs/renut-map.md, still open)
BANJO_RENUT_HOOK(sub_821B35C8, D3D_SetViewport_hook);  // S4ep1-nx (2026-11-15): Sonic 4's takes (device r3, X f1, Y f2, Width f3, Height f4, MinZ f5, MaxZ f6), checked in its generated code: D3D_SetViewport's form, not a D3DVIEWPORT9 pointer. Was  // TFU-nx: Katamari sub_824DE730, d3d_match 1.00 (next 0.44);  // D3DDevice_SetViewport  // katamari-nx: N&B sub_821F2090, D3DDevice_SetViewport: sig 0.63/0.30  // S4ep1-nx: TFU sub_822D7AC8, D3DDevice_SetViewport: BeginTiling and EndTiling call it as often as TFU's do; D3D_SetViewport inlined into it
BANJO_RENUT_UNHOOKED_HOOK(sub_822D7850, D3DDevice_SetViewport_hook);  // S4ep1-nx: the two swapped (above);  // TFU-nx: Katamari sub_824DE4B8, d3d_match 1.00 (next 0.37);  // D3D_SetViewport  // katamari-nx: N&B sub_821F2110, D3D_SetViewport: sig 0.64/0.27  // S4ep1-nx: no counterpart found in Sonic 4 (docs/renut-map.md, still open)
BANJO_RENUT_HOOK(sub_821B5B28, D3DDevice_SetScissorRect_hook);  // TFU-nx: Katamari sub_824DDEE0, d3d_match 1.00 (next 0.25);  // D3DDevice_SetScissorRect  // katamari-nx: N&B sub_821F2368, D3DDevice_SetScissorRect: Present's tail calls it; sig 0.49/0.24  // S4ep1-nx: TFU sub_822D71B0, seed SetScissorRect: called by Present in both; TFU d3d_match top 0.62 (next 0.25)
BANJO_RENUT_HOOK(sub_821B32D8, D3D_SetSurfaceClip_hook);  // TFU-nx: Katamari sub_824DB8C8, d3d_match 1.00 (next 0.41);  // D3D_SetSurfaceClip  // katamari-nx: N&B sub_821F2460, signature 0.83 (next 0.37)  // S4ep1-nx: TFU sub_822D4B98, call at +0xe4 of 822D71B0
BANJO_RENUT_HOOK(sub_821B5D40, D3DDevice_SetRenderTarget_hook);  // TFU-nx: Katamari sub_824DE868, d3d_match 0.97 (next 0.42);  // D3DDevice_SetRenderTarget  // katamari-nx: N&B sub_82248BD0, D3DDevice_SetRenderTarget: InitializePresentationParameters calls it after CreateSurface, as N&B's; sig 0.62/0.31  // S4ep1-nx: TFU sub_822D7C00, seed los1 d3d_match 1.00 (same XDK, next 0.44) SetRenderTarget; tfu 0.59
BANJO_RENUT_HOOK(sub_821B6090, D3DDevice_SetDepthStencilSurface_hook);  // TFU-nx: Katamari sub_824DEBD0, d3d_match 0.97 (next 0.43);  // D3DDevice_SetDepthStencilSurface  // katamari-nx: N&B sub_82248FB0, D3DDevice_SetDepthStencilSurface: likewise after the depth surface; sig 0.71/0.30  // S4ep1-nx: TFU sub_822D7F68, seed los1 d3d_match 1.00 (same XDK, next 0.44) SetDepthStencilSurface; tfu 0.87 (next 0.38)
BANJO_RENUT_HOOK(sub_821B3220, D3D_SetSurfaceInfo_hook);  // TFU-nx: Katamari sub_824DE7D0, d3d_match 1.00 (next 0.14);  // D3D_SetSurfaceInfo  // katamari-nx: N&B sub_82248F20, D3D_SetSurfaceInfo: sig 0.57/0.19; full viewport 0x820FAEB4  // S4ep1-nx: TFU sub_822D7B68, call at +0x34 of 822D7F68
BANJO_RENUT_HOOK(sub_821C2BC0, D3DDevice_SetPredication_hook);  // TFU-nx: Katamari sub_824EFA58, d3d_match 0.96 (next 0.41);  // D3DDevice_SetPredication  // katamari-nx: N&B sub_821F9A48, D3DDevice_SetPredication: sig 0.69 (SetCommandBufferPredication's best is the same function at 0.63: XDK 2.0.5632 has one)  // S4ep1-nx: TFU sub_822F3570, seed tfu d3d_match 0.90 (next 0.39) D3DDevice_SetPredication
BANJO_RENUT_UNHOOKED_HOOK_SHARED(sub_826411C0, D3DDevice_SetCommandBufferPredication_hook);  // D3DDevice_SetCommandBufferPredication  // katamari-nx: not hooked (D3DDevice_SetCommandBufferPredication: see SetPredication)  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_HOOK(sub_821BA258, D3DDevice_SetShaderGPRAllocation_hook);  // TFU-nx: Katamari sub_824D4CD8, d3d_match 0.98 (next 0.25);  // D3DDevice_SetShaderGPRAllocation  // katamari-nx: N&B sub_821EFD38, signature 0.78 (next 0.30)  // S4ep1-nx: TFU sub_822E4228, call at +0x25c of 822E1218
BANJO_RENUT_HOOK(sub_821B8EB0, D3D_CleanupGprAllocationSetup_hook);  // TFU-nx: Katamari sub_824D3FE0, d3d_match 0.94 (next 0.06);  // D3D_CleanupGprAllocationSetup  // katamari-nx: N&B sub_82637208, signature 0.98 (next 0.13)  // S4ep1-nx: TFU sub_822E3540, D3D_CleanupGprAllocationSetup: TFU match 0.77 (next 0.13), call neighbours
BANJO_RENUT_HOOK(sub_821C6B98, D3DDevice_SetScreenExtentQueryMode_hook);  // TFU-nx: Katamari sub_824E67C8, by hand: 0.82;  // D3DDevice_SetScreenExtentQueryMode  // katamari-nx: N&B sub_826414C0, signature 1.00 (next 0.75)  // S4ep1-nx: TFU sub_822DE9F0, call at +0x128 of 822E1218
BANJO_RENUT_HOOK(sub_821C4BD8, D3DDevice_UnsetAll_hook);  // TFU-nx: Katamari sub_824D0488, d3d_match 1.00 (next 0.44);  // D3DDevice_UnsetAll  // katamari-nx: N&B sub_82634C10, signature 0.81 (next 0.34)  // S4ep1-nx: TFU sub_822D85F8, D3DDevice_UnsetAll: TFU match top 0.68, call neighbours (4)
BANJO_RENUT_HOOK(sub_821B30F8, D3D_ResetAllState_hook);  // TFU-nx: Katamari sub_824DE3C0, d3d_match 0.98 (next 0.47);  // D3D_ResetAllState  // katamari-nx: N&B sub_82255C98, D3D_ResetAllState: sig 0.73/0.33; the device init sub_824E18D0 calls it last  // S4ep1-nx: TFU sub_822D7758, D3D_ResetAllState: called by BeginTiling, EndTiling and the device init as in TFU; TFU match top 0.50
BANJO_RENUT_UNHOOKED_HOOK_SHARED(sub_82634B90, D3D_InvalidateConstants_hook);  // D3D_InvalidateConstants  // katamari-nx: not hooked (D3D_InvalidateConstants: ambiguous (0.65 / 0.61))  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_HOOK(sub_821C6678, D3DDevice_BlockUntilIdle_hook);  // TFU-nx: Katamari sub_824E0518, d3d_match 0.97 (next 0.57);  // D3DDevice_BlockUntilIdle  // katamari-nx: N&B sub_82640010, signature 0.87 (next 0.63)  // S4ep1-nx: TFU sub_822D9E70, call at +0x74 of 822F3488
// Through a thunk (j_D3DDevice_SetRenderTarget 0x82636EC8).
BANJO_RENUT_UNHOOKED_HOOK(sub_82636EC8, D3DDevice_SetRenderTarget_hook);  // D3DDevice_SetRenderTarget_Thunk  // katamari-nx: not hooked (signature too weak or ambiguous (sub_8257F318 1.00, next 1.00))  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_UNHOOKED_HOOK(sub_82636ED0, D3DDevice_GetRenderTarget_hook);  // D3DDevice_GetRenderTarget  // katamari-nx: not hooked (signature too weak or ambiguous (sub_824B12B8 0.63, next 0.63))  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_UNHOOKED_HOOK(sub_82636F10, D3DDevice_GetDepthStencilSurface_hook);  // D3DDevice_GetDepthStencilSurface  // katamari-nx: not hooked (signature too weak or ambiguous (sub_824B12B8 0.69, next 0.69))  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_UNHOOKED_HOOK(sub_82636E40, D3DDevice_GetStreamSource_hook);  // D3DDevice_GetStreamSource  // katamari-nx: not hooked (signature too weak or ambiguous (sub_824DE100 0.56, next 0.42))  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_UNHOOKED_HOOK(sub_82244B18, D3DDevice_GetViewport_hook);  // D3DDevice_GetViewport  // katamari-nx: not hooked (signature too weak or ambiguous (sub_824DC1A0 0.20, next 0.11))  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_UNHOOKED_HOOK(sub_826370E8, D3DDevice_SetClipPlane_hook);  // D3DDevice_SetClipPlane  // katamari-nx: not hooked (signature too weak or ambiguous (sub_824D5048 0.34, next 0.33))  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_UNHOOKED_HOOK(sub_82637138, D3DDevice_SetVertexFetchConstant_hook);  // D3DDevice_SetVertexFetchConstant  // katamari-nx: not hooked (signature too weak or ambiguous (sub_824DE100 0.38, next 0.29))  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_UNHOOKED_HOOK(sub_822E39C0, D3DDevice_SetVertexShaderConstantB_hook);  // TFU-nx: Katamari sub_824D4470, by hand: twin, VS first as in Katamari;  // D3DDevice_SetVertexShaderConstantB  // katamari-nx: N&B sub_821F0B40, signature 0.84 (next 0.77)  // S4ep1-nx: no counterpart found in Sonic 4 (docs/renut-map.md, still open)
BANJO_RENUT_UNHOOKED_HOOK(sub_822E3A20, D3DDevice_SetPixelShaderConstantB_hook);  // TFU-nx: Katamari sub_824D44D0, by hand: twin, PS second;  // D3DDevice_SetPixelShaderConstantB  // katamari-nx: N&B sub_82237208, signature 0.84 (next 0.77)  // S4ep1-nx: no counterpart found in Sonic 4 (docs/renut-map.md, still open)
BANJO_RENUT_UNHOOKED_HOOK(sub_826374B0, D3DDevice_SetVertexShaderConstantI_hook);  // D3DDevice_SetVertexShaderConstantI  // katamari-nx: not hooked (signature too weak or ambiguous (sub_824D4588 0.33, next 0.33))  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_UNHOOKED_HOOK(sub_82637500, D3DDevice_SetPixelShaderConstantI_hook);  // D3DDevice_SetPixelShaderConstantI  // katamari-nx: not hooked (signature too weak or ambiguous (sub_824D4588 0.33, next 0.33))  // S4ep1-nx: unhooked as in TFU
BANJO_RENUT_UNHOOKED_HOOK(sub_8263ED90, D3DDevice_EndIndexedVertices_hook);  // D3DDevice_EndIndexedVertices  // katamari-nx: not hooked (signature too weak or ambiguous (sub_8255A1E8 0.63, next 0.31))  // S4ep1-nx: unhooked as in TFU

// Render state getters, by their index in N&B's table (off_82E506F0, field 0).
BANJO_RENUT_HOOK(sub_821B3FA0, GetRenderStateHook<10>);  // TFU-nx: Katamari sub_824DC330, render state 10 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_ZEnable  // katamari-nx: N&B sub_82635778, render state 10 getter (state tables, this game index 10)  // S4ep1-nx: TFU sub_822D5600, state table row 10 col 0
BANJO_RENUT_HOOK(sub_821B4000, GetRenderStateHook<11>);  // TFU-nx: Katamari sub_824DC390, render state 11 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_ZFunc  // katamari-nx: N&B sub_826357B0, render state 11 getter (state tables, this game index 11)  // S4ep1-nx: TFU sub_822D5660, state table row 11 col 0
BANJO_RENUT_HOOK(sub_821B3FC8, GetRenderStateHook<12>);  // TFU-nx: Katamari sub_824DC358, render state 12 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_ZWriteEnable  // katamari-nx: N&B sub_826357A0, render state 12 getter (state tables, this game index 12)  // S4ep1-nx: TFU sub_822D5628, state table row 12 col 0
BANJO_RENUT_HOOK(sub_821B3870, GetRenderStateHook<13>);  // TFU-nx: Katamari sub_824DBBF8, render state 13 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_FillMode  // katamari-nx: N&B sub_82635488, render state 13 getter (state tables, this game index 13)  // S4ep1-nx: TFU sub_822D4EC8, state table row 13 col 0
BANJO_RENUT_HOOK(sub_821B3840, GetRenderStateHook<14>);  // TFU-nx: Katamari sub_824DBBC8, render state 14 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_CullMode  // katamari-nx: N&B sub_82635478, render state 14 getter (state tables, this game index 14)  // S4ep1-nx: TFU sub_822D4E98, state table row 14 col 0
BANJO_RENUT_HOOK(sub_821B3938, GetRenderStateHook<15>);  // TFU-nx: Katamari sub_824DBCC0, render state 15 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_AlphaBlendEnable  // katamari-nx: N&B sub_826354D0, render state 15 getter (state tables, this game index 15)  // S4ep1-nx: TFU sub_822D4F90, state table row 15 col 0
BANJO_RENUT_HOOK(sub_821B3CD0, GetRenderStateHook<16>);  // TFU-nx: Katamari sub_824DC058, render state 16 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_SeparateAlphaBlendEnable  // katamari-nx: N&B sub_82635540, render state 16 getter (state tables, this game index 16)  // S4ep1-nx: TFU sub_822D5328, state table row 16 col 0
BANJO_RENUT_HOOK(sub_821B3E10, GetRenderStateHook<17>);  // TFU-nx: Katamari sub_824DC1A0, render state 17 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_BlendFactor  // katamari-nx: N&B sub_826355E8, render state 17 getter (state tables, this game index 17)  // S4ep1-nx: TFU sub_822D5470, state table row 17 col 0
BANJO_RENUT_HOOK(sub_821B3A58, GetRenderStateHook<18>);  // TFU-nx: Katamari sub_824DBDE0, render state 18 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_SrcBlend  // katamari-nx: N&B sub_826354F0, render state 18 getter (state tables, this game index 18)  // S4ep1-nx: TFU sub_822D50B0, state table row 18 col 0
BANJO_RENUT_HOOK(sub_821B3AE8, GetRenderStateHook<19>);  // TFU-nx: Katamari sub_824DBE70, render state 19 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_DestBlend  // katamari-nx: N&B sub_82635500, render state 19 getter (state tables, this game index 19)  // S4ep1-nx: TFU sub_822D5140, state table row 19 col 0
BANJO_RENUT_HOOK(sub_821B39C8, GetRenderStateHook<20>);  // TFU-nx: Katamari sub_824DBD50, render state 20 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_BlendOp  // katamari-nx: N&B sub_826354E0, render state 20 getter (state tables, this game index 20)  // S4ep1-nx: TFU sub_822D5020, state table row 20 col 0
BANJO_RENUT_HOOK(sub_821B3BC8, GetRenderStateHook<21>);  // TFU-nx: Katamari sub_824DBF50, render state 21 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_SrcBlendAlpha  // katamari-nx: N&B sub_82635520, render state 21 getter (state tables, this game index 21)  // S4ep1-nx: TFU sub_822D5220, state table row 21 col 0
BANJO_RENUT_HOOK(sub_821B3C38, GetRenderStateHook<22>);  // TFU-nx: Katamari sub_824DBFC0, render state 22 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_DestBlendAlpha  // katamari-nx: N&B sub_82635530, render state 22 getter (state tables, this game index 22)  // S4ep1-nx: TFU sub_822D5290, state table row 22 col 0
BANJO_RENUT_HOOK(sub_821B3B58, GetRenderStateHook<23>);  // TFU-nx: Katamari sub_824DBEE0, render state 23 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_BlendOpAlpha  // katamari-nx: N&B sub_82635510, render state 23 getter (state tables, this game index 23)  // S4ep1-nx: TFU sub_822D51B0, state table row 23 col 0
BANJO_RENUT_HOOK(sub_821B38A8, GetRenderStateHook<24>);  // TFU-nx: Katamari sub_824DBC30, render state 24 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_AlphaTestEnable  // katamari-nx: N&B sub_826354C0, render state 24 getter (state tables, this game index 24)  // S4ep1-nx: TFU sub_822D4F00, state table row 24 col 0
BANJO_RENUT_HOOK(sub_821B3D18, GetRenderStateHook<25>);  // TFU-nx: Katamari sub_824DC0A0, render state 25 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_AlphaRef  // katamari-nx: N&B sub_82635588, render state 25 getter (state tables, this game index 25)  // S4ep1-nx: TFU sub_822D5370, state table row 25 col 0
BANJO_RENUT_HOOK(sub_821B3D68, GetRenderStateHook<26>);  // TFU-nx: Katamari sub_824DC0F0, render state 26 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_AlphaFunc  // katamari-nx: N&B sub_826355D8, render state 26 getter (state tables, this game index 26)  // S4ep1-nx: TFU sub_822D53C0, state table row 26 col 0
BANJO_RENUT_HOOK(sub_821B4048, GetRenderStateHook<27>);  // TFU-nx: Katamari sub_824DC3D8, render state 27 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_StencilEnable  // katamari-nx: N&B sub_826357F8, render state 27 getter (state tables, this game index 27)  // S4ep1-nx: TFU sub_822D56A8, state table row 27 col 0
BANJO_RENUT_HOOK(sub_821B4078, GetRenderStateHook<28>);  // TFU-nx: Katamari sub_824DC408, render state 28 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_TwoSidedStencilMode  // katamari-nx: N&B sub_82635828, render state 28 getter (state tables, this game index 28)  // S4ep1-nx: TFU sub_822D56D8, state table row 28 col 0
BANJO_RENUT_HOOK(sub_821B40E0, GetRenderStateHook<29>);  // TFU-nx: Katamari sub_824DC470, render state 29 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_StencilFail  // katamari-nx: N&B sub_82635890, render state 29 getter (state tables, this game index 29)  // S4ep1-nx: TFU sub_822D5740, state table row 29 col 0
BANJO_RENUT_HOOK(sub_821B4118, GetRenderStateHook<30>);  // TFU-nx: Katamari sub_824DC4A8, render state 30 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_StencilZFail  // katamari-nx: N&B sub_826358C8, render state 30 getter (state tables, this game index 30)  // S4ep1-nx: TFU sub_822D5778, state table row 30 col 0
BANJO_RENUT_HOOK(sub_821B4148, GetRenderStateHook<31>);  // TFU-nx: Katamari sub_824DC4D8, render state 31 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_StencilPass  // katamari-nx: N&B sub_826358F8, render state 31 getter (state tables, this game index 31)  // S4ep1-nx: TFU sub_822D57A8, state table row 31 col 0
BANJO_RENUT_HOOK(sub_821B40A8, GetRenderStateHook<32>);  // TFU-nx: Katamari sub_824DC438, render state 32 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_StencilFunc  // katamari-nx: N&B sub_82635858, render state 32 getter (state tables, this game index 32)  // S4ep1-nx: TFU sub_822D5708, state table row 32 col 0
BANJO_RENUT_HOOK(sub_821B4240, GetRenderStateHook<33>);  // TFU-nx: Katamari sub_824DC5D0, render state 33 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_StencilRef  // katamari-nx: N&B sub_826359F0, render state 33 getter (state tables, this game index 33)  // S4ep1-nx: TFU sub_822D58A0, state table row 33 col 0
BANJO_RENUT_HOOK(sub_821B4260, GetRenderStateHook<34>);  // TFU-nx: Katamari sub_824DC5F0, render state 34 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_StencilMask  // katamari-nx: N&B sub_82635A10, render state 34 getter (state tables, this game index 34)  // S4ep1-nx: TFU sub_822D58C0, state table row 34 col 0
BANJO_RENUT_HOOK(sub_821B4280, GetRenderStateHook<35>);  // TFU-nx: Katamari sub_824DC610, render state 35 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_StencilWriteMask  // katamari-nx: N&B sub_82635A30, render state 35 getter (state tables, this game index 35)  // S4ep1-nx: TFU sub_822D58E0, state table row 35 col 0
BANJO_RENUT_HOOK(sub_821B41B0, GetRenderStateHook<36>);  // TFU-nx: Katamari sub_824DC540, render state 36 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_CCWStencilFail  // katamari-nx: N&B sub_82635960, render state 36 getter (state tables, this game index 36)  // S4ep1-nx: TFU sub_822D5810, state table row 36 col 0
BANJO_RENUT_HOOK(sub_821B41E8, GetRenderStateHook<37>);  // TFU-nx: Katamari sub_824DC578, render state 37 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_CCWStencilZFail  // katamari-nx: N&B sub_82635998, render state 37 getter (state tables, this game index 37)  // S4ep1-nx: TFU sub_822D5848, state table row 37 col 0
BANJO_RENUT_HOOK(sub_821B4218, GetRenderStateHook<38>);  // TFU-nx: Katamari sub_824DC5A8, render state 38 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_CCWStencilPass  // katamari-nx: N&B sub_826359C8, render state 38 getter (state tables, this game index 38)  // S4ep1-nx: TFU sub_822D5878, state table row 38 col 0
BANJO_RENUT_HOOK(sub_821B4178, GetRenderStateHook<39>);  // TFU-nx: Katamari sub_824DC508, render state 39 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_CCWStencilFunc  // katamari-nx: N&B sub_82635928, render state 39 getter (state tables, this game index 39)  // S4ep1-nx: TFU sub_822D57D8, state table row 39 col 0
BANJO_RENUT_HOOK(sub_821B42A0, GetRenderStateHook<40>);  // TFU-nx: Katamari sub_824DC630, render state 40 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_CCWStencilRef  // katamari-nx: N&B sub_82635A50, render state 40 getter (state tables, this game index 40)  // S4ep1-nx: TFU sub_822D5900, state table row 40 col 0
BANJO_RENUT_HOOK(sub_821B42C0, GetRenderStateHook<41>);  // TFU-nx: Katamari sub_824DC650, render state 41 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_CCWStencilMask  // katamari-nx: N&B sub_82635A70, render state 41 getter (state tables, this game index 41)  // S4ep1-nx: TFU sub_822D5920, state table row 41 col 0
BANJO_RENUT_HOOK(sub_821B42E0, GetRenderStateHook<42>);  // TFU-nx: Katamari sub_824DC670, render state 42 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_CCWStencilWriteMask  // katamari-nx: N&B sub_82635A90, render state 42 getter (state tables, this game index 42)  // S4ep1-nx: TFU sub_822D5940, state table row 42 col 0
BANJO_RENUT_HOOK(sub_821B4330, GetRenderStateHook<43>);  // TFU-nx: Katamari sub_824DC6B8, render state 43 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_ClipPlaneEnable  // katamari-nx: N&B sub_82635A98, render state 43 getter (state tables, this game index 43)  // S4ep1-nx: TFU sub_822D5988, state table row 43 col 0
BANJO_RENUT_HOOK(sub_821B4678, GetRenderStateHook<44>);  // TFU-nx: Katamari sub_824DC9F0, render state 44 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_PointSize  // katamari-nx: N&B sub_82635D10, render state 44 getter (state tables, this game index 44)  // S4ep1-nx: TFU sub_822D5CC0, state table row 44 col 0
BANJO_RENUT_HOOK(sub_821B46C8, GetRenderStateHook<45>);  // TFU-nx: Katamari sub_824DCA48, render state 45 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_PointSizeMin  // katamari-nx: N&B sub_82635D68, render state 45 getter (state tables, this game index 45)  // S4ep1-nx: TFU sub_822D5D18, state table row 45 col 0
BANJO_RENUT_HOOK(sub_821B4620, GetRenderStateHook<46>);  // TFU-nx: Katamari sub_824DC998, render state 46 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_PointSpriteEnable  // katamari-nx: N&B sub_82635CB8, render state 46 getter (state tables, this game index 46)  // S4ep1-nx: TFU sub_822D5C68, state table row 46 col 0
BANJO_RENUT_HOOK(sub_821B4718, GetRenderStateHook<47>);  // TFU-nx: Katamari sub_824DCAA0, render state 47 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_PointSizeMax  // katamari-nx: N&B sub_82635D78, render state 47 getter (state tables, this game index 47)  // S4ep1-nx: TFU sub_822D5D70, state table row 47 col 0
BANJO_RENUT_HOOK(sub_821B44E8, GetRenderStateHook<48>);  // TFU-nx: Katamari sub_824DC860, render state 48 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_MultiSampleAntiAlias  // katamari-nx: N&B sub_82635BB8, render state 48 getter (state tables, this game index 48)  // S4ep1-nx: TFU sub_822D5B30, state table row 48 col 0
BANJO_RENUT_HOOK(sub_821B4510, GetRenderStateHook<49>);  // TFU-nx: Katamari sub_824DC888, render state 49 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_MultiSampleMask  // katamari-nx: N&B sub_82635BE0, render state 49 getter (state tables, this game index 49)  // S4ep1-nx: TFU sub_822D5B58, state table row 49 col 0
BANJO_RENUT_HOOK(sub_821B4350, GetRenderStateHook<50>);  // TFU-nx: Katamari sub_824DC6C8, render state 50 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_ScissorTestEnable  // katamari-nx: N&B sub_82635AB8, render state 50 getter (state tables, this game index 50)  // S4ep1-nx: TFU sub_822D5998, state table row 50 col 0
BANJO_RENUT_HOOK(sub_821B4400, GetRenderStateHook<51>);  // TFU-nx: Katamari sub_824DC778, render state 51 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_SlopeScaleDepthBias  // katamari-nx: N&B sub_82635B68, render state 51 getter (state tables, this game index 51)  // S4ep1-nx: TFU sub_822D5A48, state table row 51 col 0
BANJO_RENUT_HOOK(sub_821B44B8, GetRenderStateHook<52>);  // TFU-nx: Katamari sub_824DC830, render state 52 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_DepthBias  // katamari-nx: N&B sub_82635B88, render state 52 getter (state tables, this game index 52)  // S4ep1-nx: TFU sub_822D5B00, state table row 52 col 0
BANJO_RENUT_HOOK(sub_821B4550, GetRenderStateHook<53>);  // TFU-nx: Katamari sub_824DC8C8, render state 53 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_ColorWriteEnable  // katamari-nx: N&B sub_82635BE8, render state 53 getter (state tables, this game index 53)  // S4ep1-nx: TFU sub_822D5B98, state table row 53 col 0
BANJO_RENUT_HOOK(sub_821B4590, GetRenderStateHook<54>);  // TFU-nx: Katamari sub_824DC908, render state 54 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_ColorWriteEnable1  // katamari-nx: N&B sub_82635C28, render state 54 getter (state tables, this game index 54)  // S4ep1-nx: TFU sub_822D5BD8, state table row 54 col 0
BANJO_RENUT_HOOK(sub_821B45D0, GetRenderStateHook<55>);  // TFU-nx: Katamari sub_824DC948, render state 55 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_ColorWriteEnable2  // katamari-nx: N&B sub_82635C68, render state 55 getter (state tables, this game index 55)  // S4ep1-nx: TFU sub_822D5C18, state table row 55 col 0
BANJO_RENUT_HOOK(sub_821B4610, GetRenderStateHook<56>);  // TFU-nx: Katamari sub_824DC988, render state 56 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_ColorWriteEnable3  // katamari-nx: N&B sub_82635CA8, render state 56 getter (state tables, this game index 56)  // S4ep1-nx: TFU sub_822D5C58, state table row 56 col 0
BANJO_RENUT_HOOK(sub_821B4DD0, GetRenderStateHook<57>);  // TFU-nx: Katamari sub_824DD1B8, render state 57 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_TessellationMode  // katamari-nx: N&B sub_826363F0, render state 57 getter (state tables, this game index 57)  // S4ep1-nx: TFU sub_822D6488, state table row 57 col 0
BANJO_RENUT_HOOK(sub_821B4D60, GetRenderStateHook<58>);  // TFU-nx: Katamari sub_824DD148, render state 58 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_MinTessellationLevel  // katamari-nx: N&B sub_82636380, render state 58 getter (state tables, this game index 58)  // S4ep1-nx: TFU sub_822D6418, state table row 58 col 0
BANJO_RENUT_HOOK(sub_821B4D98, GetRenderStateHook<59>);  // TFU-nx: Katamari sub_824DD180, render state 59 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_MaxTessellationLevel  // katamari-nx: N&B sub_826363B8, render state 59 getter (state tables, this game index 59)  // S4ep1-nx: TFU sub_822D6450, state table row 59 col 0
BANJO_RENUT_HOOK(sub_821B4988, GetRenderStateHook<60>);  // TFU-nx: Katamari sub_824DCD10, render state 60 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap0  // katamari-nx: N&B sub_82635FE8, render state 60 getter (state tables, this game index 60)  // S4ep1-nx: TFU sub_822D5FE0, state table row 60 col 0
BANJO_RENUT_HOOK(sub_821B4998, GetRenderStateHook<61>);  // TFU-nx: Katamari sub_824DCD20, render state 61 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap1  // katamari-nx: N&B sub_82635FF8, render state 61 getter (state tables, this game index 61)  // S4ep1-nx: TFU sub_822D5FF0, state table row 61 col 0
BANJO_RENUT_HOOK(sub_821B49A8, GetRenderStateHook<62>);  // TFU-nx: Katamari sub_824DCD30, render state 62 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap2  // katamari-nx: N&B sub_82636008, render state 62 getter (state tables, this game index 62)  // S4ep1-nx: TFU sub_822D6000, state table row 62 col 0
BANJO_RENUT_HOOK(sub_821B49B8, GetRenderStateHook<63>);  // TFU-nx: Katamari sub_824DCD40, render state 63 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap3  // katamari-nx: N&B sub_82636018, render state 63 getter (state tables, this game index 63)  // S4ep1-nx: TFU sub_822D6010, state table row 63 col 0
BANJO_RENUT_HOOK(sub_821B49C8, GetRenderStateHook<64>);  // TFU-nx: Katamari sub_824DCD50, render state 64 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap4  // katamari-nx: N&B sub_82636028, render state 64 getter (state tables, this game index 64)  // S4ep1-nx: TFU sub_822D6020, state table row 64 col 0
BANJO_RENUT_HOOK(sub_821B49D8, GetRenderStateHook<65>);  // TFU-nx: Katamari sub_824DCD60, render state 65 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap5  // katamari-nx: N&B sub_82636038, render state 65 getter (state tables, this game index 65)  // S4ep1-nx: TFU sub_822D6030, state table row 65 col 0
BANJO_RENUT_HOOK(sub_821B49E8, GetRenderStateHook<66>);  // TFU-nx: Katamari sub_824DCD70, render state 66 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap6  // katamari-nx: N&B sub_82636048, render state 66 getter (state tables, this game index 66)  // S4ep1-nx: TFU sub_822D6040, state table row 66 col 0
BANJO_RENUT_HOOK(sub_821B49F8, GetRenderStateHook<67>);  // TFU-nx: Katamari sub_824DCD80, render state 67 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap7  // katamari-nx: N&B sub_82636058, render state 67 getter (state tables, this game index 67)  // S4ep1-nx: TFU sub_822D6050, state table row 67 col 0
BANJO_RENUT_HOOK(sub_821B4A08, GetRenderStateHook<68>);  // TFU-nx: Katamari sub_824DCD90, render state 68 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap8  // katamari-nx: N&B sub_82636068, render state 68 getter (state tables, this game index 68)  // S4ep1-nx: TFU sub_822D6060, state table row 68 col 0
BANJO_RENUT_HOOK(sub_821B4A18, GetRenderStateHook<69>);  // TFU-nx: Katamari sub_824DCDA0, render state 69 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap9  // katamari-nx: N&B sub_82636078, render state 69 getter (state tables, this game index 69)  // S4ep1-nx: TFU sub_822D6070, state table row 69 col 0
BANJO_RENUT_HOOK(sub_821B4A28, GetRenderStateHook<70>);  // TFU-nx: Katamari sub_824DCDB0, render state 70 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap10  // katamari-nx: N&B sub_82636088, render state 70 getter (state tables, this game index 70)  // S4ep1-nx: TFU sub_822D6080, state table row 70 col 0
BANJO_RENUT_HOOK(sub_821B4A38, GetRenderStateHook<71>);  // TFU-nx: Katamari sub_824DCDC0, render state 71 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap11  // katamari-nx: N&B sub_82636098, render state 71 getter (state tables, this game index 71)  // S4ep1-nx: TFU sub_822D6090, state table row 71 col 0
BANJO_RENUT_HOOK(sub_821B4A48, GetRenderStateHook<72>);  // TFU-nx: Katamari sub_824DCDD0, render state 72 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap12  // katamari-nx: N&B sub_826360A8, render state 72 getter (state tables, this game index 72)  // S4ep1-nx: TFU sub_822D60A0, state table row 72 col 0
BANJO_RENUT_HOOK(sub_821B4A58, GetRenderStateHook<73>);  // TFU-nx: Katamari sub_824DCDE0, render state 73 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap13  // katamari-nx: N&B sub_826360B8, render state 73 getter (state tables, this game index 73)  // S4ep1-nx: TFU sub_822D60B0, state table row 73 col 0
BANJO_RENUT_HOOK(sub_821B4A68, GetRenderStateHook<74>);  // TFU-nx: Katamari sub_824DCDF0, render state 74 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap14  // katamari-nx: N&B sub_826360C8, render state 74 getter (state tables, this game index 74)  // S4ep1-nx: TFU sub_822D60C0, state table row 74 col 0
BANJO_RENUT_HOOK(sub_821B4A78, GetRenderStateHook<75>);  // TFU-nx: Katamari sub_824DCE00, render state 75 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Wrap15  // katamari-nx: N&B sub_826360D8, render state 75 getter (state tables, this game index 75)  // S4ep1-nx: TFU sub_822D60D0, state table row 75 col 0
BANJO_RENUT_HOOK(sub_821B4AC8, GetRenderStateHook<76>);  // TFU-nx: Katamari sub_824DCE50, render state 76 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_ViewportEnable  // katamari-nx: N&B sub_826360E8, render state 76 getter (state tables, this game index 76)  // S4ep1-nx: TFU sub_822D6120, state table row 76 col 0
BANJO_RENUT_HOOK(sub_821B4B68, GetRenderStateHook<77>);  // TFU-nx: Katamari sub_824DCF08, render state 77 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_HighPrecisionBlendEnable  // katamari-nx: N&B sub_82636188, render state 77 getter (state tables, this game index 77)  // S4ep1-nx: TFU sub_822D61D8, state table row 77 col 0
BANJO_RENUT_HOOK(sub_821B4C00, GetRenderStateHook<78>);  // TFU-nx: Katamari sub_824DCFB8, render state 78 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_HighPrecisionBlendEnable1  // katamari-nx: N&B sub_82636220, render state 78 getter (state tables, this game index 78)  // S4ep1-nx: TFU sub_822D6288, state table row 78 col 0
BANJO_RENUT_HOOK(sub_821B4C98, GetRenderStateHook<79>);  // TFU-nx: Katamari sub_824DD068, render state 79 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_HighPrecisionBlendEnable2  // katamari-nx: N&B sub_826362B8, render state 79 getter (state tables, this game index 79)  // S4ep1-nx: TFU sub_822D6338, state table row 79 col 0
BANJO_RENUT_HOOK(sub_821B4D30, GetRenderStateHook<80>);  // TFU-nx: Katamari sub_824DD118, render state 80 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_HighPrecisionBlendEnable3  // katamari-nx: N&B sub_82636350, render state 80 getter (state tables, this game index 80)  // S4ep1-nx: TFU sub_822D63E8, state table row 80 col 0
BANJO_RENUT_HOOK(sub_821B4E08, GetRenderStateHook<81>);  // TFU-nx: Katamari sub_824DD1F0, render state 81 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_HalfPixelOffset  // katamari-nx: N&B sub_82636428, render state 81 getter (state tables, this game index 81)  // S4ep1-nx: TFU sub_822D64C0, state table row 81 col 0
BANJO_RENUT_HOOK(sub_821B4E38, GetRenderStateHook<82>);  // TFU-nx: Katamari sub_824DD220, render state 82 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_PrimitiveResetEnable  // katamari-nx: N&B sub_82636458, render state 82 getter (state tables, this game index 82)  // S4ep1-nx: TFU sub_822D64F0, state table row 82 col 0
BANJO_RENUT_HOOK(sub_821B4E68, GetRenderStateHook<83>);  // TFU-nx: Katamari sub_824DD250, render state 83 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_PrimitiveResetIndex  // katamari-nx: N&B sub_82636488, render state 83 getter (state tables, this game index 83)  // S4ep1-nx: TFU sub_822D6520, state table row 83 col 0
BANJO_RENUT_HOOK(sub_821B4E90, GetRenderStateHook<84>);  // TFU-nx: Katamari sub_824DD278, render state 84 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_AlphaToMaskEnable  // katamari-nx: N&B sub_826364B0, render state 84 getter (state tables, this game index 84)  // S4ep1-nx: TFU sub_822D6548, state table row 84 col 0
BANJO_RENUT_HOOK(sub_821B4EC0, GetRenderStateHook<85>);  // TFU-nx: Katamari sub_824DD2A8, render state 85 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_AlphaToMaskOffsets  // katamari-nx: N&B sub_826364E0, render state 85 getter (state tables, this game index 85)  // S4ep1-nx: TFU sub_822D6578, state table row 85 col 0
BANJO_RENUT_HOOK(sub_821B4EF0, GetRenderStateHook<86>);  // TFU-nx: Katamari sub_824DD2E8, render state 86 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_GuardBand_X  // katamari-nx: N&B sub_82636510, render state 86 getter (state tables, this game index 86)  // S4ep1-nx: TFU sub_822D65B8, state table row 86 col 0
BANJO_RENUT_HOOK(sub_821B4F20, GetRenderStateHook<87>);  // TFU-nx: Katamari sub_824DD318, render state 87 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_GuardBand_Y  // katamari-nx: N&B sub_82636540, render state 87 getter (state tables, this game index 87)  // S4ep1-nx: TFU sub_822D65E8, state table row 87 col 0
BANJO_RENUT_HOOK(sub_821B4F48, GetRenderStateHook<88>);  // TFU-nx: Katamari sub_824DD340, render state 88 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_DiscardBand_X  // katamari-nx: N&B sub_82636568, render state 88 getter (state tables, this game index 88)  // S4ep1-nx: TFU sub_822D6610, state table row 88 col 0
BANJO_RENUT_HOOK(sub_821B4F78, GetRenderStateHook<89>);  // TFU-nx: Katamari sub_824DD370, render state 89 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_DiscardBand_Y  // katamari-nx: N&B sub_82636598, render state 89 getter (state tables, this game index 89)  // S4ep1-nx: TFU sub_822D6640, state table row 89 col 0
BANJO_RENUT_HOOK(sub_821B5038, GetRenderStateHook<90>);  // TFU-nx: Katamari sub_824DD430, render state 90 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_HiStencilEnable  // katamari-nx: N&B sub_82636600, render state 90 getter (state tables, this game index 90)  // S4ep1-nx: TFU sub_822D6700, state table row 90 col 0
BANJO_RENUT_HOOK(sub_821B5068, GetRenderStateHook<91>);  // TFU-nx: Katamari sub_824DD460, render state 91 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_HiStencilWriteEnable  // katamari-nx: N&B sub_82636630, render state 91 getter (state tables, this game index 91)  // S4ep1-nx: TFU sub_822D6730, state table row 91 col 0
BANJO_RENUT_HOOK(sub_821B5098, GetRenderStateHook<92>);  // TFU-nx: Katamari sub_824DD490, render state 92 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_HiStencilFunc  // katamari-nx: N&B sub_82636660, render state 92 getter (state tables, this game index 92)  // S4ep1-nx: TFU sub_822D6760, state table row 92 col 0
BANJO_RENUT_HOOK(sub_821B50C0, GetRenderStateHook<93>);  // TFU-nx: Katamari sub_824DD4B8, render state 93 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_HiStencilRef  // katamari-nx: N&B sub_82636688, render state 93 getter (state tables, this game index 93)  // S4ep1-nx: TFU sub_822D6788, state table row 93 col 0
BANJO_RENUT_HOOK(sub_821B50D0, GetRenderStateHook<94>);  // TFU-nx: Katamari sub_824DD4C8, render state 94 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_PresentInterval  // katamari-nx: N&B sub_82636698, render state 94 getter (state tables, this game index 94)  // S4ep1-nx: TFU sub_822D6798, state table row 94 col 0
BANJO_RENUT_HOOK(sub_821B50E8, GetRenderStateHook<95>);  // TFU-nx: Katamari sub_824DD4E0, render state 95 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_PresentImmediateThreshold  // katamari-nx: N&B sub_826366B0, render state 95 getter (state tables, this game index 95)  // S4ep1-nx: TFU sub_822D67B0, state table row 95 col 0
BANJO_RENUT_HOOK(sub_821B4FA0, GetRenderStateHook<96>);  // TFU-nx: Katamari sub_824DD398, render state 96 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_HiZEnable  // katamari-nx: N&B sub_826365C0, render state 96 getter (state tables, this game index 96)  // S4ep1-nx: TFU sub_822D6668, state table row 96 col 0
BANJO_RENUT_HOOK(sub_821B5008, GetRenderStateHook<97>);  // TFU-nx: Katamari sub_824DD400, render state 97 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_HiZWriteEnable  // katamari-nx: N&B sub_826365D0, render state 97 getter (state tables, this game index 97)  // S4ep1-nx: TFU sub_822D66D0, state table row 97 col 0
BANJO_RENUT_HOOK(sub_821B3EB0, GetRenderStateHook<98>);  // TFU-nx: Katamari sub_824DC248, render state 98 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_LastPixel  // katamari-nx: N&B sub_82635688, render state 98 getter (state tables, this game index 98)  // S4ep1-nx: TFU sub_822D5518, state table row 98 col 0
BANJO_RENUT_HOOK(sub_821B3F00, GetRenderStateHook<99>);  // TFU-nx: Katamari sub_824DC298, render state 99 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_LineWidth  // katamari-nx: N&B sub_826356D8, render state 99 getter (state tables, this game index 99)  // S4ep1-nx: TFU sub_822D5568, state table row 99 col 0
BANJO_RENUT_HOOK(sub_821B3F58, GetRenderStateHook<100>);  // TFU-nx: Katamari sub_824DC2E8, render state 100 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetRenderState_Buffer2Frames  // katamari-nx: N&B sub_82635730, render state 100 getter (state tables, this game index 100)  // S4ep1-nx: TFU sub_822D55B8, state table row 100 col 0

// Sampler state getters (off_82E50BD0, field 0).
BANJO_RENUT_HOOK(sub_821B58B0, GetSamplerStateHook<0>);  // TFU-nx: Katamari sub_824DDC68, sampler state 0 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_AddressU  // katamari-nx: N&B sub_82636C00, sampler state 0 getter (state tables, this game index 0)  // S4ep1-nx: TFU sub_822D6F38, state table row 0 col 0
BANJO_RENUT_HOOK(sub_821B5900, GetSamplerStateHook<1>);  // TFU-nx: Katamari sub_824DDCB8, sampler state 1 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_AddressV  // katamari-nx: N&B sub_82636C18, sampler state 1 getter (state tables, this game index 1)  // S4ep1-nx: TFU sub_822D6F88, state table row 1 col 0
BANJO_RENUT_HOOK(sub_821B5950, GetSamplerStateHook<2>);  // TFU-nx: Katamari sub_824DDD08, sampler state 2 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_AddressW  // katamari-nx: N&B sub_82636C68, sampler state 2 getter (state tables, this game index 2)  // S4ep1-nx: TFU sub_822D6FD8, state table row 2 col 0
BANJO_RENUT_HOOK(sub_821B5850, GetSamplerStateHook<3>);  // TFU-nx: Katamari sub_824DDC10, sampler state 3 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_BorderColor  // katamari-nx: N&B sub_82636BD8, sampler state 3 getter (state tables, this game index 3)  // S4ep1-nx: TFU sub_822D6EE0, state table row 3 col 0
BANJO_RENUT_HOOK(sub_821B5368, GetSamplerStateHook<4>);  // TFU-nx: Katamari sub_824DD740, sampler state 4 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_MagFilter  // katamari-nx: N&B sub_826367A0, sampler state 4 getter (state tables, this game index 4)  // S4ep1-nx: TFU sub_822D6A10, state table row 4 col 0
BANJO_RENUT_HOOK(sub_821B51C0, GetSamplerStateHook<5>);  // TFU-nx: Katamari sub_824DD5B0, sampler state 5 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_MinFilter  // katamari-nx: N&B sub_826366C0, sampler state 5 getter (state tables, this game index 5)  // S4ep1-nx: TFU sub_822D6880, state table row 5 col 0
BANJO_RENUT_HOOK(sub_821B5480, GetSamplerStateHook<6>);  // TFU-nx: Katamari sub_824DD850, sampler state 6 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_MipFilter  // katamari-nx: N&B sub_82636878, sampler state 6 getter (state tables, this game index 6)  // S4ep1-nx: TFU sub_822D6B20, state table row 6 col 0
BANJO_RENUT_HOOK(sub_821B56C0, GetSamplerStateHook<7>);  // TFU-nx: Katamari sub_824DDA80, sampler state 7 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_MipMapLodBias  // katamari-nx: N&B sub_82636AB8, sampler state 7 getter (state tables, this game index 7)  // S4ep1-nx: TFU sub_822D6D50, state table row 7 col 0
BANJO_RENUT_HOOK(sub_821B5778, GetSamplerStateHook<8>);  // TFU-nx: Katamari sub_824DDB30, sampler state 8 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_MaxMipLevel  // katamari-nx: N&B sub_82636B00, sampler state 8 getter (state tables, this game index 8)  // S4ep1-nx: TFU sub_822D6E00, state table row 8 col 0
BANJO_RENUT_HOOK(sub_821B55A8, GetSamplerStateHook<9>);  // TFU-nx: Katamari sub_824DD968, sampler state 9 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_MaxAnisotropy  // katamari-nx: N&B sub_826369A0, sampler state 9 getter (state tables, this game index 9)  // S4ep1-nx: TFU sub_822D6C38, state table row 9 col 0
BANJO_RENUT_HOOK(sub_821B5430, GetSamplerStateHook<10>);  // TFU-nx: Katamari sub_824DD800, sampler state 10 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_MagFilterZ  // katamari-nx: N&B sub_82636868, sampler state 10 getter (state tables, this game index 10)  // S4ep1-nx: TFU sub_822D6AD0, state table row 10 col 0
BANJO_RENUT_HOOK(sub_821B5290, GetSamplerStateHook<11>);  // TFU-nx: Katamari sub_824DD670, sampler state 11 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_MinFilterZ  // katamari-nx: N&B sub_82636790, sampler state 11 getter (state tables, this game index 11)  // S4ep1-nx: TFU sub_822D6940, state table row 11 col 0
BANJO_RENUT_HOOK(sub_821B5530, GetSamplerStateHook<12>);  // TFU-nx: Katamari sub_824DD8F0, sampler state 12 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_SeparateZFilterEnable  // katamari-nx: N&B sub_82636928, sampler state 12 getter (state tables, this game index 12)  // S4ep1-nx: TFU sub_822D6BC0, state table row 12 col 0
BANJO_RENUT_HOOK(sub_821B57F8, GetSamplerStateHook<13>);  // TFU-nx: Katamari sub_824DDBB0, sampler state 13 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_MinMipLevel  // katamari-nx: N&B sub_82636B80, sampler state 13 getter (state tables, this game index 13)  // S4ep1-nx: TFU sub_822D6E80, state table row 13 col 0
BANJO_RENUT_HOOK(sub_821B59A8, GetSamplerStateHook<14>);  // TFU-nx: Katamari sub_824DDD60, sampler state 14 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_TrilinearThreshold  // katamari-nx: N&B sub_82636CC0, sampler state 14 getter (state tables, this game index 14)  // S4ep1-nx: TFU sub_822D7030, state table row 14 col 0
BANJO_RENUT_HOOK(sub_821B5618, GetSamplerStateHook<15>);  // TFU-nx: Katamari sub_824DD9D8, sampler state 15 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_AnisotropyBias  // katamari-nx: N&B sub_82636A10, sampler state 15 getter (state tables, this game index 15)  // S4ep1-nx: TFU sub_822D6CA8, state table row 15 col 0
BANJO_RENUT_HOOK(sub_821B5A00, GetSamplerStateHook<16>);  // TFU-nx: Katamari sub_824DDDB8, sampler state 16 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_HGradientExpBias  // katamari-nx: N&B sub_82636D18, sampler state 16 getter (state tables, this game index 16)  // S4ep1-nx: TFU sub_822D7088, state table row 16 col 0
BANJO_RENUT_HOOK(sub_821B5A58, GetSamplerStateHook<17>);  // TFU-nx: Katamari sub_824DDE10, sampler state 17 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_VGradientExpBias  // katamari-nx: N&B sub_82636D70, sampler state 17 getter (state tables, this game index 17)  // S4ep1-nx: TFU sub_822D70E0, state table row 17 col 0
BANJO_RENUT_HOOK(sub_821B5AB0, GetSamplerStateHook<18>);  // TFU-nx: Katamari sub_824DDE68, sampler state 18 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_WhiteBorderColorW  // katamari-nx: N&B sub_82636DC8, sampler state 18 getter (state tables, this game index 18)  // S4ep1-nx: TFU sub_822D7138, state table row 18 col 0
BANJO_RENUT_HOOK(sub_821B5B10, GetSamplerStateHook<19>);  // TFU-nx: Katamari sub_824DDEC8, sampler state 19 get (tables 0x82AD8E48/0x82F43938 by index);  // D3DDevice_GetSamplerState_PointBorderEnable  // katamari-nx: N&B sub_82636E28, sampler state 19 getter (state tables, this game index 19)  // S4ep1-nx: TFU sub_822D7198, state table row 19 col 0
