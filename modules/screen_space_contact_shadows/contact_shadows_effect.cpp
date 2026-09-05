#include "contact_shadows_effect.h"

#include "contact_shadows.glsl.gen.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "servers/rendering/renderer_rd/storage_rd/light_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "servers/rendering/rendering_device_binds.h"
#include "servers/rendering/rendering_server.h"

void ContactShadowsEffect::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_distance", "distance"), &ContactShadowsEffect::set_max_distance);
	ClassDB::bind_method(D_METHOD("get_max_distance"), &ContactShadowsEffect::get_max_distance);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_distance", PROPERTY_HINT_RANGE, "0.01,2.0,0.01,suffix:m"), "set_max_distance", "get_max_distance");

	ClassDB::bind_method(D_METHOD("set_thickness", "thickness"), &ContactShadowsEffect::set_thickness);
	ClassDB::bind_method(D_METHOD("get_thickness"), &ContactShadowsEffect::get_thickness);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "thickness", PROPERTY_HINT_RANGE, "0.001,0.5,0.001,suffix:m"), "set_thickness", "get_thickness");

	ClassDB::bind_method(D_METHOD("set_ray_steps", "steps"), &ContactShadowsEffect::set_ray_steps);
	ClassDB::bind_method(D_METHOD("get_ray_steps"), &ContactShadowsEffect::get_ray_steps);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ray_steps", PROPERTY_HINT_RANGE, "4,64,1"), "set_ray_steps", "get_ray_steps");

	ClassDB::bind_method(D_METHOD("set_shadow_intensity", "intensity"), &ContactShadowsEffect::set_shadow_intensity);
	ClassDB::bind_method(D_METHOD("get_shadow_intensity"), &ContactShadowsEffect::get_shadow_intensity);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "shadow_intensity", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_shadow_intensity", "get_shadow_intensity");

	ClassDB::bind_method(D_METHOD("set_normal_bias", "bias"), &ContactShadowsEffect::set_normal_bias);
	ClassDB::bind_method(D_METHOD("get_normal_bias"), &ContactShadowsEffect::get_normal_bias);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "normal_bias", PROPERTY_HINT_RANGE, "0.0,0.1,0.001,suffix:m"), "set_normal_bias", "get_normal_bias");

	ClassDB::bind_method(D_METHOD("set_custom_light_direction", "direction"), &ContactShadowsEffect::set_custom_light_direction);
	ClassDB::bind_method(D_METHOD("get_custom_light_direction"), &ContactShadowsEffect::get_custom_light_direction);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "custom_light_direction"), "set_custom_light_direction", "get_custom_light_direction");
}

void ContactShadowsEffect::set_max_distance(float p_dist) {
	max_distance = MAX(0.01f, p_dist);
}

float ContactShadowsEffect::get_max_distance() const {
	return max_distance;
}

void ContactShadowsEffect::set_thickness(float p_thickness) {
	thickness = MAX(0.001f, p_thickness);
}

float ContactShadowsEffect::get_thickness() const {
	return thickness;
}

void ContactShadowsEffect::set_ray_steps(int p_steps) {
	ray_steps = CLAMP(p_steps, 4, 64);
}

int ContactShadowsEffect::get_ray_steps() const {
	return ray_steps;
}

void ContactShadowsEffect::set_shadow_intensity(float p_intensity) {
	shadow_intensity = CLAMP(p_intensity, 0.0f, 1.0f);
}

float ContactShadowsEffect::get_shadow_intensity() const {
	return shadow_intensity;
}

void ContactShadowsEffect::set_normal_bias(float p_bias) {
	normal_bias = MAX(0.0f, p_bias);
}

float ContactShadowsEffect::get_normal_bias() const {
	return normal_bias;
}

void ContactShadowsEffect::set_custom_light_direction(const Vector3 &p_dir) {
	custom_light_direction = p_dir;
}

Vector3 ContactShadowsEffect::get_custom_light_direction() const {
	return custom_light_direction;
}

bool ContactShadowsEffect::_init_gpu_resources() {
	if (rd != nullptr && pipeline.is_valid()) {
		return true;
	}

	rd = RenderingServer::get_singleton()->get_rendering_device();
	if (!rd) {
		return false;
	}

	Ref<RDShaderFile> shader_file;
	shader_file.instantiate();
	Error err = shader_file->parse_versions_from_text(contact_shadows_shader_glsl);
	if (err != OK) {
		ERR_PRINT("ContactShadowsEffect: Failed to parse contact_shadows.glsl");
		return false;
	}

	Vector<RD::ShaderStageSPIRVData> stages = shader_file->get_spirv_stages("compute");
	if (stages.is_empty()) {
		ERR_PRINT("ContactShadowsEffect: No compute stage found in contact_shadows.glsl");
		return false;
	}

	shader = rd->shader_create_from_spirv(stages);
	if (!shader.is_valid()) {
		ERR_PRINT("ContactShadowsEffect: Failed to create shader from SPIR-V");
		return false;
	}

	pipeline = rd->compute_pipeline_create(shader);
	if (!pipeline.is_valid()) {
		ERR_PRINT("ContactShadowsEffect: Failed to create compute pipeline");
		return false;
	}

	// Create SceneData UBO buffer
	scene_data_buffer = rd->uniform_buffer_create(sizeof(SceneDataUBO));

	// Create linear clamp sampler for depth sampling
	RD::SamplerState sampler_state;
	sampler_state.mag_filter = RD::SAMPLER_FILTER_LINEAR;
	sampler_state.min_filter = RD::SAMPLER_FILTER_LINEAR;
	sampler_state.repeat_u = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
	sampler_state.repeat_v = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
	linear_sampler = rd->sampler_create(sampler_state);

	return true;
}

