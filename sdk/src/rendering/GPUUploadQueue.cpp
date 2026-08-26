#include "spatial/rendering/GPUUploadQueue.h"

#include <utility>

namespace spatial::rendering
{
    void GPUUploadQueue::enqueueMesh(data::Mesh mesh, MeshUploadCallback onComplete)
    {
        m_meshQueue.push_back(MeshUploadRequest{std::move(mesh), std::move(onComplete)});
    }

    void GPUUploadQueue::enqueueMaterial(data::Material material, MaterialUploadCallback onComplete)
    {
        m_materialQueue.push_back(MaterialUploadRequest{std::move(material), std::move(onComplete)});
    }

    std::size_t GPUUploadQueue::processQueue(IRenderer& renderer, std::size_t maxUploads)
    {
        std::size_t processed = 0;

        while (processed < maxUploads && !m_meshQueue.empty())
        {
            MeshUploadRequest request = std::move(m_meshQueue.front());
            m_meshQueue.pop_front();

            const MeshHandle handle = renderer.createMesh(request.mesh);
            if (request.callback)
            {
                request.callback(MeshResource(renderer, handle));
            }
            ++processed;
        }

        while (processed < maxUploads && !m_materialQueue.empty())
        {
            MaterialUploadRequest request = std::move(m_materialQueue.front());
            m_materialQueue.pop_front();

            const MaterialHandle handle = renderer.createMaterial(request.material);
            if (request.callback)
            {
                request.callback(MaterialResource(renderer, handle));
            }
            ++processed;
        }

        return processed;
    }

    std::size_t GPUUploadQueue::pendingCount() const noexcept { return m_meshQueue.size() + m_materialQueue.size(); }
}
