/*
 *  MoldWing - Lasso Renderer
 *  Renders 2D lasso selection path overlay in screen space
 */

#pragma once

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
 * @brief Renders a 2D lasso selection path in screen space
 *
 * The lasso is rendered as a filled polygon with a border line.
 * Points are added dynamically as the user drags the mouse.
 */
class LassoRenderer
{
public:
    LassoRenderer() = default;
    ~LassoRenderer() = default;

    // Non-copyable
    LassoRenderer(const LassoRenderer&) = delete;
    LassoRenderer& operator=(const LassoRenderer&) = delete;

    /**
     * @brief Initialize the renderer
     * @param pDevice Render device
     * @param pSwapChain Swap chain for format info
     * @return true if initialization succeeded
     */
    bool initialize(Diligent::IRenderDevice* pDevice, Diligent::ISwapChain* pSwapChain);

    /**
     * @brief Begin a new lasso path
     * @param x, y Starting position in screen pixels
     */
    void beginPath(int x, int y);

    /**
     * @brief Add a point to the lasso path
     * @param x, y Position in screen pixels
     */
    void addPoint(int x, int y);

    /**
     * @brief Clear the lasso path
     */
    void clearPath();

    /**
     * @brief Get the current lasso path points
     */
    const std::vector<std::pair<int, int>>& getPath() const { return m_pathPoints; }

    /**
     * @brief Check if the lasso path has enough points to form a polygon
     */
    bool hasValidPath() const { return m_pathPoints.size() >= 3; }

    /**
     * @brief Render the lasso path
     * @param pContext Device context
     * @param viewportWidth, viewportHeight Current viewport size
     * @param closePath Whether to close the path (connect last point to first)
     */
    void render(Diligent::IDeviceContext* pContext,
                int viewportWidth, int viewportHeight,
                bool closePath = false);

    bool isInitialized() const { return m_initialized; }

    // Maximum number of vertices (to limit buffer size)
    static constexpr size_t MAX_PATH_POINTS = 4096;

    // Minimum distance between points (to avoid too many points)
    static constexpr int MIN_POINT_DISTANCE = 3;

private:
    bool createPipeline(Diligent::IRenderDevice* pDevice, Diligent::ISwapChain* pSwapChain);

    // Pipeline for filled polygon
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pFillPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pFillSRB;

    // Pipeline for border (line strip)
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pBorderPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pBorderSRB;

    // Buffers
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pVertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pConstantBuffer;

    // Path points in screen coordinates
    std::vector<std::pair<int, int>> m_pathPoints;

    bool m_initialized = false;
};

} // namespace MoldWing
