#include "world_partition_setup.h"
#include "core/object/class_db.h"

void WorldPartitionSetup::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_cell_size", "size"), &WorldPartitionSetup::set_cell_size);
	ClassDB::bind_method(D_METHOD("get_cell_size"), &WorldPartitionSetup::get_cell_size);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_size"), "set_cell_size", "get_cell_size");

	ClassDB::bind_method(D_METHOD("set_grid_color", "color"), &WorldPartitionSetup::set_grid_color);
	ClassDB::bind_method(D_METHOD("get_grid_color"), &WorldPartitionSetup::get_grid_color);
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "grid_color"), "set_grid_color", "get_grid_color");

	ClassDB::bind_method(D_METHOD("set_grid_radius", "radius"), &WorldPartitionSetup::set_grid_radius);
	ClassDB::bind_method(D_METHOD("get_grid_radius"), &WorldPartitionSetup::get_grid_radius);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "grid_radius"), "set_grid_radius", "get_grid_radius");
}

void WorldPartitionSetup::set_cell_size(float p_size) {
	cell_size = p_size;
	update_gizmos();
}

float WorldPartitionSetup::get_cell_size() const {
	return cell_size;
}

void WorldPartitionSetup::set_grid_color(const Color &p_color) {
	grid_color = p_color;
	update_gizmos();
}

Color WorldPartitionSetup::get_grid_color() const {
	return grid_color;
}

void WorldPartitionSetup::set_grid_radius(int p_radius) {
	grid_radius = p_radius;
	update_gizmos();
}

int WorldPartitionSetup::get_grid_radius() const {
	return grid_radius;
}

WorldPartitionSetup::WorldPartitionSetup() {
}

WorldPartitionSetup::~WorldPartitionSetup() {
}
