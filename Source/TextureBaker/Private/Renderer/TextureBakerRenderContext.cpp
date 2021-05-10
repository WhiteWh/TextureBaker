#include "Renderer/TextureBakerRenderContext.h"
#include "Engine/Canvas.h"
#include "TextureBakerScenario.h"

FTextureBakerRenderContext::FTextureBakerRenderContext(UTextureBakerScenario* InitializedTemplate, const FString& OutputPath, bool bIsPreview) :
	bIsPreviewContext(bIsPreview), OwnedScenario(nullptr), CurrentRenderScope(MakeShared<FTextureBakerRenderScope>(this)), OutputDirectoryPath(OutputPath)
{
	OwnedScenario = DuplicateObject<UTextureBakerScenario>(InitializedTemplate, GetTransientPackage(), NAME_None);

	TArray<FTextureBakerOutputWriteout>  UnorderedOutputs;
	OwnedScenario->RegisterOutputTarget(OutputDirectoryPath, UnorderedOutputs);
	for (const FTextureBakerOutputWriteout& Output : UnorderedOutputs)
	{
		if (Output.OnRenderOutputTarget.IsBound())
		{
			OutputInfos.Add(Output.OutputName, Output);
		}
	}

	OwnedScenario->EnterRenderScope(CurrentRenderScope);
}

bool FTextureBakerRenderContext::AddOutputToRender(FName OutputName)
{
	if (OutputInfos.Contains(OutputName))
	{
		OutputsToRender.Add(OutputName);
		return true;
	}
	return false;
}

bool FTextureBakerRenderContext::PrepareToBakeOutputs()
{
	if (OwnedScenario)
	{
		OwnedScenario->RenderCommonTargets(bIsPreviewContext);
		return true;
	}
	return false;
}

FTextureBakerRenderResult FTextureBakerRenderContext::BakeOutput(FName OutputToBake)
{
	if (OwnedScenario && OutputInfos.Contains(OutputToBake))
	{
		const FTextureBakerOutputWriteout& OutputInfo = OutputInfos.FindChecked(OutputToBake);
		if (OutputInfo.IsValid())
		{
			if (OutputInfo.OnRenderOutputTarget.IsBoundToObject(OwnedScenario))
			{
				UCanvas* DrawingCanvas = CurrentRenderScope->CreateTemporaryDrawRT(OutputInfo.OutputDimensions, OutputInfo.GetRenderTargetFormat(), OutputInfo.DefaultColor, false);
				if (OutputInfo.OnRenderOutputTarget.Execute(OutputInfo, bIsPreviewContext, DrawingCanvas))
				{
					return FTextureBakerRenderResult(OutputInfo, OutputInfo.OutputAssetPath, CurrentRenderScope->ResolveTemporaryDrawRT_AsRenderTarget(DrawingCanvas));
				}
			}
			else
			{
				UCanvas* DrawingCanvas = CurrentRenderScope->CreateTemporaryDrawRT(OutputInfo.OutputDimensions, OutputInfo.GetRenderTargetFormat(), OutputInfo.DefaultColor, false);
				DrawingCanvas->DrawText(GEngine->GetSmallFont(), FText::FromString(TEXT("Missing handler!")), 0.0f, 0.0f);
				return FTextureBakerRenderResult(OutputInfo, OutputInfo.OutputAssetPath, CurrentRenderScope->ResolveTemporaryDrawRT_AsRenderTarget(DrawingCanvas));
			}
			return FTextureBakerRenderResult(OutputInfo, OutputInfo.OutputAssetPath);
		}
	}
	return FTextureBakerRenderResult();
}

void FTextureBakerRenderContext::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(OwnedScenario);
	CurrentRenderScope->AddReferencedObjects(Collector);

	for (TPair<FName, FTextureBakerOutputWriteout>& NamedDelegate : OutputInfos)
	{
		if (NamedDelegate.Value.OnRenderOutputTarget.IsBound())
		{
			UObject* BoundToObject = NamedDelegate.Value.OnRenderOutputTarget.GetUObject();
			Collector.AddReferencedObject(BoundToObject);
			if (BoundToObject == nullptr)
			{
				NamedDelegate.Value.OnRenderOutputTarget.Unbind();
				OutputsToRender.Remove(NamedDelegate.Key);
			}
		}
	}

	for (TPair<FTextureBakerDerivedArtKey, UTexture2D*>& DerivedArtEntry : DerivedArtPool)
	{
		Collector.AddReferencedObject(DerivedArtEntry.Value);
	}
	Collector.AddReferencedObjects(RenderTargetPool);
	Collector.AddReferencedObjects(CanvasPool);

	for (auto It = DerivedArtPool.CreateIterator(); It; ++It)
	{
		if (!IsValid(It.Value()))
		{
			It.RemoveCurrent();
		}
	}
}

TSharedRef<FTextureBakerRenderScope> FTextureBakerRenderContext::EnterRenderScope()
{
	CurrentRenderScope = MakeShared<FTextureBakerRenderScope>(CurrentRenderScope);
	return CurrentRenderScope;
}

bool FTextureBakerRenderContext::ExitRenderScope()
{
	TSharedPtr<FTextureBakerRenderScope> ParentScope = CurrentRenderScope->GetParentScope();
	if (ParentScope.IsValid())
	{
		CurrentRenderScope = ParentScope.ToSharedRef();
		return true;
	}
	return false;
}

