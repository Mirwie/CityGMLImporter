// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FCityGMLImporterModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void PluginButtonClicked();
	void AddMenuExtension(FMenuBuilder& Builder);
	void ProcessCityGML(const FString& FString);
	void CreateMeshFromPolygon(TArray<FVector>& Vertices, TArray<int32>& Triangles);


	TSharedPtr<FUICommandList> PluginCommands;

};
