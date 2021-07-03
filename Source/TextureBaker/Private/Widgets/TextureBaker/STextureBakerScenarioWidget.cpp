#include "Widgets/TextureBaker/STextureBakerScenarioWidget.h"
#include "PropertyEditorModule.h"
#include "ClassViewerModule.h"
#include "TextureBakerStyle.h"
#include "ClassViewerFilter.h"
#include "Styling/SlateIconFinder.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "TextureBaker"

/** Class filter implementation, ensuring we only show valid UDataSourceFilter (sub)classes */
class FTextureBakerScenarioClassFilter : public IClassViewerFilter
{
public:
	bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		if (InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_HideDropDown | CLASS_Deprecated))
		{
			return false;
		}

		return InClass->IsChildOf(UTextureBakerScenario::StaticClass());
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		return InClass->IsChildOf(UTextureBakerScenario::StaticClass());
	}
};

STextureBakerScenarioWidget::STextureBakerScenarioWidget() 
{
	ChoosenScenarioClass = nullptr;
	TextureBakerScenarioTemplate = nullptr;
}

STextureBakerScenarioWidget::~STextureBakerScenarioWidget() 
{
	
}

void STextureBakerScenarioWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

void STextureBakerScenarioWidget::OnFinishedChangingTemplateProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	UpdateOutputInfo();
}

void STextureBakerScenarioWidget::Construct(const FArguments& InArgs)
{
	// Property details viewer
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bShowPropertyMatrixButton = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bAllowFavoriteSystem = false;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.ViewIdentifier = NAME_None;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;

	SettingsDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	SettingsDetailsView->OnFinishedChangingProperties().AddSP(this, &STextureBakerScenarioWidget::OnFinishedChangingTemplateProperties);

	ChildSlot
	[
		SNew(SBorder)
		.Padding(4.0f)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 2.0f))
			[			
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(ScenarioComboBox, SComboButton)
					.Visibility(EVisibility::Visible)
					//.ComboButtonStyle(FTextureBakerStyle::Get(), "ScenarioSelector.ComboButton")
					//.ForegroundColor(FLinearColor::White)
					.ContentPadding(FMargin(0, 2))
					.OnGetMenuContent(this, &STextureBakerScenarioWidget::OnGetMenuContextMenu)					
					.HasDownArrow(true)
					.ButtonContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(this, &STextureBakerScenarioWidget::GetCurrentScenarioIcon)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2, 0, 0, 0)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.TextStyle(FTextureBakerStyle::Get(), "ScenarioSelector.TextStyle")
							.Text(this, &STextureBakerScenarioWidget::GetCurrentScenarioText)
						]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4, 0, 0, 0)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
					.IsEnabled(this, &STextureBakerScenarioWidget::IsBakerTemplateAvailable)
					.ForegroundColor(FLinearColor::White)
					.ContentPadding(FMargin(6, 0))
					.OnClicked(this, &STextureBakerScenarioWidget::OnStartBake)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(FEditorStyle::GetBrush("MeshPaint.TexturePaint.Small"))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(8, 0)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							//.TextStyle(FTextureBakerStyle::Get(), "ScenarioSelector.TextStyle")
							.Text(LOCTEXT("BakeScenarioButton", "Bake"))
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.HAlign(HAlign_Fill)
			[
				SAssignNew(Splitter, SSplitter)
				.Orientation(Orient_Vertical)
				+ SSplitter::Slot()
				.Value(0.4f)
				[
					SNew(SBorder)
					.Padding(2.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0, 0, 8, 0)
							[
								SNew(STextBlock)
								.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
								.Text(LOCTEXT("OutputPathPropertyName", "Output directory:"))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.VAlign(VAlign_Center)
							[
								SAssignNew(OutputDirectoryEditor, SEditableTextBox)
								.IsEnabled(true)
								.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
								.Text(this, &STextureBakerScenarioWidget::GetCurrentOutputLocation)
								//.OnBeginTextEdit(this, &SAssetListItem::HandleBeginNameChange)
								.OnTextCommitted(this, &STextureBakerScenarioWidget::HandleOutputPathCommitted)
								.OnVerifyTextChanged(this, &STextureBakerScenarioWidget::HandleVerifyOutputPathChanged)
								.IsReadOnly(this, &STextureBakerScenarioWidget::IsOutputPathReadOnly)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &STextureBakerScenarioWidget::OnOpenInContentBrowser))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								PropertyCustomizationHelpers::MakeUseSelectedButton(FSimpleDelegate::CreateSP(this, &STextureBakerScenarioWidget::OnUsePathFromContentBrowser))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SAssignNew(OutputDirectoryPicker, SButton)
								.ToolTipText(LOCTEXT("UseButtonToolTipText", "Use Selected Asset from Content Browser"))
								.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
								.ContentPadding(4.0f)
								.ForegroundColor(FSlateColor::UseForeground())
								.IsFocusable(false)
								.OnClicked(this, &STextureBakerScenarioWidget::OnPickContent)
								[
									SNew(SImage)
									.Image(FEditorStyle::GetBrush("SceneOutliner.FolderOpen"))
								]
							]
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SAssignNew(OutputDeclList, SListView<FTextureRepackOutputDeclPtr>)
							.ListItemsSource(&RegisteredOutputDecls)
							.OnGenerateRow(this, &STextureBakerScenarioWidget::GenerateOutputDeclRow)
							.SelectionMode(ESelectionMode::SingleToggle)
							.HeaderRow
							(
								SNew(SHeaderRow)
								+ SHeaderRow::Column(STextureBakerOutputDeclListItem::NAME_OutputButtons)
								.DefaultLabel(LOCTEXT("OutputButtons", ""))
								.ManualWidth(18.0f)
								+ SHeaderRow::Column(STextureBakerOutputDeclListItem::NAME_OutputName)
								.DefaultLabel(LOCTEXT("OutputName", "Output name"))
								.FillWidth(.33f)
								+ SHeaderRow::Column(STextureBakerOutputDeclListItem::NAME_OutputPath)
								.DefaultLabel(LOCTEXT("OutputPath", "Save path"))
								.FillWidth(.33f)
								+ SHeaderRow::Column(STextureBakerOutputDeclListItem::NAME_OutputInfo)
								.DefaultLabel(LOCTEXT("OutputInfo", "Information"))
								.FillWidth(.33f)
							)
						]
					]
				]
				+ SSplitter::Slot()
				.Value(0.6f)
				[
					SettingsDetailsView.ToSharedRef()
				]
			]
		]
	];
}

