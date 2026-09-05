#include "contact_shadows_3d.h"

#include "core/object/class_db.h"
#include "scene/main/viewport.h"
#include "scene/resources/3d/world_3d.h"

void ContactShadows3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &ContactShadows3D::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &ContactShadows3D::is_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");

	ClassDB::bind_method(D_METHOD("set_max_distance", "distance"), &ContactShadows3D::set_max_distance);
	ClassDB::bind_method(D_METHOD("get_max_distance"), &ContactShadows3D::get_max_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_distance", PROPERTY_HINT_RANGE, "0.01,2.0,0.01,suffix:m"), "set_max_distance", "get_max_distance");

	ClassDB::bind_method(D_METHOD("set_thickness", "thickness"), &ContactShadows3D::set_thickness);
	ClassDB::bind_method(D_METHOD("get_thickness"), &ContactShadows3D::get_thickness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "thickness", PROPERTY_HINT_RANGE, "0.001,0.5,0.001,suffix:m"), "set_thickness", "get_thickness");

	ClassDB::bind_method(D_METHOD("set_ray_steps", "steps"), &ContactShadows3D::set_ray_steps);
	ClassDB::bind_method(D_METHOD("get_ray_steps"), &ContactShadows3D::get_ray_steps);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ray_steps", PROPERTY_HINT_RANGE, "4,64,1"), "set_ray_steps", "get_ray_steps");

	ClassDB::bind_method(D_METHOD("set_shadow_intensity", "intensity"), &ContactShadows3D::set_shadow_intensity);
	ClassDB::bind_method(D_METHOD("get_shadow_intensity"), &ContactShadows3D::get_shadow_intensity);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "shadow_intensity", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_shadow_intensity", "get_shadow_intensity");

	ClassDB::bind_method(D_METHOD("set_normal_bias", "bias"), &ContactShadows3D::set_normal_bias);
	ClassDB::bind_method(D_METHOD("get_normal_bias"), &ContactShadows3D::get_normal_bias);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "normal_bias", PROPERTY_HINT_RANGE, "0.0,0.1,0.001,suffix:m"), "set_normal_bias", "get_normal_bias");

	ClassDB::bind_method(D_METHOD("set_custom_light_direction", "direction"), &ContactShadows3D::set_custom_light_direction);
	ClassDB::bind_method(D_METHOD("get_custom_light_direction"), &ContactShadows3D::get_custom_light_direction);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "custom_light_direction"), "set_custom_light_direction", "get_custom_light_direction");

	ClassDB::bind_method(D_METHOD("get_effect"), &ContactShadows3D::get_effect);
	ClassDB::bind_method(D_METHOD("get_active_compositor"), &ContactShadows3D::get_active_compositor);
}

void ContactShadows3D::_update_effect_properties() {
	if (effect.is_null()) {
		return;
	}
	effect->set_enabled(enabled);
	effect->set_max_distance(max_distance);
	effect->set_thickness(thickness);
	effect->set_ray_steps(ray_steps);
	effect->set_shadow_intensity(shadow_intensity);
	effect->set_normal_bias(normal_bias);
	effect->set_custom_light_direction(custom_light_direction);
}

void ContactShadows3D::set_enabled(bool p_enabled) {
	enabled = p_enabled;
	if (effect.is_valid()) {
		effect->set_enabled(enabled);
	}
}

bool ContactShadows3D::is_enabled() const {
	return enabled;
}

void ContactShadows3D::set_max_distance(float p_dist) {
	max_distance = MAX(0.01f, p_dist);
	if (effect.is_valid()) {
		effect->set_max_distance(max_distance);
	}
}

float ContactShadows3D::get_max_distance() const {
	return max_distance;
}

void ContactShadows3D::set_thickness(float p_thickness) {
	thickness = MAX(0.001f, p_thickness);
	if (effect.is_valid()) {
		effect->set_thickness(thickness);
	}
}

float ContactShadows3D::get_thickness() const {
	return thickness;
}

void ContactShadows3D::set_ray_steps(int p_steps) {
	ray_steps = CLAMP(p_steps, 4, 64);
	if (effect.is_valid()) {
		effect->set_ray_steps(ray_steps);
	}
}

int ContactShadows3D::get_ray_steps() const {
	return ray_steps;
}

void ContactShadows3D::set_shadow_intensity(float p_intensity) {
	shadow_intensity = CLAMP(p_intensity, 0.0f, 1.0f);
	if (effect.is_valid()) {
		effect->set_shadow_intensity(shadow_intensity);
	}
}

float ContactShadows3D::get_shadow_intensity() const {
	return shadow_intensity;
}

void ContactShadows3D::set_normal_bias(float p_bias) {
	normal_bias = MAX(0.0f, p_bias);
	if (effect.is_valid()) {
		effect->set_normal_bias(normal_bias);
	}
}

float ContactShadows3D::get_normal_bias() const {
	return normal_bias;
}

void ContactShadows3D::set_custom_light_direction(const Vector3 &p_dir) {
	custom_light_direction = p_dir;
	if (effect.is_valid()) {
		effect->set_custom_light_direction(custom_light_direction);
	}
}

Vector3 ContactShadows3D::get_custom_light_direction() const {
	return custom_light_direction;
}

void ContactShadows3D::_attach_to_compositor() {
	Ref<World3D> world = is_inside_world() ? get_world_3d() : Ref<World3D>();
	if (world.is_null() && get_viewport() != nullptr) {
		world = get_viewport()->find_world_3d();
	}
	if (world.is_valid()) {
		active_compositor = world->get_compositor();
		if (active_compositor.is_null()) {
			active_compositor.instantiate();
			world->set_compositor(active_compositor);
		}

		Array effects = active_compositor->get_compositor_effects();
		if (!effects.has(effect)) {
			effects.push_back(effect);
			active_compositor->set_compositor_effects(effects);
		}
	}
}

void ContactShadows3D::_detach_from_compositor() {
	if (active_compositor.is_valid()) {
		Array effects = active_compositor->get_compositor_effects();
		int idx = effects.find(effect);
		if (idx != -1) {
			effects.remove_at(idx);
			active_compositor->set_compositor_effects(effects);
		}
		active_compositor.unref();
	}
}

void ContactShadows3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD:
		case NOTIFICATION_ENTER_TREE: {
			_update_effect_properties();
			_attach_to_compositor();
		} break;
		case NOTIFICATION_EXIT_WORLD:
		case NOTIFICATION_EXIT_TREE: {
			_detach_from_compositor();
		} break;
	}
}

ContactShadows3D::ContactShadows3D() {
	effect.instantiate();
	_update_effect_properties();
}

ContactShadows3D::~ContactShadows3D() {
	_detach_from_compositor();
}
