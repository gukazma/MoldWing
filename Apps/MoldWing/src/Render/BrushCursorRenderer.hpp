/*
 *  MoldWing - Brush Cursor Renderer
 *  Renders 2D circular brush cursor overlay in screen space
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
 * @brief Renders a 2D circular brush cursor in screen space
 */
class BrushCursorRenderer
{
public:
    BrushCursorRenderer() = default;
    ~BrushCursorRenderer() = default;

    // Non-copyable
    BrushCursorRenderer(const BrushCursorRenderer&) = delete;
    BrushCursorRenderer& operator=(const BrushCursorRenderer&) = delete;

    /**
     * @brief Initialize the renderer
     * @param pDevice Render device
     * @param pSwapChain Swap chain for format info
     * @return true if initialization succeeded
     */
    bool initialize(Diligent::IRenderDevice* pDevice, Diligent::ISwapChain* pSwapChain);

    /**
     * @brief Render the brush cursor circle
     * @param pContext Device context
     * @param centerX, centerY Center position in screen pixels
     * @param radius Brush radius in screen pixels
     * @param viewportWidth, viewportHeight Current viewport size
     */
    void render(Diligent::IDeviceContext* pContext,
                int centerX, int centerY, int radius,
                int viewportWidth, int viewportHeight);

    bool isInitialized() const { return m_initialized; }

    // Number of segments to approximate the circle
    static constexpr int CIRCLE_SEGMENTS = 64;

private:
    bool createPipeline(Diligent::IRenderDevice* pDevice, Diligent::ISwapChain* pSwapChain);

    // Pipeline for filled circle
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
