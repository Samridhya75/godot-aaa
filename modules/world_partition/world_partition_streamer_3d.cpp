#include "world_partition_streamer_3d.h"
#include "core/object/class_db.h"
#include "core/config/engine.h"

void WorldPartitionStreamer3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_streaming_radius", "radius"), &WorldPartitionStreamer3D::set_streaming_radius);
	ClassDB::bind_method(D_METHOD("get_streaming_radius"), &WorldPartitionStreamer3D::get_streaming_radius);

	ClassDB::bind_method(D_METHOD("set_prediction_time", "time"), &WorldPartitionStreamer3D::set_prediction_time);
	ClassDB::bind_method(D_METHOD("get_prediction_time"), &WorldPartitionStreamer3D::get_prediction_time);

	ClassDB::bind_method(D_METHOD("get_velocity"), &WorldPartitionStreamer3D::get_velocity);
	ClassDB::bind_method(D_METHOD("get_predicted_bounds"), &WorldPartitionStreamer3D::get_predicted_bounds);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "streaming_radius", PROPERTY_HINT_RANGE, "0.0,10000.0,0.1"), "set_streaming_radius", "get_streaming_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "prediction_time", PROPERTY_HINT_RANGE, "0.0,10.0,0.1"), "set_prediction_time", "get_prediction_time");
}

void WorldPartitionStreamer3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				set_physics_process_internal(true);
				last_position = get_global_position();
			}
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			Vector3 current_pos = get_global_position();
			float delta = get_physics_process_delta_time();
			if (delta > 0.0) {
				current_velocity = (current_pos - last_position) / delta;
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

void WorldPartitionStreamer3D::set_prediction_time(float p_time) {
	prediction_time = p_time;
}

float WorldPartitionStreamer3D::get_prediction_time() const {
	return prediction_time;
}

Vector3 WorldPartitionStreamer3D::get_velocity() const {
	return current_velocity;
}

AABB WorldPartitionStreamer3D::get_predicted_bounds() const {
	Vector3 pos = get_global_position();
	AABB bounds(pos - Vector3(streaming_radius, streaming_radius, streaming_radius), Vector3(streaming_radius * 2, streaming_radius * 2, streaming_radius * 2));
	
	if (prediction_time > 0.0 && current_velocity.length_squared() > 0.1) {
		Vector3 predicted_pos = pos + (current_velocity * prediction_time);
		bounds.expand_to(predicted_pos - Vector3(streaming_radius, streaming_radius, streaming_radius));
		bounds.expand_to(predicted_pos + Vector3(streaming_radius, streaming_radius, streaming_radius));
	}
	
	return bounds;
}

WorldPartitionStreamer3D::WorldPartitionStreamer3D() {
}

WorldPartitionStreamer3D::~WorldPartitionStreamer3D() {
}