TSharedRef<ITableRow> STextureBakerScenarioWidget::GenerateOutputDeclRow(TSharedPtr<FTextureRepackOutputDecl> InItem, const TSharedRef<STableViewBase>& InOwningTable)
{
	return SNew(STextureBakerOutputDeclListItem, InOwningTable).OutputDecl(InItem);
}

void STextureBakerScenarioWidget::HandleOutputPathCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	HandleOutputPathCommitted(NewText.ToString());
}

void STextureBakerScenarioWidget::HandleOutputPathCommitted(const FString& NewPath)
{
	OutputDirectoryPath = NewPath;
	FPaths::NormalizeDirectoryName(OutputDirectoryPath);
	OutputDirectoryPath.AppendChar(TCHAR('/'));
	UpdateOutputInfo();
}

#define OPTIONAL_ERROR_MESSAGE(FTextPtr, Tag, Text) if (FTextPtr) { *FTextPtr = LOCTEXT(Tag, Text); }
bool STextureBakerScenarioWidget::VerifyOutputPath(const FString& NewPath, FText* OutErrorMessage) const
{
	if (NewPath.IsEmpty())
	{
		OPTIONAL_ERROR_MESSAGE(OutErrorMessage, "TextureBakerPathError_IsEmpty", "Path should not be empty!")
		return false;
	}

	if (FPaths::ValidatePath(NewPath, OutErrorMessage))
	{
		return true;
	}
	return false;
}
#undef OPTIONAL_ERROR_MESSAGE

bool STextureBakerScenarioWidget::HandleVerifyOutputPathChanged(const FText& NewText, FText& OutErrorMessage)
{
	return VerifyOutputPath(NewText.ToString(), &OutErrorMessage);
}

bool STextureBakerScenarioWidget::IsOutputPathReadOnly() const
{
	return false;
}

FText STextureBakerScenarioWidget::GetCurrentOutputLocation() const
{
	return FText::FromString(OutputDirectoryPath);
}

void STextureBakerScenarioWidget::OnOpenInContentBrowser()
{
	if (VerifyOutputPath(OutputDirectoryPath))
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FString> SelectedPaths;
		SelectedPaths.Add(OutputDirectoryPath);
		ContentBrowserModule.Get().SetSelectedPaths(SelectedPaths, true);
	}
}

void STextureBakerScenarioWidget::OnUsePathFromContentBrowser()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FString NewPath = ContentBrowserModule.Get().GetCurrentPath();
	if (VerifyOutputPath(NewPath))
	{
		HandleOutputPathCommitted(NewPath);
	}
}

