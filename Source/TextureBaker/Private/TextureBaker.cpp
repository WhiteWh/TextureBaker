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
#include "Interfaces/IPluginManager.h"
#include "EngineLogs.h"

static const FName TextureBakerTabName("TextureBaker");

#define LOCTEXT_NAMESPACE "FTextureBakerModule"

void FTextureBakerModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("TextureBaker"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/TextureBaker"), PluginShaderDir);

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
		FPaths::RemoveDuplicateSlashes(AssetLongPackageName);
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
			PackageToSaveTexture->FullyLoad();
			UTexture2D* Texture = FindObject<UTexture2D>(PackageToSaveTexture, *BaseAssetName, true);
			if (Texture == nullptr)
			{
				Texture = NewObject<UTexture2D>(PackageToSaveTexture, *BaseAssetName, RenderTarget->GetMaskedFlags() | RF_Public | RF_Standalone);
			}

			check(Texture);

			Texture->PowerOfTwoMode = Result.GetInfo().Padding;
			Texture->CompressionSettings = Result.GetInfo().CompressionSettings;
			Texture->MipGenSettings = Result.GetInfo().MipGenSettings;
			Texture->CompressionNoAlpha = Result.GetInfo().bCompressWithoutAlpha;
			Texture->PaddingColor = Result.GetInfo().DefaultColor.ToFColor(Result.GetInfo().bUseSRGB);
			WriteTexture2DSourceArt(Texture, Result.GetInfo().OutputImageFormat, RenderTarget, Result.GetInfo().Normalization);

			Texture->MarkPackageDirty();

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(Texture);

			FString PackageFileName = FPackageName::LongPackageNameToFilename(AssetLongPackageName, FPackageName::GetAssetPackageExtension());
			UPackage::SavePackage(PackageToSaveTexture, Texture, RF_Standalone, *PackageFileName, GLog, nullptr, false, true, SAVE_None);
		}
	}
}

