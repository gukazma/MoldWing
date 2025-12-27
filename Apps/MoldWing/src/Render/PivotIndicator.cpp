/*
 *  MoldWing - Pivot Indicator Implementation
 */

#include "PivotIndicator.hpp"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"
#include "Core/Logger.hpp"

using namespace Diligent;

namespace MoldWing
{

namespace
{

// Simple vertex shader for lines
const char* PivotVSSource = R"(
cbuffer Constants
{
    row_major float4x4 WorldViewProj;  // Row-major to match C++ layout
    float4 PivotPos;  // xyz = position, w = size
    float4 Color;
};

struct VSInput
{
    float3 Pos : ATTRIB0;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    float3 scaledPos = VSIn.Pos * PivotPos.w;  // Scale by size
    float3 worldPos = scaledPos + PivotPos.xyz;
    PSIn.Pos = mul(float4(worldPos, 1.0), WorldViewProj);
    PSIn.Color = Color;
}
)";

// Simple pixel shader
const char* PivotPSSource = R"(
struct PSInput
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

float4 main(in PSInput PSIn) : SV_Target
{
    return PSIn.Color;
}
)";

struct PivotConstants
{
    float WorldViewProj[16];
    float PivotPos[4];
    float Color[4];
};

void MatrixMultiply(const float* a, const float* b, float* result)
{
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            result[i * 4 + j] = 0;
            for (int k = 0; k < 4; ++k)
                result[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
        }
    }
}

} // anonymous namespace

void PivotIndicator::initialize(IRenderDevice* pDevice)
{
    if (!pDevice)
    {
        MW_LOG_ERROR("PivotIndicator::initialize - null device!");
        return;
    }
    m_pDevice = pDevice;
    createPipeline(pDevice);

    // Only mark as initialized if all resources were created
    if (m_pPSO && m_pConstantBuffer && m_pSRB && m_pVertexBuffer)
    {
        m_initialized = true;
        LOG_DEBUG("PivotIndicator initialized successfully");
    }
    else
    {
        MW_LOG_ERROR("PivotIndicator initialization failed - some resources not created");
    }
}

void PivotIndicator::createPipeline(IRenderDevice* pDevice)
{
    // Create shaders
    ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    shaderCI.Desc.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShader> pVS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        shaderCI.Desc.Name = "Pivot VS";
        shaderCI.Source = PivotVSSource;
        shaderCI.EntryPoint = "main";
        pDevice->CreateShader(shaderCI, &pVS);
        if (!pVS)
        {
            MW_LOG_ERROR("PivotIndicator: Failed to create vertex shader");
            return;
        }
    }

    RefCntAutoPtr<IShader> pPS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        shaderCI.Desc.Name = "Pivot PS";
        shaderCI.Source = PivotPSSource;
        shaderCI.EntryPoint = "main";
        pDevice->CreateShader(shaderCI, &pPS);
        if (!pPS)
        {
            MW_LOG_ERROR("PivotIndicator: Failed to create pixel shader");
            return;
        }
    }

    // Create pipeline
    GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name = "Pivot PSO";
    psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    psoCI.GraphicsPipeline.NumRenderTargets = 1;
    psoCI.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM_SRGB;
    psoCI.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
    psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_LINE_LIST;
    psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;  // Always on top
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;

    psoCI.pVS = pVS;
    psoCI.pPS = pPS;

    // Input layout
    LayoutElement layoutElems[] = {
        {0, 0, 3, VT_FLOAT32, False}  // Position
    };
    psoCI.GraphicsPipeline.InputLayout.LayoutElements = layoutElems;
    psoCI.GraphicsPipeline.InputLayout.NumElements = 1;

    // Shader resource variables - declare Constants as static
    ShaderResourceVariableDesc varDesc[] = {
        {SHADER_TYPE_VERTEX, "Constants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
    };
    psoCI.PSODesc.ResourceLayout.Variables = varDesc;
    psoCI.PSODesc.ResourceLayout.NumVariables = 1;

    pDevice->CreateGraphicsPipelineState(psoCI, &m_pPSO);
    if (!m_pPSO)
    {
        MW_LOG_ERROR("PivotIndicator: Failed to create PSO");
        return;
    }

    // Create constant buffer
    BufferDesc cbDesc;
    cbDesc.Name = "Pivot Constants";
    cbDesc.Usage = USAGE_DYNAMIC;
    cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    cbDesc.Size = sizeof(PivotConstants);
    pDevice->CreateBuffer(cbDesc, nullptr, &m_pConstantBuffer);
    if (!m_pConstantBuffer)
    {
        MW_LOG_ERROR("PivotIndicator: Failed to create constant buffer");
        return;
    }

    // Bind constant buffer to PSO (static variable)
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pConstantBuffer);

    // Create SRB
    m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);
    if (!m_pSRB)
    {
        MW_LOG_ERROR("PivotIndicator: Failed to create SRB");
        return;
    }

    // Create vertex buffer for crosshair (6 lines = 12 vertices)
    // 3 axes: X (red), Y (green), Z (blue)
    float vertices[] = {
        // X axis
        -1.0f, 0.0f, 0.0f,
         1.0f, 0.0f, 0.0f,
        // Y axis
        0.0f, -1.0f, 0.0f,
        0.0f,  1.0f, 0.0f,
        // Z axis
        0.0f, 0.0f, -1.0f,
        0.0f, 0.0f,  1.0f,
    };

    BufferDesc vbDesc;
    vbDesc.Name = "Pivot VB";
    vbDesc.Usage = USAGE_IMMUTABLE;
    vbDesc.BindFlags = BIND_VERTEX_BUFFER;
    vbDesc.Size = sizeof(vertices);

    BufferData vbData;
    vbData.pData = vertices;
    vbData.DataSize = sizeof(vertices);
    pDevice->CreateBuffer(vbDesc, &vbData, &m_pVertexBuffer);
    if (!m_pVertexBuffer)
    {
        MW_LOG_ERROR("PivotIndicator: Failed to create vertex buffer");
        return;
    }

    LOG_DEBUG("PivotIndicator: All resources created successfully");
}

