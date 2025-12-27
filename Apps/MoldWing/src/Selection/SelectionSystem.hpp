/*
 *  MoldWing - Selection System
 *  S2.2: Core selection management with Qt signals
 */

#pragma once

#include <QObject>
#include <QUndoCommand>

#include <unordered_set>
#include <vector>
#include <array>
#include <cstdint>

namespace MoldWing
{

/**
 * @brief Selection modes for different interaction types
 */
enum class SelectionMode
{
    Box,        // Rectangle selection
    Brush,      // Circle brush selection (M3)
    Lasso,      // Polygon lasso selection (M4)
    Link        // Connected selection (M5)
};

/**
 * @brief Selection operation types
 */
enum class SelectionOp
{
    Replace,    // Replace current selection
    Add,        // Add to selection (Shift)
    Subtract,   // Remove from selection (Ctrl)
    Toggle      // Toggle selection (Shift+Ctrl)
};

/**
 * @brief Core selection system managing selected faces
 *
 * Uses Qt signals to notify UI of selection changes.
 * Integrates with QUndoStack for undo/redo support.
 */
class SelectionSystem : public QObject
{
    Q_OBJECT

public:
    explicit SelectionSystem(QObject* parent = nullptr);
    ~SelectionSystem() override = default;

    // Non-copyable
    SelectionSystem(const SelectionSystem&) = delete;
    SelectionSystem& operator=(const SelectionSystem&) = delete;

    /**
     * @brief Get current selection mode
     */
    SelectionMode mode() const { return m_mode; }

    /**
     * @brief Set selection mode
     */
    void setMode(SelectionMode mode);

    /**
     * @brief Get the set of selected face indices
     */
    const std::unordered_set<uint32_t>& selectedFaces() const { return m_selectedFaces; }

    /**
     * @brief Get selected faces as a vector (for iteration)
     */
    std::vector<uint32_t> selectedFacesVector() const;

    /**
     * @brief Check if a face is selected
     */
    bool isFaceSelected(uint32_t faceIndex) const;

    /**
     * @brief Get number of selected faces
     */
    size_t selectionCount() const { return m_selectedFaces.size(); }

    /**
     * @brief Check if selection is empty
     */
    bool isEmpty() const { return m_selectedFaces.empty(); }

    /**
     * @brief Set total face count (for selectAll/invert)
     */
    void setFaceCount(uint32_t count) { m_faceCount = count; }

    /**
     * @brief Get total face count
     */
    uint32_t faceCount() const { return m_faceCount; }

    // Selection operations (directly modify selection, emits signal)
    void select(const std::vector<uint32_t>& faces, SelectionOp op = SelectionOp::Replace);
    void selectSingle(uint32_t faceIndex, SelectionOp op = SelectionOp::Replace);
    void selectAll();
    void clearSelection();
    void invertSelection();

    // Internal: set selection directly (used by undo/redo)
    void setSelection(const std::unordered_set<uint32_t>& faces);

    // M5: Connected selection operations
    /**
     * @brief Select all faces connected to the seed face via shared edges (BFS)
     * @param adjacency Face adjacency list from MeshData
     * @param seedFace The starting face index
     * @param op Selection operation mode
     * @return Set of all connected faces including the seed
     */
    std::unordered_set<uint32_t> selectLinked(
        const std::vector<std::unordered_set<uint32_t>>& adjacency,
        uint32_t seedFace,
        SelectionOp op = SelectionOp::Replace);

    /**
     * @brief Select connected faces with angle constraint
     * @param adjacency Face adjacency list from MeshData
     * @param faceNormals Pre-computed face normals
     * @param seedFace The starting face index
     * @param angleThreshold Maximum angle (degrees) between adjacent face normals
     * @param op Selection operation mode
     * @return Set of faces connected within angle threshold
     */
    std::unordered_set<uint32_t> selectByAngle(
        const std::vector<std::unordered_set<uint32_t>>& adjacency,
        const std::vector<std::array<float, 3>>& faceNormals,
        uint32_t seedFace,
        float angleThreshold,
        SelectionOp op = SelectionOp::Replace);

    /**
     * @brief Expand selection to include all adjacent faces
     * @param adjacency Face adjacency list from MeshData
     */
    void growSelection(const std::vector<std::unordered_set<uint32_t>>& adjacency);

    /**
     * @brief Shrink selection by removing boundary faces
     * @param adjacency Face adjacency list from MeshData
     */
    void shrinkSelection(const std::vector<std::unordered_set<uint32_t>>& adjacency);

signals:
    /**
     * @brief Emitted when selection changes
     */
    void selectionChanged();

    /**
     * @brief Emitted when selection mode changes
     */
    void modeChanged(SelectionMode newMode);

private:
    std::unordered_set<uint32_t> m_selectedFaces;
    SelectionMode m_mode = SelectionMode::Box;
    uint32_t m_faceCount = 0;
};

/**
 * @brief Undo command for face selection changes
 */
class SelectFacesCommand : public QUndoCommand
{
public:
    /**
     * @brief Create a selection command
     * @param system The selection system
     * @param newSelection The new selection state
     * @param text Description for undo history
     */
    SelectFacesCommand(SelectionSystem* system,
                       const std::unordered_set<uint32_t>& newSelection,
                       const QString& text = QObject::tr("Select Faces"));

    void undo() override;
    void redo() override;

    int id() const override { return 1001; }  // Unique ID for merging

    // Allow merging consecutive selections
    bool mergeWith(const QUndoCommand* other) override;

private:
    SelectionSystem* m_system;
    std::unordered_set<uint32_t> m_oldSelection;
    std::unordered_set<uint32_t> m_newSelection;
};

} // namespace MoldWing
