
#pragma once

#include "CoreMinimal.h"
#include "Data/Item/InteractableData.h"
#include "UObject/Interface.h"
#include "InteractionItemInterface.generated.h"


// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UInteractionItemInterface : public UInterface
{
	GENERATED_BODY()
};
/**
 * 
 */
class VOXEL_API IInteractionItemInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	
	virtual void InitializeItemInstance();

	virtual void BeginFocus();
	virtual void EndFocus();
	virtual void BeginInteract();
	virtual void EndInteract();
	virtual void Interact();
	
	

	FInteractableData InteractableData;
};
