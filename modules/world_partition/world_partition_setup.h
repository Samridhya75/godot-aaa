#ifndef WORLD_PARTITION_SETUP_H
#define WORLD_PARTITION_SETUP_H

#include "scene/3d/node_3d.h"

class WorldPartitionSetup : public Node3D {
	GDCLASS(WorldPartitionSetup, Node3D);

private:
	float cell_size = 100.0;
	Color grid_color = Color(0.5, 0.5, 1.0, 0.5);
	int grid_radius = 50;

protected:
	static void _bind_methods();

public:
	void set_cell_size(float p_size);
	float get_cell_size() const;

	void set_grid_color(const Color &p_color);
	Color get_grid_color() const;

	void set_grid_radius(int p_radius);
	int get_grid_radius() const;

	WorldPartitionSetup();
	~WorldPartitionSetup();
};

#endif // WORLD_PARTITION_SETUP_H
