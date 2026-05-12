// Skinning.hlsli

struct Vertex {
    float32_t4 position;
    float32_t4 texcoord;
    float32_t4 normal;
    float32_t4 color;
    float32_t4 weight;
    int32_t4 index;
};

struct WellKnownPalette {
    float32_t4x4 skeletonSpaceMatrix;
};

struct SkinningInformation {
    uint32_t numVertices;
    uint32_t numBones;
    uint32_t padding[62]; // 256 bytes alignment
};
