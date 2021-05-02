#pragma once

#include "CoreMinimal.h"
#include "TextureBakerScenario.h"
#include "Templates/SharedPointer.h"
#include "Renderer/TextureBakerRenderScope.h"

class UTexture2D;

struct TEXTUREBAKER_API FTextureBakerRenderResult
{
public:
	FTextureBakerRenderResult() : ResolvedTexture(nullptr) {}
	FTextureBakerRenderResult(const FTextureBakerOutputInfo& Info, const FString& Path) : AssetPackagePath(Path), BakerInfo(Info), ResolvedTexture(nullptr) {}
	FTextureBakerRenderResult(const FTextureBakerOutputInfo& Info, const FString& Path, UTextureRenderTarget2D* ResolvedData) : AssetPackagePath(Path), BakerInfo(Info), ResolvedTexture(ResolvedData) {}
	FTextureBakerRenderResult(const FTextureBakerRenderResult& Lhs) : AssetPackagePath(Lhs.AssetPackagePath), BakerInfo(Lhs.BakerInfo), ResolvedTexture(Lhs.ResolvedTexture) {}

	bool IsValid() const { return ResolvedTexture != nullptr && FPaths::ValidatePath(AssetPackagePath) && BakerInfo.IsValid(); }
	const FTextureBakerOutputInfo& GetInfo() const { return BakerInfo; }
	UTextureRenderTarget2D* GetTextureRenderTarget() const { return ResolvedTexture; }
	FString GetPackagePath() const { return AssetPackagePath; }

protected:
	FString AssetPackagePath;
	FTextureBakerOutputInfo BakerInfo;
	UTextureRenderTarget2D* ResolvedTexture;
};

class TEXTUREBAKER_API FTextureBakerRenderContext : public FGCObject, public ITextureBakerRTPool
{
public:

	FTextureBakerRenderContext(UTextureBakerScenario* InitializedTemplate, const FString& OutputPath, bool bIsPreview = false);

	TSharedRef<FTextureBakerRenderScope> EnterRenderScope();
	bool ExitRenderScope();

	bool AddOutputToRender(FName OutputName);
	bool PrepareToBakeOutputs();
	FTextureBakerRenderResult BakeOutput(FName OutputToBake);

	const TSet<FName>& GetOutputsToBake() const { return OutputsToRender; }
	
	/* ITextureBakerRTPool interface */
	virtual UTexture2D* GetOrCreateDerivedArt(UTexture2D* Source, const FTextureBakerResourceRequirements& Options) override;
	virtual UTextureRenderTarget2D* GetOrCreateRT(const FIntPoint& InTargetSize, ETextureRenderTargetFormat Format) override;
	virtual UCanvas* GetOrCreateCanvas() override;
	virtual bool ReleaseObject(UObject* Object) override;

	/* FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const { return "TextureBaker render context"; }

private:
	
	bool												bIsPreviewContext;
	UTextureBakerScenario*								OwnedScenario;
	TSharedRef<FTextureBakerRenderScope>				CurrentRenderScope;
	FString												OutputDirectoryPath;
	TMap<FName, FTextureBakerOutputWriteout>			OutputInfos;
	TSet<FName>											OutputsToRender;

	TMultiMap<FTextureBakerDerivedArtKey, UTexture2D*>  DerivedArtPool;
	TArray<UTextureRenderTarget2D*>						RenderTargetPool;
	TArray<UCanvas*>									CanvasPool;
};