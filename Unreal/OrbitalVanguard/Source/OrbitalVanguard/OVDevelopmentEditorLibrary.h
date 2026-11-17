#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OVDevelopmentEditorLibrary.generated.h"

/** Editor-only preview request. Does not alter game simulation or engine code. */
UCLASS()
class ORBITALVANGUARD_API UOVDevelopmentEditorLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 /** Returns false outside the editor, for invalid dimensions, or if PIE is already running. */
 UFUNCTION(BlueprintCallable, Category="Development|Editor")
 static bool StartPreviewWindow(int32 Width, int32 Height);
 /** Restore the window size captured by StartPreviewWindow. */
 UFUNCTION(BlueprintCallable, Category="Development|Editor")
 static bool RestorePreviewSettings();
};
