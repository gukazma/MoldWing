/*
 *  MoldWing - Mesh Renderer
 *  S1.5: Render mesh with DiligentEngine
 */

#pragma once

#include "Core/MeshData.hpp"
#include "OrbitCamera.hpp"

#include <RefCntAutoPtr.hpp>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <SwapChain.h>
#include <Buffer.h>
#include <PipelineState.h>
#include <ShaderResourceBinding.h>

#include <memory>

namespace MoldWing
{

class MeshRenderer
{
public:
    MeshRenderer() = default;
    ~MeshRenderer() = default;

    // Initialize renderer with device
    bool initialize(Diligent::IRenderDevice* pDevice,
                    Diligent::ISwapChain* pSwapChain);

    // Load mesh data into GPU buffers
    bool loadMesh(const MeshData& mesh);

    // Render the mesh
    void render(Diligent::IDeviceContext* pContext,
                const OrbitCamera& camera);

    // Check if mesh is loaded
    bool hasMesh() const { return m_vertexCount > 0; }

    // Get mesh bounds
    const BoundingBox& getBounds() const { return m_bounds; }

private:
    bool createPipeline(Diligent::IRenderDevice* pDevice,
                        Diligent::ISwapChain* pSwapChain);

    // Device reference (not owned)
    Diligent::IRenderDevice* m_pDevice = nullptr;

    // Pipeline objects
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pSRB;

    // Buffers
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pVertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pIndexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pConstantBuffer;

    // Mesh info
    Diligent::Uint32 m_vertexCount = 0;
    Diligent::Uint32 m_indexCount = 0;
    BoundingBox m_bounds;

    bool m_initialized = false;
};

} // namespace MoldWing
