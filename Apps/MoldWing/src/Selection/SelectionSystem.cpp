/*
 *  MoldWing - Selection System Implementation
 *  S2.2: Core selection management
 */

#include "SelectionSystem.hpp"
#include "Core/Logger.hpp"

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

} // namespace MoldWing
