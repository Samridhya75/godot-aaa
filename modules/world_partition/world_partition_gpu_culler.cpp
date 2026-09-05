#include "world_partition_gpu_culler.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "gpu_cull.glsl.gen.h"
#include "scene/3d/camera_3d.h"
#include "scene/main/viewport.h"
#include "servers/rendering/rendering_device.h"
#include "scene/resources/3d/world_3d.h"
#include "servers/rendering/rendering_device_binds.h"

// Note: Generate the shader header by compiling hiz_generate.glsl with glslangValidator or similar before building.
// For now, we assume it's included or we'll just not compile it if missing.
// #include "hiz_generate.glsl.gen.h"

void WorldPartitionGPUCuller3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_multimeshes", "multimeshes"), &WorldPartitionGPUCuller3D::set_multimeshes);
	ClassDB::bind_method(D_METHOD("get_multimeshes"), &WorldPartitionGPUCuller3D::get_multimeshes);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "multimeshes", PROPERTY_HINT_ARRAY_TYPE, "MultiMesh"), "set_multimeshes", "get_multimeshes");

	ClassDB::bind_method(D_METHOD("set_culling_enabled", "enabled"), &WorldPartitionGPUCuller3D::set_culling_enabled);
	ClassDB::bind_method(D_METHOD("is_culling_enabled"), &WorldPartitionGPUCuller3D::is_culling_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "culling_enabled"), "set_culling_enabled", "is_culling_enabled");

	ClassDB::bind_method(D_METHOD("set_distance_culling_enabled", "enabled"), &WorldPartitionGPUCuller3D::set_distance_culling_enabled);
	ClassDB::bind_method(D_METHOD("is_distance_culling_enabled"), &WorldPartitionGPUCuller3D::is_distance_culling_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "distance_culling_enabled"), "set_distance_culling_enabled", "is_distance_culling_enabled");

	ClassDB::bind_method(D_METHOD("set_occlusion_culling_enabled", "enabled"), &WorldPartitionGPUCuller3D::set_occlusion_culling_enabled);
	ClassDB::bind_method(D_METHOD("is_occlusion_culling_enabled"), &WorldPartitionGPUCuller3D::is_occlusion_culling_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "occlusion_culling_enabled"), "set_occlusion_culling_enabled", "is_occlusion_culling_enabled");

	ClassDB::bind_method(D_METHOD("set_max_cull_distance", "distance"), &WorldPartitionGPUCuller3D::set_max_cull_distance);
	ClassDB::bind_method(D_METHOD("get_max_cull_distance"), &WorldPartitionGPUCuller3D::get_max_cull_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_cull_distance", PROPERTY_HINT_RANGE, "1.0,5000.0,1.0,suffix:m"), "set_max_cull_distance", "get_max_cull_distance");

	ClassDB::bind_method(D_METHOD("set_distance_fade_margin", "margin"), &WorldPartitionGPUCuller3D::set_distance_fade_margin);
	ClassDB::bind_method(D_METHOD("get_distance_fade_margin"), &WorldPartitionGPUCuller3D::get_distance_fade_margin);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "distance_fade_margin", PROPERTY_HINT_RANGE, "0.0,200.0,1.0,suffix:m"), "set_distance_fade_margin", "get_distance_fade_margin");

	ClassDB::bind_method(D_METHOD("add_instance", "mesh_index", "transform", "local_aabb"), &WorldPartitionGPUCuller3D::add_instance);
	ClassDB::bind_method(D_METHOD("set_instances", "mesh_index", "transforms", "local_aabb"), &WorldPartitionGPUCuller3D::set_instances);
	ClassDB::bind_method(D_METHOD("add_instance_batch", "mesh_index", "transforms", "local_aabb"), &WorldPartitionGPUCuller3D::add_instance_batch);
	ClassDB::bind_method(D_METHOD("remove_instance_batch", "batch_id"), &WorldPartitionGPUCuller3D::remove_instance_batch);
	ClassDB::bind_method(D_METHOD("clear_instances"), &WorldPartitionGPUCuller3D::clear_instances);
	ClassDB::bind_method(D_METHOD("get_instance_count"), &WorldPartitionGPUCuller3D::get_instance_count);

	ClassDB::bind_method(D_METHOD("dispatch_culling", "camera"), &WorldPartitionGPUCuller3D::dispatch_culling, DEFVAL(Variant()));
}

