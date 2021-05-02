// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Renderer/TextureBakerRenderContext.h"
#include "Engine/Texture.h"

class FToolBarBuilder;
class FMenuBuilder;

class TEXTUREBAKER_API FTextureBakerSurfaceReadback
{
public:

	// Actually reads surface to the preallocated memory buffer
	virtual void ReadbackBuffer(FTextureRenderTargetResource* SourceRT, void* DestBuffer) const = 0;

	// Retuns true if result of ReadbackBuffer can be used as an image of given format (ImageFormat) without any transcoding
	virtual bool DirectlyCompatibleWithImage(ETextureSourceFormat ImageFormat) const = 0;

	// Perform transcoding. Transcoding can be performd in two ways:
	// 1. Regular transcoding - ImageDestBuffer is an output buffer with ImageFormat pixels, ReadbackBuffer is non overlapping input buffer with result of calling ReadbackBuffer. Returns ImageDestBuffer
	// 2. Inline transcoding - ImageDestBuffer is buffer containing ReadbackBuffer output placed without offset. After transcoding result will be placed inside this buffer with some offset. Returns pointer to a transcoded image data.
	//    If buffer is not enough to use inline transcoding or inline transcoding is not supported returns nullptr
	virtual const void* TranscodeToImageFormat(ETextureSourceFormat ImageFormat, void* ImageDestBuffer, const void* ReadbackBuffer, uint64 PixelNum, uint64 BufferLength) const = 0;

	// Return minimal required buffer size to read surface content
	virtual uint64 GetRequiredReadbackBufferSize(const FIntPoint& TextureSize) const = 0;

	// Return minimal required buffer size to read surface content and perform inline transcoding to a specified format
	virtual uint64 GetRequiredReadbackBufferSize(const FIntPoint& TextureSize, ETextureSourceFormat InlineTranscodeTarget) const = 0;
};

class TEXTUREBAKER_API FTextureBakerModule : public IModuleInterface
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
	static TSharedPtr<FTextureBakerSurfaceReadback> GetReadbackHandler(EPixelFormat PixelFormat, bool sRGB);

private:

	void RegisterMenus();

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
};

namespace FTextureBakerMath
{
	void* OffsetPointer(void* BasePointer, int64 Offset) { return reinterpret_cast<uint8*>(BasePointer) + Offset; }
	const void* OffsetPointer(const void* BasePointer, int64 Offset) { return reinterpret_cast<const uint8*>(BasePointer) + Offset; }
	template <typename T> T* OffsetPointer(T* BasePointer, int64 Offset) { return reinterpret_cast<T*>(reinterpret_cast<uint8*>(BasePointer) + Offset); }
	template <typename T> typename std::make_signed<T>::type Signed(T Value) { return Value; }

};