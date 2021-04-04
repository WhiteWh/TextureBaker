#include "Renderer/TextureBakerRenderScope.h"
#include "TextureBaker.h"
#include "Engine/Canvas.h"

FTextureBakerDrawTarget::FTextureBakerDrawTarget(UTextureRenderTarget2D* RenderTarget, ERHIFeatureLevel::Type FeatureLevel ) :
	RenderTargetObject(RenderTarget), RenderCanvas(RenderTarget->GetRenderTargetResource(), nullptr, FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime, FeatureLevel)
{

}

void FTextureBakerDrawTarget::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(RenderTargetObject);
}

void FTextureBakerDrawTarget::InitializeCanvasObject(UCanvas* Canvas)
{
	Canvas->Init(RenderTargetObject->GetSurfaceWidth(), RenderTargetObject->GetSurfaceHeight(), nullptr, &RenderCanvas);
}

UTexture2D* FTextureBakerDrawTarget::Resolve(FTextureBakerRenderScope* InRenderScope)
{
	if (RenderTargetObject)
	{
		RenderCanvas.Flush_GameThread();
		RenderTargetObject->UpdateResourceImmediate(false);
		UTexture2D* OutTexture = InRenderScope->CreateTemporaryTexture(RenderTargetObject, TextureMipGenSettings::TMGS_NoMipmaps);
		InRenderScope->ReleaseTemporaryResource(RenderTargetObject);
		return OutTexture;
	}
	return nullptr;
}

void FTextureBakerDrawTarget::Discard(FTextureBakerRenderScope* InRenderScope)
{
	if (RenderTargetObject)
	{
		RenderCanvas.Flush_GameThread();
		RenderTargetObject->UpdateResourceImmediate(false);
		InRenderScope->ReleaseTemporaryResource(RenderTargetObject);
	}
}

UTextureRenderTarget2D* FTextureBakerDrawTarget::ReleaseRT()
{
	if (RenderTargetObject)
	{
		RenderCanvas.Flush_GameThread();
		RenderTargetObject->UpdateResourceImmediate(false);
		
		UTextureRenderTarget2D* Result = RenderTargetObject;
		RenderTargetObject = nullptr;
		return Result;
	}
	return nullptr;
}

FSavedTextureStreamingState::FSavedTextureStreamingState(UTexture* Texture, bool bNewForceMiplevelsToBeResident, bool bNewIgnoreStreamingMipBias)
{
	if (Texture)
	{
		bForceMiplevelsToBeResident = Texture->bForceMiplevelsToBeResident;
		bIgnoreStreamingMipBias = Texture->bIgnoreStreamingMipBias;
		Texture->bForceMiplevelsToBeResident = bNewForceMiplevelsToBeResident;
		Texture->bIgnoreStreamingMipBias = bNewIgnoreStreamingMipBias;
	}
}

void FSavedTextureStreamingState::Revert(UTexture* Texture)
{
	if (Texture)
	{
		Texture->bForceMiplevelsToBeResident = bForceMiplevelsToBeResident;
		Texture->bIgnoreStreamingMipBias = bIgnoreStreamingMipBias;
	}
}

FTextureBakerRenderScope::FTextureBakerRenderScope(ITextureBakerRTPool* InRenderTargetPool) : ParentRenderScope(nullptr), RenderTargetPool(InRenderTargetPool)
{
}

FTextureBakerRenderScope::FTextureBakerRenderScope(TSharedRef<FTextureBakerRenderScope> Parent) : ParentRenderScope(Parent), RenderTargetPool(Parent->GetRenderTargetPool())
{
}

TSharedPtr<FTextureBakerRenderScope> FTextureBakerRenderScope::GetParentScope() const
{
	return ParentRenderScope;
}

ITextureBakerRTPool* FTextureBakerRenderScope::GetRenderTargetPool() const
{
	if (ParentRenderScope.IsValid())
	{
		return RenderTargetPool ? RenderTargetPool : ParentRenderScope->GetRenderTargetPool();
	}
	else
	{
		return RenderTargetPool ? RenderTargetPool : nullptr;
	}
}

