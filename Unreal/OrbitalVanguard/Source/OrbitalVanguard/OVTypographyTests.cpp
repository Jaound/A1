#if WITH_DEV_AUTOMATION_TESTS
#include "OVTypography.h"
#include "Engine/Font.h"
#include "Fonts/FontCache.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOVTypographyTest,"OrbitalVanguard.UI.Typography",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOVTypographyTest::RunTest(const FString&)
{
    const auto Renderer=FSlateApplication::Get().GetRenderer();
    const auto Cache=Renderer->GetFontCache();
    const auto Measure=Renderer->GetFontMeasureService();
    const TCHAR* Expected[]={TEXT("Body"),TEXT("Heading"),TEXT("Heading"),TEXT("Heading"),TEXT("Numeric"),TEXT("Logo"),TEXT("Comms")};
    for(int32 I=0; I<int32(EOVTextRole::Count); ++I)
    {
        const auto Role=static_cast<EOVTextRole>(I);
        const UFont* Font=OVTypography::Font(Role);
        if (!TestNotNull(TEXT("역할별 배포 글꼴"),Font)) return false;
        const auto& Family=Font->GetInternalCompositeFont();
        if (!TestEqual(TEXT("정적 페이스 하나"),Family.DefaultTypeface.Fonts.Num(),1)) return false;
        const auto* Face=Family.DefaultTypeface.Fonts[0].Font.GetFontFaceAsset();
        if (!TestNotNull(TEXT("실제 기본 페이스"),Face)) return false;
        TestEqual(TEXT("원본별 기본 페이스"),Face->GetName(),FString(TEXT("Face_"))+Expected[I]);
        if (!TestEqual(TEXT("한국어 대체 페이스"),Family.FallbackTypeface.Typeface.Fonts.Num(),1)) return false;
        const auto* Fallback=Family.FallbackTypeface.Typeface.Fonts[0].Font.GetFontFaceAsset();
        if (!TestNotNull(TEXT("실제 대체 페이스"),Fallback)) return false;
        TestEqual(TEXT("프로젝트 본문 폴백"),Fallback->GetName(),FString(TEXT("Face_Body")));
        auto& List=Cache->GetCharacterList(OVTypography::Style(Role),1.f,FFontOutlineSettings());
        for(TCHAR Ch : FString(TEXT("값쉴드이리스베인HP37%,.±초m:")))
        {
            const auto& Entry=List.GetCharacter(Ch,EFontFallback::FF_NoFallback);
            const auto* Actual=Entry.FontData ? Entry.FontData->GetFontFaceAsset() : nullptr;
            TestTrue(FString::Printf(TEXT("%s / %c 프로젝트 글리프"),Expected[I],Ch),
                Entry.GlyphIndex!=0 && Actual && (Actual==Face || Actual==Fallback));
        }
    }
    const auto Numbers=OVTypography::Style(EOVTextRole::Numeric);
    for(float Scale : {.8f,1.f,1.5f})
    {
        const auto Width=[&](const TCHAR* Value){return Measure->Measure(FStringView(Value),Numbers,Scale).X;};
        TestEqual(TEXT("타이머 숫자 폭"),Width(TEXT("00:11")),Width(TEXT("00:10")));
        TestEqual(TEXT("고정폭 숫자"),Width(TEXT("999")),Width(TEXT("100")));
        TestTrue(TEXT("천 단위 및 퍼센트는 고정 칸에 수용"),Width(TEXT("1,000"))<100*Scale && Width(TEXT("100%"))<64*Scale);
    }
    return true;
}
#endif
