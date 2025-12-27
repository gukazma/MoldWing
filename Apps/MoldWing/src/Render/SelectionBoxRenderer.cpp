/*
 *  MoldWing - Selection Box Renderer Implementation
 *  Renders 2D selection rectangle overlay in screen space
 */

#include "SelectionBoxRenderer.hpp"
#include "Core/Logger.hpp"

#include <Graphics/GraphicsTools/interface/MapHelper.hpp>

using namespace Diligent;

namespace MoldWing
{

namespace
{
    // Simple 2D vertex shader - takes NDC coordinates directly
    const char* Box2DVS = R"(
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
    const char* Box2DPS = R"(
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

bool SelectionBoxRenderer::initialize(IRenderDevice* pDevice, ISwapChain* pSwapChain)
{
    if (m_initialized)
        return true;

    if (!createPipeline(pDevice, pSwapChain))
    {
        MW_LOG_ERROR("SelectionBoxRenderer: Failed to create pipeline");
        return false;
    }

    m_initialized = true;
    LOG_DEBUG("SelectionBoxRenderer initialized");
    return true;
}

bool SelectionBoxRenderer::createPipeline(IRenderDevice* pDevice, ISwapChain* pSwapChain)
{
    // Create shaders
    ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    shaderCI.Desc.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShader> pVS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        shaderCI.Desc.Name = "SelectionBox VS";
        shaderCI.Source = Box2DVS;
        shaderCI.EntryPoint = "main";
        pDevice->CreateShader(shaderCI, &pVS);
        if (!pVS)
        {
            MW_LOG_ERROR("SelectionBoxRenderer: Failed to create vertex shader");
            return false;
        }
    }

    RefCntAutoPtr<IShader> pPS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        shaderCI.Desc.Name = "SelectionBox PS";
        shaderCI.Source = Box2DPS;
        shaderCI.EntryPoint = "main";
        pDevice->CreateShader(shaderCI, &pPS);
        if (!pPS)
        {
            MW_LOG_ERROR("SelectionBoxRenderer: Failed to create pixel shader");
            return false;
        }
    }

    // Input layout for 2D positions
    LayoutElement layoutElems[] =
    {
        {0, 0, 2, VT_FLOAT32, False}  // Position (x, y)
    };

    // Create fill PSO (triangle strip for quad)
    {
        GraphicsPipelineStateCreateInfo psoCI;
        psoCI.PSODesc.Name = "SelectionBox Fill PSO";
        psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

        psoCI.GraphicsPipeline.NumRenderTargets = 1;
        psoCI.GraphicsPipeline.RTVFormats[0] = pSwapChain->GetDesc().ColorBufferFormat;
        psoCI.GraphicsPipeline.DSVFormat = pSwapChain->GetDesc().DepthBufferFormat;
        psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

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
            MW_LOG_ERROR("SelectionBoxRenderer: Failed to create fill PSO");
            return false;
        }
    }

    // Create border PSO (line strip)
    {
        GraphicsPipelineStateCreateInfo psoCI;
        psoCI.PSODesc.Name = "SelectionBox Border PSO";
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
            MW_LOG_ERROR("SelectionBoxRenderer: Failed to create border PSO");
            return false;
        }
    }

    // Create constant buffer
    BufferDesc cbDesc;
    cbDesc.Name = "SelectionBox Constants CB";
    cbDesc.Size = sizeof(Constants);
    cbDesc.Usage = USAGE_DYNAMIC;
    cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    pDevice->CreateBuffer(cbDesc, nullptr, &m_pConstantBuffer);
    if (!m_pConstantBuffer)
    {
        MW_LOG_ERROR("SelectionBoxRenderer: Failed to create constant buffer");
        return false;
    }

    // Bind constant buffer to both PSOs
    m_pFillPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pConstantBuffer);
    m_pBorderPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pConstantBuffer);

    // Create SRBs
    m_pFillPSO->CreateShaderResourceBinding(&m_pFillSRB, true);
    m_pBorderPSO->CreateShaderResourceBinding(&m_pBorderSRB, true);

    // Create dynamic vertex buffer (enough for 5 vertices: 4 for quad + 1 to close line loop)
    BufferDesc vbDesc;
    vbDesc.Name = "SelectionBox VB";
    vbDesc.Size = sizeof(Vertex2D) * 5;
    vbDesc.Usage = USAGE_DYNAMIC;
    vbDesc.BindFlags = BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    pDevice->CreateBuffer(vbDesc, nullptr, &m_pVertexBuffer);
    if (!m_pVertexBuffer)
    {
        MW_LOG_ERROR("SelectionBoxRenderer: Failed to create vertex buffer");
        return false;
    }

    return true;
}

