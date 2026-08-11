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
#include "scene/resources/packed_scene.h"

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
	HashMap<WorldGridIndex, Node3D*, WorldGridIndexHasher> chunk_roots;

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

			if (!chunk_roots.has(idx)) {
				Node3D *chunk_root = memnew(Node3D);
				chunk_root->set_name(vformat("Chunk_%d_%d", idx.x, idx.z));
				chunk_roots[idx] = chunk_root;
			}
			
			// Duplicate the node
			Node *dup = current->duplicate();
			// Strip out any children that got duplicated automatically
			for (int i = dup->get_child_count() - 1; i >= 0; i--) {
				Node *c = dup->get_child(i);
				dup->remove_child(c);
				memdelete(c);
			}

			// Add to chunk root
			chunk_roots[idx]->add_child(dup);
			dup->set_owner(chunk_roots[idx]);
			
			// Update transform to be relative to the chunk root (identity since we bake in world space for now)
			// Wait, the chunk root is at (0,0,0), so global_xform is perfect!
			Node3D *dup_spatial = Object::cast_to<Node3D>(dup);
			if (dup_spatial) {
				dup_spatial->set_transform(global_xform);
			}
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

	for (const KeyValue<WorldGridIndex, Node3D*> &E : chunk_roots) {
		// Save the packed scene
		Ref<PackedScene> packed_scene;
		packed_scene.instantiate();
		packed_scene->pack(E.value);
		String scene_filename = vformat("res://world_partition_data/chunk_%d_%d.scn", E.key.x, E.key.z);
		ResourceSaver::save(packed_scene, scene_filename);

		// Store just the scene path in the metadata
		Ref<WorldChunkMetadata> meta;
		meta.instantiate();
		meta->add_item(scene_filename, Transform3D(), false);
		
		String meta_filename = vformat("res://world_partition_data/chunk_%d_%d.res", E.key.x, E.key.z);
		ResourceSaver::save(meta, meta_filename);
		
		main_grid->set_chunk(E.key.level, E.key.x, E.key.z, meta);
		
		// Clean up the temporary node tree
		memdelete(E.value);
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
