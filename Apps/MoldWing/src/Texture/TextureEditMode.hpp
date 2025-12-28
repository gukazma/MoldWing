/*
 *  MoldWing - Texture Edit Mode
 *  S6.3: Edit mode framework - state management, camera lock
 */

#pragma once

#include "Core/MeshData.hpp"
#include "Render/OrbitCamera.hpp"

#include <QObject>
#include <QString>
#include <memory>
#include <functional>

class QUndoStack;

namespace MoldWing
{

// Forward declarations
class TextureEditBuffer;
class TextureEditTool;

/**
 * TextureEditMode manages the texture editing state.
 * When activated:
 * - Camera is locked (no rotation/pan/zoom)
 * - Mouse events are routed to texture editing tools
 * - Status bar shows edit mode hints
 */
class TextureEditMode : public QObject
{
    Q_OBJECT

public:
    explicit TextureEditMode(QObject* parent = nullptr);
    ~TextureEditMode();

    // Mode control
    bool isActive() const { return m_active; }
    bool enterEditMode(std::shared_ptr<MeshData> mesh, QUndoStack* undoStack);
    void exitEditMode();

    // Camera lock (when in edit mode, camera operations should be blocked)
    bool isCameraLocked() const { return m_active; }

    // Get edit buffer for current texture
    TextureEditBuffer* editBuffer(int textureIndex = 0);

    // Tool management
    void setCurrentTool(TextureEditTool* tool);
    TextureEditTool* currentTool() const { return m_currentTool; }

    // Status text
    QString statusText() const;

    // Save modified textures
    bool saveTexture(int textureIndex, const QString& filePath);
    bool saveAllTextures(const QString& baseDir);

    // Check if any texture has unsaved changes
    bool hasUnsavedChanges() const;

signals:
    void editModeEntered();
    void editModeExited();
    void statusTextChanged(const QString& text);
    void textureModified(int textureIndex);

private:
    void createEditBuffers();
    void clearEditBuffers();

    bool m_active = false;
    std::shared_ptr<MeshData> m_mesh;
    QUndoStack* m_undoStack = nullptr;

    // Edit buffers for each texture (CPU copies)
    std::vector<std::unique_ptr<TextureEditBuffer>> m_editBuffers;

    // Current editing tool
    TextureEditTool* m_currentTool = nullptr;
};

} // namespace MoldWing
