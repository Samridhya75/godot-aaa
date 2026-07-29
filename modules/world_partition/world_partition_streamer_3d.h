#ifndef WORLD_PARTITION_STREAMER_3D_H
#define WORLD_PARTITION_STREAMER_3D_H

#include "scene/3d/node_3d.h"

class WorldPartitionStreamer3D : public Node3D {
	GDCLASS(WorldPartitionStreamer3D, Node3D);

protected:
	static void _bind_methods();

public:
	WorldPartitionStreamer3D();
	~WorldPartitionStreamer3D();
};

#endif // WORLD_PARTITION_STREAMER_3D_H
