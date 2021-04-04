#include "TextureBakerScenario.h"
#include "TextureBaker.h"
#include "Materials/MaterialInstanceDynamic.h"

FIntPoint EvaluateRequestedSize(const FIntPoint& SourceSize, int32 MipBias)
{
	const int32 BlockSize = FMath::Min(SourceSize.X, SourceSize.Y);
	const int32 MaxMipInBlock = FMath::CeilLogTwo(BlockSize);
	const int32 MipDowngrade = FMath::Clamp(MipBias, 0, MaxMipInBlock);
	return FIntPoint(SourceSize.X >> MipDowngrade, SourceSize.Y >> MipDowngrade);
}

FString FTextureBakerOutputInfo::ToString() const
{
	FString OutputFormatName = StaticEnum<ETextureSourceFormat>()->GetDisplayNameTextByValue(OutputImageFormat).ToString();
	FString CompressionSettingsText = StaticEnum<TextureCompressionSettings>()->GetDisplayNameTextByValue(CompressionSettings).ToString();
	const TCHAR* sRGBText = bUseSRGB ? TEXT(" (sRGB)") : TEXT("");
	const TCHAR* noAlphaText = bCompressWithoutAlpha ? TEXT(" (w/o alpha)") : TEXT("");
	return FString::Printf(TEXT("%dx%d %s%s %s%s"), OutputDimensions.X, OutputDimensions.Y, *OutputFormatName, sRGBText, *CompressionSettingsText, noAlphaText);
}

ETextureRenderTargetFormat FTextureBakerOutputInfo::GetRenderTargetFormat() const
{
	return FTextureBakerModule::SelectRenderTargetFormatForImageSourceFormat(OutputImageFormat, bUseSRGB);
}

bool FTextureBakerOutputInfo::IsValid() const
{
	return OutputDimensions.GetMin() > 0 && OutputDimensions.GetMax() <= 4096;
}

FName FTextureBakerOutputWriteout::GetOutputName() const
{
	return OutputName;
}

bool UTextureBakerScenario::InitialSettingsIsValid_Implementation() const
{
	return true;
}

void UTextureBakerScenario::RegisterOutputTarget_Implementation(const FString& DirectoryPath, TArray<FTextureBakerOutputWriteout>& OutputTargets) const
{
	
}

bool UTextureBakerScenario::RenderCommonTargets_Implementation(bool bIsPreviewRender)
{
	return true;
}

UTexture2D* UTextureBakerScenario::PrepareTexture(UTexture2D* SourceTexture, bool bForceSourceResolution, int32 MipBias, TEnumAsByte<TextureMipGenSettings> MipGenSettings)
{
	if (CurrentRenderScope.IsValid() && SourceTexture)
	{
		FIntPoint ImportedSize = SourceTexture->GetImportedSize();
		FIntPoint RequestedSize = EvaluateRequestedSize(ImportedSize, MipBias);
		FIntPoint InGameSize = FIntPoint(SourceTexture->GetSizeX(), SourceTexture->GetSizeY());
		bool      SourceTextureIsCompressed = !SourceTexture->CompressionNone;
		if ((bForceSourceResolution ? RequestedSize != InGameSize : ((RequestedSize - InGameSize).GetMin() < 0)) || SourceTextureIsCompressed)
		{
			return CurrentRenderScope->CreateTemporaryTexture(SourceTexture, RequestedSize, MipGenSettings, TextureMipGenSettings::TMGS_NoMipmaps);
		}
		CurrentRenderScope->SetTextureMipsResident(SourceTexture, true);
	}
	if (SourceTexture)
	{
		SourceTexture->WaitForStreaming();
	}
	return SourceTexture;
}

UMaterialInterface* UTextureBakerScenario::PrepareMaterial(UMaterialInterface* SourceMaterial)
{
	if (CurrentRenderScope.IsValid() && SourceMaterial)
	{
		TArray<UTexture*> Textures;
		SourceMaterial->GetUsedTextures(Textures, EMaterialQualityLevel::Num, false, ERHIFeatureLevel::Num, true);
		for (int32 TextureIndex = 0; TextureIndex < Textures.Num(); ++TextureIndex)
		{
			UTexture2D* Texture = Cast<UTexture2D>(Textures[TextureIndex]);
			CurrentRenderScope->SetTextureMipsResident(Texture, true);
		}
	}
	return SourceMaterial;
}

