#[versions]

cull = "";
reset = "#define RESET_PASS";

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
	uint culling_flags;
	uint update_command_buffer;
	uint pad;
} params;

void main() {
	visible_count = 0u;
	if (params.update_command_buffer != 0u) {
		// In Godot indirect multimesh, command_data[1] holds the instance count for surface 0
		command_data[1] = 0u;
	}
}

#else

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct InstanceData {
	vec4 xform_row0; // basis.x.x, basis.y.x, basis.z.x, origin.x
	vec4 xform_row1; // basis.x.y, basis.y.y, basis.z.y, origin.y
	vec4 xform_row2; // basis.x.z, basis.y.z, basis.z.z, origin.z
	vec4 aabb_center_radius; // xyz = center, w = radius
};

layout(push_constant, std430) uniform Params {
	vec4 frustum_planes[6];
	vec4 camera_position; // xyz = position, w = max_distance
	uint total_instances;
	uint culling_flags; // bit 0 = frustum, bit 1 = distance
	uint update_command_buffer; // bit 0 = write to indirect command buffer
	uint pad;
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

void main() {
	uint idx = gl_GlobalInvocationID.x;
	if (idx >= params.total_instances) {
		return;
	}

	InstanceData inst = instances[idx];
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
			// Godot frustum plane convention:
			// plane.is_point_over(p) is (normal.dot(p) > d).
			// If center is over plane by more than radius, it's outside.
			float signed_dist = dot(params.frustum_planes[i].xyz, center) - params.frustum_planes[i].w;
			if (signed_dist > radius) {
				visible = false;
				break;
			}
		}
	}

	// 3. Output surviving instance
	if (visible) {
		uint out_idx = atomicAdd(visible_count, 1u);

		// Output 3x4 transform matrix rows (12 floats) matching Godot's MultiMesh 3D layout
		transforms[out_idx * 3u + 0u] = inst.xform_row0;
		transforms[out_idx * 3u + 1u] = inst.xform_row1;
		transforms[out_idx * 3u + 2u] = inst.xform_row2;

		if (params.update_command_buffer != 0u) {
			// Surface 0 instance count is at index 1
			atomicMax(command_data[1], out_idx + 1u);
		}
	}
}

#endif
