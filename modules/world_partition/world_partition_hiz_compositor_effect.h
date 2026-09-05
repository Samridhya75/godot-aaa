#ifndef WORLD_PARTITION_HIZ_COMPOSITOR_EFFECT_H
#define WORLD_PARTITION_HIZ_COMPOSITOR_EFFECT_H

#include "scene/resources/compositor.h"
#include "servers/rendering/rendering_device.h"

class WorldPartitionHiZCompositorEffect : public CompositorEffect {
	GDCLASS(WorldPartitionHiZCompositorEffect, CompositorEffect);

private:
	RenderingDevice *rd = nullptr;
	RID shader;
	RID pipeline;
	RID hiz_texture;
	Vector2i hiz_size;
	int mip_levels = 1;

	bool is_initialized = false;
	bool _init_gpu_resources();
	void _free_gpu_resources();

protected:
	static void _bind_methods();
	void _render_callback_impl(int p_effect_callback_type, const RenderData *p_render_data);

public:
	RID get_hiz_texture() const;
	Vector2i get_hiz_size() const;

	WorldPartitionHiZCompositorEffect();
	~WorldPartitionHiZCompositorEffect();
};

#endif // WORLD_PARTITION_HIZ_COMPOSITOR_EFFECT_H
