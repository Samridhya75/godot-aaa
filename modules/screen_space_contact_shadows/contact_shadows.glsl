#[compute]

#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D source_depth;
layout(rgba16f, set = 0, binding = 1) uniform restrict image2D image_color;

layout(set = 0, binding = 2, std140) uniform SceneData {
	mat4 projection;
	mat4 inv_projection;
} scene_data;

layout(push_constant, std430) uniform Params {
	vec3 light_dir_view;
	float max_distance;
	ivec2 screen_size;
	float thickness;
	float shadow_intensity;
	int ray_steps;
	float normal_bias;
	int pad0;
	int pad1;
} params;

vec3 compute_view_pos(vec2 uv, float depth) {
	vec4 pos;
	pos.xy = uv * 2.0 - 1.0;
	pos.z = depth;
	pos.w = 1.0;
	pos = scene_data.inv_projection * pos;
	return pos.xyz / pos.w;
}

vec3 compute_screen_pos(vec3 pos) {
	vec4 screen_pos = scene_data.projection * vec4(pos, 1.0);
	screen_pos.xyz /= screen_pos.w;
	screen_pos.xy = screen_pos.xy * 0.5 + 0.5;
	return screen_pos.xyz;
}

vec3 compute_geometric_normal(ivec2 pixel_pos, float depth_c, vec3 view_c) {
	ivec2 p_r = min(pixel_pos + ivec2(1, 0), params.screen_size - 1);
	ivec2 p_l = max(pixel_pos - ivec2(1, 0), ivec2(0));
	ivec2 p_u = min(pixel_pos + ivec2(0, 1), params.screen_size - 1);
	ivec2 p_d = max(pixel_pos - ivec2(0, 1), ivec2(0));

	float z_r = texelFetch(source_depth, p_r, 0).r;
	float z_l = texelFetch(source_depth, p_l, 0).r;
	float z_u = texelFetch(source_depth, p_u, 0).r;
	float z_d = texelFetch(source_depth, p_d, 0).r;

	vec2 uv_r = (vec2(p_r) + 0.5) / vec2(params.screen_size);
	vec2 uv_l = (vec2(p_l) + 0.5) / vec2(params.screen_size);
	vec2 uv_u = (vec2(p_u) + 0.5) / vec2(params.screen_size);
	vec2 uv_d = (vec2(p_d) + 0.5) / vec2(params.screen_size);

	vec3 v_r = compute_view_pos(uv_r, z_r);
	vec3 v_l = compute_view_pos(uv_l, z_l);
	vec3 v_u = compute_view_pos(uv_u, z_u);
	vec3 v_d = compute_view_pos(uv_d, z_d);

	vec3 dx = (abs(v_r.z - view_c.z) < abs(v_l.z - view_c.z)) ? (v_r - view_c) : (view_c - v_l);
	vec3 dy = (abs(v_u.z - view_c.z) < abs(v_d.z - view_c.z)) ? (v_u - view_c) : (view_c - v_d);

	vec3 n = normalize(cross(dx, dy));
	return (n.z > 0.0) ? -n : n;
}

void main() {
	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	if (pos.x >= params.screen_size.x || pos.y >= params.screen_size.y) {
		return;
	}

	float depth = texelFetch(source_depth, pos, 0).r;

	// In Godot reverse-Z, depth <= 0.00001 is sky/distant background
	if (depth <= 0.00001) {
		return;
	}

	vec2 uv = (vec2(pos) + 0.5) / vec2(params.screen_size);
	vec3 view_pos = compute_view_pos(uv, depth);

	// Reconstruct normal
	vec3 normal = compute_geometric_normal(pos, depth, view_pos);

	// Ray direction towards light in view space:
	// Light vector points in direction of light propagation, so ray towards source is -light_dir_view
	vec3 ray_dir = -params.light_dir_view;
	float n_dot_l = dot(normal, ray_dir);

	if (n_dot_l < -0.2) {
		return;
	}

	// Pseudo-random spatial dither to break raymarching banding
	float dither = fract(52.9829189 * fract(0.06711056 * float(pos.x) + 0.00583715 * float(pos.y)));

	vec3 ray_start = view_pos + normal * params.normal_bias;
	vec3 ray_end = ray_start + ray_dir * params.max_distance;

	float shadow = 0.0;
	int steps = max(4, params.ray_steps);
	float step_size = 1.0 / float(steps);

	for (int i = 0; i < steps; i++) {
		float t = (float(i) + dither) * step_size;
		vec3 sample_pos = mix(ray_start, ray_end, t);

		vec3 sample_screen = compute_screen_pos(sample_pos);
		vec2 sample_uv = sample_screen.xy;

		if (sample_uv.x < 0.0 || sample_uv.x > 1.0 || sample_uv.y < 0.0 || sample_uv.y > 1.0) {
			break;
		}

		float sampled_depth = textureLod(source_depth, sample_uv, 0.0).r;
		if (sampled_depth <= 0.00001) {
			continue;
		}

		vec3 sampled_view_pos = compute_view_pos(sample_uv, sampled_depth);

		float depth_diff = sampled_view_pos.z - sample_pos.z;

		if (depth_diff > 0.0 && depth_diff < params.thickness) {
			float contact_factor = 1.0 - t;
			float thickness_factor = 1.0 - (depth_diff / params.thickness);

			vec2 border = smoothstep(vec2(0.0), vec2(0.05), sample_uv) * smoothstep(vec2(1.0), vec2(0.95), sample_uv);
			float edge_fade = border.x * border.y;

			float hit_shadow = contact_factor * thickness_factor * edge_fade;
			shadow = max(shadow, hit_shadow);

			if (shadow > 0.9) {
				break;
			}
		}
	}

	if (shadow > 0.001) {
		vec4 color = imageLoad(image_color, pos);
		float final_shadow = clamp(shadow * params.shadow_intensity, 0.0, 1.0);
		color.rgb *= (1.0 - final_shadow);
		imageStore(image_color, pos, color);
	}
}