void WorldPartitionGPUCuller3D::set_distance_fade_margin(float p_margin) {
	distance_fade_margin = MAX(0.0f, p_margin);
}

float WorldPartitionGPUCuller3D::get_distance_fade_margin() const {
	return distance_fade_margin;
}

void WorldPartitionGPUCuller3D::set_multimeshes(const TypedArray<MultiMesh> &p_multimeshes) {
	multimeshes = p_multimeshes;
	
	// Force allocate for indirect rendering if not already set, since Godot Editor doesn't expose this flag
	for (int i = 0; i < multimeshes.size(); i++) {
		Ref<MultiMesh> mm = multimeshes[i];
		if (mm.is_valid()) {
			int count = mm->get_instance_count();
			if (count > 0) {
				RenderingServer::get_singleton()->multimesh_allocate_data(mm->get_rid(), count, RSE::MULTIMESH_TRANSFORM_3D, mm->is_using_colors(), mm->is_using_custom_data(), true);
			}
		}
	}
	
	instances_dirty = true;
	update_configuration_warnings();
}

TypedArray<MultiMesh> WorldPartitionGPUCuller3D::get_multimeshes() const {
	return multimeshes;
}

void WorldPartitionGPUCuller3D::set_culling_enabled(bool p_enabled) {
	culling_enabled = p_enabled;
}

bool WorldPartitionGPUCuller3D::is_culling_enabled() const {
	return culling_enabled;
}

void WorldPartitionGPUCuller3D::set_distance_culling_enabled(bool p_enabled) {
	distance_culling_enabled = p_enabled;
}

bool WorldPartitionGPUCuller3D::is_distance_culling_enabled() const {
	return distance_culling_enabled;
}

void WorldPartitionGPUCuller3D::set_occlusion_culling_enabled(bool p_enabled) {
	occlusion_culling_enabled = p_enabled;
}

bool WorldPartitionGPUCuller3D::is_occlusion_culling_enabled() const {
	return occlusion_culling_enabled;
}

void WorldPartitionGPUCuller3D::set_max_cull_distance(float p_dist) {
	max_cull_distance = p_dist;
}

float WorldPartitionGPUCuller3D::get_max_cull_distance() const {
	return max_cull_distance;
}

void WorldPartitionGPUCuller3D::_rebuild_cpu_instances() {
	cpu_instances.clear();
	
	// Calculate prefix sums for MDI
	mesh_instance_counts.clear();
	mesh_instance_counts.resize(multimeshes.size());
	for (int i = 0; i < mesh_instance_counts.size(); i++) {
		mesh_instance_counts.write[i] = 0;
	}
	
	for (const KeyValue<int, Vector<InstanceData>> &E : instance_batches) {
		if (E.value.size() > 0) {
			uint32_t m_idx = E.value[0].mesh_index;
			if (m_idx < (uint32_t)mesh_instance_counts.size()) {
				mesh_instance_counts.write[m_idx] += E.value.size();
			}
		}
	}
	
	mesh_offsets.clear();
	mesh_offsets.resize(multimeshes.size());
	uint32_t current_offset = 0;
	for (int i = 0; i < mesh_instance_counts.size(); i++) {
		mesh_offsets.write[i] = current_offset;
		current_offset += mesh_instance_counts[i];
	}
	
	// Rebuild and assign offsets
	for (KeyValue<int, Vector<InstanceData>> &E : instance_batches) {
		if (E.value.size() > 0) {
			uint32_t m_idx = E.value[0].mesh_index;
			for (int j = 0; j < E.value.size(); j++) {
				E.value.write[j].mesh_first_instance = mesh_offsets[m_idx];
				cpu_instances.push_back(E.value[j]);
			}
		}
	}
	
	instances_dirty = true;
}

