#include "world_partition_hiz_compositor_effect.h"
#include "servers/rendering/renderer_rd/storage_rd/render_data_rd.h"
#include "servers/rendering/rendering_server.h"
#include "core/config/engine.h"
#include "hiz_generate.glsl.gen.h"
#include "servers/rendering/rendering_device_binds.h"
#include "core/object/callable_mp.h"

void WorldPartitionHiZCompositorEffect::_bind_methods() {
}

WorldPartitionHiZCompositorEffect::WorldPartitionHiZCompositorEffect() {
	set_effect_callback_type(EFFECT_CALLBACK_TYPE_POST_OPAQUE);
	set_needs_motion_vectors(false);
	set_needs_normal_roughness(false);
	set_needs_separate_specular(false);
	set_access_resolved_color(false);
	set_access_resolved_depth(true); // We need depth
	
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs != nullptr) {
		rs->compositor_effect_set_callback(get_rid(), RenderingServerEnums::COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_OPAQUE, callable_mp(this, &WorldPartitionHiZCompositorEffect::_render_callback_impl));
	}
}

WorldPartitionHiZCompositorEffect::~WorldPartitionHiZCompositorEffect() {
	_free_gpu_resources();
}

RID WorldPartitionHiZCompositorEffect::get_hiz_texture() const {
	return hiz_texture;
}

Vector2i WorldPartitionHiZCompositorEffect::get_hiz_size() const {
	return hiz_size;
}

bool WorldPartitionHiZCompositorEffect::_init_gpu_resources() {
	if (is_initialized) {
		return true;
	}
	rd = RenderingServer::get_singleton()->get_rendering_device();
	if (!rd) {
		return false;
	}

	Ref<RDShaderFile> shader_file;
	shader_file.instantiate();
	Error err = shader_file->parse_versions_from_text(hiz_generate_shader_glsl);
	if (err != OK) {
		ERR_PRINT("WorldPartitionHiZCompositorEffect: Failed to parse hiz_generate.glsl");
		return false;
	}

	shader = rd->shader_create_from_spirv(shader_file->get_spirv_stages("hiz"));
	ERR_FAIL_COND_V(shader.is_null(), false);

	pipeline = rd->compute_pipeline_create(shader);
	ERR_FAIL_COND_V(pipeline.is_null(), false);

	is_initialized = true;
	return true;
}

void WorldPartitionHiZCompositorEffect::_free_gpu_resources() {
	if (rd) {
		if (pipeline.is_valid()) {
			rd->free_rid(pipeline);
			pipeline = RID();
		}
		if (shader.is_valid()) {
			rd->free_rid(shader);
			shader = RID();
		}
		if (hiz_texture.is_valid()) {
			rd->free_rid(hiz_texture);
			hiz_texture = RID();
		}
	}
	is_initialized = false;
}

