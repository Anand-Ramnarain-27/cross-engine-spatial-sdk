#pragma once

#include <cstddef>
#include <deque>
#include <functional>

#include "spatial/Export.h"
#include "spatial/data/Material.h"
#include "spatial/data/Mesh.h"
#include "spatial/rendering/GPUResource.h"
#include "spatial/rendering/IRenderer.h"

namespace spatial::rendering
{
    // Bounds how much GPU upload work happens per call, so a burst of
    // newly-loaded tiles can't stall the render thread in a single frame.
    // Not thread-safe by design: GPU calls only ever run on the thread that
    // owns the render context, so enqueue/processQueue are both meant to be
    // called from that same thread.
    class SPATIAL_API GPUUploadQueue
    {
    public:
        using MeshUploadCallback = std::function<void(MeshResource)>;
        using MaterialUploadCallback = std::function<void(MaterialResource)>;

        void enqueueMesh(data::Mesh mesh, MeshUploadCallback onComplete);
        void enqueueMaterial(data::Material material, MaterialUploadCallback onComplete);

        // Uploads up to maxUploads pending items (meshes before materials),
        // invoking each one's callback with its new GPU resource. Returns
        // how many were actually processed.
        std::size_t processQueue(IRenderer& renderer, std::size_t maxUploads);

        [[nodiscard]] std::size_t pendingCount() const noexcept;

    private:
        struct MeshUploadRequest
        {
            data::Mesh mesh;
            MeshUploadCallback callback;
        };

        struct MaterialUploadRequest
        {
            data::Material material;
            MaterialUploadCallback callback;
        };

        std::deque<MeshUploadRequest> m_meshQueue;
        std::deque<MaterialUploadRequest> m_materialQueue;
    };
}
