#include "world_partition_manager.h"
#include "world_partition_gpu_culler.h"

#include "core/config/engine.h"
#include "core/io/resource_loader.h"
#include "core/math/geometry_2d.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/occluder_instance_3d.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"

void WorldPartitionManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_grid", "grid"), &WorldPartitionManager::set_grid);
	ClassDB::bind_method(D_METHOD("get_grid"), &WorldPartitionManager::get_grid);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "grid", PROPERTY_HINT_RESOURCE_TYPE, "WorldPartitionGrid"), "set_grid", "get_grid");

	ClassDB::bind_method(D_METHOD("set_unload_padding", "padding"), &WorldPartitionManager::set_unload_padding);
	ClassDB::bind_method(D_METHOD("get_unload_padding"), &WorldPartitionManager::get_unload_padding);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unload_padding", PROPERTY_HINT_RANGE, "0.0,500.0,1.0,suffix:m"), "set_unload_padding", "get_unload_padding");

	ClassDB::bind_method(D_METHOD("set_update_interval", "interval"), &WorldPartitionManager::set_update_interval);
	ClassDB::bind_method(D_METHOD("get_update_interval"), &WorldPartitionManager::get_update_interval);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "update_interval", PROPERTY_HINT_RANGE, "0.01,1.0,0.01,suffix:s"), "set_update_interval", "get_update_interval");

	ClassDB::bind_method(D_METHOD("set_max_instantiations_per_frame", "max_count"), &WorldPartitionManager::set_max_instantiations_per_frame);
	ClassDB::bind_method(D_METHOD("get_max_instantiations_per_frame"), &WorldPartitionManager::get_max_instantiations_per_frame);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_instantiations_per_frame", PROPERTY_HINT_RANGE, "1,20,1"), "set_max_instantiations_per_frame", "get_max_instantiations_per_frame");

	ClassDB::bind_method(D_METHOD("set_time_budget_ms", "budget_ms"), &WorldPartitionManager::set_time_budget_ms);
	ClassDB::bind_method(D_METHOD("get_time_budget_ms"), &WorldPartitionManager::get_time_budget_ms);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time_budget_ms", PROPERTY_HINT_RANGE, "0.5,33.3,0.5,suffix:ms"), "set_time_budget_ms", "get_time_budget_ms");

	ClassDB::bind_method(D_METHOD("set_cross_fade_enabled", "enabled"), &WorldPartitionManager::set_cross_fade_enabled);
	ClassDB::bind_method(D_METHOD("is_cross_fade_enabled"), &WorldPartitionManager::is_cross_fade_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "cross_fade_enabled"), "set_cross_fade_enabled", "is_cross_fade_enabled");

	ClassDB::bind_method(D_METHOD("set_fade_margin", "margin"), &WorldPartitionManager::set_fade_margin);
	ClassDB::bind_method(D_METHOD("get_fade_margin"), &WorldPartitionManager::get_fade_margin);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fade_margin", PROPERTY_HINT_RANGE, "0.0,200.0,1.0,suffix:m"), "set_fade_margin", "get_fade_margin");

	ClassDB::bind_method(D_METHOD("set_directional_bias", "bias"), &WorldPartitionManager::set_directional_bias);
	ClassDB::bind_method(D_METHOD("get_directional_bias"), &WorldPartitionManager::get_directional_bias);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "directional_bias", PROPERTY_HINT_RANGE, "0.0,10.0,0.1"), "set_directional_bias", "get_directional_bias");

	ClassDB::bind_method(D_METHOD("set_gpu_culler", "culler"), &WorldPartitionManager::set_gpu_culler);
	ClassDB::bind_method(D_METHOD("get_gpu_culler"), &WorldPartitionManager::get_gpu_culler);

	ClassDB::bind_method(D_METHOD("set_gpu_culler_path", "path"), &WorldPartitionManager::set_gpu_culler_path);
	ClassDB::bind_method(D_METHOD("get_gpu_culler_path"), &WorldPartitionManager::get_gpu_culler_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "gpu_culler_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "WorldPartitionGPUCuller3D"), "set_gpu_culler_path", "get_gpu_culler_path");

	ClassDB::bind_method(D_METHOD("get_active_chunk_count"), &WorldPartitionManager::get_active_chunk_count);
	ClassDB::bind_method(D_METHOD("get_pending_load_count"), &WorldPartitionManager::get_pending_load_count);
	ClassDB::bind_method(D_METHOD("get_render_instance_count"), &WorldPartitionManager::get_render_instance_count);
	ClassDB::bind_method(D_METHOD("get_scene_instance_count"), &WorldPartitionManager::get_scene_instance_count);

	ClassDB::bind_method(D_METHOD("register_streamer", "streamer"), &WorldPartitionManager::register_streamer);
	ClassDB::bind_method(D_METHOD("unregister_streamer", "streamer"), &WorldPartitionManager::unregister_streamer);

	ADD_SIGNAL(MethodInfo("chunk_loaded", PropertyInfo(Variant::INT, "level"), PropertyInfo(Variant::INT, "x"), PropertyInfo(Variant::INT, "z")));
	ADD_SIGNAL(MethodInfo("chunk_unloaded", PropertyInfo(Variant::INT, "level"), PropertyInfo(Variant::INT, "x"), PropertyInfo(Variant::INT, "z")));
}

