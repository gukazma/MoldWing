/*
 *  MoldWing - Texture Edit Tool Base Class
 *  S6.3: Base class for texture editing tools (clone stamp, eraser, etc.)
 */

#pragma once

#include "ScreenToTextureMapper.hpp"

#include <QString>
#include <QPoint>
#include <memory>

namespace Diligent
{
    struct IDeviceContext;
}

class QUndoStack;

namespace MoldWing
{

class TextureEditBuffer;
class TextureEditCommand;
class OrbitCamera;
class FacePicker;

/**
 * Base class for texture editing tools.
 * Tools receive mouse events and modify the texture through TextureEditBuffer.
 */
class TextureEditTool
{
public:
    TextureEditTool() = default;
    virtual ~TextureEditTool() = default;

    // Tool identification
    virtual QString name() const = 0;
    virtual QString icon() const { return QString(); }

    // Tool properties
    int brushRadius() const { return m_brushRadius; }
    void setBrushRadius(int radius) { m_brushRadius = std::max(1, radius); }

    float brushHardness() const { return m_brushHardness; }
    void setBrushHardness(float hardness) { m_brushHardness = std::clamp(hardness, 0.0f, 1.0f); }

    // Context setup (called when tool is activated)
    virtual void setContext(TextureEditBuffer* buffer,
                           ScreenToTextureMapper* mapper,
                           FacePicker* picker,
                           Diligent::IDeviceContext* context,
                           QUndoStack* undoStack);

    // Mouse event handlers
    virtual void onMousePress(int screenX, int screenY, const OrbitCamera& camera, int width, int height);
    virtual void onMouseMove(int screenX, int screenY, const OrbitCamera& camera, int width, int height);
    virtual void onMouseRelease(int screenX, int screenY, const OrbitCamera& camera, int width, int height);

    // Key event handlers (optional)
    virtual void onKeyPress(int key) { Q_UNUSED(key); }
    virtual void onKeyRelease(int key) { Q_UNUSED(key); }

    // Check if tool is currently active (during stroke)
    bool isActive() const { return m_active; }

protected:
    // Map screen position to texture and apply tool
    virtual void applyAtPosition(int screenX, int screenY, const OrbitCamera& camera, int width, int height) = 0;

    // Commit current stroke as undo command
    void commitStroke();

    // Helper: get texture coordinate from screen position
    TextureMapResult mapToTexture(int screenX, int screenY, const OrbitCamera& camera, int width, int height);

    TextureEditBuffer* m_buffer = nullptr;
    ScreenToTextureMapper* m_mapper = nullptr;
    FacePicker* m_picker = nullptr;
    Diligent::IDeviceContext* m_context = nullptr;
    QUndoStack* m_undoStack = nullptr;

    int m_brushRadius = 10;
    float m_brushHardness = 1.0f;

    bool m_active = false;
    std::unique_ptr<TextureEditCommand> m_currentCommand;
};

} // namespace MoldWing
