#pragma once

#include "scene/3d/node_3d.h"
#include "scene/resources/multimesh.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

class Camera3D;

class WorldPartitionGPUCuller3D : public Node3D {
	GDCLASS(WorldPartitionGPUCuller3D, Node3D);

public:
	struct InstanceData {
		float xform_row0[4];
		float xform_row1[4];
		float xform_row2[4];
		float aabb_center_radius[4];
	};

	struct CullParams {
		float frustum_planes[24]; // 6 planes * 4 floats
		float camera_position[4]; // xyz = camera position, w = max_distance
		uint32_t total_instances = 0;
		uint32_t culling_flags = 3; // bit 0 = frustum, bit 1 = distance
		uint32_t update_command_buffer = 0;
		uint32_t pad = 0;
	};

private:
	Ref<MultiMesh> multimesh;
	bool culling_enabled = true;
	bool distance_culling_enabled = true;
	float max_cull_distance = 500.0f;
	float distance_fade_margin = 20.0f;

	Vector<InstanceData> cpu_instances;
	HashMap<int, Vector<InstanceData>> instance_batches;
	int next_batch_id = 1;
	bool instances_dirty = false;

	RenderingDevice *rd = nullptr;
	RID reset_shader;
	RID reset_pipeline;
	RID cull_shader;
	RID cull_pipeline;

	RID input_buffer;
	RID output_buffer;
	RID counter_buffer;
	RID dummy_command_buffer;
	RID uniform_set;

	uint32_t allocated_capacity = 0;

	bool _init_gpu_resources();
	void _free_gpu_resources();
	void _update_instance_buffers();
	void _rebuild_cpu_instances();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_multimesh(const Ref<MultiMesh> &p_multimesh);
	Ref<MultiMesh> get_multimesh() const;

	void set_culling_enabled(bool p_enabled);
	bool is_culling_enabled() const;

	void set_distance_culling_enabled(bool p_enabled);
	bool is_distance_culling_enabled() const;

	void set_max_cull_distance(float p_dist);
	float get_max_cull_distance() const;

	void set_distance_fade_margin(float p_margin);
	float get_distance_fade_margin() const;

	void add_instance(const Transform3D &p_transform, const AABB &p_local_aabb);
	void set_instances(const TypedArray<Transform3D> &p_transforms, const AABB &p_local_aabb);
	int add_instance_batch(const TypedArray<Transform3D> &p_transforms, const AABB &p_local_aabb);
	void remove_instance_batch(int p_batch_id);
	void clear_instances();
	int get_instance_count() const;

	void dispatch_culling(Camera3D *p_camera = nullptr);

	WorldPartitionGPUCuller3D();
	~WorldPartitionGPUCuller3D();
};
