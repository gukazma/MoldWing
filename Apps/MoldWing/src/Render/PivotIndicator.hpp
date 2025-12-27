/*
 *  MoldWing - Pivot Indicator
 *  Visual indicator for rotation pivot point
 */

#pragma once

#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Common/interface/RefCntAutoPtr.hpp"
#include "OrbitCamera.hpp"

namespace MoldWing
{

class PivotIndicator
{
public:
    PivotIndicator() = default;
    ~PivotIndicator() = default;

    void initialize(Diligent::IRenderDevice* pDevice);

    void render(Diligent::IDeviceContext* pContext,
                const OrbitCamera& camera,
                float pivotX, float pivotY, float pivotZ,
                float size);

    bool isInitialized() const { return m_initialized; }

private:
    void createPipeline(Diligent::IRenderDevice* pDevice);

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> m_pDevice;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pSRB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pVertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pConstantBuffer;

    bool m_initialized = false;
};

} // namespace MoldWing
