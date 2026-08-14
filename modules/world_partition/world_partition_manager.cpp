#include "world_partition_manager.h"
#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "scene/resources/packed_scene.h"
#include "core/io/resource_loader.h"

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

	Vector<WorldGridIndex> load_chunks;
	Vector<WorldGridIndex> keep_chunks;

	for (int i = 0; i < streamers.size(); i++) {
		AABB load_bounds = streamers[i]->get_predicted_bounds();
		
		// Create a larger bounds for unloading (hysteresis)
		AABB unload_bounds = load_bounds;
		unload_bounds.position -= Vector3(unload_padding, unload_padding, unload_padding);
		unload_bounds.size += Vector3(unload_padding * 2, unload_padding * 2, unload_padding * 2);

		Vector<WorldGridIndex> chunks_to_load = grid->get_chunks_in_aabb(load_bounds, 0);
		for (int j = 0; j < chunks_to_load.size(); j++) {
			if (load_chunks.find(chunks_to_load[j]) == -1) {
				load_chunks.push_back(chunks_to_load[j]);
			}
		}
		
		Vector<WorldGridIndex> chunks_to_keep = grid->get_chunks_in_aabb(unload_bounds, 0);
		for (int j = 0; j < chunks_to_keep.size(); j++) {
			if (keep_chunks.find(chunks_to_keep[j]) == -1) {
				keep_chunks.push_back(chunks_to_keep[j]);
			}
		}
	}

	// 1. Unload chunks that are completely outside the unload_bounds
	Vector<WorldGridIndex> chunks_to_unload;
	for (const KeyValue<WorldGridIndex, LoadedChunk> &E : active_chunks) {
		if (keep_chunks.find(E.key) == -1) {
			chunks_to_unload.push_back(E.key);
		}
	}

	for (int i = 0; i < chunks_to_unload.size(); i++) {
		_unload_chunk(chunks_to_unload[i]);
	}

	// 2. Load newly required chunks
	for (int i = 0; i < load_chunks.size(); i++) {
		if (!active_chunks.has(load_chunks[i])) {
			_load_chunk(load_chunks[i]);
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
						// Fallback or abort chunk load on failure
						ERR_PRINT("Failed to load async chunk asset: " + path);
						// We'll mark it loaded to avoid infinite looping, but skip instantiation for failed items
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

		// Asset is already loaded async, this just retrieves the result
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
	// Cleanup any remaining chunks on exit to prevent RID leaks
	Vector<WorldGridIndex> keys;
	for (const KeyValue<WorldGridIndex, LoadedChunk> &E : active_chunks) {
		keys.push_back(E.key);
	}
	for (int i = 0; i < keys.size(); i++) {
		_unload_chunk(keys[i]);
	}
}
