#include "TextureBakerScenario.h"

bool UTextureBakerScenario::InitialSettingsIsValid_Implementation() const
{
	return true;
}

void UTextureBakerScenario::RegisterOutputTarget_Implementation(const FString& DirectoryPath, TArray<FTextureBakerOutputWriteout>& OutputTargets) const
{

}

bool UTextureBakerScenario::RenderCommonTargets_Implementation()
{
	return true;
}

bool UTextureBakerScenario::RenderSpecificOutput_Implementation(const FTextureBakerOutputInfo& Info, UCanvas* OutputCanvas)
{
	return true;
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