void WorldPartitionGPUCuller3D::add_instance(uint32_t p_mesh_index, const Transform3D &p_transform, const AABB &p_local_aabb) {
	InstanceData inst_data;
	inst_data.xform_row0[0] = p_transform.basis.rows[0][0];
	inst_data.xform_row0[1] = p_transform.basis.rows[0][1];
	inst_data.xform_row0[2] = p_transform.basis.rows[0][2];
	inst_data.xform_row0[3] = p_transform.origin.x;

	inst_data.xform_row1[0] = p_transform.basis.rows[1][0];
	inst_data.xform_row1[1] = p_transform.basis.rows[1][1];
	inst_data.xform_row1[2] = p_transform.basis.rows[1][2];
	inst_data.xform_row1[3] = p_transform.origin.y;

	inst_data.xform_row2[0] = p_transform.basis.rows[2][0];
	inst_data.xform_row2[1] = p_transform.basis.rows[2][1];
	inst_data.xform_row2[2] = p_transform.basis.rows[2][2];
	inst_data.xform_row2[3] = p_transform.origin.z;

	AABB world_aabb = p_transform.xform(p_local_aabb);
	Vector3 center = world_aabb.get_center();
	float radius = world_aabb.get_longest_axis_size() * 0.7071f;

	inst_data.aabb_center_radius[0] = center.x;
	inst_data.aabb_center_radius[1] = center.y;
	inst_data.aabb_center_radius[2] = center.z;
	inst_data.aabb_center_radius[3] = radius;
	
	inst_data.mesh_index = p_mesh_index;
	inst_data.mesh_first_instance = 0; // Updated in rebuild

	instance_batches[0].push_back(inst_data);
	_rebuild_cpu_instances(); // Inefficient for single adds, but correct
}

int WorldPartitionGPUCuller3D::add_instance_batch(uint32_t p_mesh_index, const TypedArray<Transform3D> &p_transforms, const AABB &p_local_aabb) {
	if (p_transforms.is_empty()) {
		return -1;
	}

	int batch_id = next_batch_id++;
	Vector<InstanceData> &batch = instance_batches[batch_id];
	batch.resize(p_transforms.size());

	for (int i = 0; i < p_transforms.size(); i++) {
		Transform3D p_transform = p_transforms[i];
		InstanceData &inst_data = batch.write[i];

		inst_data.xform_row0[0] = p_transform.basis.rows[0][0];
		inst_data.xform_row0[1] = p_transform.basis.rows[0][1];
		inst_data.xform_row0[2] = p_transform.basis.rows[0][2];
		inst_data.xform_row0[3] = p_transform.origin.x;

		inst_data.xform_row1[0] = p_transform.basis.rows[1][0];
		inst_data.xform_row1[1] = p_transform.basis.rows[1][1];
		inst_data.xform_row1[2] = p_transform.basis.rows[1][2];
		inst_data.xform_row1[3] = p_transform.origin.y;

		inst_data.xform_row2[0] = p_transform.basis.rows[2][0];
		inst_data.xform_row2[1] = p_transform.basis.rows[2][1];
		inst_data.xform_row2[2] = p_transform.basis.rows[2][2];
		inst_data.xform_row2[3] = p_transform.origin.z;

		AABB world_aabb = p_transform.xform(p_local_aabb);
		Vector3 center = world_aabb.get_center();
		float radius = world_aabb.get_longest_axis_size() * 0.7071f;

		inst_data.aabb_center_radius[0] = center.x;
		inst_data.aabb_center_radius[1] = center.y;
		inst_data.aabb_center_radius[2] = center.z;
		inst_data.aabb_center_radius[3] = radius;
		
		inst_data.mesh_index = p_mesh_index;
		inst_data.mesh_first_instance = 0;
	}

	_rebuild_cpu_instances();
	return batch_id;
}

void WorldPartitionGPUCuller3D::remove_instance_batch(int p_batch_id) {
	if (instance_batches.has(p_batch_id)) {
		instance_batches.erase(p_batch_id);
		_rebuild_cpu_instances();
	}
}

