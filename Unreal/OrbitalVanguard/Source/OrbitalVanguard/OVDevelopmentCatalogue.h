#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OVDevelopmentCatalogue.generated.h"

UENUM(BlueprintType)
enum class EOVCatalogCategory : uint8 { Hull, Weapon, Fighter, Pod, Hero, PointDefense };
UENUM(BlueprintType)
enum class EOVCatalogStatus : uint8 { Unknown, Available, Partial, Unavailable, NotApplicable };

USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVCatalogEvidence
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVCatalogStatus Status = EOVCatalogStatus::Unknown;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Reason;
    // Only a revision that actually produced a verified result belongs here.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString VerificationSHA;
};

/** Definition IDs identify templates; individual ship/unit/person IDs belong to their runtime owner. */
USTRUCT(BlueprintType)
struct ORBITALVANGUARD_API FOVCatalogDefinition
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName DefinitionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EOVCatalogCategory Category = EOVCatalogCategory::Hull;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString DisplayName;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Description;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString SourceEvidence;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FOVCatalogEvidence Planning;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FOVCatalogEvidence Data;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FOVCatalogEvidence Generation;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FOVCatalogEvidence Role;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FOVCatalogEvidence AutomatedVerification;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FOVCatalogEvidence DirectInput;
};

/** Read-only development inventory, not authoritative game rules or player knowledge. */
UCLASS()
class ORBITALVANGUARD_API UOVDevelopmentCatalogue : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure) TArray<FOVCatalogDefinition> GetDefinitions() const;
};