void WorldPartitionManager::set_unload_padding(float p_padding) {
	unload_padding = p_padding;
}

float WorldPartitionManager::get_unload_padding() const {
	return unload_padding;
}

void WorldPartitionManager::set_update_interval(float p_interval) {
	update_interval = p_interval;
}

float WorldPartitionManager::get_update_interval() const {
	return update_interval;
}

void WorldPartitionManager::set_max_instantiations_per_frame(int p_max) {
	max_instantiations_per_frame = MAX(1, p_max);
}

int WorldPartitionManager::get_max_instantiations_per_frame() const {
	return max_instantiations_per_frame;
}

void WorldPartitionManager::set_time_budget_ms(float p_budget) {
	time_budget_ms = MAX(0.1f, p_budget);
}

float WorldPartitionManager::get_time_budget_ms() const {
	return time_budget_ms;
}

void WorldPartitionManager::set_cross_fade_enabled(bool p_enabled) {
	cross_fade_enabled = p_enabled;
}

bool WorldPartitionManager::is_cross_fade_enabled() const {
	return cross_fade_enabled;
}

void WorldPartitionManager::set_fade_margin(float p_margin) {
	fade_margin = MAX(0.0f, p_margin);
}

float WorldPartitionManager::get_fade_margin() const {
	return fade_margin;
}

void WorldPartitionManager::set_directional_bias(float p_bias) {
	directional_bias = MAX(0.0f, p_bias);
}

float WorldPartitionManager::get_directional_bias() const {
	return directional_bias;
}

void WorldPartitionManager::set_gpu_culler(Object *p_culler) {
	if (p_culler) {
		gpu_culler_id = p_culler->get_instance_id();
	} else {
		gpu_culler_id = ObjectID();
	}
}

Object *WorldPartitionManager::get_gpu_culler() const {
	return ObjectDB::get_instance(gpu_culler_id);
}

void WorldPartitionManager::set_gpu_culler_path(const NodePath &p_path) {
	gpu_culler_path = p_path;
	if (is_inside_tree() && !gpu_culler_path.is_empty()) {
		Node *node = get_node_or_null(gpu_culler_path);
		set_gpu_culler(node);
	}
}

NodePath WorldPartitionManager::get_gpu_culler_path() const {
	return gpu_culler_path;
}

int WorldPartitionManager::get_active_chunk_count() const {
	int count = 0;
	for (const KeyValue<WorldGridIndex, LoadedChunk> &E : active_chunks) {
		if (E.value.state == STATE_LOADED) {
			count++;
		}
	}
	return count;
}

int WorldPartitionManager::get_pending_load_count() const {
	int count = 0;
	for (const KeyValue<WorldGridIndex, LoadedChunk> &E : active_chunks) {
		if (E.value.state == STATE_LOADING) {
			count++;
		}
	}
	return count;
}

int WorldPartitionManager::get_render_instance_count() const {
	int total = 0;
	for (const KeyValue<WorldGridIndex, LoadedChunk> &E : active_chunks) {
		total += E.value.render_instances.size() + E.value.occluder_instances.size();
	}
	return total;
}

