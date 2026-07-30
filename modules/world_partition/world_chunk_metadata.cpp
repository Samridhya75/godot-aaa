#include "world_chunk_metadata.h"
#include "core/object/class_db.h"

void WorldChunkMetadata::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_item", "asset_path", "transform", "is_occluder"), &WorldChunkMetadata::add_item, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("remove_item", "index"), &WorldChunkMetadata::remove_item);
	ClassDB::bind_method(D_METHOD("clear_items"), &WorldChunkMetadata::clear_items);
	ClassDB::bind_method(D_METHOD("get_item_count"), &WorldChunkMetadata::get_item_count);
	
	ClassDB::bind_method(D_METHOD("get_item_asset_path", "index"), &WorldChunkMetadata::get_item_asset_path);
	ClassDB::bind_method(D_METHOD("set_item_asset_path", "index", "asset_path"), &WorldChunkMetadata::set_item_asset_path);
	
	ClassDB::bind_method(D_METHOD("get_item_transform", "index"), &WorldChunkMetadata::get_item_transform);
	ClassDB::bind_method(D_METHOD("set_item_transform", "index", "transform"), &WorldChunkMetadata::set_item_transform);

	ClassDB::bind_method(D_METHOD("is_item_occluder", "index"), &WorldChunkMetadata::is_item_occluder);
	ClassDB::bind_method(D_METHOD("set_item_occluder", "index", "is_occluder"), &WorldChunkMetadata::set_item_occluder);

	ClassDB::bind_method(D_METHOD("set_items_data", "data"), &WorldChunkMetadata::set_items_data);
	ClassDB::bind_method(D_METHOD("get_items_data"), &WorldChunkMetadata::get_items_data);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "items_data"), "set_items_data", "get_items_data");
}

void WorldChunkMetadata::add_item(const String &p_path, const Transform3D &p_transform, bool p_is_occluder) {
	PlacedItem item;
	item.asset_path = p_path;
	item.transform = p_transform;
	item.is_occluder = p_is_occluder;
	items.push_back(item);
	emit_changed();
}

void WorldChunkMetadata::remove_item(int p_index) {
	ERR_FAIL_INDEX(p_index, items.size());
	items.remove_at(p_index);
	emit_changed();
}

void WorldChunkMetadata::clear_items() {
	items.clear();
	emit_changed();
}

int WorldChunkMetadata::get_item_count() const {
	return items.size();
}

String WorldChunkMetadata::get_item_asset_path(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, items.size(), String());
	return items[p_index].asset_path;
}

void WorldChunkMetadata::set_item_asset_path(int p_index, const String &p_path) {
	ERR_FAIL_INDEX(p_index, items.size());
	items.write[p_index].asset_path = p_path;
	emit_changed();
}

Transform3D WorldChunkMetadata::get_item_transform(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, items.size(), Transform3D());
	return items[p_index].transform;
}

void WorldChunkMetadata::set_item_transform(int p_index, const Transform3D &p_transform) {
	ERR_FAIL_INDEX(p_index, items.size());
	items.write[p_index].transform = p_transform;
	emit_changed();
}

bool WorldChunkMetadata::is_item_occluder(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, items.size(), false);
	return items[p_index].is_occluder;
}

void WorldChunkMetadata::set_item_occluder(int p_index, bool p_is_occluder) {
	ERR_FAIL_INDEX(p_index, items.size());
	items.write[p_index].is_occluder = p_is_occluder;
	emit_changed();
}

void WorldChunkMetadata::set_items_data(const Array &p_data) {
	items.clear();
	for (int i = 0; i < p_data.size(); i++) {
		Dictionary d = p_data[i];
		PlacedItem item;
		if (d.has("path")) {
			item.asset_path = d["path"];
		}
		if (d.has("transform")) {
			item.transform = d["transform"];
		}
		if (d.has("is_occluder")) {
			item.is_occluder = d["is_occluder"];
		}
		items.push_back(item);
	}
	emit_changed();
}

Array WorldChunkMetadata::get_items_data() const {
	Array data;
	for (int i = 0; i < items.size(); i++) {
		Dictionary d;
		d["path"] = items[i].asset_path;
		d["transform"] = items[i].transform;
		d["is_occluder"] = items[i].is_occluder;
		data.push_back(d);
	}
	return data;
}

WorldChunkMetadata::WorldChunkMetadata() {
}

WorldChunkMetadata::~WorldChunkMetadata() {
}
