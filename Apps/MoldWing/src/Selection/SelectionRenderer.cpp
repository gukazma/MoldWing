/*
 *  MoldWing - Selection Renderer Implementation
 *  S2.4: Render selected faces with highlight overlay
 */

#include "SelectionRenderer.hpp"
#include "Core/Logger.hpp"

#include <Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <cstring>

using namespace Diligent;

namespace MoldWing
{

namespace
{
    // Vertex shader for selection highlight
    const char* SelectionVS = R"(
cbuffer Constants
{
    row_major float4x4 g_WorldViewProj;
    float4   g_HighlightColor;
};

struct VSInput
{
    float3 Pos      : ATTRIB0;
    float3 Normal   : ATTRIB1;
    float2 TexCoord : ATTRIB2;
};

struct PSInput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    // Transform position directly (depth bias handles z-fighting)
    PSIn.Pos = mul(float4(VSIn.Pos, 1.0), g_WorldViewProj);
    PSIn.Color = g_HighlightColor;
}
)";

    // Pixel shader for selection highlight
    const char* SelectionPS = R"(
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

    // Constant buffer structure
    struct Constants
    {
        float WorldViewProj[16];
        float HighlightColor[4];
    };

    void MatrixMultiply(const float* a, const float* b, float* result)
    {
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                result[i * 4 + j] = 0;
                for (int k = 0; k < 4; k++)
                {
                    result[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
                }
            }
        }
    }

    void MatrixIdentity(float* m)
    {
        std::memset(m, 0, 16 * sizeof(float));
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }
}

bool SelectionRenderer::initialize(IRenderDevice* pDevice, ISwapChain* pSwapChain)
{
    if (m_initialized)
        return true;

    m_pDevice = pDevice;

    if (!createPipeline(pDevice, pSwapChain))
    {
        MW_LOG_ERROR("SelectionRenderer: Failed to create pipeline");
        return false;
    }

    m_initialized = true;
    LOG_INFO("SelectionRenderer initialized");
    return true;
}

bool SelectionRenderer::createPipeline(IRenderDevice* pDevice, ISwapChain* pSwapChain)
{
    // Create vertex shader
    ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    shaderCI.Desc.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShader> pVS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        shaderCI.Desc.Name = "Selection VS";
        shaderCI.Source = SelectionVS;
        shaderCI.EntryPoint = "main";
        pDevice->CreateShader(shaderCI, &pVS);
        if (!pVS)
        {
            MW_LOG_ERROR("SelectionRenderer: Failed to create vertex shader");
            return false;
        }
    }

    RefCntAutoPtr<IShader> pPS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        shaderCI.Desc.Name = "Selection PS";
        shaderCI.Source = SelectionPS;
        shaderCI.EntryPoint = "main";
        pDevice->CreateShader(shaderCI, &pPS);
        if (!pPS)
        {
            MW_LOG_ERROR("SelectionRenderer: Failed to create pixel shader");
            return false;
        }
    }

    // Create pipeline state
    GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name = "Selection PSO";
    psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    psoCI.GraphicsPipeline.NumRenderTargets = 1;
    psoCI.GraphicsPipeline.RTVFormats[0] = pSwapChain->GetDesc().ColorBufferFormat;
    psoCI.GraphicsPipeline.DSVFormat = pSwapChain->GetDesc().DepthBufferFormat;
    psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Rasterizer state
    psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;  // Draw both sides for selection
    psoCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;
    // Use depth bias to push selection in front of mesh (negative = closer to camera)
    psoCI.GraphicsPipeline.RasterizerDesc.DepthBias = -100;
    psoCI.GraphicsPipeline.RasterizerDesc.SlopeScaledDepthBias = -1.0f;
    psoCI.GraphicsPipeline.RasterizerDesc.DepthClipEnable = true;

    // Depth stencil - read depth but don't write (overlay on top)
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS_EQUAL;

    // Alpha blending for semi-transparent highlight
    auto& rt0 = psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0];
    rt0.BlendEnable = true;
    rt0.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    rt0.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    rt0.BlendOp = BLEND_OPERATION_ADD;
    rt0.SrcBlendAlpha = BLEND_FACTOR_ONE;
    rt0.DestBlendAlpha = BLEND_FACTOR_ZERO;
    rt0.BlendOpAlpha = BLEND_OPERATION_ADD;
    rt0.RenderTargetWriteMask = COLOR_MASK_ALL;

    // Input layout (same as mesh renderer)
    LayoutElement layoutElems[] =
    {
        {0, 0, 3, VT_FLOAT32, False}, // Position
        {1, 0, 3, VT_FLOAT32, False}, // Normal
        {2, 0, 2, VT_FLOAT32, False}, // TexCoord
    };
    psoCI.GraphicsPipeline.InputLayout.LayoutElements = layoutElems;
    psoCI.GraphicsPipeline.InputLayout.NumElements = _countof(layoutElems);

    psoCI.pVS = pVS;
    psoCI.pPS = pPS;

    // Shader resource variables
    ShaderResourceVariableDesc varDesc[] =
    {
        {SHADER_TYPE_VERTEX, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
    };
    psoCI.PSODesc.ResourceLayout.Variables = varDesc;
    psoCI.PSODesc.ResourceLayout.NumVariables = _countof(varDesc);

    pDevice->CreateGraphicsPipelineState(psoCI, &m_pPSO);
    if (!m_pPSO)
    {
        MW_LOG_ERROR("SelectionRenderer: Failed to create pipeline state");
        return false;
    }

    // Create constant buffer
    BufferDesc cbDesc;
    cbDesc.Name = "Selection Constants CB";
    cbDesc.Size = sizeof(Constants);
    cbDesc.Usage = USAGE_DYNAMIC;
    cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    pDevice->CreateBuffer(cbDesc, nullptr, &m_pConstantBuffer);
    if (!m_pConstantBuffer)
    {
        MW_LOG_ERROR("SelectionRenderer: Failed to create constant buffer");
        return false;
    }

    // Bind constant buffer
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pConstantBuffer);

    // Create SRB
    m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);

    return true;
}

