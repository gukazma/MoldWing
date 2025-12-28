/*
 *  MoldWing - Mesh Renderer Implementation
 *  S1.5: Render mesh with DiligentEngine
 *  T6.1.5-7: Extended for texture rendering
 */

#include "MeshRenderer.hpp"
#include "Core/Logger.hpp"

#include <Common/interface/BasicMath.hpp>
#include <Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <Graphics/GraphicsTools/interface/GraphicsUtilities.h>
#include <cstring>

using namespace Diligent;

namespace MoldWing
{

namespace
{
    // Vertex shader HLSL (T6.1.6: unchanged from before)
    const char* VSSource = R"(
cbuffer Constants
{
    row_major float4x4 g_WorldViewProj;
    row_major float4x4 g_World;
    float4   g_LightDir;
    float4   g_CameraPos;
    float4   g_Flags;  // x = hasTexture
};

struct VSInput
{
    float3 Pos      : ATTRIB0;
    float3 Normal   : ATTRIB1;
    float2 TexCoord : ATTRIB2;
};

struct PSInput
{
    float4 Pos      : SV_POSITION;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    PSIn.Pos      = mul(float4(VSIn.Pos, 1.0), g_WorldViewProj);
    PSIn.Normal   = mul(float4(VSIn.Normal, 0.0), g_World).xyz;
    PSIn.TexCoord = VSIn.TexCoord;
    PSIn.WorldPos = mul(float4(VSIn.Pos, 1.0), g_World).xyz;
}
)";

    // Pixel shader HLSL (T6.1.6: extended for texture sampling)
    const char* PSSource = R"(
cbuffer Constants
{
    row_major float4x4 g_WorldViewProj;
    row_major float4x4 g_World;
    float4   g_LightDir;
    float4   g_CameraPos;
    float4   g_Flags;  // x = hasTexture
};

Texture2D    g_Texture;
SamplerState g_Texture_sampler;

struct PSInput
{
    float4 Pos      : SV_POSITION;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 WorldPos : TEXCOORD1;
};

float4 main(in PSInput PSIn) : SV_Target
{
    // Normalize inputs
    float3 N = normalize(PSIn.Normal);
    float3 L = normalize(-g_LightDir.xyz);
    float3 V = normalize(g_CameraPos.xyz - PSIn.WorldPos);
    float3 H = normalize(L + V);

    // Lighting calculations
    float ambient = 0.2;
    float diffuse = max(dot(N, L), 0.0) * 0.7;
    float specular = pow(max(dot(N, H), 0.0), 32.0) * 0.3;

    // Base color: from texture or default gray
    float3 baseColor;
    if (g_Flags.x > 0.5)
    {
        baseColor = g_Texture.Sample(g_Texture_sampler, PSIn.TexCoord).rgb;
    }
    else
    {
        baseColor = float3(0.7, 0.7, 0.7);
    }

    // Combine lighting
    float3 color = baseColor * (ambient + diffuse) + float3(1, 1, 1) * specular;

    return float4(color, 1.0);
}
)";

    // Constant buffer structure (must match shader)
    struct Constants
    {
        float WorldViewProj[16];
        float World[16];
        float LightDir[4];
        float CameraPos[4];
        float Flags[4];  // x = hasTexture
    };

    // Matrix multiplication helper
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

bool MeshRenderer::initialize(IRenderDevice* pDevice, ISwapChain* pSwapChain)
{
    if (m_initialized)
        return true;

    m_pDevice = pDevice;

    if (!createPipeline(pDevice, pSwapChain))
        return false;

    m_initialized = true;
    return true;
}

