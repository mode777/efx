/*
 * EmotionFX post-processing passes — the single shader source for all
 * platforms (ADR 0021), transpiled by the pinned sokol-shdc into
 * shaders/post.h.
 *
 * Every pass is a fullscreen triangle-strip quad (positions in NDC, uv in
 * [0,1] top-left origin). Effect options arrive as uniforms and `mix` is a
 * uniform lerp: one uniform-driven shader per pass type, no per-effect
 * permutations (ADR 0026/0027). The pass structure (which passes run, into
 * which engine-owned target) is owned by the native effect registry and is
 * never script-visible (ADR 0015).
 *
 * `post_params.x` is the y-flip factor: +1 when the pass renders into the
 * default framebuffer, -1 when it renders into an offscreen target on a
 * GL-family backend (ADR 0028's origin convention, engine-owned). The CPU
 * folds the backend choice into this uniform.
 */

@vs post_vs
layout(binding=0) uniform post_vs_params {
    vec4 post_params; /* x = y flip (+1 default surface, -1 GL-family RT) */
};

in vec2 a_pos;
in vec2 a_uv;

out vec2 efx_uv;

void main() {
    gl_Position = vec4(a_pos.x, a_pos.y * post_params.x, 0.0, 1.0);
    efx_uv = a_uv;
}
@end

/* colorFilter: brightness (multiply) -> contrast (pivot 0.5 grey) ->
   saturation (luminance lerp) -> tint (rgb multiply), then mix. */
@fs post_color_fs
layout(binding=1) uniform post_color_params {
    vec4 color_params; /* x=brightness y=contrast z=saturation w=mix */
    vec4 tint;         /* rgb = tint (alpha ignored) */
};

layout(binding=0) uniform texture2D post_color_src;
layout(binding=0) uniform sampler post_color_smp;

in vec2 efx_uv;
out vec4 frag_color;

void main() {
    vec4 src = texture(sampler2D(post_color_src, post_color_smp), efx_uv);
    vec3 c = src.rgb;
    c = c * color_params.x;
    c = (c - 0.5) * color_params.y + 0.5;
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(vec3(lum), c, color_params.z);
    c = c * tint.rgb;
    vec3 out_rgb = mix(src.rgb, clamp(c, 0.0, 1.0), color_params.w);
    frag_color = vec4(out_rgb, src.a);
}
@end

/* separable gaussian tap pass: samples +/- 4 taps along blur_dir, which
   already carries (radius / 4) scaled into source-texel units. Weights are
   a fixed gaussian shape; the radius scales the tap spacing. */
@fs post_blur_fs
layout(binding=1) uniform post_blur_params {
    vec4 blur_dir; /* xy = per-tap uv step (direction * radius / 4) */
};

layout(binding=0) uniform texture2D post_blur_src;
layout(binding=0) uniform sampler post_blur_smp;

in vec2 efx_uv;
out vec4 frag_color;

void main() {
    vec4 acc = texture(sampler2D(post_blur_src, post_blur_smp), efx_uv);
    float wsum = 1.0;
    for (int i = 1; i <= 4; i++) {
        float fi = float(i);
        float w = exp(-0.5 * (fi / 2.0) * (fi / 2.0));
        acc += texture(sampler2D(post_blur_src, post_blur_smp),
                       efx_uv + blur_dir.xy * fi) * w;
        acc += texture(sampler2D(post_blur_src, post_blur_smp),
                       efx_uv - blur_dir.xy * fi) * w;
        wsum += 2.0 * w;
    }
    frag_color = acc / wsum;
}
@end

/* bright pass: keep only luminance above the threshold (scaled by the
   threshold, the standard soft knee-free cut) */
@fs post_bright_fs
layout(binding=1) uniform post_bright_params {
    vec4 bright_params; /* x = threshold (0..1) */
};

layout(binding=0) uniform texture2D post_bright_src;
layout(binding=0) uniform sampler post_bright_smp;

in vec2 efx_uv;
out vec4 frag_color;

void main() {
    vec4 src = texture(sampler2D(post_bright_src, post_bright_smp), efx_uv);
    float lum = dot(src.rgb, vec3(0.2126, 0.7152, 0.0722));
    float keep = lum > bright_params.x ? lum : 0.0;
    float scale = keep > 0.0 ? (keep - bright_params.x) / keep : 0.0;
    frag_color = vec4(src.rgb * scale, src.a);
}
@end

/* two-source lerp: out = mix(a, b, t) — the per-entry mix pass */
@fs post_mix_fs
layout(binding=1) uniform post_mix_params {
    vec4 mix_params; /* x = mix (0..1) */
};

layout(binding=0) uniform texture2D post_mix_a;
layout(binding=0) uniform sampler post_mix_a_smp;
layout(binding=1) uniform texture2D post_mix_b;
layout(binding=1) uniform sampler post_mix_b_smp;

in vec2 efx_uv;
out vec4 frag_color;

void main() {
    vec4 a = texture(sampler2D(post_mix_a, post_mix_a_smp), efx_uv);
    vec4 b = texture(sampler2D(post_mix_b, post_mix_b_smp), efx_uv);
    frag_color = mix(a, b, mix_params.x);
}
@end

/* additive composite (bloom upsample): out = mix(a, a + b*strength, mix) */
@fs post_composite_fs
layout(binding=1) uniform post_composite_params {
    vec4 comp_params; /* x = strength y = mix */
};

layout(binding=0) uniform texture2D post_comp_base;
layout(binding=0) uniform sampler post_comp_base_smp;
layout(binding=1) uniform texture2D post_comp_add;
layout(binding=1) uniform sampler post_comp_add_smp;

in vec2 efx_uv;
out vec4 frag_color;

void main() {
    vec4 base = texture(sampler2D(post_comp_base, post_comp_base_smp), efx_uv);
    vec4 add = texture(sampler2D(post_comp_add, post_comp_add_smp), efx_uv);
    vec3 bloomed = base.rgb + add.rgb * comp_params.x;
    frag_color = vec4(mix(base.rgb, clamp(bloomed, 0.0, 1.0), comp_params.y),
                      base.a);
}
@end

/* plain sample: the final blit (and internal downsample/upsample) */
@fs post_copy_fs
layout(binding=0) uniform texture2D post_copy_src;
layout(binding=0) uniform sampler post_copy_smp;

in vec2 efx_uv;
out vec4 frag_color;

void main() {
    frag_color = texture(sampler2D(post_copy_src, post_copy_smp), efx_uv);
}
@end

@program post_color post_vs post_color_fs
@program post_blur post_vs post_blur_fs
@program post_bright post_vs post_bright_fs
@program post_mix post_vs post_mix_fs
@program post_composite post_vs post_composite_fs
@program post_copy post_vs post_copy_fs
