#include "servers/rendering/rendering_server.h"
#include "scene/resources/multimesh.h"
void test_indirect() {
    Ref<MultiMesh> mm;
    mm.instantiate();
    mm->set_instance_count(10);
    RenderingServer::get_singleton()->multimesh_allocate_data(mm->get_rid(), 10, RenderingServer::MULTIMESH_TRANSFORM_3D, false, false, true);
}
