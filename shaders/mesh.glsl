/*
 * EmotionFX 3D mesh shader — single shader source for all platforms
 * (ADR 0021), transpiled by the pinned sokol-shdc into shaders/mesh.h.
 *
 * F4a canned fill: world-space Phong (design D1/D7, ADR 0026). Ambient +
 * diffuse + specular (Blinn-Phong) + emissive over the fixed light bank;
 * albedo = vertex color × tint. Positions arrive in clip space (MVP
 * premultiplied on the CPU, design D3/D7 — the D3D11/Metal depth-range
 * remap is folded into the MVP by the platform layer, not by per-backend
 * shader code). Normals are transformed by a CPU-computed normal matrix.
 *
 * One uniform-driven shader, no per-material permutations (F4a; maps in
 * F4b may introduce the first permutation).
 */

@vs mesh_vs
layout(binding=0) uniform vs_params {
    mat4 mvp;         /* clip = mvp * pos (premultiplied VP × model) */
    mat4 model;       /* object -> world */
    mat4 normal_mat;  /* transpose(inverse(mat3(model))) as mat4 */
};

/* Slot order is consumed-first (pos, color, then normal, uv): every slot is
   declared and bound by the pipeline so the layout matches the compiled
   input signature on D3D11/Metal; normal/uv are consumed by F4a lighting. */
in vec3 a_pos;
in vec4 a_color;
in vec3 a_normal;
in vec2 a_uv;

out vec3 efx_world_pos;
out vec3 efx_normal;
out vec4 efx_color;

void main() {
    vec4 wp = model * vec4(a_pos, 1.0);
    efx_world_pos = wp.xyz;
    efx_normal = (normal_mat * vec4(a_normal, 0.0)).xyz;
    efx_color = a_color;
    gl_Position = mvp * vec4(a_pos, 1.0);
}
@end

@fs mesh_fs
layout(binding=1) uniform fs_params {
    vec4 tint;            /* drawMesh tint */
    vec4 ambient;         /* material ambient (rgb) */
    vec4 diffuse;         /* material diffuse (rgb) */
    vec4 specular;        /* material specular (rgb) */
    vec4 emissive;        /* material emissive (rgb) */
    vec4 mat_params;      /* x = shininess */
    vec4 camera_pos;      /* xyz = eye position */
    vec4 point_pos[4];    /* xyz = position, w = range (0 = no falloff) */
    vec4 point_color[4];  /* rgb = color, w = enabled (1/0) */
    vec4 dir_dir;         /* xyz = travel direction, w = enabled (1/0) */
    vec4 dir_color;       /* rgb = color */
};

in vec3 efx_world_pos;
in vec3 efx_normal;
in vec4 efx_color;

out vec4 frag_color;

/* one light contribution: albedo already includes material diffuse ×
   vertex color × tint; specular is not modulated by the albedo (D7) */
vec3 efx_light_term(vec3 N, vec3 V, vec3 L, float atten, vec3 light_color,
                    vec3 albedo) {
    float ndl = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float ndh = max(dot(N, H), 0.0);
    float sp = pow(ndh, mat_params.x);
    return (diffuse.rgb * albedo * ndl + specular.rgb * sp) * light_color * atten;
}

void main() {
    vec3 albedo = efx_color.rgb * tint.rgb;
    vec3 N = normalize(efx_normal);
    vec3 V = normalize(camera_pos.xyz - efx_world_pos);
    vec3 col = ambient.rgb * albedo + emissive.rgb;
    for (int i = 0; i < 4; i++) {
        if (point_color[i].w > 0.5) {
            vec3 to_light = point_pos[i].xyz - efx_world_pos;
            float d = length(to_light);
            vec3 L = d > 0.0 ? to_light / d : vec3(0.0, 1.0, 0.0);
            float range = point_pos[i].w;
            float atten = range > 0.0 ? clamp(1.0 - d / range, 0.0, 1.0) : 1.0;
            col += efx_light_term(N, V, L, atten, point_color[i].rgb, albedo);
        }
    }
    if (dir_dir.w > 0.5) {
        vec3 L = normalize(-dir_dir.xyz);
        col += efx_light_term(N, V, L, 1.0, dir_color.rgb, albedo);
    }
    frag_color = vec4(clamp(col, 0.0, 1.0), efx_color.a * tint.a);
}
@end

@program mesh mesh_vs mesh_fs
