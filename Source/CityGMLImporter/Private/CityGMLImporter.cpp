// Copyright Epic Games, Inc. All Rights Reserved.

#include "CityGMLImporter.h"
#include "Misc/MessageDialog.h"
#include "LevelEditor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Framework/Application/SlateApplication.h"
#include "DesktopPlatformModule.h"
#include "XmlParser/Public/XmlFile.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"



static const FName CityGMLImporterTabName("CityGMLImporter");

#define LOCTEXT_NAMESPACE "FCityGMLImporterModule"

void FCityGMLImporterModule::StartupModule()
{
    // Dieser Code wird nach dem Laden des Moduls ausgeführt
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

    TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
    MenuExtender->AddMenuExtension("FileProject", EExtensionHook::After, nullptr, FMenuExtensionDelegate::CreateRaw(this, &FCityGMLImporterModule::AddMenuExtension));

    LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
}

void FCityGMLImporterModule::ShutdownModule()
{
    // Diese Funktion kann beim Herunterfahren aufgerufen werden, um das Modul aufzuräumen
}

void FCityGMLImporterModule::AddMenuExtension(FMenuBuilder& Builder)
{
    Builder.AddMenuEntry(
        LOCTEXT("CityGMLParser", "Select CityGML-File"),
        LOCTEXT("CityGMLParser_Tooltip", "Select a CityGML file to import"),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(this, &FCityGMLImporterModule::PluginButtonClicked))
    );
}


void FCityGMLImporterModule::PluginButtonClicked()
{
   
    TArray<FString> OutFiles;
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    bool bOpened = false;

    if (DesktopPlatform)
    {
        bOpened = DesktopPlatform->OpenFileDialog(  // Öffne den Datei-Dialog
            nullptr,
            TEXT("Select a CityGML file to import"),
            FPaths::ProjectDir(),
            TEXT(""),
            TEXT("CityGML Files (*.xml)|*.xml"),
            EFileDialogFlags::None,
            OutFiles
        );
    }

    if (bOpened && OutFiles.Num() > 0)
    {
        FString SelectedFile = OutFiles[0];
        ProcessCityGML(SelectedFile);

        FText DialogText = FText::Format(
            LOCTEXT("FileSelected", "Datei erfolgreich eingelesen: {0}"),
            FText::FromString(SelectedFile)
        );
        FMessageDialog::Open(EAppMsgType::Ok, DialogText);
    }
    else
    {
        FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FileNotSelected", "Keine Datei ausgewählt"));
    }
}

void FCityGMLImporterModule::ProcessCityGML(const FString& FilePath)
{
    // XML-Datei laden
    FXmlFile XmlFile(FilePath);
    if (!XmlFile.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load XML file: %s"), *FilePath);
        return;
    }

    // Überprüfen, ob es sich um eine CityGML-Datei handelt und Rootnode bekommen
    FXmlNode* RootNode = XmlFile.GetRootNode();
    if (!RootNode || RootNode->GetTag() != TEXT("core:CityModel"))
    {
        UE_LOG(LogTemp, Error, TEXT("The file does not appear to be a valid CityGML file: %s"), *FilePath);
        return;
    }

    const TArray<FXmlNode*>& CityObjectMembers = RootNode->GetChildrenNodes();
    FVector OffsetVector = FVector(0.0f, 0.0f, 0.0f);
    const FXmlNode* BoundaryNode = CityObjectMembers[1]->FindChildNode(TEXT("gml:Envelope"));

    UE_LOG(LogTemp, Log, TEXT("BoundaryNode sollte eigentlich ab diesem Punkt sichtbar sein"));
    if (BoundaryNode) {
        // Aus dem Boundary Vektor die x und y Koordinaten extrahieren für einen Offset
        FString lowerCorner = BoundaryNode->GetChildrenNodes()[0]->GetContent();
        TArray<FString> OffsetArray;
        lowerCorner.ParseIntoArray(OffsetArray, TEXT(" "), true);

        OffsetVector.X = FCString::Atof(*OffsetArray[0]);
        OffsetVector.Y = FCString::Atof(*OffsetArray[1]);
        OffsetVector.Z = 0.0f;
    }

        // Iteriere über alle Knoten
    TArray<TArray<TArray<FVector>>> allBuildingsFromFile; // Alle Gebäude
    TArray<TArray<TArray<int32>>> allBuildingsFromFileTriangles; // Alle Gebäude
    TArray<FString> BuildingIds;
    for (FXmlNode* CityObjectMember : CityObjectMembers)
    {

            // Suche nach Building-Knoten
            const FXmlNode* BuildingNode = CityObjectMember->FindChildNode(TEXT("bldg:Building"));
            if (BuildingNode) { 

                TArray<TArray<FVector>> BuildingVectors; // Ein Gebäude
                TArray<TArray<int32>> BuildingTriangles;
                FString BuildingID = BuildingNode->GetAttribute(TEXT("gml:id"));

                const FXmlNode* Lod1SolidNode = BuildingNode->FindChildNode(TEXT("bldg:lod1Solid"));
                if (Lod1SolidNode) {

                    const FXmlNode* SolidNode = Lod1SolidNode->FindChildNode(TEXT("gml:Solid"));
                    if (SolidNode) {

                        const FXmlNode* ExteriorNode = SolidNode->FindChildNode(TEXT("gml:exterior"));
                        if (ExteriorNode) {

                            const FXmlNode* CompositeSurfaceNode = ExteriorNode->FindChildNode(TEXT("gml:CompositeSurface"));
                            if (CompositeSurfaceNode) {

                                // Verarbeite alle gml:surfaceMember-Knoten also die einzelnen Waende oder Decken
                                for (const FXmlNode* SurfaceMemberNode : CompositeSurfaceNode->GetChildrenNodes()) {
                                    TArray<FVector> Vertices; // Für eine Fläche
                                    TArray<int32> Triangles;
                                    const FXmlNode* PolygonNode = SurfaceMemberNode->FindChildNode(TEXT("gml:Polygon"));
                                    if (PolygonNode) {
                                        const FXmlNode* PolygonExteriorNode = PolygonNode->FindChildNode(TEXT("gml:exterior"));
                                        if (PolygonExteriorNode) {
                                            const FXmlNode* LinearRingNode = PolygonExteriorNode->FindChildNode(TEXT("gml:LinearRing"));
                                            if (LinearRingNode) {
                                                const FXmlNode* PosListNode = LinearRingNode->FindChildNode(TEXT("gml:posList"));
                                                if (PosListNode) {
                                                    FString PosList = PosListNode->GetContent();
                                                    TArray<FString> PosArray;
                                                    PosList.ParseIntoArray(PosArray, TEXT(" "), true); // false damit " " auch drin ist
                                                    UE_LOG(LogTemp, Log, TEXT("Hier PosArray anschauen"))

                                                        for (int32 i = 0; i < PosArray.Num() - 3; i += 3) { // Alle Punkte außer den letzten weil Kreis
                                                            Vertices.Add(ConvertUtmToUnreal(FCString::Atof(*PosArray[i]), FCString::Atof(*PosArray[i + 1]), OffsetVector));
                                                        }

                                                    // Hier werden die Dreiecke für den Mesh-Aufbau definiert (Platzhalter)
                                                    int32 VertexOffset = Vertices.Num() - 4;
                                                    Triangles.Add(VertexOffset);
                                                    Triangles.Add(VertexOffset + 1);
                                                    Triangles.Add(VertexOffset + 2);

                                                    Triangles.Add(VertexOffset);
                                                    Triangles.Add(VertexOffset + 2);
                                                    Triangles.Add(VertexOffset + 3);
                                                }
                                            }
                                        }
                                    }
                                    BuildingVectors.Add(Vertices);
                                    BuildingTriangles.Add(Triangles);
                                } // Wand bzw. Decke Ende

                            }
                        }
                    }
                }
                BuildingIds.Add(BuildingID);
                allBuildingsFromFile.Add(BuildingVectors); // Hier weil es safe ein Gebäude sein muss
                allBuildingsFromFileTriangles.Add(BuildingTriangles);
            } 
    } // Gebäude zuende

    UE_LOG(LogTemp, Log, TEXT("Finished processing CityGML to C++ Format"));
    CreateMeshFromPolygon(allBuildingsFromFile, allBuildingsFromFileTriangles, BuildingIds);


    UE_LOG(LogTemp, Log, TEXT("Finished processing CityGML file: %s"), *FilePath);

}