int WorldPartitionManager::get_scene_instance_count() const {
	int total = 0;
	for (const KeyValue<WorldGridIndex, LoadedChunk> &E : active_chunks) {
		total += E.value.scene_instances.size();
	}
	return total;
}

float WorldPartitionManager::_calculate_chunk_priority(const WorldGridIndex &p_index, const Vector3 &p_view_pos, const Vector3 &p_view_dir, const Vector3 &p_velocity) {
	if (!grid.is_valid()) {
		return 0.0f;
	}

	float cell_w = grid->get_cell_size() * (float)(1 << p_index.level);
	Vector3 chunk_center(
			((float)p_index.x + 0.5f) * cell_w,
			p_view_pos.y,
			((float)p_index.z + 0.5f) * cell_w);

	Vector3 to_chunk = chunk_center - p_view_pos;
	to_chunk.y = 0.0f; // Ground plane distance
	float dist = to_chunk.length();

	float alignment = 0.0f;
	if (dist > 0.001f) {
		Vector3 dir = to_chunk / dist;
		Vector3 ref_dir = p_view_dir;
		if (p_velocity.length_squared() > 0.5f) {
			ref_dir = (p_view_dir + p_velocity.normalized()).normalized();
		}
		alignment = MAX(0.0f, dir.dot(ref_dir));
	}

	return (1.0f + alignment * directional_bias) / (dist + 1.0f);
}

void WorldPartitionManager::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				add_to_group("world_partition_manager");
				set_physics_process_internal(true);
			}
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			_process_streamers();
		} break;
	}
}

void WorldPartitionManager::set_grid(const Ref<WorldPartitionGrid> &p_grid) {
	grid = p_grid;
}

Ref<WorldPartitionGrid> WorldPartitionManager::get_grid() const {
	return grid;
}

void WorldPartitionManager::register_streamer(WorldPartitionStreamer3D *p_streamer) {
	if (streamers.find(p_streamer) == -1) {
		streamers.push_back(p_streamer);
	}
}

void WorldPartitionManager::unregister_streamer(WorldPartitionStreamer3D *p_streamer) {
	streamers.erase(p_streamer);
}

