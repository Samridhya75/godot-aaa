#ifdef TOOLS_ENABLED
#include "world_partition_setup_gizmo_plugin.h"
#include "world_partition_setup.h"
#include "core/object/class_db.h"
#include "scene/resources/material.h"

WorldPartitionSetupGizmoPlugin::WorldPartitionSetupGizmoPlugin() {
}
bool WorldPartitionSetupGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<WorldPartitionSetup>(p_spatial) != nullptr;
}

String WorldPartitionSetupGizmoPlugin::get_gizmo_name() const {
	return "WorldPartitionSetup";
}

int WorldPartitionSetupGizmoPlugin::get_priority() const {
	return -1;
}

void WorldPartitionSetupGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	p_gizmo->clear();

	WorldPartitionSetup *setup = Object::cast_to<WorldPartitionSetup>(p_gizmo->get_node_3d());
	if (!setup) {
		return;
	}

	float cell_size = setup->get_cell_size();
	int radius = setup->get_grid_radius();
	Color color = setup->get_grid_color();

	Ref<StandardMaterial3D> mat = memnew(StandardMaterial3D);
	mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	mat->set_albedo(color);
	mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	mat->set_cull_mode(StandardMaterial3D::CULL_DISABLED);

	Vector<Vector3> lines;
	
	float extent = cell_size * radius;
	
	for (int z = -radius; z <= radius; z++) {
		lines.push_back(Vector3(-extent, 0, z * cell_size));
		lines.push_back(Vector3(extent, 0, z * cell_size));
	}
	
	for (int x = -radius; x <= radius; x++) {
		lines.push_back(Vector3(x * cell_size, 0, -extent));
		lines.push_back(Vector3(x * cell_size, 0, extent));
	}

	p_gizmo->add_lines(lines, mat);
}

#endif // TOOLS_ENABLED