void WorldPartitionGPUCuller3D::set_instances(uint32_t p_mesh_index, const TypedArray<Transform3D> &p_transforms, const AABB &p_local_aabb) {
	clear_instances();
	add_instance_batch(p_mesh_index, p_transforms, p_local_aabb);
}

void WorldPartitionGPUCuller3D::clear_instances() {
	instance_batches.clear();
	cpu_instances.clear();
	instances_dirty = true;
}

int WorldPartitionGPUCuller3D::get_instance_count() const {
	return cpu_instances.size();
}

bool WorldPartitionGPUCuller3D::_init_gpu_resources() {
	if (rd != nullptr && cull_pipeline.is_valid()) {
		return true;
	}

	rd = RenderingServer::get_singleton()->get_rendering_device();
	if (!rd) {
		return false;
	}

	Ref<RDShaderFile> shader;
	shader.instantiate();
	Error err = shader->parse_versions_from_text(gpu_cull_shader_glsl);
	if (err != OK) {
		ERR_PRINT("WorldPartitionGPUCuller3D: Failed to parse gpu_cull.glsl");
		return false;
	}

	cull_shader = rd->shader_create_from_spirv(shader->get_spirv_stages("cull"));
	if (cull_shader.is_valid()) {
		cull_pipeline = rd->compute_pipeline_create(cull_shader);
	}
	
	reset_shader = rd->shader_create_from_spirv(shader->get_spirv_stages("reset"));
	if (reset_shader.is_valid()) {
		reset_pipeline = rd->compute_pipeline_create(reset_shader);
	}

	broadcast_shader = rd->shader_create_from_spirv(shader->get_spirv_stages("broadcast"));
	if (broadcast_shader.is_valid()) {
		broadcast_pipeline = rd->compute_pipeline_create(broadcast_shader);
	}

	ERR_FAIL_COND_V(cull_pipeline.is_null(), false);

	// 3. Counter buffer (16 bytes)
	Vector<uint8_t> counter_init;
	counter_init.resize_initialized(16);
	counter_buffer = rd->storage_buffer_create(16, counter_init);

	// 4. Dummy command buffer (large enough for MDI fallback)
	Vector<uint8_t> dummy_cmd;
	dummy_cmd.resize_initialized(32 * 1024); // support up to ~1600 meshes fallback
	dummy_command_buffer = rd->storage_buffer_create(dummy_cmd.size(), dummy_cmd);
	
	return cull_pipeline.is_valid();
}

void WorldPartitionGPUCuller3D::_free_gpu_resources() {
	if (!rd) {
		return;
	}

	for (int i = 0; i < uniform_sets.size(); i++) {
		if (uniform_sets[i].is_valid()) {
			rd->free_rid(uniform_sets[i]);
		}
	}
	uniform_sets.clear();
	if (input_buffer.is_valid()) {
		rd->free_rid(input_buffer);
		input_buffer = RID();
	}
	if (output_buffer.is_valid()) {
		rd->free_rid(output_buffer);
		output_buffer = RID();
	}
	if (counter_buffer.is_valid()) {
		rd->free_rid(counter_buffer);
		counter_buffer = RID();
	}
	if (dummy_command_buffer.is_valid()) {
		rd->free_rid(dummy_command_buffer);
		dummy_command_buffer = RID();
	}
	if (reset_pipeline.is_valid()) {
		rd->free_rid(reset_pipeline);
		reset_pipeline = RID();
	}
	if (reset_shader.is_valid()) {
		rd->free_rid(reset_shader);
		reset_shader = RID();
	}
	if (broadcast_pipeline.is_valid()) {
		rd->free_rid(broadcast_pipeline);
		broadcast_pipeline = RID();
	}
	if (broadcast_shader.is_valid()) {
		rd->free_rid(broadcast_shader);
		broadcast_shader = RID();
	}
	if (cull_shader.is_valid()) {
		rd->free_rid(cull_shader);
		cull_shader = RID();
	}
	if (cull_pipeline.is_valid()) {
		rd->free_rid(cull_pipeline);
		cull_pipeline = RID();
	}

	allocated_capacity = 0;
}