void FTextureBakerModule::WriteTexture2DSourceArt(UTexture2D* InTexture2D, ETextureSourceFormat InTextureFormat, UTextureRenderTarget2D* SourceRT, ETBImageNormalization DataRange)
{
	if (InTexture2D && InTextureFormat == ETextureSourceFormat::TSF_Invalid)
	{
		UE_LOG(LogTexture, Error, TEXT("Texture %s has source art in an invalid format."), *InTexture2D->GetName());
		return;
	}

	if (InTexture2D && SourceRT)
	{
		FIntPoint Size(SourceRT->SizeX, SourceRT->SizeY);
		FTextureRenderTargetResource* RenderTargetResource = SourceRT->GameThread_GetRenderTargetResource();
		TSharedPtr<FTextureBakerSurfaceReadback> ReadbackHandler = GetReadbackHandler(InTextureFormat, SourceRT->IsSRGB(), DataRange);

		const uint64 ImagePixelBytes = FTextureSource::GetBytesPerPixel(InTextureFormat);
		const uint64 ImagePixelsTotal = Size.X * Size.Y;
		const uint64 ImageBytesSize = ImagePixelBytes * ImagePixelsTotal;
		if (RenderTargetResource && ReadbackHandler)
		{
			if (ReadbackHandler->DirectlyCompatibleWithImage(InTextureFormat))
			{
				InTexture2D->Source.Init(Size.X, Size.Y, 1, 1, InTextureFormat);
				if (void* PixelDataBuffer = InTexture2D->Source.LockMip(0))
				{
					ReadbackHandler->ReadbackBuffer(RenderTargetResource, PixelDataBuffer);
				}
				InTexture2D->Source.UnlockMip(0);
			}
			else
			{
				// Allocate a buffer length enough to contain either transcoded or readback data and one extra transcoded pixel.
				// Readback is placed with one pixel offset and then transcoder writes first readback pixel to a reserved pixel location
				// and continues overlapped transcoding.
				const uint64 ReadbackBytesSize = ReadbackHandler->GetRequiredReadbackBufferSize(Size, InTextureFormat);
				void* PixelDataBuffer = FMemory::Malloc(ReadbackBytesSize);
				ReadbackHandler->ReadbackBuffer(RenderTargetResource, PixelDataBuffer);
				/*if (const void* EncodedDataPosition = ReadbackHandler->TranscodeToImageFormat(InTextureFormat, PixelDataBuffer, nullptr, ImagePixelsTotal, ReadbackBytesSize))
				{
					// Inline transcoding is possible, just pass result to the texture maker
					InTexture2D->Source.Init(Size.X, Size.Y, 1, 1, InTextureFormat, (uint8*)EncodedDataPosition);
				}
				else*/
				{
					// Inline transcoding is impossible, transcode using locked mip as a secondary buffer
					InTexture2D->Source.Init(Size.X, Size.Y, 1, 1, InTextureFormat);
					if (void* OutputBuffer = InTexture2D->Source.LockMip(0))
					{
						ReadbackHandler->TranscodeToImageFormat(InTextureFormat, OutputBuffer, PixelDataBuffer, ImagePixelsTotal, ReadbackBytesSize);
					}
					InTexture2D->Source.UnlockMip(0);
				}
				FMemory::Free(PixelDataBuffer);
			}
		}
		else
		{
			InTexture2D->Source.Init(Size.X, Size.Y, 1, 1, InTextureFormat);
			uint8* RawMemory = InTexture2D->Source.LockMip(0);
			FMemory::Memset(RawMemory, 0, ImageBytesSize);
			InTexture2D->Source.UnlockMip(0);
		}

		// Disable mips if they're couldn't be generated
		if (InTexture2D->PowerOfTwoMode == ETexturePowerOfTwoSetting::None && (!InTexture2D->Source.IsPowerOfTwo()))
		{
			InTexture2D->MipGenSettings = TMGS_NoMipmaps;
			InTexture2D->NeverStream = true;
		}
		InTexture2D->SRGB = SourceRT->IsSRGB();
		//InTexture2D->ForceRebuildPlatformData();
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

template <typename T> class FTextureBakerNativeReadback : public FTextureBakerSurfaceReadback
{
public:
	virtual bool DirectlyCompatibleWithImage(ETextureSourceFormat ImageFormat) const override { return false; }
	virtual uint64 GetRequiredReadbackBufferSize(const FIntPoint& TextureSize) const override { return InputPixelSize * TextureSize.X * TextureSize.Y; }
	virtual uint64 GetRequiredReadbackBufferSize(const FIntPoint& TextureSize, ETextureSourceFormat InlineTranscodeTarget) const override
	{
		const uint64 ReadbackOutputSize = GetRequiredReadbackBufferSize(TextureSize);
		const uint64 OutputPixelSize = FTextureSource::GetBytesPerPixel(InlineTranscodeTarget);
		const uint64 PixelNum = TextureSize.X * TextureSize.Y;
		const uint64 FinalTextureSize = OutputPixelSize * PixelNum;
		const uint64 SafeguardArea = ComputeOverlappingEncoderSafeArea(InputPixelSize, OutputPixelSize, PixelNum);
		return DirectlyCompatibleWithImage(InlineTranscodeTarget) ? ReadbackOutputSize : FMath::Max(FinalTextureSize + SafeguardArea, ReadbackOutputSize);
	}

	virtual const void* TranscodeToImageFormat(ETextureSourceFormat ImageFormat, void* ImageDestBuffer, const void* ReadbackBuffer, uint64 PixelNum, uint64 BufferLength) const override
	{
		check(ImageDestBuffer);
		const uint64 OutputPixelSize = FTextureSource::GetBytesPerPixel(ImageFormat);
		const uint64 SafeguardArea = ComputeOverlappingEncoderSafeArea(InputPixelSize, OutputPixelSize, PixelNum);
		const uint64 OverlappedOutputPosition = OutputPixelSize * PixelNum + SafeguardArea;

		if (ReadbackBuffer)
		{
			// Non-overlapping transcoding
			TranscodePixels(ImageFormat, ImageDestBuffer, ReadbackBuffer, OutputPixelSize, InputPixelSize, PixelNum);
			return ImageDestBuffer;
		}
		else if (BufferLength >= OverlappedOutputPosition + OutputPixelSize)
		{
			// Inline transcoding. Have to align areas to make them non-overlapping
			void* TranscoderOutputPosition = FTextureBakerMath::OffsetPointer(ImageDestBuffer, OverlappedOutputPosition);
			const void* TranscoderInputPosition = FTextureBakerMath::OffsetPointer(ImageDestBuffer, InputPixelSize * PixelNum);
			TranscodePixels(ImageFormat, TranscoderOutputPosition, TranscoderInputPosition, -FTextureBakerMath::Signed(OutputPixelSize), -FTextureBakerMath::Signed(InputPixelSize), PixelNum);
			return FTextureBakerMath::OffsetPointer(ImageDestBuffer, SafeguardArea);
		}
		return nullptr;
	}

	virtual void TranscodePixels(ETextureSourceFormat ImageFormat, void* DestPixels, const void* SrcPixels, int64 DestStep, int64 SrcStep, uint64 PixelNum) const = 0;

protected:
	const uint64 InputPixelSize = sizeof(T);
	uint64 ComputeOverlappingEncoderSafeArea(uint64 SrcPixelSize, uint64 OutputPixelSize, uint64 PixelNum) const
	{
		const uint64 PixelBufferCoveringSpeed = OutputPixelSize - FMath::Min(SrcPixelSize, OutputPixelSize);
		const uint64 SafeguardArea = PixelBufferCoveringSpeed * PixelNum + SrcPixelSize;
		return SafeguardArea;
	}
};

template <typename T> class FTextureBakerNativeMappedReadback : public FTextureBakerNativeReadback<T>
{
public:
	FTextureBakerNativeMappedReadback() : RangeMappingMode(ERangeCompressionMode::RCM_UNorm) {}
	FTextureBakerNativeMappedReadback(ERangeCompressionMode RangeMapping) : RangeMappingMode(RangeMapping) {}

protected:
	ERangeCompressionMode RangeMappingMode;
};

#if PLATFORM_LITTLE_ENDIAN
 #define _ToPackedBGRA ToPackedARGB
 #define _ToPackedRGBA ToPackedABGR
#else // PLATFORM_LITTLE_ENDIAN
 #define _ToPackedBGRA ToPackedBGRA
 #define _ToPackedRGBA ToPackedRGBA
#endif // !PLATFORM_LITTLE_ENDIAN

class FTextureBakerLinearReadback : public FTextureBakerNativeMappedReadback<FLinearColor>
{
public:
	
	FTextureBakerLinearReadback() : FTextureBakerNativeMappedReadback<FLinearColor>(ERangeCompressionMode::RCM_UNorm), bInputInSRGB(true) {}
	FTextureBakerLinearReadback(ERangeCompressionMode RangeMapping, bool bSRGBInput) : FTextureBakerNativeMappedReadback<FLinearColor>(RangeMapping), bInputInSRGB(bSRGBInput) {}

	virtual void ReadbackBuffer(FTextureRenderTargetResource* SourceRT, void* DestBuffer) const override
	{
		check(SourceRT);
		check(DestBuffer);
		FReadSurfaceDataFlags Options(RangeMappingMode);
		Options.SetLinearToGamma(!bInputInSRGB);
		SourceRT->ReadLinearColorPixelsPtr((FLinearColor*)DestBuffer, Options);
	}

	virtual void TranscodePixels(ETextureSourceFormat ImageFormat, void* DestPixels, const void* SrcPixels, int64 DestStep, int64 SrcStep, uint64 PixelNum) const override
	{
		for (uint64 PixelIndex = 0; PixelIndex < PixelNum; PixelIndex++)
		{
			switch (ImageFormat)
			{
			case ETextureSourceFormat::TSF_BGRA8:
			{
				*reinterpret_cast<uint32*>(DestPixels) = reinterpret_cast<const FLinearColor*>(SrcPixels)->ToFColor(bInputInSRGB)._ToPackedBGRA();
				break;
			}
			case ETextureSourceFormat::TSF_BGRE8:
			{
				*reinterpret_cast<uint32*>(DestPixels) = reinterpret_cast<const FLinearColor*>(SrcPixels)->ToRGBE()._ToPackedBGRA();
				break;
			}
			case ETextureSourceFormat::TSF_G16:
			{
				*reinterpret_cast<uint16*>(DestPixels) = reinterpret_cast<const FLinearColor*>(SrcPixels)->GetLuminance() * 65535;
				break;
			}
			case ETextureSourceFormat::TSF_G8:
			{
				*reinterpret_cast<uint8*>(DestPixels) = reinterpret_cast<const FLinearColor*>(SrcPixels)->GetLuminance() * 255;
				break;
			}
			case ETextureSourceFormat::TSF_RGBA16:
			{
				const FLinearColor SourceColor = *reinterpret_cast<const FLinearColor*>(SrcPixels);
				reinterpret_cast<uint16*>(DestPixels)[0] = FMath::Clamp(SourceColor.R, 0.0f, 1.0f) * 65535;
				reinterpret_cast<uint16*>(DestPixels)[1] = FMath::Clamp(SourceColor.G, 0.0f, 1.0f) * 65535;
				reinterpret_cast<uint16*>(DestPixels)[2] = FMath::Clamp(SourceColor.B, 0.0f, 1.0f) * 65535;
				reinterpret_cast<uint16*>(DestPixels)[3] = FMath::Clamp(SourceColor.A, 0.0f, 1.0f) * 65535;
				break;
			}
			case ETextureSourceFormat::TSF_RGBA16F:
			{
				*reinterpret_cast<FFloat16Color*>(DestPixels) = FFloat16Color(*reinterpret_cast<const FLinearColor*>(SrcPixels));
				break;
			}
			case ETextureSourceFormat::TSF_RGBA8:
			{
				*reinterpret_cast<uint32*>(DestPixels) = reinterpret_cast<const FLinearColor*>(SrcPixels)->ToFColor(false)._ToPackedRGBA();
				break;
			}
			case ETextureSourceFormat::TSF_RGBE8:
			{
				*reinterpret_cast<uint32*>(DestPixels) = reinterpret_cast<const FLinearColor*>(SrcPixels)->ToRGBE()._ToPackedRGBA();
				break;
			}
			}
			DestPixels = FTextureBakerMath::OffsetPointer(DestPixels, DestStep);
			SrcPixels = FTextureBakerMath::OffsetPointer(SrcPixels, SrcStep);
		}
	}
protected:
	bool bInputInSRGB;
};

class FTextureBakerColorReadback : public FTextureBakerNativeMappedReadback<FColor>
{
public:
	FTextureBakerColorReadback() : FTextureBakerNativeMappedReadback<FColor>(ERangeCompressionMode::RCM_UNorm), bInputInSRGB(true) {}
	FTextureBakerColorReadback(ERangeCompressionMode RangeMapping, bool bSRGBInput) : FTextureBakerNativeMappedReadback<FColor>(RangeMapping), bInputInSRGB(bSRGBInput) {}
	virtual bool DirectlyCompatibleWithImage(ETextureSourceFormat ImageFormat) const override { return ImageFormat == ETextureSourceFormat::TSF_BGRA8; }

	virtual void ReadbackBuffer(FTextureRenderTargetResource* SourceRT, void* DestBuffer) const override
	{
		check(SourceRT);
		check(DestBuffer);
		FReadSurfaceDataFlags Options(RangeMappingMode);
		Options.SetLinearToGamma(!bInputInSRGB);
		SourceRT->ReadPixelsPtr((FColor*)DestBuffer, Options);
	}

	virtual void TranscodePixels(ETextureSourceFormat ImageFormat, void* DestPixels, const void* SrcPixels, int64 DestStep, int64 SrcStep, uint64 PixelNum) const override
	{
		for (uint64 PixelIndex = 0; PixelIndex < PixelNum; PixelIndex++)
		{
			switch (ImageFormat)
			{
			case ETextureSourceFormat::TSF_BGRA8:
			{
				*reinterpret_cast<uint32*>(DestPixels) = reinterpret_cast<const FColor*>(SrcPixels)->_ToPackedBGRA();
				break;
			}
			case ETextureSourceFormat::TSF_BGRE8:
			{
				*reinterpret_cast<uint32*>(DestPixels) = FLinearColor(*reinterpret_cast<const FColor*>(SrcPixels)).ToRGBE()._ToPackedBGRA();
				break;
			}
			case ETextureSourceFormat::TSF_G16:
			{
				*reinterpret_cast<uint16*>(DestPixels) = FLinearColor(*reinterpret_cast<const FColor*>(SrcPixels)).GetLuminance() * 65535;
				break;
			}
			case ETextureSourceFormat::TSF_G8:
			{
				*reinterpret_cast<uint8*>(DestPixels) = FLinearColor(*reinterpret_cast<const FColor*>(SrcPixels)).GetLuminance() * 255;
				break;
			}
			case ETextureSourceFormat::TSF_RGBA16:
			{
				const FLinearColor SourceColor(*reinterpret_cast<const FColor*>(SrcPixels));
				reinterpret_cast<uint16*>(DestPixels)[0] = FMath::Clamp(SourceColor.R, 0.0f, 1.0f) * 65535;
				reinterpret_cast<uint16*>(DestPixels)[1] = FMath::Clamp(SourceColor.G, 0.0f, 1.0f) * 65535;
				reinterpret_cast<uint16*>(DestPixels)[2] = FMath::Clamp(SourceColor.B, 0.0f, 1.0f) * 65535;
				reinterpret_cast<uint16*>(DestPixels)[3] = FMath::Clamp(SourceColor.A, 0.0f, 1.0f) * 65535;
				break;
			}
			case ETextureSourceFormat::TSF_RGBA16F:
			{
				*reinterpret_cast<FFloat16Color*>(DestPixels) = FFloat16Color(FLinearColor(*reinterpret_cast<const FColor*>(SrcPixels)));
				break;
			}
			case ETextureSourceFormat::TSF_RGBA8:
			{
				*reinterpret_cast<uint32*>(DestPixels) = reinterpret_cast<const FColor*>(SrcPixels)->_ToPackedRGBA();
				break;
			}
			case ETextureSourceFormat::TSF_RGBE8:
			{
				*reinterpret_cast<uint32*>(DestPixels) = FLinearColor(*reinterpret_cast<const FColor*>(SrcPixels)).ToRGBE()._ToPackedRGBA();
				break;
			}
			}
			DestPixels = FTextureBakerMath::OffsetPointer(DestPixels, DestStep);
			SrcPixels = FTextureBakerMath::OffsetPointer(SrcPixels, SrcStep);
		}
	}
protected:
	bool bInputInSRGB;
};

ERangeCompressionMode ReadbackNormalizationMode(ETBImageNormalization DataRange)
{
	switch (DataRange)
	{
	case ETBImageNormalization::Saturate:
		return ERangeCompressionMode::RCM_UNorm;
	case ETBImageNormalization::Normalize:
		return ERangeCompressionMode::RCM_MinMax;
	case ETBImageNormalization::Auto:
		return ERangeCompressionMode::RCM_MinMaxNorm;
	};

	return ERangeCompressionMode::RCM_UNorm;
}

TSharedPtr<FTextureBakerSurfaceReadback> FTextureBakerModule::GetReadbackHandler(ETextureSourceFormat OutputFormat, bool sRGB, ETBImageNormalization DataRange)
{
	ERangeCompressionMode CompressionMode = ReadbackNormalizationMode(DataRange);
	switch (OutputFormat)
	{
	case ETextureSourceFormat::TSF_BGRA8:
	case ETextureSourceFormat::TSF_G8:
		return MakeShared<FTextureBakerColorReadback>(CompressionMode, sRGB);

	default:
		return MakeShared<FTextureBakerLinearReadback>(CompressionMode, sRGB);
	}
	return nullptr;
}


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTextureBakerModule, TextureBaker)