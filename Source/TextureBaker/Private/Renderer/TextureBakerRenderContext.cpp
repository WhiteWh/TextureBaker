#include "Renderer/TextureBakerRenderContext.h"
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
		if (OutputInfo.OnRenderOutputTarget.IsBoundToObject(OwnedScenario))
		{
			UCanvas* DrawingCanvas = CurrentRenderScope->CreateTemporaryDrawRT(OutputInfo.OutputDimensions, OutputInfo.GetRenderTargetFormat(), OutputInfo.DefaultColor, false);
			if (OutputInfo.OnRenderOutputTarget.Execute(OutputInfo, bIsPreviewContext, DrawingCanvas))
			{
				return FTextureBakerRenderResult(OutputInfo, OutputInfo.OutputAssetPath, CurrentRenderScope->ResolveTemporaryDrawRT_AsRenderTarget(DrawingCanvas));
			}
		}
		return FTextureBakerRenderResult(OutputInfo, OutputInfo.OutputAssetPath);
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

UTextureRenderTarget2D* FTextureBakerRenderContext::GetOrCreateRT(const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format)
{
	return nullptr;
}

UCanvas* FTextureBakerRenderContext::GetOrCreateCanvas()
{
	return nullptr;
}

bool FTextureBakerRenderContext::ReleaseObject(UObject* Object)
{
	return true;
}
