#ifndef WORLD_PARTITION_STREAMER_3D_H
#define WORLD_PARTITION_STREAMER_3D_H

#include "scene/3d/node_3d.h"

class WorldPartitionStreamer3D : public Node3D {
	GDCLASS(WorldPartitionStreamer3D, Node3D);

private:
	float streaming_radius = 100.0;
	float prediction_time = 2.0; // Seconds to predict ahead based on velocity
	Vector3 current_velocity;
	Vector3 last_position;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_streaming_radius(float p_radius);
	float get_streaming_radius() const;

	void set_prediction_time(float p_time);
	float get_prediction_time() const;

	Vector3 get_velocity() const;
	AABB get_predicted_bounds() const;

	WorldPartitionStreamer3D();
	~WorldPartitionStreamer3D();
};

#endif // WORLD_PARTITION_STREAMER_3D_H
