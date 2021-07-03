

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureDefines.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Renderer/TextureBakerRenderScope.h"
#include "TextureBakerScenario.generated.h"

USTRUCT(BlueprintType)
struct TEXTUREBAKER_API FTextureBakerDataContext
{
	GENERATED_BODY()
};

DECLARE_DYNAMIC_DELEGATE_RetVal_ThreeParams(bool, FTextureBakerRenderDelegate, FTextureBakerOutputInfo, Info, bool, bIsPreviewRender, UCanvas*, Canvas);
DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(bool, FTextureBakerWriteDataDelegate, UObject*, DataObject, const FTextureBakerDataContext&, BakedDataInfo);

USTRUCT(BlueprintType, meta = (HasNativeBreak = "TextureBaker.TextureBakerOutputLibrary.BreakOutputWriteout", HasNativeMake = "TextureBaker.TextureBakerOutputLibrary.MakeOutputWriteout"))
struct TEXTUREBAKER_API FTextureBakerOutputWriteout : public FTextureBakerOutputInfo
{
	GENERATED_BODY()

public:

	FTextureBakerOutputWriteout() {}
	FTextureBakerOutputWriteout(const FTextureBakerOutputInfo& Lhs, const FString& InOutputAssetPath, FTextureBakerRenderDelegate InRenderOutputTarget) : 
		FTextureBakerOutputInfo(Lhs), OutputName(InRenderOutputTarget.IsBound() ? InRenderOutputTarget.GetFunctionName() : NAME_None), OutputAssetPath(InOutputAssetPath), OnRenderOutputTarget(InRenderOutputTarget) {}
	
	FTextureBakerOutputWriteout(const FTextureBakerOutputWriteout& Lhs) : 
		FTextureBakerOutputInfo(Lhs), OutputName(Lhs.OutputName), OutputAssetPath(Lhs.OutputAssetPath), OnRenderOutputTarget(Lhs.OnRenderOutputTarget) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rendering)
	FName OutputName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Path)
	FString OutputAssetPath;

	// This Event is triggered any time new LiveLink data is available, including in the editor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rendering)
	FTextureBakerRenderDelegate OnRenderOutputTarget;

	// Get output name from variable or from function delegate
	FName GetOutputName() const;
};


USTRUCT(BlueprintType)
struct TEXTUREBAKER_API FTextureBakerSwapRT
{
	GENERATED_BODY()

public:
	FTextureBakerSwapRT();
	FTextureBakerSwapRT(UTextureBakerScenario* Environment, const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format, FLinearColor ClearColor, int32 HistoryLength);
};

USTRUCT(BlueprintType)
struct TEXTUREBAKER_API FTextureBakerOutputList
{
	GENERATED_BODY()

public:
	FTextureBakerOutputList();

	bool AddTexture2DOutput(const FTextureBakerOutputInfo& Options, const FString& InOutputAssetPath, FTextureBakerRenderDelegate InRenderOutputTarget);
	bool AddDataOutput(UClass* AssetClass, const FString& InOutputAssetPath, FTextureBakerWriteDataDelegate InGenerateOutputTarget);

	const TArray<FTextureBakerOutputWriteout>& GetTextureOutputs() const { return TextureOutputs; }

protected:
	UPROPERTY()
	TArray<FTextureBakerOutputWriteout> TextureOutputs;
};


/**
 * 
 */
UCLASS(Abstract, Blueprintable, meta = (ShowWorldContextPin))
class TEXTUREBAKER_API UTextureBakerScenario : public UObject
{
	GENERATED_BODY()
	
public:

	/* Overrides */
	virtual bool IsEditorOnly() const {	return true; }
	
	/* Events */

	UFUNCTION(BlueprintNativeEvent, Category = Events)
	bool InitialSettingsIsValid() const;

	UFUNCTION(BlueprintNativeEvent, Category = Events)
	void RegisterOutputTarget(const FString& DirectoryPath, UPARAM(ref) FTextureBakerOutputList& OutputTargets) const;

	UFUNCTION(BlueprintNativeEvent, Category = Events)
	bool RenderCommonTargets(bool bIsPreviewRender);

	/* Native things */

	void EnterRenderScope(TSharedRef<FTextureBakerRenderScope> NewScope);
	TSharedPtr<FTextureBakerRenderScope> LeaveRenderScope();

protected:

	/* Helper functions */

	// Fully stream texture in
	UFUNCTION(BlueprintCallable, Category = "TextureBaker|Streaming")
	UTexture2D* PrepareTexture(UTexture2D* SourceTexture, const FTextureBakerResourceRequirements& Options);

	// Fully stream material in (SetForceMipLevelsToBeResident)
	UFUNCTION(BlueprintCallable, Category = "TextureBaker|Streaming")
	UMaterialInterface* PrepareMaterial(UMaterialInterface* SourceMaterial);

	// Fully stream material in and create a dynamic material instance for it
	UFUNCTION(BlueprintCallable, Category = "TextureBaker|Streaming")
	UMaterialInstanceDynamic* PrepareDynamicMaterialInstance(UMaterialInterface* SourceMaterial, FName OptionalName);

	// Create regular draw target
	UFUNCTION(BlueprintCallable, Category = "TextureBaker|Render")
	UCanvas* CreateTemporaryDrawRT(const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format, FLinearColor ClearColor);

	// Create swappable draw target
	UFUNCTION(BlueprintCallable, Category = "TextureBaker|Render")
	FTextureBakerSwapRT CreateTemporarySwapRT(int32 HistoryLength, const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format, FLinearColor ClearColor);

	// Create regular draw target
	UFUNCTION(BlueprintCallable, Category = "TextureBaker|Render")
	UTexture2D* ResolveTemporaryDrawRT(UCanvas* DrawTarget, TextureMipGenSettings MipGenSettings, ETBImageNormalization Normalization);

	// Create downsampled texture
	UFUNCTION(BlueprintCallable, Category = "TextureBaker|Render")
	UTexture2D* DownsampleTexture(UTexture2D* SourceTexture, int32 MipIndex, TEnumAsByte<TextureMipGenSettings> MipGenSettings);

	// Informs renderer that you wount use this texture anymore. All temporary objects allocated for particular step are released automatically after this step, but calling this function allows to free
	// resource earlier
	UFUNCTION(BlueprintCallable, Category = "TextureBaker|Render")
	bool ReleaseTemporaryResource(UPARAM(ref) UTexture2D*& SourceTexture);

private:
	TSharedPtr<FTextureBakerRenderScope> CurrentRenderScope;

};

UCLASS(meta=(ScriptName="TextureBakerOutputLibrary"))
class TEXTUREBAKER_API UTextureBakerOutputBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Breaks out the information for a texture output
	UFUNCTION(BlueprintPure, Category=TextureBaker)
	static void BreakOutputWriteout(FTextureBakerOutputWriteout OutputWriteout, FString& OutputAssetPath, FTextureBakerOutputInfo& OutputInfo);

	// Creates a texture output from the specified information
	UFUNCTION(BlueprintPure, Category= TextureBaker)
	static FTextureBakerOutputWriteout MakeOutputWriteout(const FString& OutputAssetPath, const FTextureBakerOutputInfo& OutputInfo, FTextureBakerRenderDelegate OnRenderOutputTarget);

	// Draw to swappable RT
	UFUNCTION(BlueprintCallable, Category = "TextureBaker|Render")
	static UCanvas* DrawToSwapRT(UPARAM(ref) FTextureBakerSwapRT& Target, TArray<UTexture*>& History, FIntPoint& ScreenSize);

	// Resolve swap RT context
	UFUNCTION(BlueprintCallable, Category = "TextureBaker|Render")
	static UTexture2D* ResolveTemporarySwapRT(UPARAM(ref) FTextureBakerSwapRT& Target);

	// Register texture output
	UFUNCTION(BlueprintCallable, Category = "TextureBaker|Output")
	static bool AddTexture2DOutput(UPARAM(ref) FTextureBakerOutputList& Target, const FTextureBakerOutputInfo& Options, const FString& InOutputAssetPath, FTextureBakerRenderDelegate InRenderOutputTarget);
		
	// Register data output
	UFUNCTION(BlueprintCallable, Category = "TextureBaker|Output")
	static bool AddDataOutput(UPARAM(ref) FTextureBakerOutputList& Target, UClass* AssetClass, const FString& InOutputAssetPath, FTextureBakerWriteDataDelegate InGenerateOutputTarget);

};
