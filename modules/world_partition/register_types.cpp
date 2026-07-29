#include "register_types.h"

#include "core/object/class_db.h"

#include "world_chunk_metadata.h"
#include "world_partition_grid.h"
#include "world_partition_manager.h"
#include "world_partition_streamer_3d.h"

void initialize_world_partition_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	ClassDB::register_class<WorldPartitionGrid>();
	ClassDB::register_class<WorldChunkMetadata>();
	ClassDB::register_class<WorldPartitionManager>();
	ClassDB::register_class<WorldPartitionStreamer3D>();
}

void uninitialize_world_partition_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
