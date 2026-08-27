#include "world_partition_baker_plugin.h"

#ifdef TOOLS_ENABLED

#include "editor/editor_interface.h"
#include "world_partition_setup_gizmo_plugin.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/occluder_instance_3d.h"
#include "scene/resources/surface_tool.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
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
		bake_button->set_text("Bake World Partition (HLOD)");
		bake_button->connect("pressed", callable_mp(this, &WorldPartitionBakerPlugin::_bake_scene));
		add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, bake_button);
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		remove_control_from_container(CONTAINER_SPATIAL_EDITOR_MENU, bake_button);
		memdelete(bake_button);
	}
}

static void _merge_mesh_recursive(Node3D *p_node, const Transform3D &p_transform, HashMap<Ref<Material>, Ref<SurfaceTool>> &r_surfaces) {
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_node);
	if (mi && mi->get_mesh().is_valid()) {
		Ref<Mesh> mesh = mi->get_mesh();
		Transform3D xform = p_transform * mi->get_transform();

		for (int i = 0; i < mesh->get_surface_count(); i++) {
			Ref<Material> mat = mi->get_surface_override_material(i);
			if (mat.is_null()) mat = mesh->surface_get_material(i);

			if (!r_surfaces.has(mat)) {
				Ref<SurfaceTool> st;
				st.instantiate();
				st->begin(Mesh::PRIMITIVE_TRIANGLES);
				r_surfaces[mat] = st;
			}
			r_surfaces[mat]->append_from(mesh, i, xform);
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node3D *child = Object::cast_to<Node3D>(p_node->get_child(i));
		if (child) {
			_merge_mesh_recursive(child, p_transform * p_node->get_transform(), r_surfaces);
		}
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
				chunk_root->set_name(vformat("Chunk_L0_%d_%d", idx.x, idx.z));
				chunk_roots[idx] = chunk_root;
			}
			
			Node *dup = current->duplicate();
			for (int i = dup->get_child_count() - 1; i >= 0; i--) {
				Node *c = dup->get_child(i);
				dup->remove_child(c);
				memdelete(c);
			}

			chunk_roots[idx]->add_child(dup);
			dup->set_owner(chunk_roots[idx]);
			
			Node3D *dup_spatial = Object::cast_to<Node3D>(dup);
			if (dup_spatial) {
				dup_spatial->set_transform(global_xform);
			}
		}

		for (int i = 0; i < current->get_child_count(); i++) {
			stack.push_back(current->get_child(i));
		}
	}

	Ref<WorldPartitionGrid> main_grid;
	main_grid.instantiate();
	main_grid->set_cell_size(cell_size);

	int max_hlod_levels = 2; // Generate Level 1 and Level 2
	HashMap<WorldGridIndex, Node3D*, WorldGridIndexHasher> current_level_chunks;
	for (const KeyValue<WorldGridIndex, Node3D*> &E : chunk_roots) {
		current_level_chunks[E.key] = E.value;
	}

	for (int level = 1; level <= max_hlod_levels; level++) {
		HashMap<WorldGridIndex, Node3D*, WorldGridIndexHasher> next_level_chunks;

		for (const KeyValue<WorldGridIndex, Node3D*> &E : current_level_chunks) {
			WorldGridIndex next_idx;
			next_idx.level = level;
			next_idx.x = Math::floor((float)E.key.x / 2.0f);
			next_idx.z = Math::floor((float)E.key.z / 2.0f);

			if (!next_level_chunks.has(next_idx)) {
				Node3D *hlod_root = memnew(Node3D);
				hlod_root->set_name(vformat("Chunk_L%d_%d_%d", level, next_idx.x, next_idx.z));
				next_level_chunks[next_idx] = hlod_root;
				chunk_roots[next_idx] = hlod_root;
			}
		}

		for (const KeyValue<WorldGridIndex, Node3D*> &E : next_level_chunks) {
			HashMap<Ref<Material>, Ref<SurfaceTool>> material_surfaces;
			for (const KeyValue<WorldGridIndex, Node3D*> &child_chunk : current_level_chunks) {
				int target_x = Math::floor((float)child_chunk.key.x / 2.0f);
				int target_z = Math::floor((float)child_chunk.key.z / 2.0f);
				if (target_x == E.key.x && target_z == E.key.z) {
					Transform3D hlod_offset;
					_merge_mesh_recursive(child_chunk.value, hlod_offset, material_surfaces);
				}
			}
			Ref<ArrayMesh> merged_mesh;
			merged_mesh.instantiate();
			int surface_idx = 0;
			for (const KeyValue<Ref<Material>, Ref<SurfaceTool>> &surf : material_surfaces) {
				surf.value->commit(merged_mesh);
				merged_mesh->surface_set_material(surface_idx, surf.key);
				surface_idx++;
			}
			if (merged_mesh->get_surface_count() > 0) {
				MeshInstance3D *mi = memnew(MeshInstance3D);
				mi->set_name("HLOD_Mesh");
				mi->set_mesh(merged_mesh);
				E.value->add_child(mi);
				mi->set_owner(E.value);
			}
		}
		current_level_chunks.clear();
		for (const KeyValue<WorldGridIndex, Node3D*> &E : next_level_chunks) {
			current_level_chunks[E.key] = E.value;
		}
	}

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (!da->dir_exists("res://world_partition_data")) {
		da->make_dir("res://world_partition_data");
	}

	for (const KeyValue<WorldGridIndex, Node3D*> &E : chunk_roots) {
		Ref<PackedScene> packed_scene;
		packed_scene.instantiate();
		packed_scene->pack(E.value);
		String scene_filename = vformat("res://world_partition_data/chunk_l%d_%d_%d.scn", E.key.level, E.key.x, E.key.z);
		ResourceSaver::save(packed_scene, scene_filename);

		Ref<WorldChunkMetadata> meta;
		meta.instantiate();
		meta->add_item(scene_filename, Transform3D(), false);
		
		String meta_filename = vformat("res://world_partition_data/chunk_l%d_%d_%d.res", E.key.level, E.key.x, E.key.z);
		ResourceSaver::save(meta, meta_filename);
		
		main_grid->set_chunk(E.key.level, E.key.x, E.key.z, meta);
		memdelete(E.value);
	}

	ResourceSaver::save(main_grid, "res://world_partition_data/main_grid.res");
	print_line("World Partition Baking Complete! Grid saved to res://world_partition_data/main_grid.res");
}

WorldPartitionBakerPlugin::WorldPartitionBakerPlugin() {
	gizmo_plugin.instantiate();
	Node3DEditor::get_singleton()->add_gizmo_plugin(gizmo_plugin);
}

WorldPartitionBakerPlugin::~WorldPartitionBakerPlugin() {}

#endif // TOOLS_ENABLED
