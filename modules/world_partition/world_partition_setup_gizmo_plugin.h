#ifndef WORLD_PARTITION_SETUP_GIZMO_PLUGIN_H
#define WORLD_PARTITION_SETUP_GIZMO_PLUGIN_H

#ifdef TOOLS_ENABLED

#include "editor/scene/3d/node_3d_editor_gizmos.h"

class WorldPartitionSetupGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(WorldPartitionSetupGizmoPlugin, EditorNode3DGizmoPlugin);

public:
	bool has_gizmo(Node3D *p_spatial) override;
	String get_gizmo_name() const override;
	int get_priority() const override;
	void redraw(EditorNode3DGizmo *p_gizmo) override;

	WorldPartitionSetupGizmoPlugin();
};

#endif // TOOLS_ENABLED

#endif // WORLD_PARTITION_SETUP_GIZMO_PLUGIN_H
