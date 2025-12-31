#pragma once

#include <cstdint>

namespace MoldWing {

/**
 * @brief CompositeId - 复合面ID工具类
 *
 * 用于多模型选择系统，将 meshId 和 faceId 编码为单个 32位整数
 *
 * 编码格式:
 *   - 高 8 位: meshId (0-255, 支持最多 256 个模型)
 *   - 低 24 位: faceId (0-16,777,215, 支持每个模型最多 16M 个面)
 *
 * 示例:
 *   meshId = 2, faceId = 1000
 *   compositeId = (2 << 24) | 1000 = 0x020003E8
 */
namespace CompositeId {

/// 无效的复合面ID
constexpr uint32_t INVALID = 0xFFFFFFFF;

/// 最大模型ID (8位 = 0-255)
constexpr uint32_t MAX_MESH_ID = 255;

/// 最大面ID (24位 = 0-16,777,215)
constexpr uint32_t MAX_FACE_ID = 0x00FFFFFF;

/// meshId 在复合ID中的位移
constexpr uint32_t MESH_ID_SHIFT = 24;

/// faceId 掩码
constexpr uint32_t FACE_ID_MASK = 0x00FFFFFF;

/// meshId 掩码
constexpr uint32_t MESH_ID_MASK = 0xFF000000;

/**
 * @brief 创建复合面ID
 * @param meshId 模型ID (0-255)
 * @param faceId 面ID (0-16,777,215)
 * @return 复合面ID，如果参数超出范围返回 INVALID
 */
inline constexpr uint32_t make(uint32_t meshId, uint32_t faceId) noexcept
{
    if (meshId > MAX_MESH_ID || faceId > MAX_FACE_ID) {
        return INVALID;
    }
    return (meshId << MESH_ID_SHIFT) | faceId;
}

/**
 * @brief 从复合面ID提取模型ID
 * @param compositeId 复合面ID
 * @return 模型ID (0-255)
 */
inline constexpr uint32_t meshId(uint32_t compositeId) noexcept
{
    return (compositeId >> MESH_ID_SHIFT) & 0xFF;
}

/**
 * @brief 从复合面ID提取面ID
 * @param compositeId 复合面ID
 * @return 面ID (0-16,777,215)
 */
inline constexpr uint32_t faceId(uint32_t compositeId) noexcept
{
    return compositeId & FACE_ID_MASK;
}

/**
 * @brief 检查复合面ID是否有效
 * @param compositeId 复合面ID
 * @return true 如果有效，false 如果无效
 */
inline constexpr bool isValid(uint32_t compositeId) noexcept
{
    return compositeId != INVALID;
}

/**
 * @brief 检查两个复合面ID是否属于同一模型
 * @param a 复合面ID A
 * @param b 复合面ID B
 * @return true 如果属于同一模型
 */
inline constexpr bool sameMesh(uint32_t a, uint32_t b) noexcept
{
    return meshId(a) == meshId(b);
}

/**
 * @brief 将旧格式的面ID转换为复合面ID（假设 meshId = 0）
 * @param legacyFaceId 旧格式面ID
 * @return 复合面ID
 */
inline constexpr uint32_t fromLegacy(uint32_t legacyFaceId) noexcept
{
    if (legacyFaceId == 0xFFFFFFFF) {
        return INVALID;
    }
    return make(0, legacyFaceId);
}

/**
 * @brief 格式化复合面ID为字符串 "Mesh:Face" 格式
 * @param compositeId 复合面ID
 * @param buffer 输出缓冲区
 * @param bufferSize 缓冲区大小
 * @return 写入的字符数
 */
inline int format(uint32_t compositeId, char* buffer, size_t bufferSize) noexcept
{
    if (!isValid(compositeId)) {
        return snprintf(buffer, bufferSize, "Invalid");
    }
    return snprintf(buffer, bufferSize, "%u:%u", meshId(compositeId), faceId(compositeId));
}

} // namespace CompositeId

// ============================================================================
// 编译时验证 (static_assert)
// ============================================================================
namespace CompositeIdTests {

// 测试基本编码/解码
static_assert(CompositeId::make(0, 0) == 0x00000000, "make(0,0) should be 0");
static_assert(CompositeId::make(1, 0) == 0x01000000, "make(1,0) should be 0x01000000");
static_assert(CompositeId::make(0, 1) == 0x00000001, "make(0,1) should be 1");
static_assert(CompositeId::make(255, 0xFFFFFF) == 0xFFFFFFFF, "make(255, MAX) should be 0xFFFFFFFF");

// 测试提取 meshId
static_assert(CompositeId::meshId(0x00000000) == 0, "meshId(0) should be 0");
static_assert(CompositeId::meshId(0x01000000) == 1, "meshId should be 1");
static_assert(CompositeId::meshId(0xFF000000) == 255, "meshId should be 255");
static_assert(CompositeId::meshId(0x02001234) == 2, "meshId should be 2");

// 测试提取 faceId
static_assert(CompositeId::faceId(0x00000000) == 0, "faceId(0) should be 0");
static_assert(CompositeId::faceId(0x00000001) == 1, "faceId should be 1");
static_assert(CompositeId::faceId(0x00FFFFFF) == 0xFFFFFF, "faceId should be max");
static_assert(CompositeId::faceId(0x02001234) == 0x1234, "faceId should be 0x1234");

// 测试往返转换
static_assert(CompositeId::meshId(CompositeId::make(5, 1000)) == 5, "roundtrip meshId");
static_assert(CompositeId::faceId(CompositeId::make(5, 1000)) == 1000, "roundtrip faceId");

// 测试边界条件
static_assert(CompositeId::make(256, 0) == CompositeId::INVALID, "meshId > 255 should be invalid");
static_assert(CompositeId::make(0, 0x01000000) == CompositeId::INVALID, "faceId > 24bit should be invalid");

// 测试有效性检查
static_assert(CompositeId::isValid(0x00000000) == true, "0 is valid");
static_assert(CompositeId::isValid(0xFFFFFFFE) == true, "0xFFFFFFFE is valid");
static_assert(CompositeId::isValid(CompositeId::INVALID) == false, "INVALID should not be valid");

// 测试 sameMesh
static_assert(CompositeId::sameMesh(0x01000001, 0x01000002) == true, "same mesh");
static_assert(CompositeId::sameMesh(0x01000001, 0x02000001) == false, "different mesh");

// 测试 fromLegacy
static_assert(CompositeId::fromLegacy(100) == CompositeId::make(0, 100), "fromLegacy");
static_assert(CompositeId::fromLegacy(0xFFFFFFFF) == CompositeId::INVALID, "fromLegacy invalid");

} // namespace CompositeIdTests

} // namespace MoldWing
