#include "world_partition_grid.h"

void WorldPartitionGrid::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_cell_size", "size"), &WorldPartitionGrid::set_cell_size);
	ClassDB::bind_method(D_METHOD("get_cell_size"), &WorldPartitionGrid::get_cell_size);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_size"), "set_cell_size", "get_cell_size");

	ClassDB::bind_method(D_METHOD("set_chunk", "level", "x", "z", "chunk"), &WorldPartitionGrid::set_chunk);
	ClassDB::bind_method(D_METHOD("get_chunk", "level", "x", "z"), &WorldPartitionGrid::get_chunk);
	ClassDB::bind_method(D_METHOD("remove_chunk", "level", "x", "z"), &WorldPartitionGrid::remove_chunk);
	ClassDB::bind_method(D_METHOD("clear_chunks"), &WorldPartitionGrid::clear_chunks);

	ClassDB::bind_method(D_METHOD("set_grid_data", "data"), &WorldPartitionGrid::set_grid_data);
	ClassDB::bind_method(D_METHOD("get_grid_data"), &WorldPartitionGrid::get_grid_data);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "grid_data"), "set_grid_data", "get_grid_data");
}

void WorldPartitionGrid::set_cell_size(float p_size) {
	cell_size = p_size;
	emit_changed();
}

float WorldPartitionGrid::get_cell_size() const {
	return cell_size;
}

void WorldPartitionGrid::set_chunk(int p_level, int p_x, int p_z, const Ref<WorldChunkMetadata> &p_chunk) {
	WorldGridIndex idx;
	idx.level = p_level;
	idx.x = p_x;
	idx.z = p_z;
	if (p_chunk.is_valid()) {
		chunks[idx] = p_chunk;
	} else {
		chunks.erase(idx);
	}
	emit_changed();
}

Ref<WorldChunkMetadata> WorldPartitionGrid::get_chunk(int p_level, int p_x, int p_z) const {
	WorldGridIndex idx;
	idx.level = p_level;
	idx.x = p_x;
	idx.z = p_z;
	if (chunks.has(idx)) {
		return chunks[idx];
	}
	return Ref<WorldChunkMetadata>();
}

void WorldPartitionGrid::remove_chunk(int p_level, int p_x, int p_z) {
	WorldGridIndex idx;
	idx.level = p_level;
	idx.x = p_x;
	idx.z = p_z;
	chunks.erase(idx);
	emit_changed();
}

void WorldPartitionGrid::clear_chunks() {
	chunks.clear();
	emit_changed();
}

void WorldPartitionGrid::set_grid_data(const Array &p_data) {
	chunks.clear();
	for (int i = 0; i < p_data.size(); i++) {
		Dictionary d = p_data[i];
		if (d.has("level") && d.has("x") && d.has("z") && d.has("chunk")) {
			WorldGridIndex idx;
			idx.level = d["level"];
			idx.x = d["x"];
			idx.z = d["z"];
			Ref<WorldChunkMetadata> chunk = d["chunk"];
			if (chunk.is_valid()) {
				chunks[idx] = chunk;
			}
		}
	}
	emit_changed();
}

Array WorldPartitionGrid::get_grid_data() const {
	Array data;
	for (const KeyValue<WorldGridIndex, Ref<WorldChunkMetadata>> &E : chunks) {
		Dictionary d;
		d["level"] = E.key.level;
		d["x"] = E.key.x;
		d["z"] = E.key.z;
		d["chunk"] = E.value;
		data.push_back(d);
	}
	return data;
}

Vector<WorldGridIndex> WorldPartitionGrid::get_chunks_in_aabb(const AABB &p_aabb, int p_level) const {
	Vector<WorldGridIndex> result;
	
	// HLOD chunks (levels > 0) are exponentially larger
	float level_cell_size = cell_size * (1 << p_level);
	
	// Convert AABB bounds to grid coordinates
	int min_x = Math::floor(p_aabb.position.x / level_cell_size);
	int max_x = Math::floor((p_aabb.position.x + p_aabb.size.x) / level_cell_size);
	
	int min_z = Math::floor(p_aabb.position.z / level_cell_size);
	int max_z = Math::floor((p_aabb.position.z + p_aabb.size.z) / level_cell_size);
	
	for (int x = min_x; x <= max_x; x++) {
		for (int z = min_z; z <= max_z; z++) {
			WorldGridIndex idx;
			idx.level = p_level;
			idx.x = x;
			idx.z = z;
			if (chunks.has(idx)) {
				result.push_back(idx);
			}
		}
	}
	
	return result;
}

WorldPartitionGrid::WorldPartitionGrid() {
}

WorldPartitionGrid::~WorldPartitionGrid() {
}