void PivotIndicator::render(IDeviceContext* pContext,
                            const OrbitCamera& camera,
                            float pivotX, float pivotY, float pivotZ,
                            float size)
{
    if (!m_initialized)
    {
        LOG_TRACE("PivotIndicator::render - not initialized");
        return;
    }

    LOG_TRACE("PivotIndicator::render - drawing at ({}, {}, {})", pivotX, pivotY, pivotZ);

    // Get view-projection matrix
    float view[16], proj[16], viewProj[16];
    camera.getViewMatrix(view);
    camera.getProjectionMatrix(proj);
    MatrixMultiply(view, proj, viewProj);

    // Scale the size based on distance for consistent screen size
    float camX, camY, camZ;
    camera.getPosition(camX, camY, camZ);
    float dx = pivotX - camX;
    float dy = pivotY - camY;
    float dz = pivotZ - camZ;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    float scaledSize = size * dist * 0.05f;  // 5% of distance

    pContext->SetPipelineState(m_pPSO);

    // Set vertex buffer
    IBuffer* pBuffs[] = {m_pVertexBuffer};
    pContext->SetVertexBuffers(0, 1, pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                SET_VERTEX_BUFFERS_FLAG_RESET);

    // Draw X axis (red)
    {
        MapHelper<PivotConstants> cb(pContext, m_pConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        for (int i = 0; i < 16; ++i) cb->WorldViewProj[i] = viewProj[i];
        cb->PivotPos[0] = pivotX; cb->PivotPos[1] = pivotY; cb->PivotPos[2] = pivotZ; cb->PivotPos[3] = scaledSize;
        cb->Color[0] = 1.0f; cb->Color[1] = 0.2f; cb->Color[2] = 0.2f; cb->Color[3] = 1.0f;
    }
    pContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 2;
    drawAttrs.StartVertexLocation = 0;
    pContext->Draw(drawAttrs);

    // Draw Y axis (green)
    {
        MapHelper<PivotConstants> cb(pContext, m_pConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        for (int i = 0; i < 16; ++i) cb->WorldViewProj[i] = viewProj[i];
        cb->PivotPos[0] = pivotX; cb->PivotPos[1] = pivotY; cb->PivotPos[2] = pivotZ; cb->PivotPos[3] = scaledSize;
        cb->Color[0] = 0.2f; cb->Color[1] = 1.0f; cb->Color[2] = 0.2f; cb->Color[3] = 1.0f;
    }
    pContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    drawAttrs.StartVertexLocation = 2;
    pContext->Draw(drawAttrs);

    // Draw Z axis (blue)
    {
        MapHelper<PivotConstants> cb(pContext, m_pConstantBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        for (int i = 0; i < 16; ++i) cb->WorldViewProj[i] = viewProj[i];
        cb->PivotPos[0] = pivotX; cb->PivotPos[1] = pivotY; cb->PivotPos[2] = pivotZ; cb->PivotPos[3] = scaledSize;
        cb->Color[0] = 0.2f; cb->Color[1] = 0.5f; cb->Color[2] = 1.0f; cb->Color[3] = 1.0f;
    }
    pContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    drawAttrs.StartVertexLocation = 4;
    pContext->Draw(drawAttrs);
}

} // namespace MoldWing
