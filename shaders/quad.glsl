/*
 * EFX 2D quad + billboard shader — the single shader source for all
 * platforms (ADR 0021). Compiled at author time with the pinned sokol-shdc
 * (vendor/README.md) into shaders/quad.h.
 *
 * Vertex inputs come from the engine's interleaved quad vertex layout
 * (pos 3f, uv 2f, color ub4n); positions arrive in NDC with an already
 * divided clip depth. F11 extends the position to 3 components so the same
 * shader serves depth-tested billboards/oriented quads (the `billboard`
 * pipeline, depth test/no write) and plain 2D quads (the `quad` pipeline,
 * depth always/no write), which pass z = 0.
 */

@vs quad_vs
in vec3 a_pos;
in vec2 a_uv;
in vec4 a_color;

out vec2 efx_uv;
out vec4 efx_color;

void main() {
    gl_Position = vec4(a_pos, 1.0);
    efx_uv = a_uv;
    efx_color = a_color;
}
@end

@fs quad_fs
in vec2 efx_uv;
in vec4 efx_color;

layout(binding=0) uniform texture2D efx_tex;
layout(binding=0) uniform sampler efx_smp;

out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(efx_tex, efx_smp), efx_uv) * efx_color;
}
@end

@program quad quad_vs quad_fs
