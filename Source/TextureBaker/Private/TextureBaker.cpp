// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureBaker.h"
#include "TextureBakerStyle.h"
#include "TextureBakerCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"
#include "ToolMenus.h"
#include "Widgets/TextureBaker/STextureBakerScenarioWidget.h"
#include "Misc/ScopedSlowTask.h"
#include "AssetToolsModule.h"
#include "PackageTools.h"
#include "ObjectTools.h"
#include "AssetRegistryModule.h"

static const FName TextureBakerTabName("TextureBaker");

#define LOCTEXT_NAMESPACE "FTextureBakerModule"

void FTextureBakerModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FTextureBakerStyle::Initialize();
	FTextureBakerStyle::ReloadTextures();

	FTextureBakerCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FTextureBakerCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FTextureBakerModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FTextureBakerModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TextureBakerTabName, FOnSpawnTab::CreateRaw(this, &FTextureBakerModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FTextureBakerTabTitle", "TextureBaker"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FTextureBakerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FTextureBakerStyle::Shutdown();

	FTextureBakerCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TextureBakerTabName);
}

TSharedRef<SDockTab> FTextureBakerModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	FText WidgetText = FText::Format(
		LOCTEXT("WindowWidgetText", "Add code to {0} in {1} to override this window's contents"),
		FText::FromString(TEXT("FTextureBakerModule::OnSpawnPluginTab")),
		FText::FromString(TEXT("TextureBaker.cpp"))
		);

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(STextureBakerScenarioWidget)
		];
}

void FTextureBakerModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->InvokeTab(TextureBakerTabName);
}

void FTextureBakerModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FTextureBakerCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FTextureBakerCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

void FTextureBakerModule::ExecuteBakerRenderContext(TUniquePtr<FTextureBakerRenderContext> Context)
{
	FScopedSlowTask Feedback(Context->GetOutputsToBake().Num() + 1, NSLOCTEXT("TextureBaker", "TextureBaker_BakeTexture", "Bake requested textures..."));
	Feedback.MakeDialog(true);

	/* Prepare for baking */
	{
		Feedback.EnterProgressFrame();
		Context->PrepareToBakeOutputs();
	}

	/* Bake each output */
	for (const FName& OutputName : Context->GetOutputsToBake())
	{
		Context->EnterRenderScope();
		Feedback.EnterProgressFrame();
		const FTextureBakerRenderResult& Result = Context->BakeOutput(OutputName);
		if (Result.IsValid())
		{
			SaveBakedTextureResult(Result, true);
		}
		Context->ExitRenderScope();
	}
}

void FTextureBakerModule::SaveBakedTextureResult(const FTextureBakerRenderResult& Result, bool bOverrideExistingFiles)
{
	if (Result.IsValid())
	{
		FString AssetLongPackageName = Result.GetPackagePath();
		const FString PackagePath = FPackageName::GetLongPackagePath(AssetLongPackageName);
		const FString BaseAssetName = FPackageName::GetLongPackageAssetName(AssetLongPackageName);
		FString SanitizedBaseAssetName = ObjectTools::SanitizeObjectName(BaseAssetName);

		if (!bOverrideExistingFiles)
		{
			IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			AssetTools.CreateUniqueAssetName(AssetLongPackageName, TEXT(""), AssetLongPackageName, SanitizedBaseAssetName);
		}

		UPackage* PackageToSaveTexture = UPackageTools::FindOrCreatePackageForAssetType(*AssetLongPackageName, UTexture2D::StaticClass());
		UTextureRenderTarget2D* RenderTarget = Result.GetTextureRenderTarget();

		if (PackageToSaveTexture && RenderTarget)
		{
			UTexture2D* Texture = NewObject<UTexture2D>(PackageToSaveTexture, *BaseAssetName, RenderTarget->GetMaskedFlags() | RF_Public | RF_Standalone);
			check(Texture);

			Texture->CompressionSettings = Result.GetInfo().CompressionSettings;
			Texture->MipGenSettings = Result.GetInfo().MipGenSettings;
			Texture->CompressionNoAlpha = Result.GetInfo().bCompressWithoutAlpha;
			WriteTexture2DSourceArt(Texture, Result.GetInfo().OutputImageFormat, RenderTarget, 0);

			Texture->MarkPackageDirty();

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(Texture);
		}
	}
}

