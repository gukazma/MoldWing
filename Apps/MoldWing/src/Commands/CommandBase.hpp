/*
 *  MoldWing - Command Base
 *  S1.7: Base class for undoable commands
 */

#pragma once

#include <QUndoCommand>
#include <QString>

namespace MoldWing
{

// Forward declarations
class MeshData;

// Base class for all MoldWing commands
class MoldWingCommand : public QUndoCommand
{
public:
    explicit MoldWingCommand(const QString& text, QUndoCommand* parent = nullptr)
        : QUndoCommand(text, parent)
    {}

    virtual ~MoldWingCommand() = default;

    // Disable copy
    MoldWingCommand(const MoldWingCommand&) = delete;
    MoldWingCommand& operator=(const MoldWingCommand&) = delete;
};

} // namespace MoldWing
