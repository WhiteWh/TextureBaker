// Copyright 

#pragma once

#include "TextureBaker.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "UObject/GCObject.h"
#include "TextureBakerScenario.h"

class SComboButton;

struct FTextureRepackOutputDecl
{
public:
	FTextureRepackOutputDecl() : bOutputEnabled(true), OutputName(NAME_None) {}
	FTextureRepackOutputDecl(const FTextureRepackOutputDecl& Lhs) : bOutputEnabled(Lhs.bOutputEnabled), OutputName(Lhs.OutputName), OutputPath(Lhs.OutputPath), DetailedInfo(Lhs.DetailedInfo) {}
	FTextureRepackOutputDecl(const FTextureBakerOutputInfo& Info, bool bEnabled, FName Name, const FString& Path) : bOutputEnabled(bEnabled), OutputName(Name), OutputPath(Path), DetailedInfo(Info) {}

	FName GetOutputFName() const { return OutputName; }

	bool IsOutputEnabled() const { return bOutputEnabled; }
	FText GetOutputName() const { return FText::FromName(OutputName); }
	FText GetOutputPath() const { return FText::FromString(OutputPath); }
	FText GetOutputInfo() const { return FText::FromString(DetailedInfo.ToString()); }

protected:
	bool bOutputEnabled;
	FName OutputName;
	FString OutputPath;
	FTextureBakerOutputInfo DetailedInfo;
};
typedef TSharedPtr<FTextureRepackOutputDecl> FTextureRepackOutputDeclPtr;

class STextureBakerOutputDeclListItem : public SMultiColumnTableRow<FTextureRepackOutputDeclPtr>
{
public:
	static FName NAME_OutputButtons;
	static FName NAME_OutputName;
	static FName NAME_OutputPath;
	static FName NAME_OutputInfo;

	SLATE_BEGIN_ARGS(STextureBakerOutputDeclListItem) : _OutputDecl()
	{ }

		SLATE_ARGUMENT(TSharedPtr<FTextureRepackOutputDecl>, OutputDecl)
	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);
	virtual bool IsOutputEnabled() const;
	virtual FText GetOutputName() const;
	virtual FText GetOutputPath() const;
	virtual FText GetOutputInfo() const;

	// SMultiColumnTableRow overrides
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	TSharedPtr<FTextureRepackOutputDecl> OutputDecl;
};

class STextureBakerScenarioWidget : public SCompoundWidget, public FGCObject
{
public:
	/** Default constructor. */
	STextureBakerScenarioWidget();
	virtual ~STextureBakerScenarioWidget();

	SLATE_BEGIN_ARGS(STextureBakerScenarioWidget) {}
	SLATE_END_ARGS()

	/** Begin SCompoundWidget overrides */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	/** End SCompoundWidget overrides */

	/** Begin FGCObject overrides */
	virtual void AddReferencedObjects(FReferenceCollector& Collector);
	virtual FString GetReferencerName() const { return "STextureBakerScenarioWidget"; }
	/** End FGCObject overrides */

	/* Widget generators */
	TSharedRef<ITableRow> GenerateOutputDeclRow(TSharedPtr<FTextureRepackOutputDecl> InItem, const TSharedRef<STableViewBase>& InOwningTable);

	/* Widget events */
	void OnScenarioClassPicked(UClass* ChosenClass);
	FText GetCurrentScenarioText() const;
	FText GetCurrentOutputLocation() const;
	void OnUsePathFromContentBrowser();
	void OnOpenInContentBrowser();
	void OnFinishedChangingTemplateProperties(const FPropertyChangedEvent& PropertyChangedEvent);


	FReply OnPickContent();
	FReply OnStartBake();
	const FSlateBrush* GetCurrentScenarioIcon() const;
	bool IsBakerTemplateAvailable() const;
	void HandleOutputPathCommitted(const FText& NewText, ETextCommit::Type CommitInfo);
	void HandleOutputPathCommitted(const FString& NewPath);
	bool HandleVerifyOutputPathChanged(const FText& NewText, FText& OutErrorMessage);
	bool VerifyOutputPath(const FString& NewText, FText* OutErrorMessage = nullptr) const;
	bool IsOutputPathReadOnly() const;

	void Construct(const FArguments& InArgs);
	void UpdateClassTemplate(UClass* TargetClass);
	void UpdateOutputInfo();

protected:

	TSharedRef<SWidget> OnGetMenuContextMenu();

protected:
	/** Scenario selection combo box */
	TSharedPtr<SComboButton> ScenarioComboBox;
	
	/** Scenario selection combo box */
	TSharedPtr<SEditableTextBox> OutputDirectoryEditor;

	/** Output declarations */
	TSharedPtr<SListView<FTextureRepackOutputDeclPtr>> OutputDeclList;

	/** Output declarations */
	TArray<FTextureRepackOutputDeclPtr> RegisteredOutputDecls;

	/** Output table vs property list splitter */
	TSharedPtr<SSplitter> Splitter;

	/** Reference to connection settings struct details panel */
	TSharedPtr<class IDetailsView> SettingsDetailsView;

	/** Reference to connection settings struct details panel */
	TSharedPtr<SWidget> OutputDirectoryPicker;

	/* A path to write textures */
	FString OutputDirectoryPath;

	/** A class selected to b a scenario */
	UClass* ChoosenScenarioClass;

	/** A template object used to set properties */
	UTextureBakerScenario* TextureBakerScenarioTemplate;
};