#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureBakerRenderTypes.generated.h"

USTRUCT(BlueprintType)
struct TEXTUREBAKER_API FTextureBakerOutputInfo
{
	GENERATED_BODY()

public:

	FTextureBakerOutputInfo() : 
		OutputDimensions(1, 1), Padding(ETexturePowerOfTwoSetting::None), 
		DefaultColor(FColor::Black), MipGenSettings(TextureMipGenSettings::TMGS_FromTextureGroup),
		OutputImageFormat(ETextureSourceFormat::TSF_BGRA8),	bUseSRGB(true),
		CompressionSettings(TextureCompressionSettings::TC_Default), bCompressWithoutAlpha(false)
	{}

	FTextureBakerOutputInfo(UTexture2D* CompatibleTexture, const FIntPoint& Size) :
		OutputDimensions(Size), Padding(CompatibleTexture->PowerOfTwoMode),
		DefaultColor(CompatibleTexture->PaddingColor), MipGenSettings(CompatibleTexture->MipGenSettings),
		OutputImageFormat(CompatibleTexture->Source.GetFormat()), bUseSRGB(CompatibleTexture->SRGB),
		CompressionSettings(CompatibleTexture->CompressionSettings), bCompressWithoutAlpha(CompatibleTexture->CompressionNoAlpha)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Image)
	FIntPoint OutputDimensions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Image)
	TEnumAsByte<ETexturePowerOfTwoSetting::Type> Padding;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Image)
	FLinearColor DefaultColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Image)
	TEnumAsByte<enum TextureMipGenSettings> MipGenSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Format)
	TEnumAsByte<enum ETextureSourceFormat> OutputImageFormat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Format)
	bool bUseSRGB;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Compression)
	TEnumAsByte<enum TextureCompressionSettings> CompressionSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Compression)
	bool bCompressWithoutAlpha;

	FString ToString() const;
	ETextureRenderTargetFormat GetRenderTargetFormat() const;
	EPixelFormat GetPixelFormat() const;
	bool IsValid() const;
	void SetTextureAttributes(UTexture2D* Texture) const;
};

USTRUCT(BlueprintType)
struct TEXTUREBAKER_API FTextureBakerResourceRequirements
{
	GENERATED_BODY()

	FTextureBakerResourceRequirements() : bUseImportedResolution(true), bRequireUncompressed(true), MipGenSettings(TextureMipGenSettings::TMGS_LeaveExistingMips) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rendering)
	bool bUseImportedResolution;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rendering)
	bool bRequireUncompressed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rendering)
	TEnumAsByte<TextureMipGenSettings> MipGenSettings;

	bool IsSatisfiedByTexture(UTexture2D* Texture, const FTextureSource& OriginalArtData) const;
	friend inline uint32 GetTypeHash(const FTextureBakerResourceRequirements& Key) { return HashCombine(GetTypeHash(Key.MipGenSettings), (Key.bUseImportedResolution ? 0 : 1) + (Key.bRequireUncompressed ? 0 : 2)); };
};

USTRUCT()
struct TEXTUREBAKER_API FTextureBakerDerivedArtKey : public FTextureBakerResourceRequirements
{
	GENERATED_BODY()

public:
	FTextureBakerDerivedArtKey() : FTextureBakerResourceRequirements(), SourceImageSize(0, 0), SourceFormat(ETextureSourceFormat::TSF_Invalid), SourceArtGUID() {}
	FTextureBakerDerivedArtKey(const FTextureBakerDerivedArtKey& Lhs) = default;
	FTextureBakerDerivedArtKey(const FTextureBakerResourceRequirements& Requirements, const FTextureSource& SourceArt) : FTextureBakerResourceRequirements(Requirements)
	{
		SourceImageSize = FIntPoint(SourceArt.GetSizeX(), SourceArt.GetSizeY());
		SourceFormat = SourceArt.GetFormat();
		SourceArtGUID = SourceArt.GetId();
	}

	FTextureBakerDerivedArtKey& operator=(const FTextureBakerDerivedArtKey& Lhs) = default;
	bool operator!=(const FTextureBakerDerivedArtKey& Lhs) const { return !(*this == Lhs); }
	bool operator==(const FTextureBakerDerivedArtKey& Lhs) const { return SourceImageSize == Lhs.SourceImageSize && SourceFormat == Lhs.SourceFormat && SourceArtGUID == Lhs.SourceArtGUID;	}

	friend inline uint32 GetTypeHash(const FTextureBakerDerivedArtKey& Key) { 
		return HashCombine(
			HashCombine(GetTypeHash(static_cast<const FTextureBakerResourceRequirements&>(Key)), GetTypeHash(Key.SourceImageSize)),
			HashCombine(GetTypeHash(Key.SourceFormat), GetTypeHash(Key.SourceArtGUID))
		); 
	};

protected:

	UPROPERTY()
	FIntPoint SourceImageSize;

	UPROPERTY()
	TEnumAsByte<ETextureSourceFormat> SourceFormat;

	UPROPERTY()
	FGuid SourceArtGUID;
};


class TEXTUREBAKER_API ITextureBakerRTPool
{
public:
	virtual UTexture2D* GetOrCreateDerivedArt(UTexture2D* Source, const FTextureBakerResourceRequirements& Options) = 0;
	virtual UTextureRenderTarget2D* GetOrCreateRT(const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format) = 0;
	virtual UCanvas* GetOrCreateCanvas() = 0;
	virtual bool ReleaseObject(UObject* Object) = 0;
};

enum class ETBDerivedArtMode : uint8
{
	None = 0x00,
	AlwaysCreate = 1 << 0,
	Failsafe = 1 << 1
};
ENUM_CLASS_FLAGS(ETBDerivedArtMode);
