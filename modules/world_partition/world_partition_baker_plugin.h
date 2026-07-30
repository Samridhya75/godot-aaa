#ifndef WORLD_PARTITION_BAKER_PLUGIN_H
#define WORLD_PARTITION_BAKER_PLUGIN_H

#ifdef TOOLS_ENABLED

#include "editor/plugins/editor_plugin.h"
#include "scene/gui/button.h"

class WorldPartitionBakerPlugin : public EditorPlugin {
	GDCLASS(WorldPartitionBakerPlugin, EditorPlugin);

private:
	Button *bake_button;

	void _bake_scene();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "WorldPartitionBaker"; }
	bool has_main_screen() const override { return false; }

	WorldPartitionBakerPlugin();
	~WorldPartitionBakerPlugin();
};

#endif // TOOLS_ENABLED

#endif // WORLD_PARTITION_BAKER_PLUGIN_H