bool MeshRenderer::createPipeline(IRenderDevice* pDevice, ISwapChain* pSwapChain)
{
    // Create vertex shader
    ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    shaderCI.Desc.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShader> pVS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        shaderCI.Desc.Name = "Mesh VS";
        shaderCI.Source = VSSource;
        shaderCI.EntryPoint = "main";
        pDevice->CreateShader(shaderCI, &pVS);
        if (!pVS)
            return false;
    }

    RefCntAutoPtr<IShader> pPS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        shaderCI.Desc.Name = "Mesh PS";
        shaderCI.Source = PSSource;
        shaderCI.EntryPoint = "main";
        pDevice->CreateShader(shaderCI, &pPS);
        if (!pPS)
            return false;
    }

    // Create pipeline state
    GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name = "Mesh PSO";
    psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    psoCI.GraphicsPipeline.NumRenderTargets = 1;
    psoCI.GraphicsPipeline.RTVFormats[0] = pSwapChain->GetDesc().ColorBufferFormat;
    psoCI.GraphicsPipeline.DSVFormat = pSwapChain->GetDesc().DepthBufferFormat;
    psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Rasterizer state
    psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    psoCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;  // OBJ files use CCW as front

    // Depth stencil state (standard depth)
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS;

    // Input layout
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

    // Shader resource variables (T6.1.7: add texture variable)
    ShaderResourceVariableDesc varDesc[] =
    {
        {SHADER_TYPE_VERTEX, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_PIXEL, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    psoCI.PSODesc.ResourceLayout.Variables = varDesc;
    psoCI.PSODesc.ResourceLayout.NumVariables = _countof(varDesc);

    // Immutable samplers (T6.1.5)
    SamplerDesc samplerDesc;
    samplerDesc.MinFilter = FILTER_TYPE_LINEAR;
    samplerDesc.MagFilter = FILTER_TYPE_LINEAR;
    samplerDesc.MipFilter = FILTER_TYPE_LINEAR;
    samplerDesc.AddressU = TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = TEXTURE_ADDRESS_WRAP;

    ImmutableSamplerDesc immutableSamplers[] =
    {
        {SHADER_TYPE_PIXEL, "g_Texture", samplerDesc}
    };
    psoCI.PSODesc.ResourceLayout.ImmutableSamplers = immutableSamplers;
    psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(immutableSamplers);

    pDevice->CreateGraphicsPipelineState(psoCI, &m_pPSO);
    if (!m_pPSO)
        return false;

    // Create constant buffer
    BufferDesc cbDesc;
    cbDesc.Name = "Constants CB";
    cbDesc.Size = sizeof(Constants);
    cbDesc.Usage = USAGE_DYNAMIC;
    cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    pDevice->CreateBuffer(cbDesc, nullptr, &m_pConstantBuffer);
    if (!m_pConstantBuffer)
        return false;

    // Bind constant buffer to PSO
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pConstantBuffer);
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "Constants")->Set(m_pConstantBuffer);

    // Create shader resource binding
    m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);

    return true;
}

bool MeshRenderer::createGPUTexture(const TextureData& texData, int index)
{
    if (!texData.isValid() || !m_pDevice)
        return false;

    TextureDesc texDesc;
    texDesc.Name = "Diffuse Texture";
    texDesc.Type = RESOURCE_DIM_TEX_2D;
    texDesc.Width = static_cast<Uint32>(texData.width());
    texDesc.Height = static_cast<Uint32>(texData.height());
    texDesc.Format = TEX_FORMAT_RGBA8_UNORM;
    texDesc.MipLevels = 1;
    texDesc.Usage = USAGE_DEFAULT;
    texDesc.BindFlags = BIND_SHADER_RESOURCE;

    TextureSubResData subResData;
    subResData.pData = texData.data();
    subResData.Stride = static_cast<Uint64>(texData.bytesPerLine());

    Diligent::TextureData texDataInit;
    texDataInit.pSubResources = &subResData;
    texDataInit.NumSubresources = 1;

    RefCntAutoPtr<ITexture> pTexture;
    m_pDevice->CreateTexture(texDesc, &texDataInit, &pTexture);
    if (!pTexture)
    {
        MW_LOG_ERROR("Failed to create GPU texture");
        return false;
    }

    // Ensure we have space in the vectors
    if (index >= static_cast<int>(m_textures.size()))
    {
        m_textures.resize(index + 1);
        m_textureSRVs.resize(index + 1);
    }

    m_textures[index] = pTexture;
    m_textureSRVs[index] = pTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    MW_LOG_INFO("Created GPU texture {} ({}x{})", index, texData.width(), texData.height());

    return true;
}