void WorldPartitionManager::_process_streamers() {
	if (grid.is_null() || streamers.is_empty()) {
		return;
	}

	float delta = get_physics_process_delta_time();
	time_since_last_update += delta;

	if (time_since_last_update >= update_interval) {
		time_since_last_update = 0.0;

		desired_chunks.clear();
		desired_chunks_set.clear();

		for (int i = 0; i < streamers.size(); i++) {
			WorldPartitionStreamer3D *streamer = streamers[i];
			PackedFloat32Array ranges = streamer->get_hlod_ranges();
			int max_level = ranges.size();
			float r0 = streamer->get_streaming_radius();
			float max_radius = (max_level == 0) ? r0 : ranges[max_level - 1];

			AABB max_bounds = streamer->get_predicted_bounds(max_radius);
			Vector<WorldGridIndex> top_chunks = grid->get_chunks_in_aabb(max_bounds, max_level);

			for (int j = 0; j < top_chunks.size(); j++) {
				_evaluate_chunk_tree(top_chunks[j], streamer, ranges, r0, desired_chunks);
			}
		}

		for (int i = 0; i < desired_chunks.size(); i++) {
			desired_chunks_set.insert(desired_chunks[i]);
		}

		// Priority sort desired_chunks based on distance and camera/streamer forward alignment
		if (desired_chunks.size() > 1 && !streamers.is_empty()) {
			Vector3 streamer_pos = streamers[0]->get_global_position();
			Vector3 streamer_dir = -streamers[0]->get_global_transform().basis.get_column(2).normalized(); // Forward (-Z)
			Vector3 streamer_vel = streamers[0]->get_velocity();

			Vector<ChunkPriorityItem> priority_items;
			priority_items.resize(desired_chunks.size());

			for (int i = 0; i < desired_chunks.size(); i++) {
				priority_items.write[i].index = desired_chunks[i];
				priority_items.write[i].priority = _calculate_chunk_priority(desired_chunks[i], streamer_pos, streamer_dir, streamer_vel);
			}

			priority_items.sort();

			for (int i = 0; i < priority_items.size(); i++) {
				desired_chunks.write[i] = priority_items[i].index;
			}
		}

		// 1. Unload chunks that are NO LONGER desired, but ONLY if replacement chunks are fully loaded to prevent popping!
		Vector<WorldGridIndex> chunks_to_unload;
		float base_cell_size = grid->get_cell_size();

		for (const KeyValue<WorldGridIndex, LoadedChunk> &E : active_chunks) {
			if (!desired_chunks_set.has(E.key)) {
				// If chunk was still loading and never displayed, drop it immediately without waiting
				if (E.value.state == STATE_LOADING) {
					chunks_to_unload.push_back(E.key);
					continue;
				}

				bool safe_to_unload = true;

				float size_A = base_cell_size * (1 << E.key.level);
				float min_ax = E.key.x * size_A;
				float max_ax = min_ax + size_A;
				float min_az = E.key.z * size_A;
				float max_az = min_az + size_A;

				for (int i = 0; i < desired_chunks.size(); i++) {
					WorldGridIndex D = desired_chunks[i];
					float size_B = base_cell_size * (1 << D.level);
					float min_bx = D.x * size_B;
					float max_bx = min_bx + size_B;
					float min_bz = D.z * size_B;
					float max_bz = min_bz + size_B;

					// Check if desired chunk D overlaps with old chunk A (A is being replaced by D)
					if (min_ax >= max_bx || min_bx >= max_ax || min_az >= max_bz || min_bz >= max_az) {
						continue;
					}

					// If they overlap, D must be fully loaded before we safely hide A!
					if (!active_chunks.has(D) || active_chunks[D].state != STATE_LOADED) {
						safe_to_unload = false;
						break;
					}
				}

				if (safe_to_unload) {
					chunks_to_unload.push_back(E.key);
				}
			}
		}

		for (int i = 0; i < chunks_to_unload.size(); i++) {
			_unload_chunk(chunks_to_unload[i]);
		}

		// 2. Load newly desired chunks in priority order
		for (int i = 0; i < desired_chunks.size(); i++) {
			if (!active_chunks.has(desired_chunks[i])) {
				_load_chunk(desired_chunks[i]);
			}
		}
	}

	// 3. Process instantiations in strict priority order (Governor limits per frame)
	uint64_t start_time = OS::get_singleton()->get_ticks_usec();
	uint64_t max_usec = (uint64_t)(time_budget_ms * 1000.0f);
	int instantiated_this_frame = 0;
	Vector<WorldGridIndex> obsolete_loading;

	for (int i = 0; i < desired_chunks.size(); i++) {
		const WorldGridIndex &idx = desired_chunks[i];
		if (!active_chunks.has(idx)) {
			continue;
		}

		LoadedChunk &lc = active_chunks[idx];
		if (lc.state == STATE_LOADING) {
			if (lc.metadata.is_valid()) {
				bool all_loaded = true;
				for (int j = 0; j < lc.metadata->get_item_count(); j++) {
					String path = lc.metadata->get_item_asset_path(j);
					ResourceLoader::ThreadLoadStatus status = ResourceLoader::load_threaded_get_status(path);

					if (status == ResourceLoader::THREAD_LOAD_FAILED || status == ResourceLoader::THREAD_LOAD_INVALID_RESOURCE) {
						ERR_PRINT("Failed to load async chunk asset: " + path);
					} else if (status == ResourceLoader::THREAD_LOAD_IN_PROGRESS) {
						all_loaded = false;
						break;
					}
				}

				if (all_loaded) {
					uint64_t elapsed = OS::get_singleton()->get_ticks_usec() - start_time;
					if (instantiated_this_frame < max_instantiations_per_frame && elapsed < max_usec) {
						_instantiate_chunk(idx);
						lc.state = STATE_LOADED;
						instantiated_this_frame++;
					}
				}
			} else {
				lc.state = STATE_LOADED;
			}
		}
	}

	// Clean up any loading chunks that are no longer desired
	for (KeyValue<WorldGridIndex, LoadedChunk> &E : active_chunks) {
		if (E.value.state == STATE_LOADING && !desired_chunks_set.has(E.key)) {
			obsolete_loading.push_back(E.key);
		}
	}

	for (int i = 0; i < obsolete_loading.size(); i++) {
		_unload_chunk(obsolete_loading[i]);
	}
}

