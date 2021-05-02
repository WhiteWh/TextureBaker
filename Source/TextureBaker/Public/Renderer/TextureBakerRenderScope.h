#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Renderer/TextureBakerRenderTypes.h"

class FTextureBakerRenderScope;

class TEXTUREBAKER_API FTextureBakerDrawTarget
{
public:
	FTextureBakerDrawTarget(UTextureRenderTarget2D* RenderTarget, ERHIFeatureLevel::Type FeatureLevel);
	void AddReferencedObjects(FReferenceCollector& Collector);
	void InitializeCanvasObject(UCanvas* Canvas);
	UTexture2D* Resolve(FTextureBakerRenderScope* InRenderScope);
	void Discard(FTextureBakerRenderScope* InRenderScope);
	void WaitDrawCompletion();

	UTextureRenderTarget2D*		ReleaseRT();

private:
	UTextureRenderTarget2D*		RenderTargetObject;
	FCanvas						RenderCanvas;
	FDrawEvent*					DrawEvent;
};

class TEXTUREBAKER_API FSavedTextureStreamingState
{
public:
	FSavedTextureStreamingState() : bForceMiplevelsToBeResident(false), bIgnoreStreamingMipBias(false) {}
	FSavedTextureStreamingState(bool bSavedForceMiplevelsToBeResident, bool bSavedIgnoreStreamingMipBias) : bForceMiplevelsToBeResident(bSavedForceMiplevelsToBeResident), bIgnoreStreamingMipBias(bSavedIgnoreStreamingMipBias) {}
	FSavedTextureStreamingState(const FSavedTextureStreamingState& Lhs) : bForceMiplevelsToBeResident(Lhs.bForceMiplevelsToBeResident), bIgnoreStreamingMipBias(Lhs.bIgnoreStreamingMipBias) {}
	FSavedTextureStreamingState(UTexture* Texture, bool bNewForceMiplevelsToBeResident, bool bNewIgnoreStreamingMipBias);

	void Revert(UTexture* Texture);

private:
	bool bForceMiplevelsToBeResident;
	bool bIgnoreStreamingMipBias;
};

class TEXTUREBAKER_API FTextureBakerRenderScope : public TSharedFromThis<FTextureBakerRenderScope>
{
public:
	FTextureBakerRenderScope(ITextureBakerRTPool* RenderTargetPool);
	FTextureBakerRenderScope(TSharedRef<FTextureBakerRenderScope> Parent);
	virtual ~FTextureBakerRenderScope();

	TSharedPtr<FTextureBakerRenderScope> GetParentScope() const;

	ITextureBakerRTPool* GetRenderTargetPool() const;
	UTextureRenderTarget2D* CreateTemporaryRT(const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format, FLinearColor ClearColor, bool bAutoGenerateMipMaps);
	UTexture2D* ConditionallyCreateDerivedArt(UTexture2D* SourceTexture, const FTextureBakerResourceRequirements& Options, ETBDerivedArtMode Mode = ETBDerivedArtMode::None);
	UTexture2D* CreateTemporaryTexture(UTextureRenderTarget2D* SourceRT, TextureMipGenSettings MipFilter);
	UTexture2D* CreateTemporaryTexture(const FTextureBakerOutputInfo& TextureInfo, ETextureSourceFormat InDataFormat, const void* Data);
	UCanvas* CreateTemporaryDrawRT(const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format, FLinearColor ClearColor, bool bAutoGenerateMipMaps);
	UTexture2D* ResolveTemporaryDrawRT(UCanvas* DrawTarget);
	UTextureRenderTarget2D* ResolveTemporaryDrawRT_AsRenderTarget(UCanvas* DrawTarget);
	bool ReleaseTemporaryResource(UObject* ResourceObject);
	bool SetTextureMipsResident(UTexture2D* SourceTexture, bool Value);
	bool IsTextureSetToBeResident(UTexture2D* Texture);

	virtual void AddReferencedObjects(FReferenceCollector& Collector);

private:
	TSharedPtr<FTextureBakerRenderScope>			ParentRenderScope;
	ITextureBakerRTPool*							RenderTargetPool;

	TMap<UCanvas*, FTextureBakerDrawTarget>			ActiveDrawTargets;
	TMap<UTexture2D*, FSavedTextureStreamingState>	TexturesAreSetToBeResident;
	TArray<UTexture2D*>								TemporaryTextures;
};