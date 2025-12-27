/*
 *  MoldWing - Brush Cursor Renderer Implementation
 *  Renders 2D circular brush cursor overlay in screen space
 */

#include "BrushCursorRenderer.hpp"
#include "Core/Logger.hpp"

#include <Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <cmath>

using namespace Diligent;

namespace MoldWing
{

namespace
{
    // Simple 2D vertex shader - takes NDC coordinates directly
    const char* Circle2DVS = R"(
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
    const char* Circle2DPS = R"(
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

bool BrushCursorRenderer::initialize(IRenderDevice* pDevice, ISwapChain* pSwapChain)
{
    if (m_initialized)
        return true;

    if (!createPipeline(pDevice, pSwapChain))
    {
        MW_LOG_ERROR("BrushCursorRenderer: Failed to create pipeline");
        return false;
    }

    m_initialized = true;
    LOG_DEBUG("BrushCursorRenderer initialized");
    return true;
}

bool BrushCursorRenderer::createPipeline(IRenderDevice* pDevice, ISwapChain* pSwapChain)
{
    // Create shaders
    ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    shaderCI.Desc.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShader> pVS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        shaderCI.Desc.Name = "BrushCursor VS";
        shaderCI.Source = Circle2DVS;
        shaderCI.EntryPoint = "main";
        pDevice->CreateShader(shaderCI, &pVS);
        if (!pVS)
        {
            MW_LOG_ERROR("BrushCursorRenderer: Failed to create vertex shader");
            return false;
        }
    }

    RefCntAutoPtr<IShader> pPS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        shaderCI.Desc.Name = "BrushCursor PS";
        shaderCI.Source = Circle2DPS;
        shaderCI.EntryPoint = "main";
        pDevice->CreateShader(shaderCI, &pPS);
        if (!pPS)
        {
            MW_LOG_ERROR("BrushCursorRenderer: Failed to create pixel shader");
            return false;
        }
    }

    // Input layout for 2D positions
    LayoutElement layoutElems[] =
    {
        {0, 0, 2, VT_FLOAT32, False}  // Position (x, y)
    };

    // Create fill PSO (triangle list for circle - more compatible)
    {
        GraphicsPipelineStateCreateInfo psoCI;
        psoCI.PSODesc.Name = "BrushCursor Fill PSO";
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
            MW_LOG_ERROR("BrushCursorRenderer: Failed to create fill PSO");
            return false;
        }
    }

    // Create border PSO (line strip for circle outline)
    {
        GraphicsPipelineStateCreateInfo psoCI;
        psoCI.PSODesc.Name = "BrushCursor Border PSO";
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
            MW_LOG_ERROR("BrushCursorRenderer: Failed to create border PSO");
            return false;
        }
    }

    // Create constant buffer
    BufferDesc cbDesc;
    cbDesc.Name = "BrushCursor Constants CB";
    cbDesc.Size = sizeof(Constants);
    cbDesc.Usage = USAGE_DYNAMIC;
    cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    pDevice->CreateBuffer(cbDesc, nullptr, &m_pConstantBuffer);
    if (!m_pConstantBuffer)
    {
        MW_LOG_ERROR("BrushCursorRenderer: Failed to create constant buffer");
        return false;
    }

    // Bind constant buffer to both PSOs
    m_pFillPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pConstantBuffer);
    m_pBorderPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pConstantBuffer);

    // Create SRBs
    m_pFillPSO->CreateShaderResourceBinding(&m_pFillSRB, true);
    m_pBorderPSO->CreateShaderResourceBinding(&m_pBorderSRB, true);

    // Create dynamic vertex buffer
    // For triangle list: CIRCLE_SEGMENTS * 3 vertices (each triangle: center + 2 edge points)
    // For line strip: CIRCLE_SEGMENTS + 1 (to close the circle)
    BufferDesc vbDesc;
    vbDesc.Name = "BrushCursor VB";
    vbDesc.Size = sizeof(Vertex2D) * (CIRCLE_SEGMENTS * 3);
    vbDesc.Usage = USAGE_DYNAMIC;
    vbDesc.BindFlags = BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    pDevice->CreateBuffer(vbDesc, nullptr, &m_pVertexBuffer);
    if (!m_pVertexBuffer)
    {
        MW_LOG_ERROR("BrushCursorRenderer: Failed to create vertex buffer");
        return false;
    }

    return true;
}