void SelectionBoxRenderer::render(IDeviceContext* pContext,
                                   int x1, int y1, int x2, int y2,
                                   int viewportWidth, int viewportHeight)
{
    if (!m_initialized)
        return;

    // Convert screen coordinates to NDC (-1 to 1)
    // Screen: (0,0) top-left, (w,h) bottom-right
    // NDC: (-1,1) top-left, (1,-1) bottom-right
    float ndcX1 = (2.0f * x1 / viewportWidth) - 1.0f;
    float ndcY1 = 1.0f - (2.0f * y1 / viewportHeight);
    float ndcX2 = (2.0f * x2 / viewportWidth) - 1.0f;
    float ndcY2 = 1.0f - (2.0f * y2 / viewportHeight);

    // Update vertex buffer with quad vertices (triangle strip order)
    // For triangle strip: top-left, bottom-left, top-right, bottom-right
    {
        MapHelper<Vertex2D> verts(pContext, m_pVertexBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        // Triangle strip order for quad
        verts[0] = {ndcX1, ndcY1};  // top-left
        verts[1] = {ndcX1, ndcY2};  // bottom-left
        verts[2] = {ndcX2, ndcY1};  // top-right
        verts[3] = {ndcX2, ndcY2};  // bottom-right
        // For line strip: close the loop
        verts[4] = {ndcX1, ndcY1};  // back to top-left
    }

    IBuffer* pBuffs[] = {m_pVertexBuffer};

    // Draw filled rectangle with semi-transparent blue
    {
        MapHelper<Constants> cb(pContext, m_pConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        cb->Color[0] = 51.0f / 255.0f;   // R
        cb->Color[1] = 153.0f / 255.0f;  // G
        cb->Color[2] = 255.0f / 255.0f;  // B
        cb->Color[3] = 0.23f;            // A (60/255)
    }

    pContext->SetPipelineState(m_pFillPSO);
    pContext->CommitShaderResources(m_pFillSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->SetVertexBuffers(0, 1, pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                SET_VERTEX_BUFFERS_FLAG_RESET);

    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 4;
    drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    pContext->Draw(drawAttrs);

    // Draw border with more opaque blue
    {
        // Reorder vertices for line strip: top-left, top-right, bottom-right, bottom-left, top-left
        MapHelper<Vertex2D> verts(pContext, m_pVertexBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        verts[0] = {ndcX1, ndcY1};  // top-left
        verts[1] = {ndcX2, ndcY1};  // top-right
        verts[2] = {ndcX2, ndcY2};  // bottom-right
        verts[3] = {ndcX1, ndcY2};  // bottom-left
        verts[4] = {ndcX1, ndcY1};  // close loop
    }

    {
        MapHelper<Constants> cb(pContext, m_pConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        cb->Color[0] = 51.0f / 255.0f;
        cb->Color[1] = 153.0f / 255.0f;
        cb->Color[2] = 255.0f / 255.0f;
        cb->Color[3] = 0.78f;  // More opaque (200/255)
    }

    pContext->SetPipelineState(m_pBorderPSO);
    pContext->CommitShaderResources(m_pBorderSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->SetVertexBuffers(0, 1, pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                SET_VERTEX_BUFFERS_FLAG_RESET);

    drawAttrs.NumVertices = 5;
    pContext->Draw(drawAttrs);
}

} // namespace MoldWing
