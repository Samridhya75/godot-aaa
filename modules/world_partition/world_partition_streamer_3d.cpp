#include "world_partition_streamer_3d.h"
#include "core/object/class_db.h"
#include "core/config/engine.h"
#include "scene/main/scene_tree.h"

void WorldPartitionStreamer3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_streaming_radius", "radius"), &WorldPartitionStreamer3D::set_streaming_radius);
	ClassDB::bind_method(D_METHOD("get_streaming_radius"), &WorldPartitionStreamer3D::get_streaming_radius);

	ClassDB::bind_method(D_METHOD("set_hlod_ranges", "ranges"), &WorldPartitionStreamer3D::set_hlod_ranges);
	ClassDB::bind_method(D_METHOD("get_hlod_ranges"), &WorldPartitionStreamer3D::get_hlod_ranges);

	ClassDB::bind_method(D_METHOD("set_prediction_time", "time"), &WorldPartitionStreamer3D::set_prediction_time);
	ClassDB::bind_method(D_METHOD("get_prediction_time"), &WorldPartitionStreamer3D::get_prediction_time);

	ClassDB::bind_method(D_METHOD("get_velocity"), &WorldPartitionStreamer3D::get_velocity);
	ClassDB::bind_method(D_METHOD("get_predicted_bounds", "radius"), &WorldPartitionStreamer3D::get_predicted_bounds);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "streaming_radius", PROPERTY_HINT_RANGE, "0.0,10000.0,0.1"), "set_streaming_radius", "get_streaming_radius");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "hlod_ranges"), "set_hlod_ranges", "get_hlod_ranges");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "prediction_time", PROPERTY_HINT_RANGE, "0.0,10.0,0.1"), "set_prediction_time", "get_prediction_time");
}

void WorldPartitionStreamer3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				set_physics_process_internal(true);
				last_position = get_global_position();
				
				if (is_inside_tree()) {
					get_tree()->call_group("world_partition_manager", "register_streamer", this);
				}
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				if (is_inside_tree()) {
					get_tree()->call_group("world_partition_manager", "unregister_streamer", this);
				}
			}
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			Vector3 current_pos = get_global_position();
			float delta = get_physics_process_delta_time();
			if (delta > 0.0) {
				Vector3 diff = current_pos - last_position;
				if (diff.length_squared() > 10000.0) { 
					current_velocity = Vector3();
				} else {
					current_velocity = diff / delta;
				}
			}
			last_position = current_pos;
		} break;
	}
}

void WorldPartitionStreamer3D::set_streaming_radius(float p_radius) {
	streaming_radius = p_radius;
}

float WorldPartitionStreamer3D::get_streaming_radius() const {
	return streaming_radius;
}

void WorldPartitionStreamer3D::set_hlod_ranges(const PackedFloat32Array &p_ranges) {
	hlod_ranges = p_ranges;
}

PackedFloat32Array WorldPartitionStreamer3D::get_hlod_ranges() const {
	return hlod_ranges;
}

void WorldPartitionStreamer3D::set_prediction_time(float p_time) {
	prediction_time = p_time;
}

float WorldPartitionStreamer3D::get_prediction_time() const {
	return prediction_time;
}

Vector3 WorldPartitionStreamer3D::get_velocity() const {
	return current_velocity;
}

AABB WorldPartitionStreamer3D::get_predicted_bounds(float p_radius) const {
	Vector3 pos = get_global_position();
	AABB bounds(pos - Vector3(p_radius, p_radius, p_radius), Vector3(p_radius * 2, p_radius * 2, p_radius * 2));
	
	if (prediction_time > 0.0 && current_velocity.length_squared() > 0.1) {
		Vector3 predicted_pos = pos + (current_velocity * prediction_time);
		bounds.expand_to(predicted_pos - Vector3(p_radius, p_radius, p_radius));
		bounds.expand_to(predicted_pos + Vector3(p_radius, p_radius, p_radius));
	}
	
	return bounds;
}

WorldPartitionStreamer3D::WorldPartitionStreamer3D() {
	hlod_ranges.push_back(200.0);
	hlod_ranges.push_back(400.0);
}

WorldPartitionStreamer3D::~WorldPartitionStreamer3D() {
}
