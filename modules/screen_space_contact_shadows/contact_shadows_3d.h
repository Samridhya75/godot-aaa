#pragma once

#include "contact_shadows_effect.h"
#include "scene/3d/node_3d.h"

class ContactShadows3D : public Node3D {
	GDCLASS(ContactShadows3D, Node3D);

private:
	Ref<ContactShadowsEffect> effect;
	Ref<Compositor> active_compositor;
	bool enabled = true;
	float max_distance = 0.3f;
	float thickness = 0.05f;
	int ray_steps = 16;
	float shadow_intensity = 0.8f;
	float normal_bias = 0.01f;
	Vector3 custom_light_direction = Vector3(0, 0, 0);

	void _update_effect_properties();
	void _attach_to_compositor();
	void _detach_from_compositor();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_enabled(bool p_enabled);
	bool is_enabled() const;

	void set_max_distance(float p_dist);
	float get_max_distance() const;

	void set_thickness(float p_thickness);
	float get_thickness() const;

	void set_ray_steps(int p_steps);
	int get_ray_steps() const;

	void set_shadow_intensity(float p_intensity);
	float get_shadow_intensity() const;

	void set_normal_bias(float p_bias);
	float get_normal_bias() const;

	void set_custom_light_direction(const Vector3 &p_dir);
	Vector3 get_custom_light_direction() const;

	Ref<ContactShadowsEffect> get_effect() const { return effect; }
	Ref<Compositor> get_active_compositor() const { return active_compositor; }

	ContactShadows3D();
	~ContactShadows3D();
};
