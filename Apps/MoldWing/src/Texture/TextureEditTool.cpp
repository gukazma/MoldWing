/*
 *  MoldWing - Texture Edit Tool Base Class Implementation
 *  S6.3: Base class for texture editing tools
 */

#include "TextureEditTool.hpp"
#include "TextureEditBuffer.hpp"
#include "TextureEditCommand.hpp"
#include "Selection/FacePicker.hpp"
#include "Core/Logger.hpp"

#include <QUndoStack>

namespace MoldWing
{

void TextureEditTool::setContext(TextureEditBuffer* buffer,
                                 ScreenToTextureMapper* mapper,
                                 FacePicker* picker,
                                 Diligent::IDeviceContext* context,
                                 QUndoStack* undoStack)
{
    m_buffer = buffer;
    m_mapper = mapper;
    m_picker = picker;
    m_context = context;
    m_undoStack = undoStack;
}

void TextureEditTool::onMousePress(int screenX, int screenY, const OrbitCamera& camera, int width, int height)
{
    if (!m_buffer || !m_mapper)
    {
        MW_LOG_WARN("TextureEditTool: missing buffer or mapper");
        return;
    }

    // Start new stroke
    m_active = true;

    // Create undo command for this stroke
    m_currentCommand = std::make_unique<TextureEditCommand>(m_buffer, 0);

    // Apply tool at initial position
    applyAtPosition(screenX, screenY, camera, width, height);
}

void TextureEditTool::onMouseMove(int screenX, int screenY, const OrbitCamera& camera, int width, int height)
{
    if (!m_active)
        return;

    // Continue stroke
    applyAtPosition(screenX, screenY, camera, width, height);
}

void TextureEditTool::onMouseRelease(int screenX, int screenY, const OrbitCamera& camera, int width, int height)
{
    Q_UNUSED(screenX);
    Q_UNUSED(screenY);
    Q_UNUSED(camera);
    Q_UNUSED(width);
    Q_UNUSED(height);

    if (!m_active)
        return;

    m_active = false;

    // Commit stroke
    commitStroke();
}

void TextureEditTool::commitStroke()
{
    if (!m_currentCommand)
        return;

    m_currentCommand->finalize();

    if (m_currentCommand->pixelCount() > 0 && m_undoStack)
    {
        // Transfer ownership to undo stack
        m_undoStack->push(m_currentCommand.release());
        MW_LOG_DEBUG("Committed stroke to undo stack");
    }
    else
    {
        // No pixels changed, discard command
        m_currentCommand.reset();
    }
}

TextureMapResult TextureEditTool::mapToTexture(int screenX, int screenY, const OrbitCamera& camera, int width, int height)
{
    if (!m_mapper || !m_picker || !m_context)
    {
        return TextureMapResult();
    }

    return m_mapper->pickAndMap(*m_picker, m_context, screenX, screenY, camera, width, height);
}

} // namespace MoldWing