bool MeshRenderer::loadMesh(const MeshData& mesh)
{
    if (!m_pDevice || mesh.vertices.empty())
        return false;

    // Clear previous textures
    m_textures.clear();
    m_textureSRVs.clear();
    m_hasTextures = false;

    // Create vertex buffer
    BufferDesc vbDesc;
    vbDesc.Name = "Mesh VB";
    vbDesc.Size = static_cast<Uint64>(mesh.vertices.size() * sizeof(Vertex));
    vbDesc.Usage = USAGE_IMMUTABLE;
    vbDesc.BindFlags = BIND_VERTEX_BUFFER;

    BufferData vbData;
    vbData.pData = mesh.vertices.data();
    vbData.DataSize = vbDesc.Size;

    m_pDevice->CreateBuffer(vbDesc, &vbData, &m_pVertexBuffer);
    if (!m_pVertexBuffer)
        return false;

    // Create index buffer
    BufferDesc ibDesc;
    ibDesc.Name = "Mesh IB";
    ibDesc.Size = static_cast<Uint64>(mesh.indices.size() * sizeof(uint32_t));
    ibDesc.Usage = USAGE_IMMUTABLE;
    ibDesc.BindFlags = BIND_INDEX_BUFFER;

    BufferData ibData;
    ibData.pData = mesh.indices.data();
    ibData.DataSize = ibDesc.Size;

    m_pDevice->CreateBuffer(ibDesc, &ibData, &m_pIndexBuffer);
    if (!m_pIndexBuffer)
        return false;

    // Load textures (T6.1.5)
    for (size_t i = 0; i < mesh.textures.size(); ++i)
    {
        if (mesh.textures[i] && mesh.textures[i]->isValid())
        {
            if (createGPUTexture(*mesh.textures[i], static_cast<int>(i)))
            {
                m_hasTextures = true;
            }
        }
    }

    // Bind first texture to SRB if available
    if (m_hasTextures && !m_textureSRVs.empty() && m_textureSRVs[0])
    {
        auto* pVar = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture");
        if (pVar)
        {
            pVar->Set(m_textureSRVs[0]);
        }
    }

    m_vertexCount = static_cast<Uint32>(mesh.vertices.size());
    m_indexCount = static_cast<Uint32>(mesh.indices.size());
    m_bounds = mesh.bounds;

    return true;
}

void MeshRenderer::updateTexture(IDeviceContext* pContext, int textureIndex, const TextureData& texData)
{
    if (textureIndex < 0 || textureIndex >= static_cast<int>(m_textures.size()))
        return;

    auto& pTexture = m_textures[textureIndex];
    if (!pTexture || !texData.isValid())
        return;

    // Check dimensions match
    const auto& texDesc = pTexture->GetDesc();
    if (texDesc.Width != static_cast<Uint32>(texData.width()) ||
        texDesc.Height != static_cast<Uint32>(texData.height()))
    {
        MW_LOG_ERROR("Texture dimensions mismatch during update");
        return;
    }

    // Update texture data
    Box updateBox;
    updateBox.MinX = 0;
    updateBox.MinY = 0;
    updateBox.MaxX = texDesc.Width;
    updateBox.MaxY = texDesc.Height;

    TextureSubResData subResData;
    subResData.pData = texData.data();
    subResData.Stride = static_cast<Uint64>(texData.bytesPerLine());

    pContext->UpdateTexture(pTexture, 0, 0, updateBox, subResData,
                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void MeshRenderer::render(IDeviceContext* pContext, const OrbitCamera& camera)
{
    if (!m_initialized || !m_pVertexBuffer || !m_pIndexBuffer)
        return;

    // Update constant buffer
    {
        MapHelper<Constants> cb(pContext, m_pConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);

        // World matrix (identity for now)
        MatrixIdentity(cb->World);

        // View and projection matrices
        float view[16], proj[16];
        camera.getViewMatrix(view);
        camera.getProjectionMatrix(proj);

        // WorldViewProj = World * View * Proj
        float viewProj[16];
        MatrixMultiply(view, proj, viewProj);
        MatrixMultiply(cb->World, viewProj, cb->WorldViewProj);

        // Light direction (from top-right-front)
        cb->LightDir[0] = -0.5f;
        cb->LightDir[1] = -1.0f;
        cb->LightDir[2] = -0.5f;
        cb->LightDir[3] = 0.0f;

        // Camera position
        float camX, camY, camZ;
        camera.getPosition(camX, camY, camZ);
        cb->CameraPos[0] = camX;
        cb->CameraPos[1] = camY;
        cb->CameraPos[2] = camZ;
        cb->CameraPos[3] = 1.0f;

        // Flags (T6.1.7: hasTexture flag)
        cb->Flags[0] = m_hasTextures ? 1.0f : 0.0f;
        cb->Flags[1] = 0.0f;
        cb->Flags[2] = 0.0f;
        cb->Flags[3] = 0.0f;
    }

    // Set pipeline state
    pContext->SetPipelineState(m_pPSO);
    pContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set vertex/index buffers
    IBuffer* pBuffs[] = {m_pVertexBuffer};
    pContext->SetVertexBuffers(0, 1, pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                SET_VERTEX_BUFFERS_FLAG_RESET);
    pContext->SetIndexBuffer(m_pIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Draw
    DrawIndexedAttribs drawAttrs;
    drawAttrs.IndexType = VT_UINT32;
    drawAttrs.NumIndices = m_indexCount;
    drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    pContext->DrawIndexed(drawAttrs);
}

} // namespace MoldWing
