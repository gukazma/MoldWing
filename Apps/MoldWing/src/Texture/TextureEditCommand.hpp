/*
 *  MoldWing - Texture Edit Command
 *  S6.3: QUndoCommand for texture editing undo/redo
 */

#pragma once

#include <QUndoCommand>
#include <QRect>
#include <vector>
#include <cstdint>

namespace MoldWing
{

class TextureEditBuffer;

/**
 * TextureEditCommand stores pixel changes for undo/redo.
 * Each brush stroke creates one command.
 */
class TextureEditCommand : public QUndoCommand
{
public:
    TextureEditCommand(TextureEditBuffer* buffer, int textureIndex, QUndoCommand* parent = nullptr);
    ~TextureEditCommand() override = default;

    // Record a pixel change (call during stroke)
    void recordPixel(int x, int y, uint8_t oldR, uint8_t oldG, uint8_t oldB, uint8_t oldA,
                                   uint8_t newR, uint8_t newG, uint8_t newB, uint8_t newA);

    // Finalize the command (call at end of stroke)
    void finalize();

    // QUndoCommand interface
    void undo() override;
    void redo() override;
    int id() const override { return 1001; } // Custom ID for texture edits

    // Merging support (optional - for continuous strokes)
    bool mergeWith(const QUndoCommand* other) override;

    // Info
    int textureIndex() const { return m_textureIndex; }
    int pixelCount() const { return static_cast<int>(m_pixels.size()); }
    QRect boundingRect() const { return m_bounds; }

private:
    struct PixelChange
    {
        int16_t x, y;
        uint8_t oldR, oldG, oldB, oldA;
        uint8_t newR, newG, newB, newA;
    };

    TextureEditBuffer* m_buffer;
    int m_textureIndex;
    std::vector<PixelChange> m_pixels;
    QRect m_bounds;
    bool m_finalized = false;
};

} // namespace MoldWing
