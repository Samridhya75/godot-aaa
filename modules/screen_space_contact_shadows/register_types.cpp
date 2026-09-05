#include "register_types.h"

#include "contact_shadows_3d.h"
#include "contact_shadows_effect.h"
#include "core/object/class_db.h"

void initialize_screen_space_contact_shadows_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		ClassDB::register_class<ContactShadowsEffect>();
		ClassDB::register_class<ContactShadows3D>();
	}
}

void uninitialize_screen_space_contact_shadows_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
