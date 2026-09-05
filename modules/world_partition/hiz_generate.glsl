#[versions]
hiz = "";

#[compute]
#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D source_depth;
layout(set = 0, binding = 1, r32f) uniform restrict writeonly image2D dest_depth;

layout(push_constant, std430) uniform Params {
	vec2 src_inv_size;
} params;

void main() {
	ivec2 dst_pos = ivec2(gl_GlobalInvocationID.xy);
	ivec2 dst_size = imageSize(dest_depth);

	if (dst_pos.x >= dst_size.x || dst_pos.y >= dst_size.y) {
		return;
	}

	vec2 src_uv = (vec2(dst_pos) + 0.5) * 2.0 * params.src_inv_size;

	// Gather 4 depth values
	float d0 = texture(source_depth, src_uv + vec2(-0.5, -0.5) * params.src_inv_size).r;
	float d1 = texture(source_depth, src_uv + vec2( 0.5, -0.5) * params.src_inv_size).r;
	float d2 = texture(source_depth, src_uv + vec2(-0.5,  0.5) * params.src_inv_size).r;
	float d3 = texture(source_depth, src_uv + vec2( 0.5,  0.5) * params.src_inv_size).r;

	// In Godot 4 Vulkan (Reverse-Z), 0.0 is far and 1.0 is near.
	// To be conservative for occlusion culling, we need the furthest depth among the 4 texels.
	// Furthest depth in Reverse-Z is the minimum value.
	float min_depth = min(min(d0, d1), min(d2, d3));

	imageStore(dest_depth, dst_pos, vec4(min_depth, 0.0, 0.0, 0.0));
}
