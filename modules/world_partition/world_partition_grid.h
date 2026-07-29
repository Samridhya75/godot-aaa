#ifndef WORLD_PARTITION_GRID_H
#define WORLD_PARTITION_GRID_H

#include "core/io/resource.h"

class WorldPartitionGrid : public Resource {
	GDCLASS(WorldPartitionGrid, Resource);

protected:
	static void _bind_methods();

public:
	WorldPartitionGrid();
	~WorldPartitionGrid();
};

#endif // WORLD_PARTITION_GRID_H
