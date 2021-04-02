// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureBakerCommands.h"

#define LOCTEXT_NAMESPACE "FTextureBakerModule"

void FTextureBakerCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "TextureBaker", "Bring up TextureBaker window", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
