#include "world_partition_manager.h"
#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "scene/resources/packed_scene.h"
#include "core/io/resource_loader.h"
#include "scene/3d/node_3d.h"
#include "core/math/geometry_2d.h"
#include "scene/3d/camera_3d.h"
#include "scene/main/viewport.h"

void WorldPartitionManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_grid", "grid"), &WorldPartitionManager::set_grid);
	ClassDB::bind_method(D_METHOD("get_grid"), &WorldPartitionManager::get_grid);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "grid", PROPERTY_HINT_RESOURCE_TYPE, "WorldPartitionGrid"), "set_grid", "get_grid");

	ClassDB::bind_method(D_METHOD("set_unload_padding", "padding"), &WorldPartitionManager::set_unload_padding);
	ClassDB::bind_method(D_METHOD("get_unload_padding"), &WorldPartitionManager::get_unload_padding);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unload_padding"), "set_unload_padding", "get_unload_padding");

	ClassDB::bind_method(D_METHOD("register_streamer", "streamer"), &WorldPartitionManager::register_streamer);
	ClassDB::bind_method(D_METHOD("unregister_streamer", "streamer"), &WorldPartitionManager::unregister_streamer);
}

void WorldPartitionManager::set_unload_padding(float p_padding) {
	unload_padding = p_padding;
}

float WorldPartitionManager::get_unload_padding() const {
	return unload_padding;
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

	Vector<WorldGridIndex> desired_chunks;

	for (int i = 0; i < streamers.size(); i++) {
		WorldPartitionStreamer3D *streamer = streamers[i];
		PackedFloat32Array ranges = streamer->get_hlod_ranges();
		int max_level = ranges.size();
		float r0 = streamer->get_streaming_radius();
		float max_radius = (max_level == 0) ? r0 : ranges[max_level - 1];

		AABB max_bounds = streamer->get_predicted_bounds(max_radius);
		
		// To ensure we don't miss any top-level chunks, we get all chunks at max_level in bounds
		Vector<WorldGridIndex> top_chunks = grid->get_chunks_in_aabb(max_bounds, max_level);
		
		for (int j = 0; j < top_chunks.size(); j++) {
			_evaluate_chunk_tree(top_chunks[j], streamer, ranges, r0, desired_chunks);
		}
	}

	// 1. Unload chunks that are NO LONGER desired, but ONLY if their replacements are fully loaded to prevent popping!
	Vector<WorldGridIndex> chunks_to_unload;
	float base_cell_size = grid->get_cell_size();

	for (const KeyValue<WorldGridIndex, LoadedChunk> &E : active_chunks) {
		if (desired_chunks.find(E.key) == -1) {
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

				// If they overlap, D must be fully loaded before we can safely hide A!
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

	// 2. Load newly desired chunks
	for (int i = 0; i < desired_chunks.size(); i++) {
		if (!active_chunks.has(desired_chunks[i])) {
			_load_chunk(desired_chunks[i]);
		}
	}

	// 3. Process loading chunks (Async State Machine)
	int instantiated_this_frame = 0;
	for (KeyValue<WorldGridIndex, LoadedChunk> &E : active_chunks) {
		if (E.value.state == STATE_LOADING) {
			if (E.value.metadata.is_valid()) {
				bool all_loaded = true;
				for (int i = 0; i < E.value.metadata->get_item_count(); i++) {
					String path = E.value.metadata->get_item_asset_path(i);
					ResourceLoader::ThreadLoadStatus status = ResourceLoader::load_threaded_get_status(path);
					
					if (status == ResourceLoader::THREAD_LOAD_FAILED || status == ResourceLoader::THREAD_LOAD_INVALID_RESOURCE) {
						ERR_PRINT("Failed to load async chunk asset: " + path);
					} else if (status == ResourceLoader::THREAD_LOAD_IN_PROGRESS) {
						all_loaded = false;
						break;
					}
				}
				
				if (all_loaded) {
					if (instantiated_this_frame < 1) { // Time-slice instantiation to prevent main thread stutters
						_instantiate_chunk(E.key);
						E.value.state = STATE_LOADED;
						instantiated_this_frame++;
					}
				}
			} else {
				E.value.state = STATE_LOADED; 
			}
		}
	}
}

void WorldPartitionManager::_evaluate_chunk_tree(const WorldGridIndex &p_idx, WorldPartitionStreamer3D *p_streamer, const PackedFloat32Array &p_ranges, float p_r0, Vector<WorldGridIndex> &r_load_chunks) {
	bool should_split = false;

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
		float dist_sq = Vector2(closest_path_point.x - cx, closest_path_point.y - cz).length_squared();
		
		if (dist_sq < (threshold * threshold)) {
			should_split = true;
		}

		// Frustum Culling: If we are deciding to split to Level 0, we aggressively cull chunks behind the camera
		if (should_split && p_idx.level == 1) {
			Camera3D *cam = get_viewport() ? get_viewport()->get_camera_3d() : nullptr;
			if (cam) {
				AABB chunk_aabb(Vector3(p_idx.x * cell_size, -10000, p_idx.z * cell_size), Vector3(cell_size, 20000, cell_size));
				Vector<Plane> frustum = cam->get_frustum();
				
				bool in_frustum = true;
				for (int i = 0; i < frustum.size(); i++) {
					Plane p = frustum[i];
					Vector3 center = chunk_aabb.get_center();
					Vector3 half_extents = chunk_aabb.get_size() / 2.0;
					
					Vector3 neg_vertex = center;
					neg_vertex.x -= (p.normal.x > 0) ? half_extents.x : -half_extents.x;
					neg_vertex.y -= (p.normal.y > 0) ? half_extents.y : -half_extents.y;
					neg_vertex.z -= (p.normal.z > 0) ? half_extents.z : -half_extents.z;
					
					if (p.is_point_over(neg_vertex)) {
						in_frustum = false;
						break;
					}
				}

				if (!in_frustum) {
					should_split = false; // Don't load high-res chunks if completely outside frustum!
				}
			}
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
			ResourceLoader::load_threaded_request(path, "PackedScene", true); // use_sub_threads = true
		}
	}
	
	active_chunks[p_index] = lc;
}

void WorldPartitionManager::_instantiate_chunk(const WorldGridIndex &p_index) {
	if (!active_chunks.has(p_index)) {
		return;
	}

	LoadedChunk &lc = active_chunks[p_index];
	if (lc.metadata.is_null()) {
		return;
	}

	for (int i = 0; i < lc.metadata->get_item_count(); i++) {
		String path = lc.metadata->get_item_asset_path(i);

		Ref<PackedScene> scene = ResourceLoader::load_threaded_get(path);
		if (scene.is_valid()) {
			Node *instance = scene->instantiate();
			if (instance) {
				add_child(instance);
				lc.scene_instances.push_back(instance);
			}
		}
	}
}

void WorldPartitionManager::_unload_chunk(const WorldGridIndex &p_index) {
	if (!active_chunks.has(p_index)) {
		return;
	}

	LoadedChunk &lc = active_chunks[p_index];

	for (int i = 0; i < lc.scene_instances.size(); i++) {
		Node *node = lc.scene_instances[i];
		if (node) {
			node->queue_free();
		}
	}

	for (int i = 0; i < lc.render_instances.size(); i++) {
		RenderingServer::get_singleton()->free_rid(lc.render_instances[i]);
	}

	for (int i = 0; i < lc.physics_instances.size(); i++) {
		PhysicsServer3D::get_singleton()->free_rid(lc.physics_instances[i]);
	}

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
