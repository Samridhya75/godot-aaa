#include "world_partition_gpu_culler.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "gpu_cull.glsl.gen.h"
#include "scene/3d/camera_3d.h"
#include "scene/main/viewport.h"
#include "servers/rendering/rendering_device_binds.h"

void WorldPartitionGPUCuller3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_multimesh", "multimesh"), &WorldPartitionGPUCuller3D::set_multimesh);
	ClassDB::bind_method(D_METHOD("get_multimesh"), &WorldPartitionGPUCuller3D::get_multimesh);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "multimesh", PROPERTY_HINT_RESOURCE_TYPE, "MultiMesh"), "set_multimesh", "get_multimesh");

	ClassDB::bind_method(D_METHOD("set_culling_enabled", "enabled"), &WorldPartitionGPUCuller3D::set_culling_enabled);
	ClassDB::bind_method(D_METHOD("is_culling_enabled"), &WorldPartitionGPUCuller3D::is_culling_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "culling_enabled"), "set_culling_enabled", "is_culling_enabled");

	ClassDB::bind_method(D_METHOD("set_distance_culling_enabled", "enabled"), &WorldPartitionGPUCuller3D::set_distance_culling_enabled);
	ClassDB::bind_method(D_METHOD("is_distance_culling_enabled"), &WorldPartitionGPUCuller3D::is_distance_culling_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "distance_culling_enabled"), "set_distance_culling_enabled", "is_distance_culling_enabled");

	ClassDB::bind_method(D_METHOD("set_max_cull_distance", "distance"), &WorldPartitionGPUCuller3D::set_max_cull_distance);
	ClassDB::bind_method(D_METHOD("get_max_cull_distance"), &WorldPartitionGPUCuller3D::get_max_cull_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_cull_distance", PROPERTY_HINT_RANGE, "1.0,5000.0,1.0,suffix:m"), "set_max_cull_distance", "get_max_cull_distance");

	ClassDB::bind_method(D_METHOD("set_distance_fade_margin", "margin"), &WorldPartitionGPUCuller3D::set_distance_fade_margin);
	ClassDB::bind_method(D_METHOD("get_distance_fade_margin"), &WorldPartitionGPUCuller3D::get_distance_fade_margin);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "distance_fade_margin", PROPERTY_HINT_RANGE, "0.0,200.0,1.0,suffix:m"), "set_distance_fade_margin", "get_distance_fade_margin");

	ClassDB::bind_method(D_METHOD("add_instance", "transform", "local_aabb"), &WorldPartitionGPUCuller3D::add_instance);
	ClassDB::bind_method(D_METHOD("set_instances", "transforms", "local_aabb"), &WorldPartitionGPUCuller3D::set_instances);
	ClassDB::bind_method(D_METHOD("add_instance_batch", "transforms", "local_aabb"), &WorldPartitionGPUCuller3D::add_instance_batch);
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

void WorldPartitionGPUCuller3D::set_multimesh(const Ref<MultiMesh> &p_multimesh) {
	multimesh = p_multimesh;
	instances_dirty = true;
}

