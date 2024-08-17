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
    TArray<TArray<TArray<int32>>> allBuildingsFromFileTriangles;
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
    testGebauedeGenerationUE();
    //CreateMeshFromPolygon(allBuildingsFromFile, allBuildingsFromFileTriangles, BuildingIds);


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

    // Erstelle ein neues statisches Mesh in der Welt
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (World) {
        for (int32 i = 0; i < Buildings.Num(); ++i) { // Jedes Gebäude durchlaufen
            const TArray<TArray<FVector>>& Building = Buildings[i];
            const TArray<TArray<int32>>& BuildingTriangles = Triangles[i];
            FString BuildingID = BuildingIds.IsValidIndex(i) ? BuildingIds[i] : FString::Printf(TEXT("Building_%d"), i);

            AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform());
            if (MeshActor) {
                // Erstelle ein neues ProceduralMeshComponent für das Gebäude
                UProceduralMeshComponent* ProceduralMesh = NewObject<UProceduralMeshComponent>(MeshActor);
                MeshActor->SetRootComponent(ProceduralMesh);
                ProceduralMesh->RegisterComponent();

                // Für jeden Abschnitt des Gebäudes (jede Fläche/Wand)
                for (int32 j = 0; j < Building.Num(); ++j) {
                    const TArray<FVector>& Vertices = Building[j];
                    const TArray<int32>& TrianglesArray = BuildingTriangles[j];

                    ProceduralMesh->CreateMeshSection(j, Vertices, TrianglesArray, TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
                }

                // Setze einen Label oder eine ID für das Actor-Objekt (optional) und Adresse geht in Lod1 auch
                MeshActor->SetActorLabel(BuildingID);
            }
        }
    }
}

