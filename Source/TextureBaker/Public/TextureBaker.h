// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Renderer/TextureBakerRenderContext.h"

class FToolBarBuilder;
class FMenuBuilder;

class FTextureBakerModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command (by default it will bring up plugin window) */
	void PluginButtonClicked();
	void ExecuteBakerRenderContext(TUniquePtr<FTextureBakerRenderContext> Context);
	void SaveBakedTextureResult(const FTextureBakerRenderResult& Result, bool bOverrideExistingFiles);

	/** Generate texture source data from render target content */
	void WriteTexture2DSourceArt(UTexture2D* InTexture2D, ETextureSourceFormat InTextureFormat, UTextureRenderTarget2D* SourceRT, uint32 Flags);

	static FTextureBakerModule* Get() { return static_cast<FTextureBakerModule*>(FModuleManager::Get().GetModule("TextureBaker")); }
	static FTextureBakerModule& GetChecked() { return FModuleManager::LoadModuleChecked<FTextureBakerModule>("TextureBaker"); }
	static ETextureRenderTargetFormat SelectRenderTargetFormatForPixelFormat(EPixelFormat PixelFormat, bool sRGB);
	static ETextureRenderTargetFormat SelectRenderTargetFormatForImageSourceFormat(ETextureSourceFormat OutputImageFormat, bool bUseSRGB);
	static ETextureSourceFormat SelectImageSourceFormatForRenderTargetFormat(ETextureRenderTargetFormat OutputImageFormat);


private:

	void RegisterMenus();

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
};