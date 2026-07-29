#ifndef WORLD_PARTITION_MANAGER_H
#define WORLD_PARTITION_MANAGER_H

#include "scene/3d/node_3d.h"

class WorldPartitionManager : public Node3D {
	GDCLASS(WorldPartitionManager, Node3D);

protected:
	static void _bind_methods();

public:
	WorldPartitionManager();
	~WorldPartitionManager();
};

#endif // WORLD_PARTITION_MANAGER_H
