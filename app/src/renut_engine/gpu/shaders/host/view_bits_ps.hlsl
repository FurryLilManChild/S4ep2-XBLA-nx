// S4ep2-XBLA-nx (2026-11-21, GitHub issue 1): one EDRAM view read through another, carried over bit for bit as the
// console holds the pixel (draw.cpp, SyncColorViewLocked). On the console the views are the same 32-bit EDRAM pixels;
// here each is its own host target, so the pixel is packed into the console's 32 bits from the target drawn last and
// unpacked as the view being resolved reads it. Episode II draws 2_10_10_10_FLOAT (7e3) and 32_FLOAT targets and
// resolves the same EDRAM through 8_8_8_8 (title, special stage pause and results, waterfalls), and the reverse.
// Layouts (as Xenos stores them, R in the low bits): 8_8_8_8 R 0-7, G 8-15, B 16-23, A 24-31; 2_10_10_10(_FLOAT)
// R 0-9, G 10-19, B 20-29, A 30-31; 32_FLOAT one IEEE float.
// g_Push.param1 is the mode: 1 = 7e3 to 8_8_8_8, 2 = 8_8_8_8 to 7e3, 3 = 32_FLOAT to 8_8_8_8, 4 = 7e3 to 32_FLOAT
// (2026-11-22: Mark's run of 4d08d3da dropped that pair 70-601 times every 10 s; it feeds the 8_8_8_8 copies).
// g_Push.param0 is 2^bias of the float view (a host target holds the shader output without the bias).
// The pixel plus g_Push.translate is the source texel; present_vs draws the fullscreen triangle.
#include "utility.hlsli"

// The 10 bits of a 7e3 float: 3 bits of exponent, 7 of mantissa; 0 to 31.875, denormal below 0.25.
uint Encode7e3(float value)
{
    const float v = clamp(value, 0.0, 31.875);
    if (!(v >= 0.25))
        return uint(floor(v * 512.0 + 0.5));
    const float exponent = floor(log2(v));
    const uint mantissa = uint(floor((v / exp2(exponent) - 1.0) * 128.0 + 0.5));
    return min(uint(exponent + 3.0) * 128u + mantissa, 1023u);
}

float Decode7e3(uint bits)
{
    const uint exponent = (bits >> 7) & 7u;
    const uint mantissa = bits & 127u;
    if (exponent == 0u)
        return float(mantissa) / 512.0;
    return (1.0 + float(mantissa) / 128.0) * exp2(float(exponent) - 3.0);
}

float4 Bytes(uint word)
{
    return float4(float(word & 255u), float((word >> 8) & 255u), float((word >> 16) & 255u), float(word >> 24)) / 255.0;
}

float4 main(float4 position : SV_Position) : SV_Target0
{
    const int2 texel = int2(position.xy) + int2(g_Push.translate);
    const float4 c = g_Textures[NonUniformResourceIndex(g_Push.textureIndex)].Load(int3(texel, 0));
    const uint mode = uint(g_Push.param1 + 0.5);
    if (mode == 1u || mode == 4u)
    {
        const uint a = uint(clamp(c.a, 0.0, 1.0) * 3.0 + 0.5);
        const uint word = Encode7e3(c.r * g_Push.param0) | (Encode7e3(c.g * g_Push.param0) << 10) |
                          (Encode7e3(c.b * g_Push.param0) << 20) | (a << 30);
        if (mode == 4u)
            return float4(asfloat(word), 0.0, 0.0, 1.0);
        return Bytes(word);
    }
    if (mode == 2u)
    {
        const uint4 b = uint4(clamp(c, 0.0, 1.0) * 255.0 + 0.5);
        const uint word = b.r | (b.g << 8) | (b.b << 16) | (b.a << 24);
        const float scale = 1.0 / g_Push.param0;
        return float4(Decode7e3(word & 1023u) * scale, Decode7e3((word >> 10) & 1023u) * scale,
                      Decode7e3((word >> 20) & 1023u) * scale, float(word >> 30) / 3.0);
    }
    return Bytes(asuint(c.r));
}
