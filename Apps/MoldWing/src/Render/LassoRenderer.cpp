/*
 *  MoldWing - Lasso Renderer Implementation
 *  Renders 2D lasso selection path overlay in screen space
 */

#include "LassoRenderer.hpp"
#include "Core/Logger.hpp"

#include <Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <cmath>
#include <algorithm>

using namespace Diligent;

namespace MoldWing
{

namespace
{
    // Simple 2D vertex shader - takes NDC coordinates directly
    const char* Lasso2DVS = R"(
cbuffer Constants
{
    float4 g_Color;
};

struct VSInput
{
    float2 Pos : ATTRIB0;
};

struct PSInput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    PSIn.Pos = float4(VSIn.Pos, 0.0, 1.0);
    PSIn.Color = g_Color;
}
)";

    // Simple pass-through pixel shader
    const char* Lasso2DPS = R"(
struct PSInput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR;
};

float4 main(in PSInput PSIn) : SV_Target
{
    return PSIn.Color;
}
)";

    struct Constants
    {
        float Color[4];
    };

    // Vertex structure for 2D positions
    struct Vertex2D
    {
        float x, y;
    };
}

bool LassoRenderer::initialize(IRenderDevice* pDevice, ISwapChain* pSwapChain)
{
    if (m_initialized)
        return true;

    if (!createPipeline(pDevice, pSwapChain))
    {
        MW_LOG_ERROR("LassoRenderer: Failed to create pipeline");
        return false;
    }

    m_initialized = true;
    LOG_DEBUG("LassoRenderer initialized");
    return true;
}

bool LassoRenderer::createPipeline(IRenderDevice* pDevice, ISwapChain* pSwapChain)
{
    // Create shaders
    ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    shaderCI.Desc.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShader> pVS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        shaderCI.Desc.Name = "Lasso VS";
        shaderCI.Source = Lasso2DVS;
        shaderCI.EntryPoint = "main";
        pDevice->CreateShader(shaderCI, &pVS);
        if (!pVS)
        {
            MW_LOG_ERROR("LassoRenderer: Failed to create vertex shader");
            return false;
        }
    }

    RefCntAutoPtr<IShader> pPS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        shaderCI.Desc.Name = "Lasso PS";
        shaderCI.Source = Lasso2DPS;
        shaderCI.EntryPoint = "main";
        pDevice->CreateShader(shaderCI, &pPS);
        if (!pPS)
        {
            MW_LOG_ERROR("LassoRenderer: Failed to create pixel shader");
            return false;
        }
    }

    // Input layout for 2D positions
    LayoutElement layoutElems[] =
    {
        {0, 0, 2, VT_FLOAT32, False}  // Position (x, y)
    };

    // Create fill PSO (triangle list for polygon using ear clipping or fan)
    {
        GraphicsPipelineStateCreateInfo psoCI;
        psoCI.PSODesc.Name = "Lasso Fill PSO";
        psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

        psoCI.GraphicsPipeline.NumRenderTargets = 1;
        psoCI.GraphicsPipeline.RTVFormats[0] = pSwapChain->GetDesc().ColorBufferFormat;
        psoCI.GraphicsPipeline.DSVFormat = pSwapChain->GetDesc().DepthBufferFormat;
        psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // No culling, no depth
        psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;

        // Alpha blending
        auto& rt0 = psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0];
        rt0.BlendEnable = true;
        rt0.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
        rt0.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
        rt0.BlendOp = BLEND_OPERATION_ADD;
        rt0.SrcBlendAlpha = BLEND_FACTOR_ONE;
        rt0.DestBlendAlpha = BLEND_FACTOR_ZERO;
        rt0.BlendOpAlpha = BLEND_OPERATION_ADD;
        rt0.RenderTargetWriteMask = COLOR_MASK_ALL;

        psoCI.GraphicsPipeline.InputLayout.LayoutElements = layoutElems;
        psoCI.GraphicsPipeline.InputLayout.NumElements = _countof(layoutElems);

        psoCI.pVS = pVS;
        psoCI.pPS = pPS;

        ShaderResourceVariableDesc varDesc[] =
        {
            {SHADER_TYPE_VERTEX, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
        };
        psoCI.PSODesc.ResourceLayout.Variables = varDesc;
        psoCI.PSODesc.ResourceLayout.NumVariables = _countof(varDesc);

        pDevice->CreateGraphicsPipelineState(psoCI, &m_pFillPSO);
        if (!m_pFillPSO)
        {
            MW_LOG_ERROR("LassoRenderer: Failed to create fill PSO");
            return false;
        }
    }

    // Create border PSO (line strip for polygon outline)
    {
        GraphicsPipelineStateCreateInfo psoCI;
        psoCI.PSODesc.Name = "Lasso Border PSO";
        psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

        psoCI.GraphicsPipeline.NumRenderTargets = 1;
        psoCI.GraphicsPipeline.RTVFormats[0] = pSwapChain->GetDesc().ColorBufferFormat;
        psoCI.GraphicsPipeline.DSVFormat = pSwapChain->GetDesc().DepthBufferFormat;
        psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_LINE_STRIP;

        psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;

        // Alpha blending for border too
        auto& rt0 = psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0];
        rt0.BlendEnable = true;
        rt0.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
        rt0.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
        rt0.BlendOp = BLEND_OPERATION_ADD;
        rt0.SrcBlendAlpha = BLEND_FACTOR_ONE;
        rt0.DestBlendAlpha = BLEND_FACTOR_ZERO;
        rt0.BlendOpAlpha = BLEND_OPERATION_ADD;
        rt0.RenderTargetWriteMask = COLOR_MASK_ALL;

        psoCI.GraphicsPipeline.InputLayout.LayoutElements = layoutElems;
        psoCI.GraphicsPipeline.InputLayout.NumElements = _countof(layoutElems);

        psoCI.pVS = pVS;
        psoCI.pPS = pPS;

        ShaderResourceVariableDesc varDesc[] =
        {
            {SHADER_TYPE_VERTEX, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
        };
        psoCI.PSODesc.ResourceLayout.Variables = varDesc;
        psoCI.PSODesc.ResourceLayout.NumVariables = _countof(varDesc);

        pDevice->CreateGraphicsPipelineState(psoCI, &m_pBorderPSO);
        if (!m_pBorderPSO)
        {
            MW_LOG_ERROR("LassoRenderer: Failed to create border PSO");
            return false;
        }
    }

    // Create constant buffer
    BufferDesc cbDesc;
    cbDesc.Name = "Lasso Constants CB";
    cbDesc.Size = sizeof(Constants);
    cbDesc.Usage = USAGE_DYNAMIC;
    cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    pDevice->CreateBuffer(cbDesc, nullptr, &m_pConstantBuffer);
    if (!m_pConstantBuffer)
    {
        MW_LOG_ERROR("LassoRenderer: Failed to create constant buffer");
        return false;
    }

    // Bind constant buffer to both PSOs
    m_pFillPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pConstantBuffer);
    m_pBorderPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pConstantBuffer);

    // Create SRBs
    m_pFillPSO->CreateShaderResourceBinding(&m_pFillSRB, true);
    m_pBorderPSO->CreateShaderResourceBinding(&m_pBorderSRB, true);

    // Create dynamic vertex buffer
    // For triangle fan converted to list: (n-2) * 3 triangles for n points
    // For line strip: n + 1 (to close the loop)
    BufferDesc vbDesc;
    vbDesc.Name = "Lasso VB";
    vbDesc.Size = sizeof(Vertex2D) * MAX_PATH_POINTS * 3;  // Enough for triangulation
    vbDesc.Usage = USAGE_DYNAMIC;
    vbDesc.BindFlags = BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    pDevice->CreateBuffer(vbDesc, nullptr, &m_pVertexBuffer);
    if (!m_pVertexBuffer)
    {
        MW_LOG_ERROR("LassoRenderer: Failed to create vertex buffer");
        return false;
    }

    return true;
}