bool FTextureBakerRenderScope::SetTextureMipsResident(UTexture2D* SourceTexture, bool Value)
{
	if (SourceTexture)
	{
		if (Value && !IsTextureSetToBeResident(SourceTexture))
		{
			TexturesAreSetToBeResident.Emplace(SourceTexture, FSavedTextureStreamingState(SourceTexture, true, true));
		}
		else if (!Value && TexturesAreSetToBeResident.Contains(SourceTexture))
		{
			FSavedTextureStreamingState SavedState;
			TexturesAreSetToBeResident.RemoveAndCopyValue(SourceTexture, SavedState);
			SavedState.Revert(SourceTexture);
		}
	}
	return false;
}

bool FTextureBakerRenderScope::IsTextureSetToBeResident(UTexture2D* Texture)
{
	if (Texture)
	{
		return TexturesAreSetToBeResident.Contains(Texture) || (ParentRenderScope.IsValid() && ParentRenderScope->IsTextureSetToBeResident(Texture));
	}
	return false;
}

UTexture2D* FTextureBakerRenderScope::CreateTemporaryTexture(UTextureRenderTarget2D* SourceRT, TextureMipGenSettings MipFilter)
{
	if (SourceRT)
	{
		if (ITextureBakerRTPool* RTPool = GetRenderTargetPool())
		{
			EPixelFormat PixelFormat = SourceRT->GetFormat();
			ETextureRenderTargetFormat RTFormat = SourceRT->RenderTargetFormat;
			ETextureSourceFormat ImageFormat = FTextureBakerModule::SelectImageSourceFormatForRenderTargetFormat(RTFormat);
			FIntPoint InTargetSize = FIntPoint(SourceRT->SizeX, SourceRT->SizeY);
			if (UTexture2D* OutTexture = UTexture2D::CreateTransient(InTargetSize.X, InTargetSize.Y, PixelFormat))
			{
				UTextureRenderTarget2D* CopyBuffer = RTPool->GetOrCreateRT(InTargetSize, RTFormat);
				FTextureBakerModule::GetChecked().WriteTexture2DSourceArt(OutTexture, ImageFormat, CopyBuffer, 0);
				return OutTexture;
			}
		}
	}
	return nullptr;
}

UTexture2D* FTextureBakerRenderScope::CreateTemporaryTexture(UTexture2D* SourceTexture, const FIntPoint& InTargetSize, TextureMipGenSettings MipFilter, TextureMipGenSettings MagFilter)
{
	if (SourceTexture && InTargetSize.GetMin() > 0 && InTargetSize.GetMax() <= 4096)
	{
		if (ITextureBakerRTPool* RTPool = GetRenderTargetPool())
		{
			EPixelFormat Format = SourceTexture->GetPixelFormat();
			if (UTexture2D* OutTexture = UTexture2D::CreateTransient(InTargetSize.X, InTargetSize.Y, Format))
			{
				ETextureRenderTargetFormat RTFormat = FTextureBakerModule::SelectRenderTargetFormatForPixelFormat(Format, SourceTexture->SRGB);
				UTextureRenderTarget2D* CopyBuffer = RTPool->GetOrCreateRT(InTargetSize, RTFormat);
				FTextureBakerModule::GetChecked().WriteTexture2DSourceArt(OutTexture, SourceTexture->Source.GetFormat(), CopyBuffer, 0);
				return OutTexture;
			}
		}
	}
	return nullptr;
}

UTextureRenderTarget2D* FTextureBakerRenderScope::CreateTemporaryRT(const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format, FLinearColor ClearColor, bool bAutoGenerateMipMaps)
{
	if (ITextureBakerRTPool* RTPool = GetRenderTargetPool())
	{
		UTextureRenderTarget2D* AllocatedRT = RTPool->GetOrCreateRT(InTargetSize, Format);
		AllocatedRT->ClearColor = ClearColor;
		AllocatedRT->bAutoGenerateMips = bAutoGenerateMipMaps;
		return AllocatedRT;
	}
	return nullptr;
}

