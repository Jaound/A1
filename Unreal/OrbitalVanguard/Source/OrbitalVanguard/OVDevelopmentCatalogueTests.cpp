#include "OVDevelopmentCatalogue.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVDevelopmentCatalogueTest, "OrbitalVanguard.Development.CatalogueDefinitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOVDevelopmentCatalogueTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    auto* Catalogue = World->GetSubsystem<UOVDevelopmentCatalogue>();
    const auto Definitions = Catalogue->GetDefinitions();
    TSet<FName> Ids; int32 Counts[6] = {};
    for (const FOVCatalogDefinition& D : Definitions)
    {
        TestFalse(TEXT("Definition ID is unique"), Ids.Contains(D.DefinitionId)); Ids.Add(D.DefinitionId);
        ++Counts[static_cast<int32>(D.Category)];
        TestFalse(TEXT("Description and provenance are present"), D.Description.IsEmpty() || D.SourceEvidence.IsEmpty());
        TestTrue(TEXT("Evidence is either absent or explicitly historical"),
            (D.AutomatedVerification.Status == EOVCatalogStatus::Unknown && D.AutomatedVerification.VerificationSHA.IsEmpty()) ||
            (D.AutomatedVerification.Status == EOVCatalogStatus::Partial && D.AutomatedVerification.VerificationSHA.Len() == 40));
    }
    TestEqual(TEXT("Hull definitions"), Counts[0], 16);
    TestEqual(TEXT("Weapon profiles"), Counts[1], 25);
    TestEqual(TEXT("Fighter definition"), Counts[2], 1);
    TestEqual(TEXT("Pod definition"), Counts[3], 1);
    TestEqual(TEXT("Hero types"), Counts[4], 4);
    TestEqual(TEXT("Point defense profiles"), Counts[5], 2);
    World->DestroyWorld(false); return true;
}
#endif
