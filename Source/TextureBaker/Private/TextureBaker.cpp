// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureBaker.h"
#include "TextureBakerStyle.h"
#include "TextureBakerCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"
#include "ToolMenus.h"
#include "Widgets/TextureBaker/STextureBakerScenarioWidget.h"

static const FName TextureBakerTabName("TextureBaker");

#define LOCTEXT_NAMESPACE "FTextureBakerModule"

void FTextureBakerModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FTextureBakerStyle::Initialize();
	FTextureBakerStyle::ReloadTextures();

	FTextureBakerCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FTextureBakerCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FTextureBakerModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FTextureBakerModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TextureBakerTabName, FOnSpawnTab::CreateRaw(this, &FTextureBakerModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FTextureBakerTabTitle", "TextureBaker"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FTextureBakerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FTextureBakerStyle::Shutdown();

	FTextureBakerCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TextureBakerTabName);
}

TSharedRef<SDockTab> FTextureBakerModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	FText WidgetText = FText::Format(
		LOCTEXT("WindowWidgetText", "Add code to {0} in {1} to override this window's contents"),
		FText::FromString(TEXT("FTextureBakerModule::OnSpawnPluginTab")),
		FText::FromString(TEXT("TextureBaker.cpp"))
		);

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(STextureBakerScenarioWidget)
		];
}

void FTextureBakerModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->InvokeTab(TextureBakerTabName);
}

void FTextureBakerModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FTextureBakerCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FTextureBakerCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTextureBakerModule, TextureBaker)