/*
 *  MoldWing - Face Picker Implementation
 *  S2.1: GPU-based face ID picking
 *  T6.2.1: Extended pickPoint method
 */

#include "FacePicker.hpp"
#include "Core/Logger.hpp"

#include <Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <algorithm>
#include <unordered_set>
#include <cstring>

using namespace Diligent;

namespace MoldWing
{

namespace
{
    // Vertex shader for ID rendering - just transforms vertices
    const char* IDVertexShader = R"(
cbuffer Constants
{
    row_major float4x4 g_WorldViewProj;
};

struct VSInput
{
    float3 Pos      : ATTRIB0;
    float3 Normal   : ATTRIB1;
    float2 TexCoord : ATTRIB2;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    PSIn.Pos = mul(float4(VSIn.Pos, 1.0), g_WorldViewProj);
}
)";

    // Pixel shader for ID rendering - uses SV_PrimitiveID for correct face ID
    const char* IDPixelShader = R"(
struct PSInput
{
    float4 Pos : SV_POSITION;
};

uint main(in PSInput PSIn, uint primitiveID : SV_PrimitiveID) : SV_Target
{
    return primitiveID;
}
)";

    // Constant buffer structure
    struct Constants
    {
        float WorldViewProj[16];
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

bool FacePicker::initialize(IRenderDevice* pDevice, uint32_t width, uint32_t height)
{
    if (m_initialized)
        return true;

    m_pDevice = pDevice;
    m_width = width;
    m_height = height;

    if (!createPipeline(pDevice))
    {
        MW_LOG_ERROR("FacePicker: Failed to create pipeline");
        return false;
    }

    if (!createRenderTargets(pDevice, width, height))
    {
        MW_LOG_ERROR("FacePicker: Failed to create render targets");
        return false;
    }

    m_initialized = true;
    LOG_INFO("FacePicker initialized ({}x{})", width, height);
    return true;
}

bool FacePicker::createPipeline(IRenderDevice* pDevice)
{
    // Create vertex shader
    ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    shaderCI.Desc.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShader> pVS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        shaderCI.Desc.Name = "FaceID VS";
        shaderCI.Source = IDVertexShader;
        shaderCI.EntryPoint = "main";
        pDevice->CreateShader(shaderCI, &pVS);
        if (!pVS)
        {
            MW_LOG_ERROR("FacePicker: Failed to create vertex shader");
            return false;
        }
    }

    RefCntAutoPtr<IShader> pPS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        shaderCI.Desc.Name = "FaceID PS";
        shaderCI.Source = IDPixelShader;
        shaderCI.EntryPoint = "main";
        pDevice->CreateShader(shaderCI, &pPS);
        if (!pPS)
        {
            MW_LOG_ERROR("FacePicker: Failed to create pixel shader");
            return false;
        }
    }

    // Create pipeline state
    GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name = "FaceID PSO";
    psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    psoCI.GraphicsPipeline.NumRenderTargets = 1;
    psoCI.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_R32_UINT;  // Face ID format
    psoCI.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
    psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Rasterizer state (same as mesh renderer)
    psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    psoCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;

    // Depth stencil state
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS;

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
        MW_LOG_ERROR("FacePicker: Failed to create pipeline state");
        return false;
    }

    // Create constant buffer
    BufferDesc cbDesc;
    cbDesc.Name = "FaceID Constants CB";
    cbDesc.Size = sizeof(Constants);
    cbDesc.Usage = USAGE_DYNAMIC;
    cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    pDevice->CreateBuffer(cbDesc, nullptr, &m_pConstantBuffer);
    if (!m_pConstantBuffer)
    {
        MW_LOG_ERROR("FacePicker: Failed to create constant buffer");
        return false;
    }

    // Bind constant buffer
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pConstantBuffer);

    // Create SRB
    m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);

    return true;
}

