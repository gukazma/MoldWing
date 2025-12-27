/*
 *  MoldWing - Selection Renderer
 *  S2.4: Render selected faces with highlight overlay
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

namespace MoldWing
{

/**
 * @brief Renders selected faces with a highlight overlay
 *
 * Uses additive blending to overlay a highlight color on selected faces.
 * Renders selected faces slightly in front of the mesh to prevent z-fighting.
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
     * @brief Load mesh data
     * @param mesh The mesh data
     * @return true if loading succeeded
     */
    bool loadMesh(const MeshData& mesh);

    /**
     * @brief Update selection (rebuild selection index buffer)
     * @param selectedFaces Set of selected face indices
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
    bool hasSelection() const { return m_selectionIndexCount > 0; }

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

    // Buffers
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pVertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pSelectionIndexBuffer;  // Dynamic buffer for selected faces
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pConstantBuffer;

    // Mesh data
    const MeshData* m_pMeshData = nullptr;
    uint32_t m_vertexCount = 0;
    uint32_t m_selectionIndexCount = 0;

    // Cached selection indices for lazy GPU update
    std::vector<uint32_t> m_cachedSelectionIndices;
    bool m_selectionDirty = false;

    // Highlight color
    float m_highlightColor[4] = {0.2f, 0.5f, 1.0f, 0.4f};  // Light blue with transparency

    bool m_initialized = false;
};

} // namespace MoldWing