void LassoRenderer::beginPath(int x, int y)
{
    m_pathPoints.clear();
    m_pathPoints.emplace_back(x, y);
}

void LassoRenderer::addPoint(int x, int y)
{
    if (m_pathPoints.size() >= MAX_PATH_POINTS)
        return;

    // Only add point if it's far enough from the last point
    if (!m_pathPoints.empty())
    {
        const auto& last = m_pathPoints.back();
        int dx = x - last.first;
        int dy = y - last.second;
        int distSq = dx * dx + dy * dy;
        if (distSq < MIN_POINT_DISTANCE * MIN_POINT_DISTANCE)
            return;
    }

    m_pathPoints.emplace_back(x, y);
}

void LassoRenderer::clearPath()
{
    m_pathPoints.clear();
}

void LassoRenderer::render(IDeviceContext* pContext,
                            int viewportWidth, int viewportHeight,
                            bool closePath)
{
    if (!m_initialized)
        return;

    // Need at least 2 points to draw a line
    if (m_pathPoints.size() < 2)
        return;

    size_t numPoints = m_pathPoints.size();
    IBuffer* pBuffs[] = {m_pVertexBuffer};

    // Convert screen coordinates to NDC
    auto screenToNDC = [viewportWidth, viewportHeight](int sx, int sy, float& ndcX, float& ndcY)
    {
        ndcX = (2.0f * sx / viewportWidth) - 1.0f;
        ndcY = 1.0f - (2.0f * sy / viewportHeight);
    };

    // Draw border line only (fill removed - triangle fan doesn't work for non-convex polygons)
    {
        MapHelper<Vertex2D> verts(pContext, m_pVertexBuffer, MAP_WRITE, MAP_FLAG_DISCARD);

        // Line strip points
        for (size_t i = 0; i < numPoints; ++i)
        {
            float x, y;
            screenToNDC(m_pathPoints[i].first, m_pathPoints[i].second, x, y);
            verts[i] = {x, y};
        }

        // Determine number of vertices to draw
        Uint32 numVertsToDraw = static_cast<Uint32>(numPoints);

        // Close the loop by adding the first point again (only if closePath is true)
        if (closePath)
        {
            float x0, y0;
            screenToNDC(m_pathPoints[0].first, m_pathPoints[0].second, x0, y0);
            verts[numPoints] = {x0, y0};
            numVertsToDraw = static_cast<Uint32>(numPoints + 1);
        }

        // Set border color (cyan/teal)
        {
            MapHelper<Constants> cb(pContext, m_pConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
            cb->Color[0] = 50.0f / 255.0f;   // R
            cb->Color[1] = 200.0f / 255.0f;  // G
            cb->Color[2] = 255.0f / 255.0f;  // B
            cb->Color[3] = 1.0f;             // A - fully opaque
        }

        pContext->SetPipelineState(m_pBorderPSO);
        pContext->CommitShaderResources(m_pBorderSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        pContext->SetVertexBuffers(0, 1, pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                    SET_VERTEX_BUFFERS_FLAG_RESET);

        DrawAttribs drawAttrs;
        drawAttrs.NumVertices = numVertsToDraw;
        drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
        pContext->Draw(drawAttrs);
    }
}

} // namespace MoldWing
