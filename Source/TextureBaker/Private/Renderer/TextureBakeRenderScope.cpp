#include "Renderer/TextureBakerRenderScope.h"
#include "TextureBaker.h"
#include "Engine/Canvas.h"
#include "ClearQuad.h"

FTextureBakerDrawTarget::FTextureBakerDrawTarget(UTextureRenderTarget2D* RenderTarget, ERHIFeatureLevel::Type FeatureLevel ) :
	RenderTargetObject(RenderTarget), 
	RenderCanvas(RenderTarget->GameThread_GetRenderTargetResource(), nullptr, FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime, FeatureLevel),
	DrawEvent(nullptr)
{
}

void FTextureBakerDrawTarget::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(RenderTargetObject);
}

void FTextureBakerDrawTarget::InitializeCanvasObject(UCanvas* Canvas)
{
	WaitDrawCompletion();
	Canvas->Init(RenderTargetObject->GetSurfaceWidth(), RenderTargetObject->GetSurfaceHeight(), nullptr, &RenderCanvas);
	Canvas->Update();
	DrawEvent = new FDrawEvent();

	FName RTName = RenderTargetObject->GetFName();
	FDrawEvent* LocalDrawEvent = DrawEvent;
	FTextureRenderTargetResource* RenderTargetResource = RenderTargetObject->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(BeginDrawEventCommand)(
		[RTName, LocalDrawEvent, RenderTargetResource](FRHICommandListImmediate& RHICmdList)
		{
			RenderTargetResource->FlushDeferredResourceUpdate(RHICmdList);

			BEGIN_DRAW_EVENTF(
				RHICmdList,
				DrawCanvasToTarget,
				(*LocalDrawEvent),
				*RTName.ToString());
		});
}

void FTextureBakerDrawTarget::WaitDrawCompletion()
{
	if (DrawEvent && RenderTargetObject)
	{
		FTextureRenderTargetResource* RenderTargetResource = RenderTargetObject->GameThread_GetRenderTargetResource();
		FDrawEvent* LocalDrawEvent = DrawEvent;
		ENQUEUE_RENDER_COMMAND(CanvasRenderTargetResolveCommand)(
			[RenderTargetResource, LocalDrawEvent](FRHICommandList& RHICmdList)
			{
				RHICmdList.CopyToResolveTarget(RenderTargetResource->GetRenderTargetTexture(), RenderTargetResource->TextureRHI, FResolveParams());
				STOP_DRAW_EVENT((*LocalDrawEvent));
				delete LocalDrawEvent;
			}
		);
	}
	else if (DrawEvent)
	{
		// Draw event have never been pushed to a render thread command list, so we have to remove it manually
		delete DrawEvent;
		DrawEvent = nullptr;
	}
}

UTexture2D* FTextureBakerDrawTarget::Resolve(FTextureBakerRenderScope* InRenderScope, TextureMipGenSettings MipGenSettings, ETBImageNormalization Normalization)
{
	if (UTextureRenderTarget2D* ReleasedRT = ReleaseRT())
	{
		UTexture2D* OutTexture = InRenderScope->CreateTemporaryTexture(ReleasedRT, MipGenSettings, Normalization);
		InRenderScope->ReleaseTemporaryResource(ReleasedRT);
		return OutTexture;
	}
	return nullptr;
}

void FTextureBakerDrawTarget::Discard(FTextureBakerRenderScope* InRenderScope)
{
	if (RenderTargetObject)
	{
		WaitDrawCompletion();
		InRenderScope->ReleaseTemporaryResource(RenderTargetObject);
	}
}

