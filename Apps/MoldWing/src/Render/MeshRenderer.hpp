/*
 *  MoldWing - Mesh Renderer
 *  S1.5: Render mesh with DiligentEngine
 *  T6.1.5-7: Extended for texture rendering
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
#include <Texture.h>
#include <Sampler.h>

#include <memory>
#include <vector>

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

    // Check if current mesh has textures
    bool hasTextures() const { return m_hasTextures; }

    // Update a GPU texture from TextureData (for texture editing)
    void updateTexture(Diligent::IDeviceContext* pContext, int textureIndex, const TextureData& texData);

private:
    bool createPipeline(Diligent::IRenderDevice* pDevice,
                        Diligent::ISwapChain* pSwapChain);

    // Create GPU texture from TextureData
    bool createGPUTexture(const TextureData& texData, int index);

    // Device reference (not owned)
    Diligent::IRenderDevice* m_pDevice = nullptr;

    // Pipeline objects
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pSRB;

    // Buffers
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pVertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pIndexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pConstantBuffer;

    // Texture resources (T6.1.5)
    std::vector<Diligent::RefCntAutoPtr<Diligent::ITexture>> m_textures;
    std::vector<Diligent::RefCntAutoPtr<Diligent::ITextureView>> m_textureSRVs;
    Diligent::RefCntAutoPtr<Diligent::ISampler> m_pSampler;

    // Mesh info
    Diligent::Uint32 m_vertexCount = 0;
    Diligent::Uint32 m_indexCount = 0;
    BoundingBox m_bounds;

    // Texture state
    bool m_hasTextures = false;

    bool m_initialized = false;
};

} // namespace MoldWing