bool SelectionRenderer::loadMesh(const MeshData& mesh)
{
    if (!m_pDevice || mesh.vertices.empty())
        return false;

    m_pMeshData = &mesh;

    // Create vertex buffer
    BufferDesc vbDesc;
    vbDesc.Name = "Selection VB";
    vbDesc.Size = static_cast<Uint64>(mesh.vertices.size() * sizeof(Vertex));
    vbDesc.Usage = USAGE_IMMUTABLE;
    vbDesc.BindFlags = BIND_VERTEX_BUFFER;

    BufferData vbData;
    vbData.pData = mesh.vertices.data();
    vbData.DataSize = vbDesc.Size;

    m_pDevice->CreateBuffer(vbDesc, &vbData, &m_pVertexBuffer);
    if (!m_pVertexBuffer)
        return false;

    m_vertexCount = static_cast<uint32_t>(mesh.vertices.size());

    // Create a dynamic index buffer for selection (max size = full mesh)
    BufferDesc ibDesc;
    ibDesc.Name = "Selection IB";
    ibDesc.Size = static_cast<Uint64>(mesh.indices.size() * sizeof(uint32_t));
    ibDesc.Usage = USAGE_DYNAMIC;
    ibDesc.BindFlags = BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

    m_pDevice->CreateBuffer(ibDesc, nullptr, &m_pSelectionIndexBuffer);
    if (!m_pSelectionIndexBuffer)
        return false;

    LOG_DEBUG("SelectionRenderer loaded mesh: {} vertices", m_vertexCount);
    return true;
}

void SelectionRenderer::updateSelection(const std::unordered_set<uint32_t>& selectedFaces)
{
    if (!m_pMeshData || !m_pSelectionIndexBuffer)
    {
        m_selectionIndexCount = 0;
        m_cachedSelectionIndices.clear();
        return;
    }

    if (selectedFaces.empty())
    {
        m_selectionIndexCount = 0;
        m_cachedSelectionIndices.clear();
        return;
    }

    // Build index list for selected faces
    m_cachedSelectionIndices.clear();
    m_cachedSelectionIndices.reserve(selectedFaces.size() * 3);

    for (uint32_t faceIdx : selectedFaces)
    {
        uint32_t baseIdx = faceIdx * 3;
        if (baseIdx + 2 < m_pMeshData->indices.size())
        {
            m_cachedSelectionIndices.push_back(m_pMeshData->indices[baseIdx]);
            m_cachedSelectionIndices.push_back(m_pMeshData->indices[baseIdx + 1]);
            m_cachedSelectionIndices.push_back(m_pMeshData->indices[baseIdx + 2]);
        }
    }

    m_selectionIndexCount = static_cast<uint32_t>(m_cachedSelectionIndices.size());
    m_selectionDirty = true;

    LOG_DEBUG("Selection updated: {} faces, {} indices", selectedFaces.size(), m_selectionIndexCount);
}

void SelectionRenderer::render(IDeviceContext* pContext, const OrbitCamera& camera)
{
    if (!m_initialized || !m_pVertexBuffer || m_selectionIndexCount == 0)
        return;

    // Update selection index buffer if dirty
    if (m_selectionDirty && !m_cachedSelectionIndices.empty())
    {
        MapHelper<uint32_t> indices(pContext, m_pSelectionIndexBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        std::memcpy(indices, m_cachedSelectionIndices.data(),
                    m_cachedSelectionIndices.size() * sizeof(uint32_t));
        m_selectionDirty = false;
    }

    // Update constant buffer
    {
        MapHelper<Constants> cb(pContext, m_pConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);

        // World matrix (identity)
        float world[16];
        MatrixIdentity(world);

        // View and projection matrices
        float view[16], proj[16];
        camera.getViewMatrix(view);
        camera.getProjectionMatrix(proj);

        // WorldViewProj
        float viewProj[16];
        MatrixMultiply(view, proj, viewProj);
        MatrixMultiply(world, viewProj, cb->WorldViewProj);

        // Highlight color
        std::memcpy(cb->HighlightColor, m_highlightColor, sizeof(m_highlightColor));
    }

    // Set pipeline
    pContext->SetPipelineState(m_pPSO);
    pContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set buffers
    IBuffer* pBuffs[] = {m_pVertexBuffer};
    pContext->SetVertexBuffers(0, 1, pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                SET_VERTEX_BUFFERS_FLAG_RESET);
    pContext->SetIndexBuffer(m_pSelectionIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Draw
    DrawIndexedAttribs drawAttrs;
    drawAttrs.IndexType = VT_UINT32;
    drawAttrs.NumIndices = m_selectionIndexCount;
    drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    pContext->DrawIndexed(drawAttrs);
}

void SelectionRenderer::setHighlightColor(float r, float g, float b, float a)
{
    m_highlightColor[0] = r;
    m_highlightColor[1] = g;
    m_highlightColor[2] = b;
    m_highlightColor[3] = a;
}

} // namespace MoldWing