bool FacePicker::createRenderTargets(IRenderDevice* pDevice, uint32_t width, uint32_t height)
{
    // Create ID texture (R32_UINT)
    TextureDesc texDesc;
    texDesc.Name = "FaceID Texture";
    texDesc.Type = RESOURCE_DIM_TEX_2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.Format = TEX_FORMAT_R32_UINT;
    texDesc.BindFlags = BIND_RENDER_TARGET;
    texDesc.Usage = USAGE_DEFAULT;
    texDesc.ClearValue.Format = TEX_FORMAT_R32_UINT;
    texDesc.ClearValue.Color[0] = 1.0f;  // Clear to 0xFFFFFFFF (invalid ID)

    pDevice->CreateTexture(texDesc, nullptr, &m_pIDTexture);
    if (!m_pIDTexture)
    {
        MW_LOG_ERROR("FacePicker: Failed to create ID texture");
        return false;
    }

    // Get render target view
    m_pIDRTV = m_pIDTexture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);

    // Create depth texture
    texDesc.Name = "FaceID Depth Texture";
    texDesc.Format = TEX_FORMAT_D32_FLOAT;
    texDesc.BindFlags = BIND_DEPTH_STENCIL;
    texDesc.ClearValue.Format = TEX_FORMAT_D32_FLOAT;
    texDesc.ClearValue.DepthStencil.Depth = 1.0f;

    pDevice->CreateTexture(texDesc, nullptr, &m_pDepthTexture);
    if (!m_pDepthTexture)
    {
        MW_LOG_ERROR("FacePicker: Failed to create depth texture");
        return false;
    }

    m_pDepthDSV = m_pDepthTexture->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);

    // Create staging texture for CPU readback
    texDesc.Name = "FaceID Staging Texture";
    texDesc.Format = TEX_FORMAT_R32_UINT;
    texDesc.BindFlags = BIND_NONE;
    texDesc.Usage = USAGE_STAGING;
    texDesc.CPUAccessFlags = CPU_ACCESS_READ;

    pDevice->CreateTexture(texDesc, nullptr, &m_pStagingTexture);
    if (!m_pStagingTexture)
    {
        MW_LOG_ERROR("FacePicker: Failed to create staging texture");
        return false;
    }

    m_width = width;
    m_height = height;

    return true;
}

void FacePicker::resize(uint32_t width, uint32_t height)
{
    if (width == m_width && height == m_height)
        return;

    if (!m_pDevice)
        return;

    // Recreate render targets with new size
    m_pIDTexture.Release();
    m_pIDRTV.Release();
    m_pDepthTexture.Release();
    m_pDepthDSV.Release();
    m_pStagingTexture.Release();

    createRenderTargets(m_pDevice, width, height);
    m_bufferDirty = true;

    LOG_DEBUG("FacePicker resized to {}x{}", width, height);
}

bool FacePicker::loadMesh(const MeshData& mesh)
{
    if (!m_pDevice || mesh.vertices.empty())
        return false;

    // Create vertex buffer
    BufferDesc vbDesc;
    vbDesc.Name = "FaceID VB";
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
    ibDesc.Name = "FaceID IB";
    ibDesc.Size = static_cast<Uint64>(mesh.indices.size() * sizeof(uint32_t));
    ibDesc.Usage = USAGE_IMMUTABLE;
    ibDesc.BindFlags = BIND_INDEX_BUFFER;

    BufferData ibData;
    ibData.pData = mesh.indices.data();
    ibData.DataSize = ibDesc.Size;

    m_pDevice->CreateBuffer(ibDesc, &ibData, &m_pIndexBuffer);
    if (!m_pIndexBuffer)
        return false;

    m_vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    m_indexCount = static_cast<uint32_t>(mesh.indices.size());
    m_bufferDirty = true;

    LOG_DEBUG("FacePicker loaded mesh: {} vertices, {} indices", m_vertexCount, m_indexCount);
    return true;
}