void WorldPartitionGPUCuller3D::_update_instance_buffers() {
	if (!_init_gpu_resources()) {
		return;
	}

	if (cpu_instances.is_empty()) {
		return;
	}

	uint32_t needed_count = cpu_instances.size();
	if (needed_count > allocated_capacity) {
		for (int i = 0; i < uniform_sets.size(); i++) {
			if (uniform_sets[i].is_valid()) {
				rd->free_rid(uniform_sets[i]);
			}
		}
		uniform_sets.clear();
		if (input_buffer.is_valid()) {
			rd->free_rid(input_buffer);
			input_buffer = RID();
		}
		if (output_buffer.is_valid()) {
			rd->free_rid(output_buffer);
			output_buffer = RID();
		}

		allocated_capacity = needed_count;

		// Input buffer
		uint32_t input_size = allocated_capacity * sizeof(InstanceData);
		Vector<uint8_t> in_data;
		in_data.resize(input_size);
		memcpy(in_data.ptrw(), cpu_instances.ptr(), input_size);
		input_buffer = rd->storage_buffer_create(input_size, in_data);

		// Output buffer (global)
		uint32_t output_size = allocated_capacity * sizeof(float) * 12;
		Vector<uint8_t> out_data;
		out_data.resize_initialized(output_size);
		output_buffer = rd->storage_buffer_create(output_size, out_data);
	} else {
		// Update existing input buffer
		uint32_t update_size = needed_count * sizeof(InstanceData);
		Vector<uint8_t> in_data;
		in_data.resize(update_size);
		memcpy(in_data.ptrw(), cpu_instances.ptr(), update_size);
		rd->buffer_update(input_buffer, 0, update_size, in_data.ptr());
	}

	for (int i = 0; i < uniform_sets.size(); i++) {
		if (uniform_sets[i].is_valid()) {
			rd->free_rid(uniform_sets[i]);
		}
	}
	uniform_sets.clear();
	uniform_sets.resize(multimeshes.size());

	RID default_sampler;
	{
		RD::SamplerState sampler_state;
		sampler_state.mag_filter = RD::SAMPLER_FILTER_NEAREST;
		sampler_state.min_filter = RD::SAMPLER_FILTER_NEAREST;
		sampler_state.mip_filter = RD::SAMPLER_FILTER_NEAREST;
		sampler_state.repeat_u = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		sampler_state.repeat_v = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		default_sampler = rd->sampler_create(sampler_state);
	}

	RID current_hiz_texture = hiz_effect.is_valid() ? hiz_effect->get_hiz_texture() : RID();
	if (current_hiz_texture.is_null()) {
		RD::TextureFormat tf;
		tf.format = RD::DATA_FORMAT_R32_SFLOAT;
		tf.width = 1;
		tf.height = 1;
		tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT;
		tf.texture_type = RD::TEXTURE_TYPE_2D;
		current_hiz_texture = rd->texture_create(tf, RD::TextureView());
	}

	for (int i = 0; i < multimeshes.size(); i++) {
		Ref<MultiMesh> mm = multimeshes[i];
		RID target_out_buffer = output_buffer;
		RID target_cmd_buffer = dummy_command_buffer;

		if (mm.is_valid()) {
			RID mm_rid = mm->get_rid();
			RID mm_buf = RenderingServer::get_singleton()->multimesh_get_buffer_rd_rid(mm_rid);
			if (mm_buf.is_valid()) target_out_buffer = mm_buf;
			
			RID mm_cmd = RenderingServer::get_singleton()->multimesh_get_command_buffer_rd_rid(mm_rid);
			if (mm_cmd.is_valid()) target_cmd_buffer = mm_cmd;
		}

		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 0;
			u.append_id(input_buffer);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 1;
			u.append_id(target_out_buffer);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 2;
			u.append_id(counter_buffer);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = 3;
			u.append_id(target_cmd_buffer);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
			u.binding = 4;
			u.append_id(default_sampler);
			u.append_id(current_hiz_texture);
			uniforms.push_back(u);
		}

		uniform_sets.write[i] = rd->uniform_set_create(uniforms, cull_shader, 0);
	}
	instances_dirty = false;
}