UCanvas* FTextureBakerRenderScope::CreateTemporaryDrawRT(const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format, FLinearColor ClearColor, bool bAutoGenerateMipMaps)
{
	if (ITextureBakerRTPool* RTPool = GetRenderTargetPool())
	{
		UTextureRenderTarget2D* AllocatedRT = RTPool->GetOrCreateRT(InTargetSize, Format);
		AllocatedRT->ClearColor = ClearColor;
		AllocatedRT->bAutoGenerateMips = bAutoGenerateMipMaps;

		const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
		UCanvas* RenderCanvas = RTPool->GetOrCreateCanvas();
		FTextureBakerDrawTarget& DrawTarget = ActiveDrawTargets.Emplace(RenderCanvas, FTextureBakerDrawTarget(AllocatedRT, FeatureLevel));
		return RenderCanvas;
	}
	return nullptr;
}

UTexture2D* FTextureBakerRenderScope::ResolveTemporaryDrawRT(UCanvas* DrawTarget)
{
	if (FTextureBakerDrawTarget* DrawContext = ActiveDrawTargets.Find(DrawTarget))
	{
		if (ITextureBakerRTPool* RTPool = GetRenderTargetPool())
		{
			if (UTexture2D* Texture = DrawContext->Resolve(this))
			{
				ActiveDrawTargets.Remove(DrawTarget);
				RTPool->ReleaseObject(DrawTarget);
				return Texture;
			}
		}
	}
	return nullptr;
}

UTextureRenderTarget2D* FTextureBakerRenderScope::ResolveTemporaryDrawRT_AsRenderTarget(UCanvas* DrawTarget)
{
	if (FTextureBakerDrawTarget* DrawContext = ActiveDrawTargets.Find(DrawTarget))
	{
		UTextureRenderTarget2D* ResultRT = DrawContext->ReleaseRT();
		ActiveDrawTargets.Remove(DrawTarget);
		return ResultRT;
	}
	return nullptr;
}

bool FTextureBakerRenderScope::ReleaseTemporaryResource(UObject* ResourceObject)
{
	bool bResult = false;
	if (ITextureBakerRTPool* RTPool = GetRenderTargetPool())
	{
		bResult |= RTPool->ReleaseObject(ResourceObject);
	}
	if (UTexture2D* SourceTexture = Cast<UTexture2D>(ResourceObject))
	{
		int32 TextureIndex = INDEX_NONE;
		if (TemporaryTextures.Find(SourceTexture, TextureIndex))
		{
			TemporaryTextures[TextureIndex]->ReleaseResource();
			TemporaryTextures.RemoveAt(TextureIndex);
			bResult = true;
		}
	}
	return bResult;
}

void FTextureBakerRenderScope::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (ParentRenderScope.IsValid())
	{
		ParentRenderScope->AddReferencedObjects(Collector);
	}
	for (TPair<UCanvas*, FTextureBakerDrawTarget> DrawTarget : ActiveDrawTargets)
	{
		Collector.AddReferencedObject(DrawTarget.Key);
		DrawTarget.Value.AddReferencedObjects(Collector);
	}
	Collector.AddReferencedObjects(TemporaryTextures);
}

FTextureBakerRenderScope::~FTextureBakerRenderScope()
{
	if (ITextureBakerRTPool* RTPool = GetRenderTargetPool())
	{
		for (TPair<UCanvas*, FTextureBakerDrawTarget> DrawTarget : ActiveDrawTargets)
		{
			DrawTarget.Value.Discard(this);
			RTPool->ReleaseObject(DrawTarget.Key);
		}
	}
	
	for (TPair<UTexture2D*, FSavedTextureStreamingState>& ResidentTextureInfo : TexturesAreSetToBeResident)
	{
		UTexture2D* StreamedTexture = ResidentTextureInfo.Key;
		ResidentTextureInfo.Value.Revert(StreamedTexture);
	}
}