Ref<MultiMesh> WorldPartitionGPUCuller3D::get_multimesh() const {
	return multimesh;
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

void WorldPartitionGPUCuller3D::set_max_cull_distance(float p_dist) {
	max_cull_distance = p_dist;
}

float WorldPartitionGPUCuller3D::get_max_cull_distance() const {
	return max_cull_distance;
}

void WorldPartitionGPUCuller3D::_rebuild_cpu_instances() {
	cpu_instances.clear();
	for (const KeyValue<int, Vector<InstanceData>> &E : instance_batches) {
		cpu_instances.append_array(E.value);
	}
	instances_dirty = true;
}

void WorldPartitionGPUCuller3D::add_instance(const Transform3D &p_transform, const AABB &p_local_aabb) {
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

	instance_batches[0].push_back(inst_data);
	cpu_instances.push_back(inst_data);
	instances_dirty = true;
}

int WorldPartitionGPUCuller3D::add_instance_batch(const TypedArray<Transform3D> &p_transforms, const AABB &p_local_aabb) {
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

void WorldPartitionGPUCuller3D::set_instances(const TypedArray<Transform3D> &p_transforms, const AABB &p_local_aabb) {
	clear_instances();
	add_instance_batch(p_transforms, p_local_aabb);
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

	// 1. Reset pipeline
	Vector<RD::ShaderStageSPIRVData> reset_stages = shader->get_spirv_stages("reset");
	if (!reset_stages.is_empty()) {
		reset_shader = rd->shader_create_from_spirv(reset_stages);
		if (reset_shader.is_valid()) {
			reset_pipeline = rd->compute_pipeline_create(reset_shader);
		}
	}

	// 2. Cull pipeline
	Vector<RD::ShaderStageSPIRVData> cull_stages = shader->get_spirv_stages("cull");
	if (!cull_stages.is_empty()) {
		cull_shader = rd->shader_create_from_spirv(cull_stages);
		if (cull_shader.is_valid()) {
			cull_pipeline = rd->compute_pipeline_create(cull_shader);
		}
	}

	// 3. Counter buffer (16 bytes)
	Vector<uint8_t> counter_init;
	counter_init.resize_initialized(16);
	counter_buffer = rd->storage_buffer_create(16, counter_init);

	// 4. Dummy command buffer (32 bytes fallback)
	Vector<uint8_t> dummy_cmd;
	dummy_cmd.resize_initialized(32);
	dummy_command_buffer = rd->storage_buffer_create(32, dummy_cmd);

	return cull_pipeline.is_valid();
}

void WorldPartitionGPUCuller3D::_free_gpu_resources() {
	if (!rd) {
		return;
	}

	if (uniform_set.is_valid()) {
		rd->free_rid(uniform_set);
		uniform_set = RID();
	}
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
	if (cull_pipeline.is_valid()) {
		rd->free_rid(cull_pipeline);
		cull_pipeline = RID();
	}
	if (cull_shader.is_valid()) {
		rd->free_rid(cull_shader);
		cull_shader = RID();
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
		if (uniform_set.is_valid()) {
			rd->free_rid(uniform_set);
			uniform_set = RID();
		}
		if (input_buffer.is_valid()) {
			rd->free_rid(input_buffer);
			input_buffer = RID();
		}
		if (output_buffer.is_valid()) {
			rd->free_rid(output_buffer);
			output_buffer = RID();
		}

		allocated_capacity = needed_count;

		// Input buffer: capacity * 64 bytes
		uint32_t input_size = allocated_capacity * sizeof(InstanceData);
		Vector<uint8_t> in_data;
		in_data.resize(input_size);
		memcpy(in_data.ptrw(), cpu_instances.ptr(), input_size);
		input_buffer = rd->storage_buffer_create(input_size, in_data);

		// Output buffer: capacity * 12 floats (48 bytes)
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

	// Determine output target: either MultiMesh's internal buffer or our own output_buffer
	RID target_out_buffer = output_buffer;
	RID target_cmd_buffer = dummy_command_buffer;

	if (multimesh.is_valid()) {
		RID mm_rid = multimesh->get_rid();
		RID mm_buf = RenderingServer::get_singleton()->multimesh_get_buffer_rd_rid(mm_rid);
		if (mm_buf.is_valid()) {
			target_out_buffer = mm_buf;
		}

		RID mm_cmd = RenderingServer::get_singleton()->multimesh_get_command_buffer_rd_rid(mm_rid);
		if (mm_cmd.is_valid()) {
			target_cmd_buffer = mm_cmd;
		}
	}

	// Recreate uniform set
	if (uniform_set.is_valid()) {
		rd->free_rid(uniform_set);
		uniform_set = RID();
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

	uniform_set = rd->uniform_set_create(uniforms, cull_shader, 0);
	instances_dirty = false;
}

void WorldPartitionGPUCuller3D::dispatch_culling(Camera3D *p_camera) {
	if (cpu_instances.is_empty()) {
		return;
	}

	if (instances_dirty) {
		_update_instance_buffers();
	}

	if (!rd || !cull_pipeline.is_valid() || !uniform_set.is_valid()) {
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

	if (culling_enabled) {
		params.culling_flags |= 1u;
	}
	if (distance_culling_enabled) {
		params.culling_flags |= 2u;
	}

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

	if (multimesh.is_valid() && RenderingServer::get_singleton()->multimesh_get_command_buffer_rd_rid(multimesh->get_rid()).is_valid()) {
		params.update_command_buffer = 1u;
	} else {
		params.update_command_buffer = 0u;
	}

	// 1. Reset counter & command buffer
	if (reset_pipeline.is_valid()) {
		RD::ComputeListID compute_list = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(compute_list, reset_pipeline);
		rd->compute_list_bind_uniform_set(compute_list, uniform_set, 0);
		rd->compute_list_set_push_constant(compute_list, &params, sizeof(CullParams));
		rd->compute_list_dispatch(compute_list, 1, 1, 1);
		rd->compute_list_end();
	}

	rd->barrier(RD::BARRIER_MASK_COMPUTE, RD::BARRIER_MASK_COMPUTE);

	// 2. Dispatch main culling pass
	{
		RD::ComputeListID compute_list = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(compute_list, cull_pipeline);
		rd->compute_list_bind_uniform_set(compute_list, uniform_set, 0);
		rd->compute_list_set_push_constant(compute_list, &params, sizeof(CullParams));
		uint32_t x_groups = (cpu_instances.size() + 63) / 64;
		rd->compute_list_dispatch(compute_list, x_groups, 1, 1);
		rd->compute_list_end();
	}

	rd->barrier(RD::BARRIER_MASK_COMPUTE, RD::BARRIER_MASK_VERTEX | RD::BARRIER_MASK_TRANSFER);
}

void WorldPartitionGPUCuller3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				set_process_internal(true);
			}
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			dispatch_culling();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_free_gpu_resources();
		} break;
	}
}

WorldPartitionGPUCuller3D::WorldPartitionGPUCuller3D() {
}

WorldPartitionGPUCuller3D::~WorldPartitionGPUCuller3D() {
	_free_gpu_resources();
}