void ContactShadowsEffect::_free_gpu_resources() {
	if (rd) {
		if (scene_data_buffer.is_valid()) {
			rd->free_rid(scene_data_buffer);
			scene_data_buffer = RID();
		}
		if (linear_sampler.is_valid()) {
			rd->free_rid(linear_sampler);
			linear_sampler = RID();
		}
		if (pipeline.is_valid()) {
			rd->free_rid(pipeline);
			pipeline = RID();
		}
		if (shader.is_valid()) {
			rd->free_rid(shader);
			shader = RID();
		}
		rd = nullptr;
	}
}

void ContactShadowsEffect::_render_callback(int p_effect_callback_type, const RenderData *p_render_data) {
	if (!get_enabled() || shadow_intensity <= 0.001f) {
		return;
	}

	if (p_effect_callback_type != EFFECT_CALLBACK_TYPE_POST_OPAQUE) {
		return;
	}

	if (!_init_gpu_resources()) {
		return;
	}

	const RenderDataRD *rd_data = Object::cast_to<RenderDataRD>(p_render_data);
	if (!rd_data) {
		return;
	}

	Ref<RenderSceneBuffersRD> rb = rd_data->render_buffers;
	if (rb.is_null()) {
		return;
	}

	RenderSceneDataRD *scene_data = rd_data->scene_data;
	if (!scene_data) {
		return;
	}

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	if (!uniform_set_cache) {
		return;
	}

	Size2i internal_sz = rb->get_internal_size();
	if (internal_sz.x <= 0 || internal_sz.y <= 0) {
		return;
	}

	// 1. Determine world-space light direction
	Vector3 light_world = Vector3(0.5f, -1.0f, 0.5f).normalized(); // Default sunlight vector
	if (custom_light_direction.length_squared() > 0.001f) {
		light_world = custom_light_direction.normalized();
	} else {
		RendererRD::LightStorage *light_storage = RendererRD::LightStorage::get_singleton();
		if (light_storage && rd_data->lights) {
			const PagedArray<RID> &lights = *rd_data->lights;
			for (int i = 0; i < (int)lights.size(); i++) {
				if (light_storage->owns_light_instance(lights[i])) {
					RID base = light_storage->light_instance_get_base_light(lights[i]);
					if (base.is_valid() && light_storage->light_get_type(base) == RSE::LIGHT_DIRECTIONAL) {
						Transform3D light_xf = light_storage->light_instance_get_base_transform(lights[i]);
						// Godot directional lights shine along -Z of their transform
						light_world = -light_xf.basis.get_column(Vector3::AXIS_Z).normalized();
						break;
					}
				}
			}
		}
	}

	// 2. Transform light direction into camera view space
	Vector3 light_dir_view = scene_data->cam_transform.basis.xform_inv(light_world).normalized();

	uint32_t view_count = rb->get_view_count();
	for (uint32_t v = 0; v < view_count; v++) {
		RID depth_tex = rb->get_depth_texture(v);
		RID color_tex = rb->get_internal_texture(v);

		if (!depth_tex.is_valid() || !color_tex.is_valid()) {
			continue;
		}

		// Update SceneData UBO with current view projection & inverse projection
		SceneDataUBO ubo;
		Projection proj = scene_data->view_projection[v];
		Projection inv_proj = proj.inverse();

		for (int r = 0; r < 4; r++) {
			for (int c = 0; c < 4; c++) {
				ubo.projection[r * 4 + c] = proj.columns[r][c];
				ubo.inv_projection[r * 4 + c] = inv_proj.columns[r][c];
			}
		}
		rd->buffer_update(scene_data_buffer, 0, sizeof(SceneDataUBO), &ubo);

		// Build Uniforms
		RD::Uniform u_depth(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>{ linear_sampler, depth_tex });
		RD::Uniform u_color(RD::UNIFORM_TYPE_IMAGE, 1, Vector<RID>{ color_tex });
		RD::Uniform u_scene_data(RD::UNIFORM_TYPE_UNIFORM_BUFFER, 2, Vector<RID>{ scene_data_buffer });

		RID uniform_set = uniform_set_cache->get_cache(shader, 0, u_depth, u_color, u_scene_data);

		// Push Constants
		PushConstants pc;
		pc.light_dir_view[0] = light_dir_view.x;
		pc.light_dir_view[1] = light_dir_view.y;
		pc.light_dir_view[2] = light_dir_view.z;
		pc.max_distance = max_distance;
		pc.screen_size[0] = internal_sz.x;
		pc.screen_size[1] = internal_sz.y;
		pc.thickness = thickness;
		pc.shadow_intensity = shadow_intensity;
		pc.ray_steps = ray_steps;
		pc.normal_bias = normal_bias;
		pc.pad0 = 0;
		pc.pad1 = 0;

		// Dispatch compute
		RD::ComputeListID compute_list = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(compute_list, pipeline);
		rd->compute_list_bind_uniform_set(compute_list, uniform_set, 0);
		rd->compute_list_set_push_constant(compute_list, &pc, sizeof(PushConstants));
		rd->compute_list_dispatch_threads(compute_list, internal_sz.x, internal_sz.y, 1);
		rd->compute_list_end();
	}
}

ContactShadowsEffect::ContactShadowsEffect() {
	set_effect_callback_type(EFFECT_CALLBACK_TYPE_POST_OPAQUE);
	set_access_resolved_depth(true);
	set_access_resolved_color(true);
	set_needs_normal_roughness(false);

	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs != nullptr) {
		rs->compositor_effect_set_callback(get_rid(), RSE::CompositorEffectCallbackType(get_effect_callback_type()), callable_mp(this, &ContactShadowsEffect::_render_callback));
	}
}

ContactShadowsEffect::~ContactShadowsEffect() {
	_free_gpu_resources();
}
