#ifndef WORLD_CHUNK_METADATA_H
#define WORLD_CHUNK_METADATA_H

#include "core/io/resource.h"
#include "core/math/transform_3d.h"
#include "core/templates/vector.h"

class WorldChunkMetadata : public Resource {
	GDCLASS(WorldChunkMetadata, Resource);

private:
	struct PlacedItem {
		String asset_path;
		Transform3D transform;
		bool is_occluder = false;
	};
	Vector<PlacedItem> items;

protected:
	static void _bind_methods();

public:
	void add_item(const String &p_path, const Transform3D &p_transform, bool p_is_occluder = false);
	void remove_item(int p_index);
	void clear_items();
	int get_item_count() const;
	
	String get_item_asset_path(int p_index) const;
	void set_item_asset_path(int p_index, const String &p_path);
	
	Transform3D get_item_transform(int p_index) const;
	void set_item_transform(int p_index, const Transform3D &p_transform);

	bool is_item_occluder(int p_index) const;
	void set_item_occluder(int p_index, bool p_is_occluder);

	// Helpers for Godot Editor serialization (Array of Dictionaries)
	void set_items_data(const Array &p_data);
	Array get_items_data() const;

	WorldChunkMetadata();
	~WorldChunkMetadata();
};

#endif // WORLD_CHUNK_METADATA_H
