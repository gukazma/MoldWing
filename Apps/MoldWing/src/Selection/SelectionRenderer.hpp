/*
 *  MoldWing - Selection Renderer
 *  S2.4: Render selected faces with highlight overlay
 *  M8: Support multi-mesh selection rendering
 */

#pragma once

#include "Core/MeshData.hpp"
#include "Render/OrbitCamera.hpp"
#include "Selection/SelectionSystem.hpp"

#include <RefCntAutoPtr.hpp>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <SwapChain.h>
#include <Buffer.h>
#include <PipelineState.h>
#include <ShaderResourceBinding.h>

#include <vector>
#include <unordered_map>

namespace MoldWing
{

/**
 * @brief Per-mesh render buffers for selection highlight
 */
struct SelectionMeshBuffers
{
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> indexBuffer;  // Dynamic buffer for selection indices
    const MeshData* meshData = nullptr;
    uint32_t meshId = 0;
    uint32_t vertexCount = 0;
    uint32_t maxIndexCount = 0;  // Max capacity of index buffer
    std::vector<uint32_t> cachedIndices;
    uint32_t currentIndexCount = 0;
    bool dirty = false;
};

/**
 * @brief Renders selected faces with a highlight overlay
 *
 * Uses additive blending to overlay a highlight color on selected faces.
 * Renders selected faces slightly in front of the mesh to prevent z-fighting.
 * M8: Supports multi-mesh rendering with composite face IDs.
 */
class SelectionRenderer
{
public:
    SelectionRenderer() = default;
    ~SelectionRenderer() = default;

    // Non-copyable
    SelectionRenderer(const SelectionRenderer&) = delete;
    SelectionRenderer& operator=(const SelectionRenderer&) = delete;

    /**
     * @brief Initialize the renderer
     * @param pDevice Render device
     * @param pSwapChain Swap chain for format info
     * @return true if initialization succeeded
     */
    bool initialize(Diligent::IRenderDevice* pDevice, Diligent::ISwapChain* pSwapChain);

    /**
     * @brief Load mesh data (legacy single-mesh API, sets meshId=0)
     * @param mesh The mesh data
     * @return true if loading succeeded
     */
    bool loadMesh(const MeshData& mesh);

    /**
     * @brief Add a mesh with specified ID (M8 multi-mesh API)
     * @param mesh The mesh data
     * @param meshId The mesh ID (0-255)
     * @return true if adding succeeded
     */
    bool addMesh(const MeshData& mesh, uint32_t meshId);

    /**
     * @brief Clear all loaded meshes
     */
    void clearMeshes();

    /**
     * @brief Update selection (rebuild selection index buffers)
     * Supports both legacy single-mesh face IDs and M8 composite IDs
     * @param selectedFaces Set of selected face indices (composite IDs for multi-mesh)
     */
    void updateSelection(const std::unordered_set<uint32_t>& selectedFaces);

    /**
     * @brief Render selected faces highlight
     * @param pContext Device context
     * @param camera Current camera
     */
    void render(Diligent::IDeviceContext* pContext, const OrbitCamera& camera);

    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return m_initialized; }

    /**
     * @brief Check if has selection to render
     */
    bool hasSelection() const;

    /**
     * @brief Get number of loaded meshes
     */
    size_t meshCount() const { return m_meshBuffers.size(); }

    /**
     * @brief Set highlight color (RGBA, 0-1)
     */
    void setHighlightColor(float r, float g, float b, float a);

private:
    bool createPipeline(Diligent::IRenderDevice* pDevice, Diligent::ISwapChain* pSwapChain);

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> m_pDevice;

    // Pipeline
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pSRB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pConstantBuffer;

    // M8: Per-mesh buffers (indexed by meshId)
    std::unordered_map<uint32_t, SelectionMeshBuffers> m_meshBuffers;

    // Legacy single-mesh support (kept for backward compatibility)
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pVertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pSelectionIndexBuffer;
    const MeshData* m_pMeshData = nullptr;
    uint32_t m_vertexCount = 0;
    uint32_t m_selectionIndexCount = 0;
    std::vector<uint32_t> m_cachedSelectionIndices;
    bool m_selectionDirty = false;

    // Highlight color
    float m_highlightColor[4] = {0.2f, 0.5f, 1.0f, 0.4f};  // Light blue with transparency

    bool m_initialized = false;
};

} // namespace MoldWing