void WorldPartitionGPUCuller3D::dispatch_culling(Camera3D *p_camera) {
	if (cpu_instances.is_empty()) {
		return;
	}

	if (instances_dirty) {
		_update_instance_buffers();
	}

	if (!rd || !cull_pipeline.is_valid() || uniform_sets.is_empty()) {
		return;
	}

	Camera3D *cam = p_camera;
	if (!cam && get_viewport()) {
		cam = get_viewport()->get_camera_3d();
	}
	if (!cam) {
		return;
	}
	
	CullParams params;
	params.total_instances = cpu_instances.size();
	params.culling_flags = 0;
	params.instance_offset = 0;
	params.screen_size[0] = 0.0f;
	params.screen_size[1] = 0.0f;

	if (hiz_effect.is_valid()) {
		Vector2i size = hiz_effect->get_hiz_size();
		if (size.x > 0 && size.y > 0) {
			params.screen_size[0] = size.x;
			params.screen_size[1] = size.y;
		}
	}

	if (culling_enabled) params.culling_flags |= 1u;
	if (distance_culling_enabled) params.culling_flags |= 2u;
	if (occlusion_culling_enabled) params.culling_flags |= 4u;

	Vector<Plane> frustum = cam->get_frustum();
	if (frustum.size() >= 6) {
		for (int i = 0; i < 6; i++) {
			params.frustum_planes[i * 4 + 0] = frustum[i].normal.x;
			params.frustum_planes[i * 4 + 1] = frustum[i].normal.y;
			params.frustum_planes[i * 4 + 2] = frustum[i].normal.z;
			params.frustum_planes[i * 4 + 3] = frustum[i].d;
		}
	}

	Vector3 cam_pos = cam->get_global_position();
	params.camera_position[0] = cam_pos.x;
	params.camera_position[1] = cam_pos.y;
	params.camera_position[2] = cam_pos.z;
	params.camera_position[3] = max_cull_distance;
	
	Projection proj = cam->get_camera_projection();
	Transform3D view = cam->get_global_transform().affine_inverse();
	Projection vp = proj * Projection(view);
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			params.vp_matrix[i * 4 + j] = vp.columns[i][j];
		}
	}
	
	// Screen size might have been overridden by HiZ
	if (params.screen_size[0] == 0.0f) {
		Size2 vp_size = get_viewport()->get_visible_rect().size;
		params.screen_size[0] = vp_size.x;
		params.screen_size[1] = vp_size.y;
	}
	params.p11 = proj.columns[1][1];
	params.p22 = proj.columns[2][2];
	params.p32 = proj.columns[3][2];
	params.pad = 0.0f;

	params.update_command_buffer = 1u;

	// 1. Reset counter & command buffer
	if (reset_pipeline.is_valid()) {
		RD::ComputeListID compute_list = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(compute_list, reset_pipeline);
		for (int i = 0; i < multimeshes.size(); i++) {
			if (i >= mesh_instance_counts.size() || mesh_instance_counts[i] == 0) continue;
			if (uniform_sets[i].is_valid()) {
				rd->compute_list_bind_uniform_set(compute_list, uniform_sets[i], 0);
				params.total_instances = mesh_instance_counts[i];
				params.instance_offset = mesh_offsets[i];
				
				Ref<MultiMesh> mm = multimeshes[i];
				if (mm.is_valid() && mm->get_mesh().is_valid()) {
					params.surface_count = mm->get_mesh()->get_surface_count();
				} else {
					params.surface_count = 1;
				}
				
				rd->compute_list_set_push_constant(compute_list, &params, sizeof(CullParams));
				rd->compute_list_dispatch(compute_list, 1, 1, 1);
			}
		}
		rd->compute_list_end();
	}

	rd->barrier(RD::BARRIER_MASK_COMPUTE, RD::BARRIER_MASK_COMPUTE);

	// 2. Dispatch main culling pass
	{
		RD::ComputeListID compute_list = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(compute_list, cull_pipeline);
		for (int i = 0; i < multimeshes.size(); i++) {
			if (i >= mesh_instance_counts.size() || mesh_instance_counts[i] == 0) continue;
			if (uniform_sets[i].is_valid()) {
				rd->compute_list_bind_uniform_set(compute_list, uniform_sets[i], 0);
				
				params.total_instances = mesh_instance_counts[i];
				params.instance_offset = mesh_offsets[i];
				
				rd->compute_list_set_push_constant(compute_list, &params, sizeof(CullParams));
				uint32_t x_groups = (params.total_instances + 63) / 64;
				rd->compute_list_dispatch(compute_list, x_groups, 1, 1);
			}
		}
		rd->compute_list_end();
	}

	rd->barrier(RD::BARRIER_MASK_COMPUTE, RD::BARRIER_MASK_COMPUTE);

	// 3. Broadcast instance count to all surfaces
	if (broadcast_pipeline.is_valid()) {
		RD::ComputeListID compute_list = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(compute_list, broadcast_pipeline);
		for (int i = 0; i < multimeshes.size(); i++) {
			if (i >= mesh_instance_counts.size() || mesh_instance_counts[i] == 0) continue;
			if (uniform_sets[i].is_valid()) {
				rd->compute_list_bind_uniform_set(compute_list, uniform_sets[i], 0);
				
				params.total_instances = mesh_instance_counts[i];
				params.instance_offset = mesh_offsets[i];
				
				Ref<MultiMesh> mm = multimeshes[i];
				if (mm.is_valid() && mm->get_mesh().is_valid()) {
					params.surface_count = mm->get_mesh()->get_surface_count();
				} else {
					params.surface_count = 1;
				}
				
				rd->compute_list_set_push_constant(compute_list, &params, sizeof(CullParams));
				rd->compute_list_dispatch(compute_list, 1, 1, 1);
			}
		}
		rd->compute_list_end();
	}

	rd->barrier(RD::BARRIER_MASK_COMPUTE, RD::BARRIER_MASK_VERTEX | RD::BARRIER_MASK_TRANSFER);
}

