

#pragma once

#include "CoreMinimal.h"
#include "TextureBakerScenario.generated.h"

USTRUCT(BlueprintType)
struct TEXTUREBAKER_API FTextureBakerOutputInfo
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Dimensions)
	FIntPoint OutputDimensions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Dimensions)
	bool PadToSquare;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Format)
	TEnumAsByte<enum ETextureSourceFormat> OutputImageFormat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Format)
	bool bUseSRGB;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Compression)
	TEnumAsByte<enum TextureCompressionSettings> CompressionSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Compression)
	bool bCompressWithoutAlpha;

	FString ToString() const
	{
		FString OutputFormatName = StaticEnum<ETextureSourceFormat>()->GetDisplayNameTextByValue(OutputImageFormat).ToString();
		FString CompressionSettingsText = StaticEnum<TextureCompressionSettings>()->GetDisplayNameTextByValue(CompressionSettings).ToString();
		const TCHAR* sRGBText = bUseSRGB ? TEXT(" (sRGB)") : TEXT("");
		const TCHAR* noAlphaText = bCompressWithoutAlpha ? TEXT(" (w/o alpha)") : TEXT("");
		return FString::Printf(TEXT("%dx%d %s%s %s%s"), OutputDimensions.X, OutputDimensions.Y, *OutputFormatName, sRGBText, *CompressionSettingsText, noAlphaText);
	}
};

DECLARE_DYNAMIC_DELEGATE_TwoParams(FTextureBakerRenderDelegate, FTextureBakerOutputInfo, Info, UCanvas*, Canvas);

USTRUCT(BlueprintType, meta = (HasNativeBreak = "TextureBaker.TextureBakerOutputLibrary.BreakOutputWriteout", HasNativeMake = "TextureBaker.TextureBakerOutputLibrary.MakeOutputWriteout"))
struct TEXTUREBAKER_API FTextureBakerOutputWriteout : public FTextureBakerOutputInfo
{
	GENERATED_BODY()

public:

	FTextureBakerOutputWriteout() {}
	FTextureBakerOutputWriteout(const FTextureBakerOutputInfo& Lhs, const FString& InOutputAssetPath, FTextureBakerRenderDelegate InRenderOutputTarget) : 
		FTextureBakerOutputInfo(Lhs), OutputAssetPath(InOutputAssetPath), OnRenderOutputTarget(InRenderOutputTarget) {}
	
	FTextureBakerOutputWriteout(const FTextureBakerOutputWriteout& Lhs) : 
		FTextureBakerOutputInfo(Lhs), OutputAssetPath(Lhs.OutputAssetPath), OnRenderOutputTarget(Lhs.OnRenderOutputTarget) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Path)
	FString OutputAssetPath;

	// This Event is triggered any time new LiveLink data is available, including in the editor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rendering)
	FTextureBakerRenderDelegate OnRenderOutputTarget;
};

/**
 * 
 */
UCLASS(Abstract, Blueprintable, meta = (ShowWorldContextPin))
class TEXTUREBAKER_API UTextureBakerScenario : public UObject
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintNativeEvent, Category = Events)
	bool InitialSettingsIsValid() const;

	UFUNCTION(BlueprintNativeEvent, Category = Events)
	void RegisterOutputTarget(const FString& DirectoryPath, TArray<FTextureBakerOutputWriteout>& OutputTargets) const;

	UFUNCTION(BlueprintNativeEvent, Category = Events)
	bool RenderCommonTargets();

	UFUNCTION(BlueprintNativeEvent, Category = Events)
	bool RenderSpecificOutput(const FTextureBakerOutputInfo& Info, UCanvas* OutputCanvas);

protected:

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
};
