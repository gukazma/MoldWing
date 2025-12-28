/*
 *  MoldWing - Texture Edit Mode Implementation
 *  S6.3: Edit mode framework - state management, camera lock
 */

#include "TextureEditMode.hpp"
#include "TextureEditBuffer.hpp"
#include "Core/Logger.hpp"

#include <QUndoStack>
#include <QDir>
#include <QFileInfo>

namespace MoldWing
{

TextureEditMode::TextureEditMode(QObject* parent)
    : QObject(parent)
{
}

TextureEditMode::~TextureEditMode()
{
    if (m_active)
    {
        exitEditMode();
    }
}

bool TextureEditMode::enterEditMode(std::shared_ptr<MeshData> mesh, QUndoStack* undoStack)
{
    if (m_active)
    {
        MW_LOG_WARN("Already in edit mode");
        return false;
    }

    if (!mesh)
    {
        MW_LOG_ERROR("Cannot enter edit mode: no mesh loaded");
        return false;
    }

    if (!mesh->hasTextures())
    {
        MW_LOG_WARN("Cannot enter edit mode: mesh has no textures");
        return false;
    }

    m_mesh = mesh;
    m_undoStack = undoStack;

    // Create edit buffers for all textures
    createEditBuffers();

    m_active = true;

    MW_LOG_INFO("Entered texture edit mode");
    emit editModeEntered();
    emit statusTextChanged(statusText());

    return true;
}

void TextureEditMode::exitEditMode()
{
    if (!m_active)
        return;

    // Check for unsaved changes
    if (hasUnsavedChanges())
    {
        MW_LOG_WARN("Exiting edit mode with unsaved changes");
    }

    clearEditBuffers();

    m_mesh.reset();
    m_undoStack = nullptr;
    m_currentTool = nullptr;
    m_active = false;

    MW_LOG_INFO("Exited texture edit mode");
    emit editModeExited();
    emit statusTextChanged(QString());
}

TextureEditBuffer* TextureEditMode::editBuffer(int textureIndex)
{
    if (textureIndex < 0 || textureIndex >= static_cast<int>(m_editBuffers.size()))
        return nullptr;

    return m_editBuffers[textureIndex].get();
}

void TextureEditMode::setCurrentTool(TextureEditTool* tool)
{
    m_currentTool = tool;
    emit statusTextChanged(statusText());
}

QString TextureEditMode::statusText() const
{
    if (!m_active)
        return QString();

    QString text = tr("Edit Mode - View Locked");

    if (m_currentTool)
    {
        // Tool name would be added here when TextureEditTool is implemented
        text += " | Tool: [None]";
    }

    return text;
}

bool TextureEditMode::saveTexture(int textureIndex, const QString& filePath)
{
    TextureEditBuffer* buffer = editBuffer(textureIndex);
    if (!buffer)
    {
        MW_LOG_ERROR("Cannot save: invalid texture index {}", textureIndex);
        return false;
    }

    if (!buffer->save(filePath))
    {
        return false;
    }

    buffer->setModified(false);
    return true;
}

bool TextureEditMode::saveAllTextures(const QString& baseDir)
{
    if (!m_mesh)
        return false;

    bool allSaved = true;
    for (size_t i = 0; i < m_editBuffers.size(); ++i)
    {
        TextureEditBuffer* buffer = m_editBuffers[i].get();
        if (!buffer || !buffer->isModified())
            continue;

        // Determine output path
        QString outputPath;
        if (i < m_mesh->textures.size() && m_mesh->textures[i])
        {
            // Use original filename
            QString originalPath = m_mesh->textures[i]->filePath;
            QFileInfo fi(originalPath);
            outputPath = QDir(baseDir).filePath(fi.fileName());
        }
        else
        {
            outputPath = QDir(baseDir).filePath(QString("texture_%1.png").arg(i));
        }

        if (!buffer->save(outputPath))
        {
            allSaved = false;
        }
        else
        {
            buffer->setModified(false);
        }
    }

    return allSaved;
}

bool TextureEditMode::hasUnsavedChanges() const
{
    for (const auto& buffer : m_editBuffers)
    {
        if (buffer && buffer->isModified())
            return true;
    }
    return false;
}

void TextureEditMode::createEditBuffers()
{
    clearEditBuffers();

    if (!m_mesh)
        return;

    for (size_t i = 0; i < m_mesh->textures.size(); ++i)
    {
        auto buffer = std::make_unique<TextureEditBuffer>();

        if (m_mesh->textures[i] && m_mesh->textures[i]->isValid())
        {
            if (buffer->initialize(*m_mesh->textures[i]))
            {
                MW_LOG_INFO("Created edit buffer for texture {}", i);
            }
            else
            {
                MW_LOG_ERROR("Failed to create edit buffer for texture {}", i);
            }
        }

        m_editBuffers.push_back(std::move(buffer));
    }
}

void TextureEditMode::clearEditBuffers()
{
    m_editBuffers.clear();
}

} // namespace MoldWing
