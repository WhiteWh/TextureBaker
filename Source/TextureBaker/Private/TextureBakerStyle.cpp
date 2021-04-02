// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureBakerStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr< FSlateStyleSet > FTextureBakerStyle::StyleInstance = NULL;

void FTextureBakerStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FTextureBakerStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FTextureBakerStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("TextureBakerStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( Style->RootToContentDir( RelativePath, TEXT(".png") ), __VA_ARGS__ )
#define TTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".ttf") ), __VA_ARGS__ )
#define OTF_FONT( RelativePath, ... ) FSlateFontInfo( Style->RootToContentDir( RelativePath, TEXT(".otf") ), __VA_ARGS__ )
#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)
#define ICON_FONT(...) FSlateFontInfo(Style->RootToContentDir("Fonts/FontAwesome", TEXT(".ttf")), __VA_ARGS__)

const FVector2D Icon8x8(8.0f, 8.0f);
const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);

TSharedRef< FSlateStyleSet > FTextureBakerStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("TextureBakerStyle"));
	const FTextBlockStyle& NormalText = FEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");

	Style->SetContentRoot(IPluginManager::Get().FindPlugin("TextureBaker")->GetBaseDir() / TEXT("Resources"));

	Style->Set("TextureBaker.OpenPluginWindow", new IMAGE_BRUSH(TEXT("ButtonIcon_40x"), Icon40x40));
	Style->Set("TextureBaker.OpenPluginWindow.Small", new IMAGE_BRUSH(TEXT("ButtonIcon_40x"), Icon20x20));

	FComboButtonStyle ToolbarComboButton = FComboButtonStyle()
		.SetButtonStyle(Style->GetWidgetStyle<FButtonStyle>("ToggleButton"))
		.SetDownArrowImage(IMAGE_BRUSH("Common/ShadowComboArrow", Icon8x8))
		.SetMenuBorderBrush(BOX_BRUSH("Old/Menu_Background", FMargin(8.0f / 64.0f)))
		.SetMenuBorderPadding(FMargin(0.0f));
	
	Style->Set("ScenarioSelector.ComboButton", ToolbarComboButton);

	Style->Set("TextureBakerWidget.TabIcon", new IMAGE_BRUSH("Icons/AssetIcons/Character_16x", Icon16x16));

	Style->Set("ScenarioSelector.NewPreset", new IMAGE_BRUSH("Icons/icon_file_saveas_16px", Icon16x16));
	Style->Set("ScenarioSelector.LoadPreset", new IMAGE_BRUSH("Icons/icon_file_open_16px", Icon16x16));
	Style->Set("ScenarioSelector.SavePreset", new IMAGE_BRUSH("Icons/icon_file_save_16px", Icon16x16));


	Style->Set("ScenarioSelector.TextStyle", FTextBlockStyle(NormalText)
		.SetFont(DEFAULT_FONT("Bold", 9))
		.SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f))
		.SetShadowOffset(FVector2D(0, 0))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.0f)));

	Style->Set("ScenarioSelector.DragDrop.Border", new BOX_BRUSH("Old/Menu_Background", FMargin(8.0f / 64.0f)));

	Style->Set("FontAwesome.9", ICON_FONT(9));

	return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT
#undef DEFAULT_FONT
#undef ICON_FONT

void FTextureBakerStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FTextureBakerStyle::Get()
{
	return *StyleInstance;
}