void WorldPartitionManager::_evaluate_chunk_tree(const WorldGridIndex &p_idx, WorldPartitionStreamer3D *p_streamer, const PackedFloat32Array &p_ranges, float p_r0, Vector<WorldGridIndex> &r_load_chunks) {
	bool should_split = false;
	bool keep_parent = false;

	if (p_idx.level > 0) {
		float threshold = (p_idx.level == 1) ? p_r0 : p_ranges[p_idx.level - 2];

		float cell_size = grid->get_cell_size() * (1 << p_idx.level);
		Vector3 cam_pos = p_streamer->get_global_position();
		Vector3 velocity = p_streamer->get_velocity();
		float time = p_streamer->get_prediction_time();

		Vector2 cam_pos_2d(cam_pos.x, cam_pos.z);
		Vector2 predicted_pos_2d(cam_pos.x + velocity.x * time, cam_pos.z + velocity.z * time);

		Vector2 chunk_center(p_idx.x * cell_size + cell_size / 2.0f, p_idx.z * cell_size + cell_size / 2.0f);
		Vector2 closest_path_point = Geometry2D::get_closest_point_to_segment(chunk_center, cam_pos_2d, predicted_pos_2d);

		float cx = CLAMP(closest_path_point.x, p_idx.x * cell_size, (p_idx.x + 1) * cell_size);
		float cz = CLAMP(closest_path_point.y, p_idx.z * cell_size, (p_idx.z + 1) * cell_size);
		float dist = Math::sqrt(Vector2(closest_path_point.x - cx, closest_path_point.y - cz).length_squared());

		float margin = cross_fade_enabled ? fade_margin : 0.0f;

		// Hysteresis: if any child is already active, add unload_padding to threshold
		float split_threshold = threshold;
		bool any_child_active = false;
		for (int child_x = 0; child_x < 2; child_x++) {
			for (int child_z = 0; child_z < 2; child_z++) {
				WorldGridIndex child_idx;
				child_idx.level = p_idx.level - 1;
				child_idx.x = p_idx.x * 2 + child_x;
				child_idx.z = p_idx.z * 2 + child_z;
				if (active_chunks.has(child_idx)) {
					any_child_active = true;
					break;
				}
			}
			if (any_child_active) {
				break;
			}
		}

		if (any_child_active) {
			split_threshold += unload_padding;
		}

		// Split if within upper boundary of transition band
		float split_margin = (cross_fade_enabled && grid.is_valid() && grid->has_chunk(p_idx.level, p_idx.x, p_idx.z)) ? fade_margin : 0.0f;
		if (dist < (split_threshold + split_margin)) {
			should_split = true;
		}

		// Keep parent active if within or beyond lower boundary of transition band ONLY if parent chunk actually exists
		if (cross_fade_enabled && grid.is_valid() && grid->has_chunk(p_idx.level, p_idx.x, p_idx.z) && dist >= (split_threshold - split_margin)) {
			keep_parent = true;
		}
	}

	if (should_split) {
		for (int cx = 0; cx < 2; cx++) {
			for (int cz = 0; cz < 2; cz++) {
				WorldGridIndex child_idx;
				child_idx.level = p_idx.level - 1;
				child_idx.x = p_idx.x * 2 + cx;
				child_idx.z = p_idx.z * 2 + cz;

				if (grid->has_chunk(child_idx.level, child_idx.x, child_idx.z)) {
					_evaluate_chunk_tree(child_idx, p_streamer, p_ranges, p_r0, r_load_chunks);
				}
			}
		}
		if (keep_parent) {
			if (r_load_chunks.find(p_idx) == -1) {
				r_load_chunks.push_back(p_idx);
			}
		}
	} else {
		if (r_load_chunks.find(p_idx) == -1) {
			r_load_chunks.push_back(p_idx);
		}
	}
}

void WorldPartitionManager::_load_chunk(const WorldGridIndex &p_index) {
	LoadedChunk lc;
	lc.state = STATE_LOADING;
	lc.metadata = grid->get_chunk(p_index.level, p_index.x, p_index.z);

	if (lc.metadata.is_valid()) {
		for (int i = 0; i < lc.metadata->get_item_count(); i++) {
			String path = lc.metadata->get_item_asset_path(i);
			ResourceLoader::load_threaded_request(path, "", true); // use_sub_threads = true
		}
	}

	active_chunks[p_index] = lc;
}