void WorldPartitionGPUCuller3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				set_process_internal(true);
				
				if (get_viewport() && get_viewport()->find_world_3d().is_valid()) {
					Ref<World3D> world = get_viewport()->find_world_3d();
					Ref<Compositor> compositor = world->get_compositor();
					if (compositor.is_null()) {
						compositor.instantiate();
						world->set_compositor(compositor);
					}
					
					if (hiz_effect.is_null()) {
						hiz_effect.instantiate();
					}
					
					TypedArray<CompositorEffect> effects = compositor->get_compositor_effects();
					if (!effects.has(hiz_effect)) {
						effects.push_back(hiz_effect);
						compositor->set_compositor_effects(effects);
					}
				}
			}
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			dispatch_culling(nullptr);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (get_viewport() && get_viewport()->find_world_3d().is_valid() && hiz_effect.is_valid()) {
				Ref<Compositor> compositor = get_viewport()->find_world_3d()->get_compositor();
				if (compositor.is_valid()) {
					TypedArray<CompositorEffect> effects = compositor->get_compositor_effects();
					int idx = effects.find(hiz_effect);
					if (idx != -1) {
						effects.remove_at(idx);
						compositor->set_compositor_effects(effects);
					}
				}
			}
			_free_gpu_resources();
		} break;
	}
}

WorldPartitionGPUCuller3D::WorldPartitionGPUCuller3D() {
}

WorldPartitionGPUCuller3D::~WorldPartitionGPUCuller3D() {
	_free_gpu_resources();
}

PackedStringArray WorldPartitionGPUCuller3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();
	if (multimeshes.is_empty()) {
		warnings.push_back(RTR("At least one MultiMesh resource must be assigned to WorldPartitionGPUCuller3D for GPU instance culling."));
	}
	return warnings;
}