UTextureRenderTarget2D* FTextureBakerDrawTarget::ReleaseRT()
{
	if (RenderTargetObject)
	{
		RenderCanvas.Flush_GameThread();
		WaitDrawCompletion();
		
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

UTexture2D* FTextureBakerRenderScope::CreateTemporaryTexture(const FTextureBakerOutputInfo& TextureInfo, ETextureSourceFormat InDataFormat, const void* Data)
{
	if (UTexture2D* OutTexture = UTexture2D::CreateTransient(TextureInfo.OutputDimensions.X, TextureInfo.OutputDimensions.Y, TextureInfo.GetPixelFormat()))
	{
		TextureInfo.SetTextureAttributes(OutTexture);
		TemporaryTextures.Add(OutTexture);
		return OutTexture;
	}
	return nullptr;
}

UTexture2D* FTextureBakerRenderScope::CreateTemporaryTexture(UTextureRenderTarget2D* SourceRT, TextureMipGenSettings MipFilter, ETBImageNormalization Normalization)
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
				OutTexture->MipGenSettings = MipFilter;
				FTextureBakerModule::GetChecked().WriteTexture2DSourceArt(OutTexture, ImageFormat, SourceRT, Normalization);
				TemporaryTextures.Add(OutTexture);
				return OutTexture;
			}
		}
	}
	return nullptr;
}

UTexture2D* FTextureBakerRenderScope::ConditionallyCreateDerivedArt(UTexture2D* SourceTexture, const FTextureBakerResourceRequirements& Options, ETBDerivedArtMode Mode)
{
	if (SourceTexture)
	{
		if (!EnumHasAllFlags(Mode, ETBDerivedArtMode::AlwaysCreate) && Options.IsSatisfiedByTexture(SourceTexture, SourceTexture->Source))
		{
			return SourceTexture;
		}
		else if (ITextureBakerRTPool* RTPool = GetRenderTargetPool())
		{
			if (UTexture2D* DerivedArt = RTPool->GetOrCreateDerivedArt(SourceTexture, Options))
			{
				TemporaryTextures.Add(DerivedArt);
				return DerivedArt;
			}
		}
	}
	return EnumHasAllFlags(Mode, ETBDerivedArtMode::Failsafe) ? SourceTexture : nullptr;
}

UTextureRenderTarget2D* FTextureBakerRenderScope::CreateTemporaryRT(const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format, FLinearColor ClearColor, bool bAutoGenerateMipMaps)
{
	if (ITextureBakerRTPool* RTPool = GetRenderTargetPool())
	{
		UTextureRenderTarget2D* AllocatedRT = RTPool->GetOrCreateRT(InTargetSize, Format);
		check(AllocatedRT);
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
		check(AllocatedRT);
		AllocatedRT->ClearColor = ClearColor;
		AllocatedRT->bAutoGenerateMips = bAutoGenerateMipMaps;
		AllocatedRT->UpdateResource();

		FTextureRenderTargetResource* RenderTargetResource = AllocatedRT->GameThread_GetRenderTargetResource();
		ENQUEUE_RENDER_COMMAND(ClearRTCommand)(
			[RenderTargetResource, ClearColor](FRHICommandList& RHICmdList)
			{
				FRHIRenderPassInfo RPInfo(RenderTargetResource->GetRenderTargetTexture(), ERenderTargetActions::DontLoad_Store);
				TransitionRenderPassTargets(RHICmdList, RPInfo);
				RHICmdList.BeginRenderPass(RPInfo, TEXT("ClearRT"));
				DrawClearQuad(RHICmdList, ClearColor);
				RHICmdList.EndRenderPass();
			});

		const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
		UCanvas* RenderCanvas = RTPool->GetOrCreateCanvas();
		FTextureBakerDrawTarget& DrawTarget = ActiveDrawTargets.Emplace(RenderCanvas, FTextureBakerDrawTarget(AllocatedRT, FeatureLevel));
		DrawTarget.InitializeCanvasObject(RenderCanvas);
		return RenderCanvas;
	}
	return nullptr;
}

UTexture2D* FTextureBakerRenderScope::ResolveTemporaryDrawRT(UCanvas* DrawTarget, TextureMipGenSettings MipGenSettings, ETBImageNormalization Normalization)
{
	if (FTextureBakerDrawTarget* DrawContext = ActiveDrawTargets.Find(DrawTarget))
	{
		if (ITextureBakerRTPool* RTPool = GetRenderTargetPool())
		{
			if (UTexture2D* Texture = DrawContext->Resolve(this, MipGenSettings, Normalization))
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
