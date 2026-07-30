#include "world_partition_manager.h"
#include "core/config/engine.h"
#include "core/object/class_db.h"

void WorldPartitionManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_grid", "grid"), &WorldPartitionManager::set_grid);
	ClassDB::bind_method(D_METHOD("get_grid"), &WorldPartitionManager::get_grid);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "grid", PROPERTY_HINT_RESOURCE_TYPE, "WorldPartitionGrid"), "set_grid", "get_grid");

	ClassDB::bind_method(D_METHOD("register_streamer", "streamer"), &WorldPartitionManager::register_streamer);
	ClassDB::bind_method(D_METHOD("unregister_streamer", "streamer"), &WorldPartitionManager::unregister_streamer);
}

void WorldPartitionManager::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!Engine::get_singleton()->is_editor_hint()) {
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

	Vector<WorldGridIndex> required_chunks;

	for (int i = 0; i < streamers.size(); i++) {
		AABB bounds = streamers[i]->get_predicted_bounds();
		Vector<WorldGridIndex> chunks = grid->get_chunks_in_aabb(bounds, 0); // Query level 0 chunks
		for (int j = 0; j < chunks.size(); j++) {
			if (required_chunks.find(chunks[j]) == -1) {
				required_chunks.push_back(chunks[j]);
			}
		}
	}

	// 1. Unload chunks that are no longer required
	Vector<WorldGridIndex> chunks_to_unload;
	for (const KeyValue<WorldGridIndex, LoadedChunk> &E : active_chunks) {
		if (required_chunks.find(E.key) == -1) {
			chunks_to_unload.push_back(E.key);
		}
	}

	for (int i = 0; i < chunks_to_unload.size(); i++) {
		_unload_chunk(chunks_to_unload[i]);
	}

	// 2. Load newly required chunks
	for (int i = 0; i < required_chunks.size(); i++) {
		if (!active_chunks.has(required_chunks[i])) {
			_load_chunk(required_chunks[i]);
		}
	}

	// 3. Process loading chunks (State Machine)
	for (KeyValue<WorldGridIndex, LoadedChunk> &E : active_chunks) {
		if (E.value.state == STATE_LOADING) {
			// In full implementation, we'd check ResourceLoader::load_threaded_get_status here.
			// For the skeleton, we instantly instantiate the metadata from the grid.
			Ref<WorldChunkMetadata> meta = grid->get_chunk(E.key.level, E.key.x, E.key.z);
			if (meta.is_valid()) {
				E.value.metadata = meta;
				_instantiate_chunk(E.key);
				E.value.state = STATE_LOADED;
			} else {
				E.value.state = STATE_LOADED; 
			}
		}
	}
}

void WorldPartitionManager::_load_chunk(const WorldGridIndex &p_index) {
	LoadedChunk lc;
	lc.state = STATE_LOADING;
	active_chunks[p_index] = lc;
	// Initiate threaded load request here in a full implementation
}

void WorldPartitionManager::_instantiate_chunk(const WorldGridIndex &p_index) {
	if (!active_chunks.has(p_index)) {
		return;
	}

	LoadedChunk &lc = active_chunks[p_index];
	if (lc.metadata.is_null()) {
		return;
	}

	// Directly instantiate assets into Servers without blocking SceneTree
	for (int i = 0; i < lc.metadata->get_item_count(); i++) {
		// String path = lc.metadata->get_item_asset_path(i);
		// Transform3D xform = lc.metadata->get_item_transform(i);
		// bool is_occluder = lc.metadata->is_item_occluder(i);

		// Boilerplate for Direct Server Instantiation:
		// Ref<Mesh> mesh = ResourceLoader::load(path);
		// if (mesh.is_valid()) {
		//     RID instance = RenderingServer::get_singleton()->instance_create();
		//     RenderingServer::get_singleton()->instance_set_base(instance, mesh->get_rid());
		//     RenderingServer::get_singleton()->instance_set_transform(instance, xform);
		//     lc.render_instances.push_back(instance);
		// }
	}
}

void WorldPartitionManager::_unload_chunk(const WorldGridIndex &p_index) {
	if (!active_chunks.has(p_index)) {
		return;
	}

	LoadedChunk &lc = active_chunks[p_index];

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
