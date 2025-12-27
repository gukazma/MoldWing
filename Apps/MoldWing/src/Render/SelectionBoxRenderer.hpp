/*
 *  MoldWing - Selection Box Renderer
 *  Renders 2D selection rectangle overlay in screen space
 */

#pragma once

#include <RefCntAutoPtr.hpp>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <SwapChain.h>
#include <Buffer.h>
#include <PipelineState.h>
#include <ShaderResourceBinding.h>

namespace MoldWing
{

/**
 * @brief Renders a 2D selection rectangle in screen space
 */
class SelectionBoxRenderer
{
public:
    SelectionBoxRenderer() = default;
    ~SelectionBoxRenderer() = default;

    // Non-copyable
    SelectionBoxRenderer(const SelectionBoxRenderer&) = delete;
    SelectionBoxRenderer& operator=(const SelectionBoxRenderer&) = delete;

    /**
     * @brief Initialize the renderer
     * @param pDevice Render device
     * @param pSwapChain Swap chain for format info
     * @return true if initialization succeeded
     */
    bool initialize(Diligent::IRenderDevice* pDevice, Diligent::ISwapChain* pSwapChain);

    /**
     * @brief Render the selection box
     * @param pContext Device context
     * @param x1, y1, x2, y2 Selection rectangle in screen pixels
     * @param viewportWidth, viewportHeight Current viewport size
     */
    void render(Diligent::IDeviceContext* pContext,
                int x1, int y1, int x2, int y2,
                int viewportWidth, int viewportHeight);

    bool isInitialized() const { return m_initialized; }

private:
    bool createPipeline(Diligent::IRenderDevice* pDevice, Diligent::ISwapChain* pSwapChain);

    // Pipeline for filled rectangle
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pFillPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pFillSRB;

    // Pipeline for border (line strip)
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pBorderPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pBorderSRB;

    // Buffers
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pVertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pConstantBuffer;

    bool m_initialized = false;
};

} // namespace MoldWing
