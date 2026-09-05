#[versions]

cull = "";
reset = "#define RESET_PASS";
broadcast = "#define BROADCAST_PASS";

#[compute]
#version 450

#VERSION_DEFINES

#if defined(RESET_PASS)

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(std430, set = 0, binding = 2) buffer CounterBuffer {
	uint visible_count;
	uint counter_pad[3];
};

layout(std430, set = 0, binding = 3) buffer CommandBuffer {
	uint command_data[];
};

layout(push_constant, std430) uniform Params {
	vec4 frustum_planes[6];
	vec4 camera_position; // xyz = position, w = max_distance
	uint total_instances;
	uint culling_flags; // bit 0=frustum, 1=dist, 2=occlusion
	uint instance_offset;
	uint surface_count;
	
	mat4 vp_matrix;
	vec2 screen_size;
	float p11;
	float p22;
	float p32;
	float pad;
} params;

void main() {
	visible_count = 0u;
}

#elif defined(BROADCAST_PASS)

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(std430, set = 0, binding = 2) buffer CounterBuffer {
	uint visible_count;
	uint counter_pad[3];
};

layout(std430, set = 0, binding = 3) buffer CommandBuffer {
	uint command_data[];
};

layout(push_constant, std430) uniform Params {
	vec4 frustum_planes[6];
	vec4 camera_position;
	uint total_instances;
	uint culling_flags;
	uint instance_offset;
	uint surface_count;
	mat4 vp_matrix;
	vec2 screen_size;
	float p11;
	float p22;
	float p32;
	float pad;
} params;

void main() {
	// Broadcast visible_count to all surfaces for this MultiMesh
	// Godot's INDIRECT_MULTIMESH_COMMAND_STRIDE is 8 uints. instanceCount is at index 1.
	for (uint i = 0; i < params.surface_count; i++) {
		command_data[i * 8 + 1] = visible_count;
	}
}

#else

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct InstanceData {
	vec4 xform_row0; // basis.x.x, basis.y.x, basis.z.x, origin.x
	vec4 xform_row1; // basis.x.y, basis.y.y, basis.z.y, origin.y
	vec4 xform_row2; // basis.x.z, basis.y.z, basis.z.z, origin.z
	vec4 aabb_center_radius; // xyz = center, w = radius
	uint mesh_index; // For MDI
	uint mesh_first_instance; // Offset in global transform buffer
	uint pad1;
	uint pad2;
};

layout(push_constant, std430) uniform Params {
	vec4 frustum_planes[6];
	vec4 camera_position; // xyz = position, w = max_distance
	uint total_instances;
	uint culling_flags; // bit 0 = frustum, bit 1 = distance, bit 2 = occlusion
	uint instance_offset;
	uint surface_count;
	
	mat4 vp_matrix;
	vec2 screen_size;
	float p11;
	float p22;
	float p32;
	float pad;
} params;

layout(std430, set = 0, binding = 0) readonly buffer InputInstances {
	InstanceData instances[];
};

layout(std430, set = 0, binding = 1) writeonly buffer OutputTransforms {
	vec4 transforms[];
};

layout(std430, set = 0, binding = 2) buffer CounterBuffer {
	uint visible_count;
	uint counter_pad[3];
};

layout(std430, set = 0, binding = 3) buffer CommandBuffer {
	uint command_data[];
};

layout(set = 0, binding = 4) uniform sampler2D depth_pyramid;

void main() {
	uint idx = gl_GlobalInvocationID.x;
	if (idx >= params.total_instances) {
		return;
	}

	InstanceData inst = instances[idx + params.instance_offset];
	vec3 center = inst.aabb_center_radius.xyz;
	float radius = inst.aabb_center_radius.w;

	bool visible = true;

	// 1. Distance Culling
	if ((params.culling_flags & 2u) != 0u) {
		vec3 diff = center - params.camera_position.xyz;
		float dist_sq = dot(diff, diff);
		float max_dist = params.camera_position.w;
		if (dist_sq > (max_dist * max_dist)) {
			visible = false;
		}
	}

	// 2. Frustum Culling
	if (visible && (params.culling_flags & 1u) != 0u) {
		for (int i = 0; i < 6; i++) {
			float signed_dist = dot(params.frustum_planes[i].xyz, center) - params.frustum_planes[i].w;
			if (signed_dist > radius) {
				visible = false;
				break;
			}
		}
	}

	// 3. Occlusion Culling (HiZ)
	if (visible && (params.culling_flags & 4u) != 0u) {
		vec4 clip_center = params.vp_matrix * vec4(center, 1.0);
		float w = clip_center.w;
		
		if (w - radius > 0.1) {
			vec2 ndc_center = clip_center.xy / w;
			vec2 uv = ndc_center * 0.5 + 0.5;
					// Approximate screen space radius in NDC (Y-axis)
			float screen_radius_y = radius * abs(params.p11) / w;
			float screen_radius_x = screen_radius_y * (params.screen_size.y / max(params.screen_size.x, 1.0));
			
			vec2 uv_radius = vec2(screen_radius_x, screen_radius_y) * 0.5;
			vec2 min_uv = clamp(uv - uv_radius, 0.0, 1.0);
			vec2 max_uv = clamp(uv + uv_radius, 0.0, 1.0);
			
			// Size in pixels
			vec2 size_pixels = (max_uv - min_uv) * params.screen_size;
			float max_size = max(size_pixels.x, size_pixels.y);
			
			// Calculate mip level for 4-tap
			float mip = floor(log2(max(max_size, 1.0)));
			mip = clamp(mip, 0.0, 10.0);
			
			float d0 = textureLod(depth_pyramid, vec2(min_uv.x, min_uv.y), mip).r;
			float d1 = textureLod(depth_pyramid, vec2(max_uv.x, min_uv.y), mip).r;
			float d2 = textureLod(depth_pyramid, vec2(min_uv.x, max_uv.y), mip).r;
			float d3 = textureLod(depth_pyramid, vec2(max_uv.x, max_uv.y), mip).r;
			
			float depth = min(min(d0, d1), min(d2, d3));
			
			// Approximate closest z of object
			float w_closest = max(w - radius, 0.01);
			float closest_ndc_z = -params.p22 + params.p32 / w_closest;
			
			// If our closest z is further than the depth buffer's furthest z, it's occluded
			// (Reverse-Z: smaller Z means further away)
			if (closest_ndc_z < depth) {
				visible = false;
			}
		}
	}

	// 4. Output surviving instance
	if (visible) {
		uint local_idx = atomicAdd(visible_count, 1u);

		transforms[local_idx * 3u + 0u] = inst.xform_row0;
		transforms[local_idx * 3u + 1u] = inst.xform_row1;
		transforms[local_idx * 3u + 2u] = inst.xform_row2;
	}
}

#endif
