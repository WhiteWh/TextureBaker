#include "Renderer/TextureBakerRenderTypes.h"

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

EPixelFormat FTextureBakerOutputInfo::GetPixelFormat() const
{
	return GetPixelFormatFromRenderTargetFormat(GetRenderTargetFormat());
}

bool FTextureBakerOutputInfo::IsValid() const
{
	return OutputDimensions.GetMin() > 0 && OutputDimensions.GetMax() <= 4096 && OutputImageFormat != ETextureSourceFormat::TSF_Invalid;
}

void FTextureBakerOutputInfo::SetTextureAttributes(UTexture2D* Texture) const
{
	if (Texture)
	{
		Texture->PowerOfTwoMode = Padding;
		Texture->CompressionSettings = CompressionSettings;
		Texture->MipGenSettings = MipGenSettings;
		Texture->CompressionNoAlpha = bCompressWithoutAlpha;
		Texture->PaddingColor = DefaultColor.ToFColor(bUseSRGB);
		Texture->Filter = Filter;
		Texture->LossyCompressionAmount = LossyCompressionAmount;
		Texture->MaxTextureSize = MaxTextureSize;
		Texture->CompressionQuality = CompressionQuality;
		Texture->LODBias = LODBias;
		Texture->AddressX = AddressX;
		Texture->AddressY = AddressY;
	}
}

bool FTextureBakerResourceRequirements::IsSatisfiedByTexture(UTexture2D* Texture, const FTextureSource& OriginalArtData) const
{
	return (Texture
		&& (!bRequireUncompressed || Texture->CompressionNone) 
		&& (!bUseImportedResolution || (Texture->GetSizeX() >= OriginalArtData.GetSizeX() && Texture->GetSizeY() >= OriginalArtData.GetSizeY()))
		&& (MipGenSettings == TextureMipGenSettings::TMGS_LeaveExistingMips || Texture->MipGenSettings == MipGenSettings)
	);
}