void WorldPartitionManager::_apply_visibility_range(Node *p_node, float p_begin, float p_end, float p_begin_margin, float p_end_margin, RenderingServerEnums::VisibilityRangeFadeMode p_fade_mode) {
	if (!p_node) {
		return;
	}

	GeometryInstance3D *gi = Object::cast_to<GeometryInstance3D>(p_node);
	if (gi) {
		gi->set_visibility_range_begin(p_begin);
		gi->set_visibility_range_end(p_end);
		gi->set_visibility_range_begin_margin(p_begin_margin);
		gi->set_visibility_range_end_margin(p_end_margin);
		gi->set_visibility_range_fade_mode((GeometryInstance3D::VisibilityRangeFadeMode)p_fade_mode);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_apply_visibility_range(p_node->get_child(i), p_begin, p_end, p_begin_margin, p_end_margin, p_fade_mode);
	}
}

void WorldPartitionManager::_instantiate_chunk(const WorldGridIndex &p_index) {
	if (!active_chunks.has(p_index)) {
		return;
	}

	LoadedChunk &lc = active_chunks[p_index];
	if (lc.metadata.is_null()) {
		return;
	}

	float r0 = 150.0f;
	PackedFloat32Array ranges;
	if (!streamers.is_empty()) {
		r0 = streamers[0]->get_streaming_radius();
		ranges = streamers[0]->get_hlod_ranges();
	}

	float margin = cross_fade_enabled ? fade_margin : 0.0f;
	float range_begin = 0.0f;
	float range_end = 0.0f;
	float begin_margin = 0.0f;
	float end_margin = 0.0f;
	RenderingServerEnums::VisibilityRangeFadeMode fade_mode = RenderingServerEnums::VISIBILITY_RANGE_FADE_DISABLED;

	// Only apply cross-fade if enabled AND a parent HLOD chunk actually exists to replace this chunk!
	if (cross_fade_enabled && grid.is_valid()) {
		int parent_x = Math::floor((float)p_index.x / 2.0f);
		int parent_z = Math::floor((float)p_index.z / 2.0f);
		bool has_parent_hlod = grid->has_chunk(p_index.level + 1, parent_x, parent_z);

		if (p_index.level == 0) {
			range_begin = 0.0f;
			begin_margin = 0.0f;
			if (has_parent_hlod && !ranges.is_empty()) {
				range_end = r0;
				end_margin = margin;
				fade_mode = RenderingServerEnums::VISIBILITY_RANGE_FADE_SELF;
			} else {
				range_end = 0.0f;
				end_margin = 0.0f;
			}
		} else {
			// Level >= 1 (HLOD)
			range_begin = (p_index.level == 1) ? r0 : ((p_index.level - 2 < ranges.size()) ? ranges[p_index.level - 2] : r0);
			begin_margin = margin;
			fade_mode = RenderingServerEnums::VISIBILITY_RANGE_FADE_SELF;

			if (has_parent_hlod && (p_index.level - 1 < ranges.size())) {
				range_end = ranges[p_index.level - 1];
				end_margin = margin;
			} else {
				range_end = 0.0f;
				end_margin = 0.0f;
			}
		}
	}

	WorldPartitionGPUCuller3D *culler = Object::cast_to<WorldPartitionGPUCuller3D>(get_gpu_culler());
	TypedArray<Transform3D> gpu_transforms;
	AABB combined_local_aabb;
	bool has_gpu_instances = false;

	for (int i = 0; i < lc.metadata->get_item_count(); i++) {
		String path = lc.metadata->get_item_asset_path(i);
		Transform3D item_xform = lc.metadata->get_item_transform(i);

		Ref<Resource> res = ResourceLoader::load_threaded_get(path);
		if (res.is_null()) {
			continue;
		}

		// Occluder3D Direct Server Instancing for fast occlusion culling without SceneTree overhead
		if (lc.metadata->is_item_occluder(i)) {
			Ref<Occluder3D> occ = res;
			if (occ.is_valid()) {
				RID occ_instance = RenderingServer::get_singleton()->instance_create();
				RenderingServer::get_singleton()->instance_set_base(occ_instance, occ->get_rid());
				if (get_world_3d().is_valid()) {
					RenderingServer::get_singleton()->instance_set_scenario(occ_instance, get_world_3d()->get_scenario());
				}
				RenderingServer::get_singleton()->instance_set_transform(occ_instance, get_global_transform() * item_xform);
				lc.occluder_instances.push_back(occ_instance);
				continue;
			}
		}

		// Direct Server Instancing for fast static meshes without SceneTree overhead
		Ref<Mesh> mesh = res;
		if (mesh.is_valid()) {
			RID instance = RenderingServer::get_singleton()->instance_create();
			RenderingServer::get_singleton()->instance_set_base(instance, mesh->get_rid());
			if (get_world_3d().is_valid()) {
				RenderingServer::get_singleton()->instance_set_scenario(instance, get_world_3d()->get_scenario());
			}
			RenderingServer::get_singleton()->instance_set_transform(instance, get_global_transform() * item_xform);
			RenderingServer::get_singleton()->instance_geometry_set_visibility_range(instance, range_begin, range_end, begin_margin, end_margin, fade_mode);
			lc.render_instances.push_back(instance);

			if (culler) {
				gpu_transforms.push_back(get_global_transform() * item_xform);
				if (!has_gpu_instances) {
					combined_local_aabb = mesh->get_aabb();
					has_gpu_instances = true;
				} else {
					combined_local_aabb = combined_local_aabb.merge(mesh->get_aabb());
				}
			}
			continue;
		}

		// PackedScene Instancing for complex composite scenes
		Ref<PackedScene> scene = res;
		if (scene.is_valid()) {
			Node *instance = scene->instantiate();
			if (instance) {
				add_child(instance);
				Node3D *spatial = Object::cast_to<Node3D>(instance);
				if (spatial) {
					spatial->set_transform(item_xform);
				}
				_apply_visibility_range(instance, range_begin, range_end, begin_margin, end_margin, fade_mode);
				lc.scene_instances.push_back(instance);
			}
		}
	}

	if (culler && has_gpu_instances && !gpu_transforms.is_empty()) {
		lc.gpu_cull_batch_id = culler->add_instance_batch(gpu_transforms, combined_local_aabb);
	}

	emit_signal("chunk_loaded", p_index.level, p_index.x, p_index.z);
}