void FCityGMLImporterModule::testGebauedeGenerationUE() { // Generierte Testdaten für 3 Häuser
    TArray<TArray<TArray<FVector>>> Buildings;
    TArray<TArray<TArray<int32>>> Triangles;
    TArray<FString> BuildingIds;

    TArray<TArray<FVector>> Building1Vertices;
    TArray<TArray<int32>> Building1Triangles;

    // Boden (rechteckige Grundfläche)
    TArray<FVector> FloorVertices1 = {
        FVector(0, 0, 200),      // Unten links
        FVector(100, 0, 200),    // Unten rechts
        FVector(100, 100, 200),  // Oben rechts
        FVector(0, 100, 200)     // Oben links
    };
    TArray<int32> FloorTriangles1 = { 0, 1, 2, 0, 2, 3 };

    // Wand 1
    TArray<FVector> Wall1Vertices1 = {
        FVector(0, 0, 200),      // Unten links
        FVector(100, 0, 200),    // Unten rechts
        FVector(100, 0, 400),    // Oben rechts
        FVector(0, 0, 400)       // Oben links
    };
    TArray<int32> Wall1Triangles1 = { 0, 1, 2, 0, 2, 3 };

    // Wand 2
    TArray<FVector> Wall2Vertices1 = {
        FVector(100, 0, 200),    // Unten links
        FVector(100, 100, 200),  // Unten rechts
        FVector(100, 100, 400),  // Oben rechts
        FVector(100, 0, 400)     // Oben links
    };
    TArray<int32> Wall2Triangles1 = { 0, 1, 2, 0, 2, 3 };

    // Wand 3
    TArray<FVector> Wall3Vertices1 = {
        FVector(100, 100, 200),  // Unten links
        FVector(0, 100, 200),    // Unten rechts
        FVector(0, 100, 400),    // Oben rechts
        FVector(100, 100, 400)   // Oben links
    };
    TArray<int32> Wall3Triangles1 = { 0, 1, 2, 0, 2, 3 };

    // Wand 4
    TArray<FVector> Wall4Vertices1 = {
        FVector(0, 100, 200),    // Unten links
        FVector(0, 0, 200),      // Unten rechts
        FVector(0, 0, 400),      // Oben rechts
        FVector(0, 100, 400)     // Oben links
    };
    TArray<int32> Wall4Triangles1 = { 0, 1, 2, 0, 2, 3 };

    // Dach (rechteckige obere Fläche)
    TArray<FVector> RoofVertices1 = {
        FVector(0, 0, 400),      // Unten links
        FVector(100, 0, 400),    // Unten rechts
        FVector(100, 100, 400),  // Oben rechts
        FVector(0, 100, 400)     // Oben links
    };
    TArray<int32> RoofTriangles1 = { 0, 1, 2, 0, 2, 3 };

    // Füge Boden, Wände und Dach zum Gebäude hinzu
    Building1Vertices.Add(FloorVertices1);
    Building1Triangles.Add(FloorTriangles1);

    Building1Vertices.Add(Wall1Vertices1);
    Building1Triangles.Add(Wall1Triangles1);

    Building1Vertices.Add(Wall2Vertices1);
    Building1Triangles.Add(Wall2Triangles1);

    Building1Vertices.Add(Wall3Vertices1);
    Building1Triangles.Add(Wall3Triangles1);

    Building1Vertices.Add(Wall4Vertices1);
    Building1Triangles.Add(Wall4Triangles1);

    Building1Vertices.Add(RoofVertices1);
    Building1Triangles.Add(RoofTriangles1);

    // Gebäude hinzufügen
    Buildings.Add(Building1Vertices);
    Triangles.Add(Building1Triangles);
    BuildingIds.Add(TEXT("Building_1"));

// Gebäude 2 (ähnliches Gebäude, etwas größer)
    TArray<TArray<FVector>> Building2Vertices;
    TArray<TArray<int32>> Building2Triangles;

    // Boden
    TArray<FVector> FloorVertices2 = {
        FVector(150, 0, 200),     // Unten links
        FVector(300, 0, 200),     // Unten rechts
        FVector(300, 150, 200),   // Oben rechts
        FVector(150, 150, 200)    // Oben links
    };
    TArray<int32> FloorTriangles2 = { 0, 1, 2, 0, 2, 3 };

    // Wand 1
    TArray<FVector> Wall1Vertices2 = {
        FVector(150, 0, 200),     // Unten links
        FVector(300, 0, 200),     // Unten rechts
        FVector(300, 0, 500),     // Oben rechts
        FVector(150, 0, 500)      // Oben links
    };
    TArray<int32> Wall1Triangles2 = { 0, 1, 2, 0, 2, 3 };

    // Wand 2
    TArray<FVector> Wall2Vertices2 = {
        FVector(300, 0, 200),     // Unten links
        FVector(300, 150, 200),   // Unten rechts
        FVector(300, 150, 500),   // Oben rechts
        FVector(300, 0, 500)      // Oben links
    };
    TArray<int32> Wall2Triangles2 = { 0, 1, 2, 0, 2, 3 };

    // Wand 3
    TArray<FVector> Wall3Vertices2 = {
        FVector(300, 150, 200),   // Unten links
        FVector(150, 150, 200),   // Unten rechts
        FVector(150, 150, 500),   // Oben rechts
        FVector(300, 150, 500)    // Oben links
    };
    TArray<int32> Wall3Triangles2 = { 0, 1, 2, 0, 2, 3 };

    // Wand 4
    TArray<FVector> Wall4Vertices2 = {
        FVector(150, 150, 200),   // Unten links
        FVector(150, 0, 200),     // Unten rechts
        FVector(150, 0, 500),     // Oben rechts
        FVector(150, 150, 500)    // Oben links
    };
    TArray<int32> Wall4Triangles2 = { 0, 1, 2, 0, 2, 3 };

    // Dach
    TArray<FVector> RoofVertices2 = {
        FVector(150, 0, 500),     // Unten links
        FVector(300, 0, 500),     // Unten rechts
        FVector(300, 150, 500),   // Oben rechts
        FVector(150, 150, 500)    // Oben links
    };
    TArray<int32> RoofTriangles2 = { 0, 1, 2, 0, 2, 3 };

    // Füge Boden, Wände und Dach zum Gebäude hinzu
    Building2Vertices.Add(FloorVertices2);
    Building2Triangles.Add(FloorTriangles2);

    Building2Vertices.Add(Wall1Vertices2);
    Building2Triangles.Add(Wall1Triangles2);

    Building2Vertices.Add(Wall2Vertices2);
    Building2Triangles.Add(Wall2Triangles2);

    Building2Vertices.Add(Wall3Vertices2);
    Building2Triangles.Add(Wall3Triangles2);

    Building2Vertices.Add(Wall4Vertices2);
    Building2Triangles.Add(Wall4Triangles2);

    Building2Vertices.Add(RoofVertices2);
    Building2Triangles.Add(RoofTriangles2);

    // Gebäude hinzufügen
    Buildings.Add(Building2Vertices);
    Triangles.Add(Building2Triangles);
    BuildingIds.Add(TEXT("Building_2"));

// Gebäude 3 (noch größer und komplexer mit 6 Wänden)
    TArray<TArray<FVector>> Building3Vertices;
    TArray<TArray<int32>> Building3Triangles;

    // Boden
    TArray<FVector> FloorVertices3 = {
        FVector(350, 0, 200),     // Unten links
        FVector(550, 0, 200),     // Unten rechts
        FVector(550, 200, 200),   // Oben rechts
        FVector(350, 200, 200)    // Oben links
    };
    TArray<int32> FloorTriangles3 = { 0, 1, 2, 0, 2, 3 };

    // Wand 1
    TArray<FVector> Wall1Vertices3 = {
        FVector(350, 0, 200),     // Unten links
        FVector(550, 0, 200),     // Unten rechts
        FVector(550, 0, 600),     // Oben rechts
        FVector(350, 0, 600)      // Oben links
    };
    TArray<int32> Wall1Triangles3 = { 0, 1, 2, 0, 2, 3 };

    // Wand 2
    TArray<FVector> Wall2Vertices3 = {
        FVector(550, 0, 200),     // Unten links
        FVector(550, 200, 200),   // Unten rechts
        FVector(550, 200, 600),   // Oben rechts
        FVector(550, 0, 600)      // Oben links
    };
    TArray<int32> Wall2Triangles3 = { 0, 1, 2, 0, 2, 3 };

    // Wand 3
    TArray<FVector> Wall3Vertices3 = {
        FVector(550, 200, 200),   // Unten links
        FVector(350, 200, 200),   // Unten rechts
        FVector(350, 200, 600),   // Oben rechts
        FVector(550, 200, 600)    // Oben links
    };
    TArray<int32> Wall3Triangles3 = { 0, 1, 2, 0, 2, 3 };

    // Wand 4
    TArray<FVector> Wall4Vertices3 = {
        FVector(350, 200, 200),   // Unten links
        FVector(350, 0, 200),     // Unten rechts
        FVector(350, 0, 600),     // Oben rechts
        FVector(350, 200, 600)    // Oben links
    };
    TArray<int32> Wall4Triangles3 = { 0, 1, 2, 0, 2, 3 };

    // Wand 5 (extrawand zur Variation)
    TArray<FVector> Wall5Vertices3 = {
        FVector(350, 100, 200),   // Unten links
        FVector(550, 100, 200),   // Unten rechts
        FVector(550, 100, 600),   // Oben rechts
        FVector(350, 100, 600)    // Oben links
    };
    TArray<int32> Wall5Triangles3 = { 0, 1, 2, 0, 2, 3 };

    // Dach
    TArray<FVector> RoofVertices3 = {
        FVector(350, 0, 600),     // Unten links
        FVector(550, 0, 600),     // Unten rechts
        FVector(550, 200, 600),   // Oben rechts
        FVector(350, 200, 600)    // Oben links
    };
    TArray<int32> RoofTriangles3 = { 0, 1, 2, 0, 2, 3 };

    // Füge Boden, Wände und Dach zum Gebäude hinzu
    Building3Vertices.Add(FloorVertices3);
    Building3Triangles.Add(FloorTriangles3);

    Building3Vertices.Add(Wall1Vertices3);
    Building3Triangles.Add(Wall1Triangles3);

    Building3Vertices.Add(Wall2Vertices3);
    Building3Triangles.Add(Wall2Triangles3);

    Building3Vertices.Add(Wall3Vertices3);
    Building3Triangles.Add(Wall3Triangles3);

    Building3Vertices.Add(Wall4Vertices3);
    Building3Triangles.Add(Wall4Triangles3);

    Building3Vertices.Add(Wall5Vertices3);
    Building3Triangles.Add(Wall5Triangles3);

    Building3Vertices.Add(RoofVertices3);
    Building3Triangles.Add(RoofTriangles3);

    // Gebäude hinzufügen
    Buildings.Add(Building3Vertices);
    Triangles.Add(Building3Triangles);
    BuildingIds.Add(TEXT("Building_3"));
    CreateMeshFromPolygon(Buildings, Triangles, BuildingIds);
}




#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCityGMLImporterModule, CityGMLImporter)