#include "Voxel/Editor/VoxelHistoryData.h"
#include "Voxel/Editor/VoxelEditorData.h"

FVoxelEditModification FVoxelEditHistoryRecord::CreateUndoModification() const
{
    FVoxelEditModification Modification;
    Modification.Reserve(Edits.Num());
    // EditCommand 使用默认值 FVoxelEditCommand::BuildNormal()

    for (const FVoxelEdit& Edit : Edits)
    {
        Modification.Add(Edit.BlockIndex, Edit.OldHandleId);
    }

    return Modification;
}

FVoxelEditModification FVoxelEditHistoryRecord::CreateRedoModification() const
{
    FVoxelEditModification Modification;
    Modification.Reserve(Edits.Num());
    Modification.SetEditCommand(EditCommand); // 透传放置模式（Replace/Complement/Normal）

    for (const FVoxelEdit& Edit : Edits)
    {
        Modification.Add(Edit.BlockIndex, Edit.NewHandleId);
    }

    return Modification;
}