FReply STextureBakerScenarioWidget::OnStartBake()
{
	if (IsBakerTemplateAvailable())
	{
		/* Initialize new render context */
		TUniquePtr<FTextureBakerRenderContext> NewRenderContext = MakeUnique<FTextureBakerRenderContext>(TextureBakerScenarioTemplate, OutputDirectoryPath);
		
		/* Setup output list to bake */
		for (TSharedPtr<FTextureRepackOutputDecl> WidgetDecl : RegisteredOutputDecls)
		{
			if (WidgetDecl->IsOutputEnabled())
			{
				NewRenderContext->AddOutputToRender(WidgetDecl->GetOutputFName());
			}
		}

		/* Push bakery context */
		FTextureBakerModule::GetChecked().ExecuteBakerRenderContext(MoveTemp(NewRenderContext));
	}
	return FReply::Handled();
}

FReply STextureBakerScenarioWidget::OnPickContent()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.bAllowContextMenu = false;
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &STextureBakerScenarioWidget::HandleOutputPathCommitted);

	FMenuBuilder MenuBuilder(true, NULL);
	MenuBuilder.AddWidget(SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(300.0f)
		[
			ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
		], FText());


	FSlateApplication::Get().PushMenu(OutputDirectoryPicker.ToSharedRef(),
		FWidgetPath(),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
	);

	return FReply::Handled();
}

const FSlateBrush* STextureBakerScenarioWidget::GetCurrentScenarioIcon() const
{
	if (ChoosenScenarioClass)
	{
		return FSlateIconFinder::FindCustomIconBrushForClass(ChoosenScenarioClass, TEXT("ClassIcon"));
	}
	return FSlateIconFinder::FindIcon("ClassIcon.Deleted").GetIcon();
}

FText STextureBakerScenarioWidget::GetCurrentScenarioText() const
{
	if (ChoosenScenarioClass)
	{
		return ChoosenScenarioClass->GetDisplayNameText();
	}
	return LOCTEXT("ScenarioLabelNone", "None");
}

TSharedRef<SWidget> STextureBakerScenarioWidget::OnGetMenuContextMenu()
{
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	Options.bShowNoneOption = true;
	Options.ClassFilter = MakeShared<FTextureBakerScenarioClassFilter>();

	return ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &STextureBakerScenarioWidget::OnScenarioClassPicked));
}

void STextureBakerScenarioWidget::OnScenarioClassPicked(UClass* ChosenClass)
{
	FSlateApplication::Get().DismissAllMenus();
	UpdateClassTemplate(ChosenClass);
}

void STextureBakerScenarioWidget::UpdateClassTemplate(UClass* TargetClass)
{
	if (TargetClass)
	{
		if (!TextureBakerScenarioTemplate || TextureBakerScenarioTemplate->GetClass() != TargetClass)
		{
			TextureBakerScenarioTemplate = NewObject<UTextureBakerScenario>(GetTransientPackage(), TargetClass);
			ChoosenScenarioClass = TargetClass;
			UpdateOutputInfo();
		}
	}
	else
	{
		TextureBakerScenarioTemplate = nullptr;
		ChoosenScenarioClass = nullptr;
		UpdateOutputInfo();
	}

	if (SettingsDetailsView.IsValid())
	{
		SettingsDetailsView->SetObject(TextureBakerScenarioTemplate);
	}
}

void STextureBakerScenarioWidget::UpdateOutputInfo()
{
	RegisteredOutputDecls.Empty();
	FTextureBakerOutputList Outputs;
	if (TextureBakerScenarioTemplate && VerifyOutputPath(OutputDirectoryPath))
	{
		TextureBakerScenarioTemplate->RegisterOutputTarget(OutputDirectoryPath, Outputs);
	}
	for (const FTextureBakerOutputWriteout& Writeout : Outputs.GetTextureOutputs())
	{
		FString OutputRelativePath = Writeout.OutputAssetPath;
		FPaths::MakePathRelativeTo(OutputRelativePath, *OutputDirectoryPath);
		RegisteredOutputDecls.Add(MakeShared<FTextureRepackOutputDecl>(Writeout, true, Writeout.GetOutputName(), OutputRelativePath));
	}
	if (OutputDeclList.IsValid())
	{
		OutputDeclList->RequestListRefresh();
	}
}


void STextureBakerScenarioWidget::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(ChoosenScenarioClass);
	Collector.AddReferencedObject(TextureBakerScenarioTemplate);
}

