#ifndef WORLD_PARTITION_MANAGER_H
#define WORLD_PARTITION_MANAGER_H

#include "scene/3d/node_3d.h"
#include "world_partition_grid.h"
#include "world_partition_streamer_3d.h"
#include "core/io/resource_loader.h"
#include "servers/rendering/rendering_server.h"
#include "servers/physics_3d/physics_server_3d.h"

class WorldPartitionManager : public Node3D {
	GDCLASS(WorldPartitionManager, Node3D);

private:
	Ref<WorldPartitionGrid> grid;
	Vector<WorldPartitionStreamer3D *> streamers;

	enum ChunkState {
		STATE_UNLOADED,
		STATE_LOADING,
		STATE_LOADED
	};

	struct LoadedChunk {
		ChunkState state = STATE_UNLOADED;
		Ref<WorldChunkMetadata> metadata;
		Vector<RID> render_instances;
		Vector<RID> physics_instances;
		Vector<Node*> scene_instances;
	};

	HashMap<WorldGridIndex, LoadedChunk, WorldGridIndexHasher> active_chunks;

	void _process_streamers();
	void _load_chunk(const WorldGridIndex &p_index);
	void _unload_chunk(const WorldGridIndex &p_index);
	void _instantiate_chunk(const WorldGridIndex &p_index);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_grid(const Ref<WorldPartitionGrid> &p_grid);
	Ref<WorldPartitionGrid> get_grid() const;

	void register_streamer(WorldPartitionStreamer3D *p_streamer);
	void unregister_streamer(WorldPartitionStreamer3D *p_streamer);

	WorldPartitionManager();
	~WorldPartitionManager();
};

#endif // WORLD_PARTITION_MANAGER_H