void BrushCursorRenderer::render(IDeviceContext* pContext,
                                  int centerX, int centerY, int radius,
                                  int viewportWidth, int viewportHeight)
{
    if (!m_initialized)
        return;

    // Don't render if radius is too small
    if (radius < 2)
        return;

    // Convert screen coordinates to NDC
    float ndcCenterX = (2.0f * centerX / viewportWidth) - 1.0f;
    float ndcCenterY = 1.0f - (2.0f * centerY / viewportHeight);
    float ndcRadiusX = 2.0f * radius / viewportWidth;
    float ndcRadiusY = 2.0f * radius / viewportHeight;

    IBuffer* pBuffs[] = {m_pVertexBuffer};

    constexpr float PI = 3.14159265358979323846f;

    // Generate circle vertices for triangle list (filled circle)
    {
        MapHelper<Vertex2D> verts(pContext, m_pVertexBuffer, MAP_WRITE, MAP_FLAG_DISCARD);

        // Each triangle: center, edge point i, edge point i+1
        for (int i = 0; i < CIRCLE_SEGMENTS; ++i)
        {
            float angle1 = 2.0f * PI * i / CIRCLE_SEGMENTS;
            float angle2 = 2.0f * PI * (i + 1) / CIRCLE_SEGMENTS;

            float x1 = ndcCenterX + ndcRadiusX * std::cos(angle1);
            float y1 = ndcCenterY + ndcRadiusY * std::sin(angle1);
            float x2 = ndcCenterX + ndcRadiusX * std::cos(angle2);
            float y2 = ndcCenterY + ndcRadiusY * std::sin(angle2);

            int baseIdx = i * 3;
            verts[baseIdx + 0] = {ndcCenterX, ndcCenterY};  // Center
            verts[baseIdx + 1] = {x1, y1};                   // Edge point 1
            verts[baseIdx + 2] = {x2, y2};                   // Edge point 2
        }
    }

    // Draw filled circle with semi-transparent color
    {
        MapHelper<Constants> cb(pContext, m_pConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        cb->Color[0] = 255.0f / 255.0f;  // R - orange/yellow
        cb->Color[1] = 200.0f / 255.0f;  // G
        cb->Color[2] = 50.0f / 255.0f;   // B
        cb->Color[3] = 0.15f;            // A - very transparent
    }

    pContext->SetPipelineState(m_pFillPSO);
    pContext->CommitShaderResources(m_pFillSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->SetVertexBuffers(0, 1, pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                SET_VERTEX_BUFFERS_FLAG_RESET);

    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = CIRCLE_SEGMENTS * 3;  // triangle list vertices
    drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    pContext->Draw(drawAttrs);

    // Draw border circle (line strip)
    {
        MapHelper<Vertex2D> verts(pContext, m_pVertexBuffer, MAP_WRITE, MAP_FLAG_DISCARD);

        // Circle points for line strip (no center needed)
        for (int i = 0; i <= CIRCLE_SEGMENTS; ++i)
        {
            float angle = 2.0f * PI * i / CIRCLE_SEGMENTS;
            float x = ndcCenterX + ndcRadiusX * std::cos(angle);
            float y = ndcCenterY + ndcRadiusY * std::sin(angle);
            verts[i] = {x, y};
        }
    }

    // Draw border with more opaque color
    {
        MapHelper<Constants> cb(pContext, m_pConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        cb->Color[0] = 255.0f / 255.0f;  // R
        cb->Color[1] = 200.0f / 255.0f;  // G
        cb->Color[2] = 50.0f / 255.0f;   // B
        cb->Color[3] = 0.9f;             // A - more opaque border
    }

    pContext->SetPipelineState(m_pBorderPSO);
    pContext->CommitShaderResources(m_pBorderSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->SetVertexBuffers(0, 1, pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                SET_VERTEX_BUFFERS_FLAG_RESET);

    drawAttrs.NumVertices = CIRCLE_SEGMENTS + 1;
    pContext->Draw(drawAttrs);
}

} // namespace MoldWing
