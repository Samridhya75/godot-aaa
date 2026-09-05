#ifndef WORLD_PARTITION_GRID_H
#define WORLD_PARTITION_GRID_H

#include "core/io/resource.h"
#include "core/templates/hash_map.h"
#include "world_chunk_metadata.h"

struct WorldGridIndex {
	int32_t level = 0; // HLOD level (0 is max detail)
	int32_t x = 0;
	int32_t z = 0;

	bool operator==(const WorldGridIndex &p_other) const {
		return level == p_other.level && x == p_other.x && z == p_other.z;
	}
};

struct WorldGridIndexHasher {
	static _FORCE_INLINE_ uint32_t hash(const WorldGridIndex &p_index) {
		uint32_t h = p_index.level;
		h = (h * 73856093) ^ p_index.x;
		h = (h * 19349663) ^ p_index.z;
		return h;
	}
};

class WorldPartitionGrid : public Resource {
	GDCLASS(WorldPartitionGrid, Resource);

private:
	float cell_size = 100.0;
	HashMap<WorldGridIndex, Ref<WorldChunkMetadata>, WorldGridIndexHasher> chunks;

protected:
	static void _bind_methods();

public:
	void set_cell_size(float p_size);
	float get_cell_size() const;

	int get_max_level() const;

	void set_chunk(int p_level, int p_x, int p_z, const Ref<WorldChunkMetadata> &p_chunk);
	Ref<WorldChunkMetadata> get_chunk(int p_level, int p_x, int p_z) const;
	bool has_chunk(int p_level, int p_x, int p_z) const;
	void remove_chunk(int p_level, int p_x, int p_z);
	void clear_chunks();

	// Helper for saving/loading via the Godot Editor
	void set_grid_data(const Array &p_data);
	Array get_grid_data() const;

	// Querying
	Vector<WorldGridIndex> get_chunks_in_aabb(const AABB &p_aabb, int p_level) const;

	WorldPartitionGrid();
	~WorldPartitionGrid();
};

#endif // WORLD_PARTITION_GRID_H
