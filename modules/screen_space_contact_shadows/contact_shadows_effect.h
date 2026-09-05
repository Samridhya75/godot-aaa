#pragma once

#include "scene/resources/compositor.h"
#include "servers/rendering/rendering_device.h"

class ContactShadowsEffect : public CompositorEffect {
	GDCLASS(ContactShadowsEffect, CompositorEffect);

private:
	float max_distance = 0.3f;
	float thickness = 0.05f;
	int ray_steps = 16;
	float shadow_intensity = 0.8f;
	float normal_bias = 0.01f;
	Vector3 custom_light_direction = Vector3(0, 0, 0);

	RenderingDevice *rd = nullptr;
	RID shader;
	RID pipeline;
	RID scene_data_buffer;
	RID linear_sampler;

	struct PushConstants {
		float light_dir_view[3];
		float max_distance;
		int32_t screen_size[2];
		float thickness;
		float shadow_intensity;
		int32_t ray_steps;
		float normal_bias;
		int32_t pad0;
		int32_t pad1;
	};

	struct SceneDataUBO {
		float projection[16];
		float inv_projection[16];
	};

	bool _init_gpu_resources();
	void _free_gpu_resources();

protected:
	static void _bind_methods();

public:
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

	void _render_callback(int p_effect_callback_type, const RenderData *p_render_data);

	ContactShadowsEffect();
	~ContactShadowsEffect();
};
