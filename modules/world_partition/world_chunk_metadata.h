#ifndef WORLD_CHUNK_METADATA_H
#define WORLD_CHUNK_METADATA_H

#include "core/io/resource.h"

class WorldChunkMetadata : public Resource {
	GDCLASS(WorldChunkMetadata, Resource);

protected:
	static void _bind_methods();

public:
	WorldChunkMetadata();
	~WorldChunkMetadata();
};

#endif // WORLD_CHUNK_METADATA_H
