/*
 *  MoldWing - Face Picker (GPU Picking System)
 *  S2.1: GPU-based face ID picking for selection
 *  T6.2.1: Extended for texture editing (pickPoint with depth)
 */

#pragma once

#include "Core/MeshData.hpp"
#include "Render/OrbitCamera.hpp"

#include <RefCntAutoPtr.hpp>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <SwapChain.h>
#include <Buffer.h>
#include <Texture.h>
#include <PipelineState.h>
#include <ShaderResourceBinding.h>

#include <vector>
#include <cstdint>

namespace MoldWing
{

/**
 * @brief Result of a single point pick operation (T6.2.1)
 */
struct PickResult
{
    uint32_t faceId = 0xFFFFFFFF;  // Face ID (INVALID_FACE_ID if no hit)
    float depth = 1.0f;            // Normalized depth (0=near, 1=far)
    bool hit = false;              // True if a face was hit
};

/**
 * @brief Per-mesh buffer data for multi-mesh picking (M8: Plan B)
 */
struct MeshPickBuffers
{
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> indexBuffer;
    uint32_t meshId = 0;        // Mesh ID (0-255)
    uint32_t indexCount = 0;    // Number of indices
    bool visible = true;        // Whether to include in picking
};

/**
 * @brief GPU-based face picking system
 *
 * Renders face IDs to an off-screen buffer and reads them back
 * to determine which faces are under the cursor or within a selection rectangle.
 *
 * M8 Plan B: Supports multi-mesh picking with composite face IDs
 * (high 8 bits = meshId, low 24 bits = faceId)
 */
class FacePicker
{
public:
    FacePicker() = default;
    ~FacePicker() = default;

    // Non-copyable
    FacePicker(const FacePicker&) = delete;
    FacePicker& operator=(const FacePicker&) = delete;

    /**
     * @brief Initialize the picker with DiligentEngine device
     * @param pDevice Render device
     * @param width Initial render target width
     * @param height Initial render target height
     * @return true if initialization succeeded
     */
    bool initialize(Diligent::IRenderDevice* pDevice, uint32_t width, uint32_t height);

    /**
     * @brief Load mesh data for picking (creates vertex/index buffers)
     * @param mesh The mesh data
     * @return true if loading succeeded
     * @note This is legacy single-mesh API. For multi-mesh, use addMesh/clearMeshes
     */
    bool loadMesh(const MeshData& mesh);

    /**
     * @brief Add a mesh for multi-mesh picking (M8: Plan B)
     * @param mesh The mesh data
     * @param meshId Unique mesh ID (0-255)
     * @param visible Whether mesh is visible for picking
     * @return true if loading succeeded
     */
    bool addMesh(const MeshData& mesh, uint32_t meshId, bool visible = true);

    /**
     * @brief Set mesh visibility for picking
     * @param meshId Mesh ID to update
     * @param visible Whether mesh should be picked
     */
    void setMeshVisible(uint32_t meshId, bool visible);

    /**
     * @brief Clear all meshes
     */
    void clearMeshes();

    /**
     * @brief Resize the picking render targets
     * @param width New width
     * @param height New height
     */
    void resize(uint32_t width, uint32_t height);

    /**
     * @brief Render face IDs to the off-screen buffer
     * @param pContext Device context
     * @param camera Current camera
     */
    void renderIDBuffer(Diligent::IDeviceContext* pContext, const OrbitCamera& camera);

    /**
     * @brief Read face ID at a single screen position
     * @param pContext Device context
     * @param x Screen X coordinate (pixels)
     * @param y Screen Y coordinate (pixels)
     * @return Face ID (0xFFFFFFFF if no face hit)
     */
    uint32_t readFaceID(Diligent::IDeviceContext* pContext, int x, int y);

    /**
     * @brief Pick a single point and return face ID with depth (T6.2.1)
     * @param pContext Device context
     * @param x Screen X coordinate (pixels)
     * @param y Screen Y coordinate (pixels)
     * @return PickResult with faceId, depth, and hit flag
     */
    PickResult pickPoint(Diligent::IDeviceContext* pContext, int x, int y);

    /**
     * @brief Read all face IDs within a screen rectangle
     * @param pContext Device context
     * @param x1 Left coordinate
     * @param y1 Top coordinate
     * @param x2 Right coordinate
     * @param y2 Bottom coordinate
     * @return Vector of unique face IDs in the rectangle
     */
    std::vector<uint32_t> readFaceIDsInRect(Diligent::IDeviceContext* pContext,
                                             int x1, int y1, int x2, int y2);

    /**
     * @brief Read all face IDs within a circular region
     * @param pContext Device context
     * @param centerX Center X coordinate in pixels
     * @param centerY Center Y coordinate in pixels
     * @param radius Radius in pixels
     * @return Vector of unique face IDs in the circle
     */
    std::vector<uint32_t> readFaceIDsInCircle(Diligent::IDeviceContext* pContext,
                                               int centerX, int centerY, int radius);

    /**
     * @brief Check if picker is initialized
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * @brief Check if mesh is loaded
     */
    bool hasMesh() const { return m_indexCount > 0 || !m_meshBuffers.empty(); }

    /**
     * @brief Get mesh count (M8)
     */
    size_t meshCount() const { return m_meshBuffers.empty() ? (m_indexCount > 0 ? 1 : 0) : m_meshBuffers.size(); }

    // Invalid face ID constant (background/no hit)
    // Note: This is now CompositeId::INVALID for consistency
    static constexpr uint32_t INVALID_FACE_ID = 0xFFFFFFFF;

private:
    bool createPipeline(Diligent::IRenderDevice* pDevice);
    bool createRenderTargets(Diligent::IRenderDevice* pDevice, uint32_t width, uint32_t height);

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> m_pDevice;

    // Pipeline
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pSRB;

    // Render targets
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pIDTexture;           // R32_UINT for face IDs
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pIDRTV;           // Render target view
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pDepthTexture;        // Depth buffer
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_pDepthDSV;        // Depth stencil view

    // Staging textures for CPU readback
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pStagingTexture;      // For face ID readback
    Diligent::RefCntAutoPtr<Diligent::ITexture> m_pDepthStagingTexture; // For depth readback (T6.2.1)

    // Legacy single-mesh buffers (for backward compatibility)
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pVertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pIndexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pConstantBuffer;

    // Multi-mesh buffers (M8: Plan B)
    std::vector<MeshPickBuffers> m_meshBuffers;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;
    bool m_initialized = false;
    bool m_bufferDirty = true;  // True when ID buffer needs re-render
};

} // namespace MoldWing
