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

FName FTextureBakerOutputWriteout::GetOutputName() const
{
	return OutputName;
}

FTextureBakerSwapRT::FTextureBakerSwapRT()
{

}

FTextureBakerSwapRT::FTextureBakerSwapRT(UTextureBakerScenario* Environment, const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format, FLinearColor ClearColor, int32 HistoryLength)
{

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

UTexture2D* UTextureBakerScenario::PrepareTexture(UTexture2D* SourceTexture, const FTextureBakerResourceRequirements& Options)
{
	UTexture2D* Result = nullptr;
	if (CurrentRenderScope.IsValid() && SourceTexture)
	{
		FIntPoint ImportedSize = SourceTexture->GetImportedSize();
		FIntPoint CurrentSize = FIntPoint(SourceTexture->GetSizeX(), SourceTexture->GetSizeY());
		Result = CurrentRenderScope->ConditionallyCreateDerivedArt(SourceTexture, Options);
		CurrentRenderScope->SetTextureMipsResident(Result, true);
	}
	if (Result)
	{
		Result->WaitForStreaming();
	}
	return Result;
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

UCanvas* UTextureBakerScenario::CreateTemporaryDrawRT(const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format, FLinearColor ClearColor)
{
	return CurrentRenderScope.IsValid() ? CurrentRenderScope->CreateTemporaryDrawRT(InTargetSize, Format, ClearColor, false) : nullptr;
}

FTextureBakerSwapRT UTextureBakerScenario::CreateTemporarySwapRT(int32 HistoryLength, const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format, FLinearColor ClearColor)
{
	return FTextureBakerSwapRT(this, InTargetSize, Format, ClearColor, HistoryLength);
}

UTexture2D* UTextureBakerScenario::ResolveTemporaryDrawRT(UCanvas* DrawTarget, TextureMipGenSettings MipGenSettings, ETBImageNormalization Normalization)
{
	return (DrawTarget && CurrentRenderScope.IsValid()) ? CurrentRenderScope->ResolveTemporaryDrawRT(DrawTarget, MipGenSettings, Normalization) : nullptr;
}

UTexture2D* UTextureBakerScenario::DownsampleTexture(UTexture2D* SourceTexture, int32 MipIndex, TEnumAsByte<TextureMipGenSettings> MipGenSettings)
{
	if (CurrentRenderScope.IsValid() && SourceTexture)
	{
		const FIntPoint TargetSize = EvaluateRequestedSize(SourceTexture->GetImportedSize(), MipIndex);
		FTextureBakerOutputInfo DownsampledInfo(SourceTexture, TargetSize);
		return MipIndex > 0 ? CurrentRenderScope->CreateTemporaryTexture(DownsampledInfo, ETextureSourceFormat::TSF_Invalid, nullptr) : SourceTexture;
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

UCanvas* UTextureBakerOutputBlueprintLibrary::DrawToSwapRT(FTextureBakerSwapRT& Target, TArray<UTexture*>& History, FIntPoint& ScreenSize)
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