FVector FCityGMLImporterModule::ConvertUtmToUnreal(float UTM_X, float UTM_Y, FVector OriginOffset) 
{
    // Vertausche die Achsen: X -> Y und Y -> X ( X Osten/ Y Norden)
    float UnrealX = UTM_Y - OriginOffset.Y;
    float UnrealY = UTM_X - OriginOffset.X;

    return FVector(UnrealX, UnrealY, 180.0f);
}

void FCityGMLImporterModule::CreateMeshFromPolygon(TArray<TArray<TArray<FVector>>>& Buildings, TArray<TArray<TArray<int32>>>& Triangles, TArray<FString> BuildingIds) {

        // Erstelle ein neues statisches Mesh in der Welt basierend auf den Vertices
        UWorld* World = GEditor->GetEditorWorldContext().World(); // Aktuelle Welt sollte das hier sein

        if (World) //  && Buildings.Num() > 0
        {
            AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform());
            if (MeshActor)
            {
                UProceduralMeshComponent* ProceduralMesh = NewObject<UProceduralMeshComponent>(MeshActor);

                //ProceduralMesh->CreateMeshSection(0, Vertices1, Triangles2, TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
                //MeshActor->SetActorLabel(BuildingID);
                MeshActor->SetRootComponent(ProceduralMesh);
                ProceduralMesh->RegisterComponent();
            }
        }
        // Die beiden noch für Tests drin
/*
TArray<FVector> Vertices1 = {
    FVector(500, 500, 500),     // Boden-Ecke
    FVector(500, 100, 500),   // Boden-Ecke
    FVector(100, 100, 500), // Boden-Ecke
    FVector(100, 500, 500),   // Boden-Ecke
    FVector(500, 500, 100),   // Dach-Ecke
    FVector(500, 100, 100), // Dach-Ecke
    FVector(100, 100, 100), // Dach-Ecke
    FVector(100, 500, 100)  // Dach-Ecke
};

// Definiere Dreiecke (die Reihenfolge der Vertices bestimmt die Flächen)
TArray<int32> Triangles2 = {
    0, 2, 1,  // Unterseite
    0, 3, 2,
    0, 1, 5,  // Vorderseite
    0, 5, 4,
    1, 2, 6,  // Seite
    1, 6, 5,
    2, 3, 7,  // Rückseite
    2, 7, 6,
    3, 0, 4,  // Andere Seite
    3, 4, 7,
    4, 5, 6,  // Dach
    4, 6, 7
}; */
}




#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCityGMLImporterModule, CityGMLImporter)