bool STextureBakerScenarioWidget::IsBakerTemplateAvailable() const
{
	return ChoosenScenarioClass 
		&& TextureBakerScenarioTemplate 
		&& TextureBakerScenarioTemplate->GetClass() == ChoosenScenarioClass
		&& VerifyOutputPath(OutputDirectoryPath)
		&& TextureBakerScenarioTemplate->InitialSettingsIsValid();
}


FName STextureBakerOutputDeclListItem::NAME_OutputButtons(TEXT("OutputButtons"));
FName STextureBakerOutputDeclListItem::NAME_OutputName(TEXT("OutputName"));
FName STextureBakerOutputDeclListItem::NAME_OutputPath(TEXT("OutputPath"));
FName STextureBakerOutputDeclListItem::NAME_OutputInfo(TEXT("OutputInfo"));

void STextureBakerOutputDeclListItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	this->OutputDecl = InArgs._OutputDecl;
	this->SetPadding(0);
	ensureAlwaysMsgf(OutputDecl.IsValid(), TEXT("Attempt to create STextureBakerOutputDeclListItem with invalid OutputDecl!"));
	SMultiColumnTableRow<FTextureRepackOutputDeclPtr>::Construct(SMultiColumnTableRow<FTextureRepackOutputDeclPtr>::FArguments().Padding(0), InOwnerTableView);
}

bool STextureBakerOutputDeclListItem::IsOutputEnabled() const
{
	return OutputDecl.IsValid() && OutputDecl->IsOutputEnabled();
}

FText STextureBakerOutputDeclListItem::GetOutputName() const
{
	return OutputDecl.IsValid() ? OutputDecl->GetOutputName() : FText();
}

FText STextureBakerOutputDeclListItem::GetOutputPath() const
{
	return OutputDecl.IsValid() ? OutputDecl->GetOutputPath() : FText();
}

FText STextureBakerOutputDeclListItem::GetOutputInfo() const
{
	return OutputDecl.IsValid() ? OutputDecl->GetOutputInfo() : FText();
}

bool STextureBakerOutputDeclListItem::CanBeModified() const
{
	return OutputDecl.IsValid();
}

ECheckBoxState STextureBakerOutputDeclListItem::GetCheckedState() const
{
	if (OutputDecl.IsValid())
	{
		return OutputDecl->IsOutputEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Undetermined;
}

void STextureBakerOutputDeclListItem::CycleCheckedState(ECheckBoxState InCheckboxState)
{
	if (OutputDecl.IsValid())
	{
		OutputDecl->SetOutputEnabled(InCheckboxState == ECheckBoxState::Checked);
	}
}

const FSlateBrush* STextureBakerOutputDeclListItem::GetCurrentOutputIcon() const
{
	if (OutputDecl.IsValid())
	{
		return FSlateIconFinder::FindCustomIconBrushForClass(UTexture2D::StaticClass(), TEXT("ClassIcon"));
	}
	return FSlateIconFinder::FindIcon("ClassIcon.Deleted").GetIcon();
}

TSharedRef<SWidget> STextureBakerOutputDeclListItem::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == NAME_OutputButtons)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.Style(FCoreStyle::Get(), "Checkbox")
				.IsEnabled(this, &STextureBakerOutputDeclListItem::CanBeModified)
				.IsChecked(this, &STextureBakerOutputDeclListItem::GetCheckedState)
				.OnCheckStateChanged(this, &STextureBakerOutputDeclListItem::CycleCheckedState)
			];
	}
	else if (ColumnName == NAME_OutputName)
	{
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(this, &STextureBakerOutputDeclListItem::GetCurrentOutputIcon)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &STextureBakerOutputDeclListItem::GetOutputName)
				.ColorAndOpacity(FLinearColor(.8f, .8f, .8f, 1.0f))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			];
	}
	else if (ColumnName == NAME_OutputPath)
	{
		return SNew(SBox)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(FMargin(0.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(this, &STextureBakerOutputDeclListItem::GetOutputPath)
				.ColorAndOpacity(FLinearColor(.8f, .8f, .8f, 1.0f))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			];
	}
	else if (ColumnName == NAME_OutputInfo)
	{
		return SNew(SBox)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(FMargin(0.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(this, &STextureBakerOutputDeclListItem::GetOutputInfo)
				.ColorAndOpacity(FLinearColor(.8f, .8f, .8f, 1.0f))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			];
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}

#undef LOCTEXT_NAMESPACE