void FTextureBakerModule::WriteTexture2DSourceArt(UTexture2D* InTexture2D, ETextureSourceFormat InTextureFormat, UTextureRenderTarget2D* SourceRT, uint32 Flags)
{
	if (InTexture2D && SourceRT)
	{
		
		
		InTexture2D->ForceRebuildPlatformData();
		InTexture2D->UpdateResource();
	}
}

ETextureRenderTargetFormat FTextureBakerModule::SelectRenderTargetFormatForPixelFormat(EPixelFormat PixelFormat, bool sRGB)
{
	switch (PixelFormat)
	{
	case PF_G8: return RTF_R8;
	case PF_R8G8: return RTF_RG8;
	case PF_B8G8R8A8: return sRGB ? RTF_RGBA8_SRGB : RTF_RGBA8;

	case PF_R16F: return RTF_R16f;
	case PF_G16R16F: return RTF_RG16f;
	case PF_FloatRGBA: return RTF_RGBA16f;

	case PF_R32_FLOAT: return RTF_R32f;
	case PF_G32R32F: return RTF_RG32f;
	case PF_A32B32G32R32F: return RTF_RGBA32f;
	case PF_A2B10G10R10: return RTF_RGB10A2;

	case PF_DXT1:
	case PF_DXT3:
		return RTF_R8;

	case PF_DXT5:
		return sRGB ? RTF_RGBA8_SRGB : RTF_RGBA8;

	case PF_BC5:
	case PF_BC7:
		return sRGB ? RTF_RGBA8_SRGB : RTF_RGBA8;

	}
	return sRGB ? RTF_RGBA8_SRGB : RTF_RGBA8;
}

ETextureRenderTargetFormat FTextureBakerModule::SelectRenderTargetFormatForImageSourceFormat(ETextureSourceFormat OutputImageFormat, bool bUseSRGB)
{
	switch (OutputImageFormat)
	{
	case ETextureSourceFormat::TSF_BGRA8:
	case ETextureSourceFormat::TSF_RGBA8:
		return bUseSRGB ? ETextureRenderTargetFormat::RTF_RGBA8_SRGB : ETextureRenderTargetFormat::RTF_RGBA8;
	case ETextureSourceFormat::TSF_BGRE8:
	case ETextureSourceFormat::TSF_RGBE8:
		return ETextureRenderTargetFormat::RTF_RGBA16f;
	case ETextureSourceFormat::TSF_G8:
		return ETextureRenderTargetFormat::RTF_R8;
	case ETextureSourceFormat::TSF_G16:
		return ETextureRenderTargetFormat::RTF_R16f;
	case ETextureSourceFormat::TSF_RGBA16:
	case ETextureSourceFormat::TSF_RGBA16F:
		return ETextureRenderTargetFormat::RTF_RGBA16f;
	default:
		return ETextureRenderTargetFormat::RTF_RGBA8;
	}
}

ETextureSourceFormat FTextureBakerModule::SelectImageSourceFormatForRenderTargetFormat(ETextureRenderTargetFormat RTFormat)
{
	switch (RTFormat)
	{
	case ETextureRenderTargetFormat::RTF_R8: 
		return ETextureSourceFormat::TSF_G8;

	case ETextureRenderTargetFormat::RTF_R16f:
	case ETextureRenderTargetFormat::RTF_R32f: 
		return ETextureSourceFormat::TSF_G16;

	case ETextureRenderTargetFormat::RTF_RG16f:
	case ETextureRenderTargetFormat::RTF_RG32f:
	case ETextureRenderTargetFormat::RTF_RGBA16f:
	case ETextureRenderTargetFormat::RTF_RGBA32f: 
		return ETextureSourceFormat::TSF_RGBA16F;

	case ETextureRenderTargetFormat::RTF_RG8:
	case ETextureRenderTargetFormat::RTF_RGB10A2:
	case ETextureRenderTargetFormat::RTF_RGBA8:
	case ETextureRenderTargetFormat::RTF_RGBA8_SRGB:
		return ETextureSourceFormat::TSF_BGRA8;
	}
	return ETextureSourceFormat::TSF_BGRA8;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTextureBakerModule, TextureBaker)