UMaterialInstanceDynamic* UTextureBakerScenario::PrepareDynamicMaterialInstance(UMaterialInterface* SourceMaterial, FName OptionalName)
{
	if (SourceMaterial)
	{
		UMaterialInstanceDynamic* DMI = UMaterialInstanceDynamic::Create(SourceMaterial, this, OptionalName);
		TArray<UTexture*> Textures;
		DMI->GetUsedTextures(Textures, EMaterialQualityLevel::Num, false, ERHIFeatureLevel::Num, true);
		for (int32 TextureIndex = 0; TextureIndex < Textures.Num(); ++TextureIndex)
		{
			UTexture2D* Texture = Cast<UTexture2D>(Textures[TextureIndex]);
			CurrentRenderScope->SetTextureMipsResident(Texture, true);
		}
		return DMI;
	}
	return nullptr;
}

UCanvas* UTextureBakerScenario::CreateTemporaryDrawRT(const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format, FLinearColor ClearColor, bool bAutoGenerateMipMaps)
{
	return CurrentRenderScope.IsValid() ? CurrentRenderScope->CreateTemporaryDrawRT(InTargetSize, Format, ClearColor, bAutoGenerateMipMaps) : nullptr;
}

FTextureBakerSwapRT UTextureBakerScenario::CreateTemporarySwapRT(int32 HistoryLength, const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format, FLinearColor ClearColor, bool bAutoGenerateMipMaps)
{
	return FTextureBakerSwapRT();
}

UTexture2D* UTextureBakerScenario::ResolveTemporaryDrawRT(UCanvas* DrawTarget)
{
	return (DrawTarget && CurrentRenderScope.IsValid()) ? CurrentRenderScope->ResolveTemporaryDrawRT(DrawTarget) : nullptr;
}

UTexture2D* UTextureBakerScenario::DownsampleTexture(UTexture2D* SourceTexture, int32 MipIndex, TEnumAsByte<TextureMipGenSettings> MipGenSettings)
{
	if (CurrentRenderScope.IsValid() && SourceTexture)
	{
		const FIntPoint TargetSize = EvaluateRequestedSize(SourceTexture->GetImportedSize(), MipIndex);
		return MipIndex > 0 ? CurrentRenderScope->CreateTemporaryTexture(SourceTexture, TargetSize, MipGenSettings, TextureMipGenSettings::TMGS_NoMipmaps) : SourceTexture;
	}
	return nullptr;
}

bool UTextureBakerScenario::ReleaseTemporaryResource(UTexture2D*& SourceTexture)
{
	if (CurrentRenderScope.IsValid() && CurrentRenderScope->ReleaseTemporaryResource(SourceTexture))
	{
		SourceTexture = nullptr;
		return true;
	}
	return false;
}

void UTextureBakerScenario::EnterRenderScope(TSharedRef<FTextureBakerRenderScope> NewScope)
{
	CurrentRenderScope = NewScope;
}

TSharedPtr<FTextureBakerRenderScope> UTextureBakerScenario::LeaveRenderScope()
{
	CurrentRenderScope = CurrentRenderScope->GetParentScope();
	return CurrentRenderScope;
}

UCanvas* UTextureBakerOutputBlueprintLibrary::DrawToSwapRT(FTextureBakerSwapRT& Target, TArray<UTexture2D*>& History, FIntPoint& ScreenSize)
{
	return nullptr;
}

UTexture2D* UTextureBakerOutputBlueprintLibrary::ResolveTemporarySwapRT(FTextureBakerSwapRT& Target)
{
	return nullptr;
}

void UTextureBakerOutputBlueprintLibrary::BreakOutputWriteout(FTextureBakerOutputWriteout OutputWriteout, FString& OutputAssetPath, FTextureBakerOutputInfo& OutputInfo)
{
	OutputInfo = OutputWriteout;
	OutputAssetPath = OutputWriteout.OutputAssetPath;
}

FTextureBakerOutputWriteout UTextureBakerOutputBlueprintLibrary::MakeOutputWriteout(const FString& OutputAssetPath, const FTextureBakerOutputInfo& OutputInfo, FTextureBakerRenderDelegate OnRenderOutputTarget)
{
	return FTextureBakerOutputWriteout(OutputInfo, OutputAssetPath, OnRenderOutputTarget);
}
