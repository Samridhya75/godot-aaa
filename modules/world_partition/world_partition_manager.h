#pragma once

#include "core/io/resource_loader.h"
#include "core/templates/hash_set.h"
#include "scene/3d/node_3d.h"
#include "servers/physics_3d/physics_server_3d.h"
#include "servers/rendering/rendering_server.h"
#include "world_partition_grid.h"
#include "world_partition_streamer_3d.h"

class WorldPartitionManager : public Node3D {
	GDCLASS(WorldPartitionManager, Node3D);

private:
	Ref<WorldPartitionGrid> grid;
	Vector<WorldPartitionStreamer3D *> streamers;
	float unload_padding = 20.0;
	float update_interval = 0.1;
	float time_since_last_update = 0.1;

	int max_instantiations_per_frame = 4;
	float time_budget_ms = 8.0;
	int max_concurrent_loads = 2;
	int current_concurrent_loads = 0;

	bool cross_fade_enabled = false;
	float fade_margin = 20.0;
	float directional_bias = 2.0;

	NodePath gpu_culler_path;
	ObjectID gpu_culler_id;

	Vector<WorldGridIndex> desired_chunks;
	HashSet<WorldGridIndex, WorldGridIndexHasher> desired_chunks_set;

	enum ChunkState {
		STATE_UNLOADED,
		STATE_QUEUED,
		STATE_LOADING,
		STATE_LOADED
	};

	struct LoadedChunk {
		ChunkState state = STATE_UNLOADED;
		Ref<WorldChunkMetadata> metadata;
		Vector<RID> render_instances;
		Vector<RID> physics_instances;
		Vector<RID> occluder_instances;
		Vector<Node *> scene_instances;
		Vector<int> gpu_cull_batch_ids;
	};

	HashMap<WorldGridIndex, LoadedChunk, WorldGridIndexHasher> active_chunks;

	struct ChunkPriorityItem {
		WorldGridIndex index;
		float priority = 0.0f;

		bool operator<(const ChunkPriorityItem &p_other) const {
			return priority > p_other.priority; // Descending sort
		}
	};

	void _process_streamers();
	void _load_chunk(const WorldGridIndex &p_index);
	void _unload_chunk(const WorldGridIndex &p_index);
	void _instantiate_chunk(const WorldGridIndex &p_index);
	void _evaluate_chunk_tree(const WorldGridIndex &p_idx, WorldPartitionStreamer3D *p_streamer, const PackedFloat32Array &p_ranges, float p_r0, Vector<WorldGridIndex> &r_load_chunks);
	void _apply_visibility_range(Node *p_node, float p_begin, float p_end, float p_begin_margin, float p_end_margin, RenderingServerEnums::VisibilityRangeFadeMode p_fade_mode);
	float _calculate_chunk_priority(const WorldGridIndex &p_index, const Vector3 &p_view_pos, const Vector3 &p_view_dir, const Vector3 &p_velocity);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_grid(const Ref<WorldPartitionGrid> &p_grid);
	Ref<WorldPartitionGrid> get_grid() const;

	void set_unload_padding(float p_padding);
	float get_unload_padding() const;

	void set_update_interval(float p_interval);
	float get_update_interval() const;

	void set_max_instantiations_per_frame(int p_max);
	int get_max_instantiations_per_frame() const;

	void set_time_budget_ms(float p_budget);
	float get_time_budget_ms() const;

	void set_cross_fade_enabled(bool p_enabled);
	bool is_cross_fade_enabled() const;

	void set_fade_margin(float p_margin);
	float get_fade_margin() const;

	void set_directional_bias(float p_bias);
	float get_directional_bias() const;

	void set_gpu_culler(Object *p_culler);
	Object *get_gpu_culler() const;

	void set_gpu_culler_path(const NodePath &p_path);
	NodePath get_gpu_culler_path() const;

	// Telemetry & Metrics
	int get_active_chunk_count() const;
	int get_pending_load_count() const;
	int get_render_instance_count() const;
	int get_scene_instance_count() const;

	void register_streamer(WorldPartitionStreamer3D *p_streamer);
	void unregister_streamer(WorldPartitionStreamer3D *p_streamer);

	WorldPartitionManager();
	~WorldPartitionManager();
};