void FacePicker::renderIDBuffer(IDeviceContext* pContext, const OrbitCamera& camera)
{
    if (!m_initialized || !m_pVertexBuffer || !m_pIndexBuffer)
        return;

    // Set render targets
    ITextureView* pRTVs[] = {m_pIDRTV};
    pContext->SetRenderTargets(1, pRTVs, m_pDepthDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set viewport
    Viewport vp;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = static_cast<float>(m_width);
    vp.Height = static_cast<float>(m_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    pContext->SetViewports(1, &vp, m_width, m_height);

    // Clear with invalid face ID (0xFFFFFFFF)
    // Note: For R32_UINT, we need to use proper clear value
    const Uint32 clearValue[] = {INVALID_FACE_ID, 0, 0, 0};
    pContext->ClearRenderTarget(m_pIDRTV, reinterpret_cast<const float*>(clearValue),
                                 RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->ClearDepthStencil(m_pDepthDSV, CLEAR_DEPTH_FLAG, 1.0f, 0,
                                 RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

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

        // WorldViewProj = World * View * Proj
        float viewProj[16];
        MatrixMultiply(view, proj, viewProj);
        MatrixMultiply(world, viewProj, cb->WorldViewProj);
    }

    // Set pipeline and draw
    pContext->SetPipelineState(m_pPSO);
    pContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    IBuffer* pBuffs[] = {m_pVertexBuffer};
    pContext->SetVertexBuffers(0, 1, pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                SET_VERTEX_BUFFERS_FLAG_RESET);
    pContext->SetIndexBuffer(m_pIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs drawAttrs;
    drawAttrs.IndexType = VT_UINT32;
    drawAttrs.NumIndices = m_indexCount;
    drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    pContext->DrawIndexed(drawAttrs);

    m_bufferDirty = false;
}

uint32_t FacePicker::readFaceID(IDeviceContext* pContext, int x, int y)
{
    if (!m_initialized || !m_pIDTexture || !m_pStagingTexture)
        return INVALID_FACE_ID;

    // Clamp coordinates
    x = std::max(0, std::min(x, static_cast<int>(m_width) - 1));
    y = std::max(0, std::min(y, static_cast<int>(m_height) - 1));

    // Copy region from ID texture to staging
    CopyTextureAttribs copyAttribs;
    copyAttribs.pSrcTexture = m_pIDTexture;
    copyAttribs.SrcMipLevel = 0;
    copyAttribs.SrcSlice = 0;
    copyAttribs.pDstTexture = m_pStagingTexture;
    copyAttribs.DstMipLevel = 0;
    copyAttribs.DstSlice = 0;
    copyAttribs.DstX = 0;
    copyAttribs.DstY = 0;
    copyAttribs.DstZ = 0;

    Box srcBox;
    srcBox.MinX = x;
    srcBox.MaxX = x + 1;
    srcBox.MinY = y;
    srcBox.MaxY = y + 1;
    srcBox.MinZ = 0;
    srcBox.MaxZ = 1;
    copyAttribs.pSrcBox = &srcBox;

    pContext->CopyTexture(copyAttribs);

    // Wait for GPU to finish
    pContext->WaitForIdle();

    // Map staging texture and read the value
    MappedTextureSubresource mappedData;
    pContext->MapTextureSubresource(m_pStagingTexture, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT,
                                     nullptr, mappedData);

    uint32_t faceID = INVALID_FACE_ID;
    if (mappedData.pData)
    {
        faceID = *static_cast<const uint32_t*>(mappedData.pData);
    }

    pContext->UnmapTextureSubresource(m_pStagingTexture, 0, 0);

    return faceID;
}

std::vector<uint32_t> FacePicker::readFaceIDsInRect(IDeviceContext* pContext,
                                                     int x1, int y1, int x2, int y2)
{
    std::vector<uint32_t> result;

    if (!m_initialized || !m_pIDTexture || !m_pStagingTexture)
        return result;

    // Normalize rectangle
    if (x1 > x2) std::swap(x1, x2);
    if (y1 > y2) std::swap(y1, y2);

    // Clamp to texture bounds
    x1 = std::max(0, std::min(x1, static_cast<int>(m_width) - 1));
    x2 = std::max(0, std::min(x2, static_cast<int>(m_width)));
    y1 = std::max(0, std::min(y1, static_cast<int>(m_height) - 1));
    y2 = std::max(0, std::min(y2, static_cast<int>(m_height)));

    int rectWidth = x2 - x1;
    int rectHeight = y2 - y1;

    if (rectWidth <= 0 || rectHeight <= 0)
        return result;

    // Copy region from ID texture to staging
    CopyTextureAttribs copyAttribs;
    copyAttribs.pSrcTexture = m_pIDTexture;
    copyAttribs.SrcMipLevel = 0;
    copyAttribs.SrcSlice = 0;
    copyAttribs.pDstTexture = m_pStagingTexture;
    copyAttribs.DstMipLevel = 0;
    copyAttribs.DstSlice = 0;
    copyAttribs.DstX = 0;
    copyAttribs.DstY = 0;
    copyAttribs.DstZ = 0;

    Box srcBox;
    srcBox.MinX = x1;
    srcBox.MaxX = x2;
    srcBox.MinY = y1;
    srcBox.MaxY = y2;
    srcBox.MinZ = 0;
    srcBox.MaxZ = 1;
    copyAttribs.pSrcBox = &srcBox;

    pContext->CopyTexture(copyAttribs);

    // Wait for GPU
    pContext->WaitForIdle();

    // Map and read
    MappedTextureSubresource mappedData;
    Box mapBox;
    mapBox.MinX = 0;
    mapBox.MaxX = rectWidth;
    mapBox.MinY = 0;
    mapBox.MaxY = rectHeight;
    mapBox.MinZ = 0;
    mapBox.MaxZ = 1;

    pContext->MapTextureSubresource(m_pStagingTexture, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT,
                                     &mapBox, mappedData);

    if (mappedData.pData)
    {
        std::unordered_set<uint32_t> uniqueFaces;
        const uint8_t* pRow = static_cast<const uint8_t*>(mappedData.pData);

        for (int row = 0; row < rectHeight; ++row)
        {
            const uint32_t* pPixels = reinterpret_cast<const uint32_t*>(pRow);
            for (int col = 0; col < rectWidth; ++col)
            {
                uint32_t faceID = pPixels[col];
                if (faceID != INVALID_FACE_ID)
                {
                    uniqueFaces.insert(faceID);
                }
            }
            pRow += mappedData.Stride;
        }

        result.assign(uniqueFaces.begin(), uniqueFaces.end());
    }

    pContext->UnmapTextureSubresource(m_pStagingTexture, 0, 0);

    return result;
}

std::vector<uint32_t> FacePicker::readFaceIDsInCircle(IDeviceContext* pContext,
                                                       int centerX, int centerY, int radius)
{
    std::vector<uint32_t> result;

    if (!m_initialized || !m_pIDTexture || !m_pStagingTexture)
        return result;

    if (radius <= 0)
        return result;

    // Calculate bounding rectangle for the circle
    int x1 = centerX - radius;
    int y1 = centerY - radius;
    int x2 = centerX + radius;
    int y2 = centerY + radius;

    // Clamp to texture bounds
    x1 = std::max(0, std::min(x1, static_cast<int>(m_width) - 1));
    x2 = std::max(0, std::min(x2, static_cast<int>(m_width)));
    y1 = std::max(0, std::min(y1, static_cast<int>(m_height) - 1));
    y2 = std::max(0, std::min(y2, static_cast<int>(m_height)));

    int rectWidth = x2 - x1;
    int rectHeight = y2 - y1;

    if (rectWidth <= 0 || rectHeight <= 0)
        return result;

    // Copy region from ID texture to staging
    CopyTextureAttribs copyAttribs;
    copyAttribs.pSrcTexture = m_pIDTexture;
    copyAttribs.SrcMipLevel = 0;
    copyAttribs.SrcSlice = 0;
    copyAttribs.pDstTexture = m_pStagingTexture;
    copyAttribs.DstMipLevel = 0;
    copyAttribs.DstSlice = 0;
    copyAttribs.DstX = 0;
    copyAttribs.DstY = 0;
    copyAttribs.DstZ = 0;

    Box srcBox;
    srcBox.MinX = x1;
    srcBox.MaxX = x2;
    srcBox.MinY = y1;
    srcBox.MaxY = y2;
    srcBox.MinZ = 0;
    srcBox.MaxZ = 1;
    copyAttribs.pSrcBox = &srcBox;

    pContext->CopyTexture(copyAttribs);

    // Wait for GPU
    pContext->WaitForIdle();

    // Map and read
    MappedTextureSubresource mappedData;
    Box mapBox;
    mapBox.MinX = 0;
    mapBox.MaxX = rectWidth;
    mapBox.MinY = 0;
    mapBox.MaxY = rectHeight;
    mapBox.MinZ = 0;
    mapBox.MaxZ = 1;

    pContext->MapTextureSubresource(m_pStagingTexture, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT,
                                     &mapBox, mappedData);

    if (mappedData.pData)
    {
        std::unordered_set<uint32_t> uniqueFaces;
        const uint8_t* pRow = static_cast<const uint8_t*>(mappedData.pData);

        int radiusSquared = radius * radius;

        for (int row = 0; row < rectHeight; ++row)
        {
            const uint32_t* pPixels = reinterpret_cast<const uint32_t*>(pRow);

            // Calculate Y position relative to circle center
            int dy = (y1 + row) - centerY;

            for (int col = 0; col < rectWidth; ++col)
            {
                // Calculate X position relative to circle center
                int dx = (x1 + col) - centerX;

                // Check if pixel is within circle
                if (dx * dx + dy * dy <= radiusSquared)
                {
                    uint32_t faceID = pPixels[col];
                    if (faceID != INVALID_FACE_ID)
                    {
                        uniqueFaces.insert(faceID);
                    }
                }
            }
            pRow += mappedData.Stride;
        }

        result.assign(uniqueFaces.begin(), uniqueFaces.end());
    }

    pContext->UnmapTextureSubresource(m_pStagingTexture, 0, 0);

    return result;
}

PickResult FacePicker::pickPoint(IDeviceContext* pContext, int x, int y)
{
    PickResult result;

    uint32_t faceId = readFaceID(pContext, x, y);
    if (faceId != INVALID_FACE_ID)
    {
        result.faceId = faceId;
        result.hit = true;
        result.depth = 0.0f;  // Depth not implemented yet - use CPU ray-triangle intersection
    }

    return result;
}

} // namespace MoldWing