void WorldPartitionHiZCompositorEffect::_render_callback_impl(int p_effect_callback_type, const RenderData *p_render_data) {
	if (p_effect_callback_type != RenderingServerEnums::COMPOSITOR_EFFECT_CALLBACK_TYPE_POST_OPAQUE) {
		return;
	}

	if (!_init_gpu_resources()) {
		return;
	}

	const RenderDataRD *rd_data = static_cast<const RenderDataRD *>(p_render_data);
	if (!rd_data || rd_data->render_buffers.is_null()) {
		return;
	}

	RID src_depth = rd_data->render_buffers->get_depth_texture();
	if (src_depth.is_null()) {
		return;
	}

	// We have the depth texture. Get its format to find out the size.
	RD::TextureFormat src_format = rd->texture_get_format(src_depth);
	Vector2i src_size(src_format.width, src_format.height);
	
	// Create or resize HiZ texture if needed
	// For occlusion culling, half resolution is usually enough for the first mip
	Vector2i expected_hiz_size = src_size / 2;
	if (expected_hiz_size.x == 0 || expected_hiz_size.y == 0) {
		return;
	}
	
	int expected_mips = 1;
	Vector2i mip_size = expected_hiz_size;
	while (mip_size.x > 1 || mip_size.y > 1) {
		expected_mips++;
		mip_size.x = MAX(1, mip_size.x / 2);
		mip_size.y = MAX(1, mip_size.y / 2);
	}

	if (hiz_texture.is_valid() && (hiz_size != expected_hiz_size || mip_levels != expected_mips)) {
		rd->free_rid(hiz_texture);
		hiz_texture = RID();
	}

	if (hiz_texture.is_null()) {
		RD::TextureFormat tf;
		tf.format = RD::DATA_FORMAT_R32_SFLOAT;
		tf.width = expected_hiz_size.x;
		tf.height = expected_hiz_size.y;
		tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
		tf.mipmaps = expected_mips;
		tf.texture_type = RD::TEXTURE_TYPE_2D;
		
		hiz_texture = rd->texture_create(tf, RD::TextureView());
		hiz_size = expected_hiz_size;
		mip_levels = expected_mips;
	}

	// Build the mip chain
	RD::ComputeListID compute_list = rd->compute_list_begin();
	rd->compute_list_bind_compute_pipeline(compute_list, pipeline);

	struct PushConstants {
		float src_inv_size[2];
	} push_constants;

	RID current_src = src_depth;
	Vector2i current_src_size = src_size;

	for (int i = 0; i < mip_levels; i++) {
		Vector2i current_dst_size(MAX(1, expected_hiz_size.x >> i), MAX(1, expected_hiz_size.y >> i));

		// Create views for binding
		RD::TextureView dest_view;
		dest_view.format_override = RD::DATA_FORMAT_R32_SFLOAT;
		RID dest_mip = rd->texture_create_shared_from_slice(dest_view, hiz_texture, 0, i);

		RID src_mip;
		if (i == 0) {
			src_mip = current_src; // Main depth texture
		} else {
			RD::TextureView src_view;
			src_view.format_override = RD::DATA_FORMAT_R32_SFLOAT;
			src_mip = rd->texture_create_shared_from_slice(src_view, hiz_texture, 0, i - 1);
		}

		// Uniforms
		Vector<RD::Uniform> uniforms;
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
			u.binding = 0;
			RD::SamplerState sampler_state;
			sampler_state.mag_filter = RD::SAMPLER_FILTER_NEAREST;
			sampler_state.min_filter = RD::SAMPLER_FILTER_NEAREST;
			sampler_state.mip_filter = RD::SAMPLER_FILTER_NEAREST;
			sampler_state.repeat_u = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
			sampler_state.repeat_v = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
			RID sampler = rd->sampler_create(sampler_state);
			u.append_id(sampler);
			u.append_id(src_mip);
			uniforms.push_back(u);
		}
		{
			RD::Uniform u;
			u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			u.binding = 1;
			u.append_id(dest_mip);
			uniforms.push_back(u);
		}

		RID uniform_set = rd->uniform_set_create(uniforms, shader, 0);

		push_constants.src_inv_size[0] = 1.0f / (float)current_src_size.x;
		push_constants.src_inv_size[1] = 1.0f / (float)current_src_size.y;

		rd->compute_list_bind_uniform_set(compute_list, uniform_set, 0);
		rd->compute_list_set_push_constant(compute_list, &push_constants, sizeof(PushConstants));

		uint32_t x_groups = (current_dst_size.x + 7) / 8;
		uint32_t y_groups = (current_dst_size.y + 7) / 8;
		rd->compute_list_dispatch(compute_list, x_groups, y_groups, 1);

		// Add barrier to ensure the mip we just wrote is ready for the next iteration
		rd->compute_list_add_barrier(compute_list);

		// Cleanup views
		rd->free_rid(dest_mip);
		if (i > 0) {
			rd->free_rid(src_mip);
		}

		current_src = hiz_texture;
		current_src_size = current_dst_size;
	}

	rd->compute_list_end();
}
