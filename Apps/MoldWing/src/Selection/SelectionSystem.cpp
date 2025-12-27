/*
 *  MoldWing - Selection System Implementation
 *  S2.2: Core selection management
 */

#include "SelectionSystem.hpp"
#include "Core/Logger.hpp"

#include <queue>
#include <cmath>

namespace MoldWing
{

SelectionSystem::SelectionSystem(QObject* parent)
    : QObject(parent)
{
    LOG_DEBUG("SelectionSystem created");
}

void SelectionSystem::setMode(SelectionMode mode)
{
    if (m_mode != mode)
    {
        m_mode = mode;
        emit modeChanged(mode);
        LOG_DEBUG("Selection mode changed to {}", static_cast<int>(mode));
    }
}

std::vector<uint32_t> SelectionSystem::selectedFacesVector() const
{
    return std::vector<uint32_t>(m_selectedFaces.begin(), m_selectedFaces.end());
}

bool SelectionSystem::isFaceSelected(uint32_t faceIndex) const
{
    return m_selectedFaces.find(faceIndex) != m_selectedFaces.end();
}

void SelectionSystem::select(const std::vector<uint32_t>& faces, SelectionOp op)
{
    bool changed = false;

    switch (op)
    {
        case SelectionOp::Replace:
            if (!m_selectedFaces.empty() || !faces.empty())
            {
                m_selectedFaces.clear();
                for (uint32_t f : faces)
                {
                    m_selectedFaces.insert(f);
                }
                changed = true;
            }
            break;

        case SelectionOp::Add:
            for (uint32_t f : faces)
            {
                if (m_selectedFaces.insert(f).second)
                {
                    changed = true;
                }
            }
            break;

        case SelectionOp::Subtract:
            for (uint32_t f : faces)
            {
                if (m_selectedFaces.erase(f) > 0)
                {
                    changed = true;
                }
            }
            break;

        case SelectionOp::Toggle:
            for (uint32_t f : faces)
            {
                auto it = m_selectedFaces.find(f);
                if (it != m_selectedFaces.end())
                {
                    m_selectedFaces.erase(it);
                }
                else
                {
                    m_selectedFaces.insert(f);
                }
                changed = true;
            }
            break;
    }

    if (changed)
    {
        emit selectionChanged();
        LOG_DEBUG("Selection changed: {} faces selected", m_selectedFaces.size());
    }
}

void SelectionSystem::selectSingle(uint32_t faceIndex, SelectionOp op)
{
    select({faceIndex}, op);
}

void SelectionSystem::selectAll()
{
    if (m_faceCount == 0)
        return;

    m_selectedFaces.clear();
    for (uint32_t i = 0; i < m_faceCount; ++i)
    {
        m_selectedFaces.insert(i);
    }

    emit selectionChanged();
    LOG_DEBUG("Selected all {} faces", m_faceCount);
}

void SelectionSystem::clearSelection()
{
    if (!m_selectedFaces.empty())
    {
        m_selectedFaces.clear();
        emit selectionChanged();
        LOG_DEBUG("Selection cleared");
    }
}

void SelectionSystem::invertSelection()
{
    if (m_faceCount == 0)
        return;

    std::unordered_set<uint32_t> newSelection;
    for (uint32_t i = 0; i < m_faceCount; ++i)
    {
        if (m_selectedFaces.find(i) == m_selectedFaces.end())
        {
            newSelection.insert(i);
        }
    }

    m_selectedFaces = std::move(newSelection);
    emit selectionChanged();
    LOG_DEBUG("Selection inverted: {} faces now selected", m_selectedFaces.size());
}

void SelectionSystem::setSelection(const std::unordered_set<uint32_t>& faces)
{
    m_selectedFaces = faces;
    emit selectionChanged();
}

// SelectFacesCommand implementation

SelectFacesCommand::SelectFacesCommand(SelectionSystem* system,
                                       const std::unordered_set<uint32_t>& newSelection,
                                       const QString& text)
    : QUndoCommand(text)
    , m_system(system)
    , m_oldSelection(system->selectedFaces())
    , m_newSelection(newSelection)
{
}

void SelectFacesCommand::undo()
{
    m_system->setSelection(m_oldSelection);
}

void SelectFacesCommand::redo()
{
    m_system->setSelection(m_newSelection);
}

bool SelectFacesCommand::mergeWith(const QUndoCommand* other)
{
    // Don't merge if other command has different ID
    if (other->id() != id())
        return false;

    // Take the new selection from the other command
    const SelectFacesCommand* cmd = static_cast<const SelectFacesCommand*>(other);
    if (cmd->m_system != m_system)
        return false;

    m_newSelection = cmd->m_newSelection;
    return true;
}

// M5: Connected selection operations

std::unordered_set<uint32_t> SelectionSystem::selectLinked(
    const std::vector<std::unordered_set<uint32_t>>& adjacency,
    uint32_t seedFace,
    SelectionOp op)
{
    std::unordered_set<uint32_t> connectedFaces;

    // Validate seed face
    if (seedFace >= adjacency.size())
    {
        LOG_WARN("selectLinked: Invalid seed face {}", seedFace);
        return connectedFaces;
    }

    // BFS to find all connected faces
    std::queue<uint32_t> queue;
    queue.push(seedFace);
    connectedFaces.insert(seedFace);

    while (!queue.empty())
    {
        uint32_t current = queue.front();
        queue.pop();

        // Visit all adjacent faces
        for (uint32_t neighbor : adjacency[current])
        {
            if (connectedFaces.find(neighbor) == connectedFaces.end())
            {
                connectedFaces.insert(neighbor);
                queue.push(neighbor);
            }
        }
    }

    // Apply selection operation
    std::vector<uint32_t> faceVector(connectedFaces.begin(), connectedFaces.end());
    select(faceVector, op);

    LOG_DEBUG("selectLinked: {} faces connected to seed {}", connectedFaces.size(), seedFace);
    return connectedFaces;
}

std::unordered_set<uint32_t> SelectionSystem::selectByAngle(
    const std::vector<std::unordered_set<uint32_t>>& adjacency,
    const std::vector<std::array<float, 3>>& faceNormals,
    uint32_t seedFace,
    float angleThreshold,
    SelectionOp op)
{
    std::unordered_set<uint32_t> connectedFaces;

    // Validate inputs
    if (seedFace >= adjacency.size() || seedFace >= faceNormals.size())
    {
        LOG_WARN("selectByAngle: Invalid seed face {}", seedFace);
        return connectedFaces;
    }

    // Convert angle threshold to cosine (for faster comparison)
    float cosThreshold = std::cos(angleThreshold * 3.14159265358979f / 180.0f);

    // BFS with angle constraint
    std::queue<uint32_t> queue;
    queue.push(seedFace);
    connectedFaces.insert(seedFace);

    while (!queue.empty())
    {
        uint32_t current = queue.front();
        queue.pop();

        const auto& currentNormal = faceNormals[current];

        // Visit all adjacent faces
        for (uint32_t neighbor : adjacency[current])
        {
            if (connectedFaces.find(neighbor) == connectedFaces.end())
            {
                // Check angle between current face normal and neighbor face normal
                const auto& neighborNormal = faceNormals[neighbor];

                // Dot product of normalized vectors gives cosine of angle
                float dot = currentNormal[0] * neighborNormal[0] +
                            currentNormal[1] * neighborNormal[1] +
                            currentNormal[2] * neighborNormal[2];

                // If angle is within threshold, include this face
                if (dot >= cosThreshold)
                {
                    connectedFaces.insert(neighbor);
                    queue.push(neighbor);
                }
            }
        }
    }

    // Apply selection operation
    std::vector<uint32_t> faceVector(connectedFaces.begin(), connectedFaces.end());
    select(faceVector, op);

    LOG_DEBUG("selectByAngle: {} faces within {} degrees of seed {}",
              connectedFaces.size(), angleThreshold, seedFace);
    return connectedFaces;
}

void SelectionSystem::growSelection(const std::vector<std::unordered_set<uint32_t>>& adjacency)
{
    if (m_selectedFaces.empty())
        return;

    // Collect all faces adjacent to current selection
    std::unordered_set<uint32_t> newFaces = m_selectedFaces;

    for (uint32_t face : m_selectedFaces)
    {
        if (face < adjacency.size())
        {
            for (uint32_t neighbor : adjacency[face])
            {
                newFaces.insert(neighbor);
            }
        }
    }

    size_t oldCount = m_selectedFaces.size();
    m_selectedFaces = newFaces;

    emit selectionChanged();
    LOG_DEBUG("growSelection: {} -> {} faces", oldCount, m_selectedFaces.size());
}

void SelectionSystem::shrinkSelection(const std::vector<std::unordered_set<uint32_t>>& adjacency)
{
    if (m_selectedFaces.empty())
        return;

    // Find boundary faces (faces that have at least one non-selected neighbor)
    std::unordered_set<uint32_t> boundaryFaces;

    for (uint32_t face : m_selectedFaces)
    {
        if (face < adjacency.size())
        {
            for (uint32_t neighbor : adjacency[face])
            {
                if (m_selectedFaces.find(neighbor) == m_selectedFaces.end())
                {
                    // This face has an unselected neighbor, it's a boundary face
                    boundaryFaces.insert(face);
                    break;
                }
            }
        }
    }

    // Remove boundary faces from selection
    size_t oldCount = m_selectedFaces.size();
    for (uint32_t face : boundaryFaces)
    {
        m_selectedFaces.erase(face);
    }

    emit selectionChanged();
    LOG_DEBUG("shrinkSelection: {} -> {} faces", oldCount, m_selectedFaces.size());
}

} // namespace MoldWing
