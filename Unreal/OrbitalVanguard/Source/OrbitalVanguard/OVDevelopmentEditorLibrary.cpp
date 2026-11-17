#include "OVDevelopmentEditorLibrary.h"
#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
namespace { int32 SavedWidth=0, SavedHeight=0; bool bHasSavedSize=false; }
#endif

bool UOVDevelopmentEditorLibrary::StartPreviewWindow(int32 Width,int32 Height)
{
#if WITH_EDITOR
 if(!GEditor||GEditor->IsPlaySessionInProgress()||Width<=0||Height<=0)return false;
 ULevelEditorPlaySettings* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
 if(!Settings||bHasSavedSize)return false;
 SavedWidth=Settings->NewWindowWidth;SavedHeight=Settings->NewWindowHeight;bHasSavedSize=true;
 Settings->NewWindowWidth=Width;Settings->NewWindowHeight=Height;
 FRequestPlaySessionParams Params;
 Params.SessionDestination=EPlaySessionDestinationType::InProcess;
 Params.WorldType=EPlaySessionWorldType::PlayInEditor;
 // No DestinationSlateViewport: UE creates a new in-process PIE window.
 GEditor->RequestPlaySession(Params);
 return true;
#else
 return false;
#endif
}

bool UOVDevelopmentEditorLibrary::RestorePreviewSettings()
{
#if WITH_EDITOR
 if(!bHasSavedSize)return false;
 ULevelEditorPlaySettings* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
 if(!Settings)return false;
 Settings->NewWindowWidth=SavedWidth;Settings->NewWindowHeight=SavedHeight;bHasSavedSize=false;return true;
#else
 return false;
#endif
}