UTexture2D* FTextureBakerRenderContext::GetOrCreateDerivedArt(UTexture2D* Source, const FTextureBakerResourceRequirements& Options)
{
	if (!Source || !Source->Source.IsValid())
	{
		return nullptr;
	}

	FTextureSource& SourceArt = Source->Source;
	FTextureBakerDerivedArtKey SourceArtKey(Options, SourceArt);
	SIZE_T ReferenceMipSize = SourceArt.CalcMipSize(0);
	if (uint8* Reference = SourceArt.LockMip(0))
	{
		for (auto It = DerivedArtPool.CreateKeyIterator(SourceArtKey); It; ++It)
		{
			UTexture2D* ExistingTexture = It.Value();
			if (!ExistingTexture->Source.IsValid())
			{
				It.RemoveCurrent();
				continue;
			}
			SIZE_T ExistingMipSize = ExistingTexture->Source.CalcMipSize(0);
			bool bTextureIsPhysicallyMatching = false;
			if (uint8* Content = (ExistingMipSize == ReferenceMipSize) ? ExistingTexture->Source.LockMip(0) : nullptr)
			{
				bTextureIsPhysicallyMatching = (FMemory::Memcmp(Reference, Content, ReferenceMipSize) == 0);
				ExistingTexture->Source.UnlockMip(0);
			}
			if (bTextureIsPhysicallyMatching)
			{
				SourceArt.UnlockMip(0);
				return ExistingTexture;
			}
		}

		// Creates new derived art object
		FIntPoint NewTextureSize = Options.bUseImportedResolution ? Source->GetImportedSize() : FIntPoint(Source->GetSizeX(), Source->GetSizeY());
		if (UTexture2D* OutTexture = UTexture2D::CreateTransient(NewTextureSize.X, NewTextureSize.Y, Source->GetPixelFormat()))
		{
			OutTexture->MipGenSettings = (Options.MipGenSettings == TextureMipGenSettings::TMGS_LeaveExistingMips) ? Source->MipGenSettings : Options.MipGenSettings;
			OutTexture->CompressionNone = Options.bRequireUncompressed || Source->CompressionNone;
			OutTexture->CompressionSettings = Options.bRequireUncompressed ? TextureCompressionSettings::TC_Default : Source->CompressionSettings;
			OutTexture->AddressX = Source->AddressX;
			OutTexture->AddressY = Source->AddressY;
			OutTexture->bFlipGreenChannel = Source->bFlipGreenChannel;
			OutTexture->SRGB = Source->SRGB;
			OutTexture->MaxTextureSize = Options.bUseImportedResolution ? NewTextureSize.GetMax() : Source->MaxTextureSize;
			OutTexture->Source.Init(SourceArt.GetSizeX(), SourceArt.GetSizeY(), 1, SourceArt.GetNumMips(), SourceArt.GetFormat(), Reference);
			SourceArt.UnlockMip(0);
			
			OutTexture->UpdateResource();
			DerivedArtPool.Add(SourceArtKey, OutTexture);
			return OutTexture;
		}
		SourceArt.UnlockMip(0);
	}

	return nullptr;
}

UTextureRenderTarget2D* FTextureBakerRenderContext::GetOrCreateRT(const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format)
{
	for (auto It = RenderTargetPool.CreateIterator(); It; ++It)
	{
		UTextureRenderTarget2D* PoolRenderTarget = *It;
		if (PoolRenderTarget->SizeX == InTargetSize.X && PoolRenderTarget->SizeY == InTargetSize.Y && PoolRenderTarget->RenderTargetFormat == Format)
		{
			It.RemoveCurrent();
			return PoolRenderTarget;
		}
	}
	
	// Not found - create a new one.
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
	check(RenderTarget);
	RenderTarget->RenderTargetFormat = Format;
	RenderTarget->InitAutoFormat(InTargetSize.X, InTargetSize.Y);
	return RenderTarget;
}

UCanvas* FTextureBakerRenderContext::GetOrCreateCanvas()
{
	while (CanvasPool.Num())
	{
		if (UCanvas* ExistingCanvas = CanvasPool.Pop())
		{
			return ExistingCanvas;
		}
	}
	UCanvas* CanvasForRenderingToTarget = NewObject<UCanvas>(GetTransientPackage(), NAME_None);
	check(CanvasForRenderingToTarget);
	return CanvasForRenderingToTarget;
}

bool FTextureBakerRenderContext::ReleaseObject(UObject* Object)
{
	if (UTextureRenderTarget2D* RenderTargetObject = Cast<UTextureRenderTarget2D>(Object))
	{
		if (!RenderTargetPool.Contains(RenderTargetObject))
		{
			RenderTargetPool.Add(RenderTargetObject);
			return true;
		}
	}
	else if (UTexture2D* DerivedArtTexture = Cast<UTexture2D>(Object))
	{
		bool bFoundAnything = false;
		for (auto It = DerivedArtPool.CreateIterator(); It; ++It)
		{
			if (It.Value() == DerivedArtTexture)
			{
				It.RemoveCurrent();
				bFoundAnything = true;
			}
		}
		return bFoundAnything;
	}
	else if (UCanvas* RenderTargetCanvas = Cast<UCanvas>(Object))
	{
		if (!CanvasPool.Contains(RenderTargetCanvas))
		{
			CanvasPool.Add(RenderTargetCanvas);
			return true;
		}
	}
	return false;
}