void WorldPartitionManager::_unload_chunk(const WorldGridIndex &p_index) {
	if (!active_chunks.has(p_index)) {
		return;
	}

	LoadedChunk &lc = active_chunks[p_index];

	WorldPartitionGPUCuller3D *culler = Object::cast_to<WorldPartitionGPUCuller3D>(get_gpu_culler());
	if (culler && lc.gpu_cull_batch_id != -1) {
		culler->remove_instance_batch(lc.gpu_cull_batch_id);
		lc.gpu_cull_batch_id = -1;
	}

	for (int i = 0; i < lc.scene_instances.size(); i++) {
		Node *node = lc.scene_instances[i];
		if (node) {
			node->queue_free();
		}
	}

	for (int i = 0; i < lc.render_instances.size(); i++) {
		RenderingServer::get_singleton()->free_rid(lc.render_instances[i]);
	}
	lc.render_instances.clear();

	for (int i = 0; i < lc.occluder_instances.size(); i++) {
		RenderingServer::get_singleton()->free_rid(lc.occluder_instances[i]);
	}
	lc.occluder_instances.clear();

	for (int i = 0; i < lc.physics_instances.size(); i++) {
		PhysicsServer3D::get_singleton()->free_rid(lc.physics_instances[i]);
	}
	lc.physics_instances.clear();

	emit_signal("chunk_unloaded", p_index.level, p_index.x, p_index.z);

	active_chunks.erase(p_index);
}

WorldPartitionManager::WorldPartitionManager() {
}

WorldPartitionManager::~WorldPartitionManager() {
	Vector<WorldGridIndex> keys;
	for (const KeyValue<WorldGridIndex, LoadedChunk> &E : active_chunks) {
		keys.push_back(E.key);
	}
	for (int i = 0; i < keys.size(); i++) {
		_unload_chunk(keys[i]);
	}
}
