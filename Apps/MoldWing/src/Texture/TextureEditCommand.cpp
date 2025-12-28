/*
 *  MoldWing - Texture Edit Command Implementation
 *  S6.3: QUndoCommand for texture editing undo/redo
 */

#include "TextureEditCommand.hpp"
#include "TextureEditBuffer.hpp"
#include "Core/Logger.hpp"

namespace MoldWing
{

TextureEditCommand::TextureEditCommand(TextureEditBuffer* buffer, int textureIndex, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_buffer(buffer)
    , m_textureIndex(textureIndex)
{
    setText(QObject::tr("Texture Edit"));
}

void TextureEditCommand::recordPixel(int x, int y,
                                     uint8_t oldR, uint8_t oldG, uint8_t oldB, uint8_t oldA,
                                     uint8_t newR, uint8_t newG, uint8_t newB, uint8_t newA)
{
    if (m_finalized)
    {
        MW_LOG_WARN("Cannot record pixel after command is finalized");
        return;
    }

    // Skip if no actual change
    if (oldR == newR && oldG == newG && oldB == newB && oldA == newA)
        return;

    PixelChange change;
    change.x = static_cast<int16_t>(x);
    change.y = static_cast<int16_t>(y);
    change.oldR = oldR;
    change.oldG = oldG;
    change.oldB = oldB;
    change.oldA = oldA;
    change.newR = newR;
    change.newG = newG;
    change.newB = newB;
    change.newA = newA;

    m_pixels.push_back(change);

    // Update bounds
    if (m_bounds.isEmpty())
    {
        m_bounds = QRect(x, y, 1, 1);
    }
    else
    {
        if (x < m_bounds.left()) m_bounds.setLeft(x);
        if (x > m_bounds.right()) m_bounds.setRight(x);
        if (y < m_bounds.top()) m_bounds.setTop(y);
        if (y > m_bounds.bottom()) m_bounds.setBottom(y);
    }
}

void TextureEditCommand::finalize()
{
    m_finalized = true;

    // Update command text with pixel count
    if (m_pixels.empty())
    {
        setText(QObject::tr("Empty Texture Edit"));
    }
    else
    {
        setText(QObject::tr("Texture Edit (%1 pixels)").arg(m_pixels.size()));
    }
}

void TextureEditCommand::undo()
{
    if (!m_buffer)
    {
        MW_LOG_ERROR("Cannot undo: buffer is null");
        return;
    }

    // Restore old pixel values
    for (const auto& change : m_pixels)
    {
        uint8_t* pixel = m_buffer->pixelAt(change.x, change.y);
        if (pixel)
        {
            pixel[0] = change.oldR;
            pixel[1] = change.oldG;
            pixel[2] = change.oldB;
            pixel[3] = change.oldA;
        }
    }

    // Mark region as dirty for GPU sync
    if (!m_bounds.isEmpty())
    {
        m_buffer->markDirty(m_bounds);
    }

    MW_LOG_DEBUG("Undo texture edit: {} pixels", m_pixels.size());
}

void TextureEditCommand::redo()
{
    if (!m_buffer)
    {
        MW_LOG_ERROR("Cannot redo: buffer is null");
        return;
    }

    // Apply new pixel values
    for (const auto& change : m_pixels)
    {
        uint8_t* pixel = m_buffer->pixelAt(change.x, change.y);
        if (pixel)
        {
            pixel[0] = change.newR;
            pixel[1] = change.newG;
            pixel[2] = change.newB;
            pixel[3] = change.newA;
        }
    }

    // Mark region as dirty for GPU sync
    if (!m_bounds.isEmpty())
    {
        m_buffer->markDirty(m_bounds);
    }

    MW_LOG_DEBUG("Redo texture edit: {} pixels", m_pixels.size());
}

bool TextureEditCommand::mergeWith(const QUndoCommand* other)
{
    // Don't merge texture edit commands
    // Each stroke should be a separate undo step
    Q_UNUSED(other);
    return false;
}

} // namespace MoldWing
