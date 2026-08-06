#include "world_partition_baker_plugin.h"

#ifdef TOOLS_ENABLED

#include "editor/editor_interface.h"
#include "world_partition_setup_gizmo_plugin.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/occluder_instance_3d.h"
#include "core/io/resource_saver.h"
#include "core/io/dir_access.h"
#include "world_chunk_metadata.h"
#include "world_partition_grid.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

void WorldPartitionBakerPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_bake_scene"), &WorldPartitionBakerPlugin::_bake_scene);
}

void WorldPartitionBakerPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		bake_button = memnew(Button);
		bake_button->set_text("Bake World Partition");
		bake_button->connect("pressed", callable_mp(this, &WorldPartitionBakerPlugin::_bake_scene));
		add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, bake_button);
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		remove_control_from_container(CONTAINER_SPATIAL_EDITOR_MENU, bake_button);
		memdelete(bake_button);
	}
}

void WorldPartitionBakerPlugin::_bake_scene() {
	Node *root = EditorInterface::get_singleton()->get_edited_scene_root();
	if (!root) {
		ERR_PRINT("No scene root to bake!");
		return;
	}

	float cell_size = 100.0;
	HashMap<WorldGridIndex, Ref<WorldChunkMetadata>, WorldGridIndexHasher> baked_chunks;

	List<Node *> stack;
	stack.push_back(root);

	while (!stack.is_empty()) {
		Node *current = stack.back()->get();
		stack.pop_back();

		MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(current);
		OccluderInstance3D *occluder_instance = Object::cast_to<OccluderInstance3D>(current);

		if (mesh_instance || occluder_instance) {
			Node3D *spatial = Object::cast_to<Node3D>(current);
			Transform3D global_xform = spatial->get_global_transform();
			
			int grid_x = Math::floor(global_xform.origin.x / cell_size);
			int grid_z = Math::floor(global_xform.origin.z / cell_size);

			WorldGridIndex idx;
			idx.level = 0;
			idx.x = grid_x;
			idx.z = grid_z;

			if (!baked_chunks.has(idx)) {
				Ref<WorldChunkMetadata> new_chunk;
				new_chunk.instantiate();
				baked_chunks[idx] = new_chunk;
			}

			// Dummy asset path for skeleton. In real system, this extracts mesh paths.
			String asset_path = "res://dummy_asset_" + current->get_name() + ".res";
			bool is_occluder = (occluder_instance != nullptr);
			
			baked_chunks[idx]->add_item(asset_path, global_xform, is_occluder);
		}

		for (int i = 0; i < current->get_child_count(); i++) {
			stack.push_back(current->get_child(i));
		}
	}

	// Save to disk
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (!da->dir_exists("res://world_partition_data")) {
		da->make_dir("res://world_partition_data");
	}

	Ref<WorldPartitionGrid> main_grid;
	main_grid.instantiate();
	main_grid->set_cell_size(cell_size);

	for (const KeyValue<WorldGridIndex, Ref<WorldChunkMetadata>> &E : baked_chunks) {
		String filename = vformat("res://world_partition_data/chunk_%d_%d.res", E.key.x, E.key.z);
		ResourceSaver::save(E.value, filename);
		
		main_grid->set_chunk(E.key.level, E.key.x, E.key.z, E.value);
	}

	ResourceSaver::save(main_grid, "res://world_partition_data/main_grid.res");
	print_line("World Partition Baking Complete! Grid saved to res://world_partition_data/main_grid.res");
}

WorldPartitionBakerPlugin::WorldPartitionBakerPlugin() {
	gizmo_plugin.instantiate();
	Node3DEditor::get_singleton()->add_gizmo_plugin(gizmo_plugin);
}

WorldPartitionBakerPlugin::~WorldPartitionBakerPlugin() {
}

#endif // TOOLS_ENABLED
