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
    UE_LOG(LogTemp, Log, TEXT("Process geoeffnet"));
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
    // Davor in bounded bekommt man das Koordinatensystem und die Eckpunkte wenn man die benutzen möchte 
    // für offset benutzen oder georef anschauen


        // Iteriere über alle cityObjectMember-Knoten
    for (FXmlNode* CityObjectMember : CityObjectMembers)
    {
        // Suche nach Building-Knoten
        const FXmlNode* BuildingNode = CityObjectMember->FindChildNode(TEXT("bldg:Building")); //GetFirst geht evtl auch
        if (BuildingNode)
        {
            FString BuildingID = BuildingNode->GetAttribute(TEXT("gml:id"));
            UE_LOG(LogTemp, Log, TEXT("Bulding ID hier"));

            const FXmlNode* Lod1SolidNode = BuildingNode->FindChildNode(TEXT("bldg:lod1Solid"));
            if (Lod1SolidNode)
            {
                const FXmlNode* SolidNode = Lod1SolidNode->FindChildNode(TEXT("gml:Solid"));
                if (SolidNode)
                {
                    const FXmlNode* ExteriorNode = SolidNode->FindChildNode(TEXT("gml:exterior"));
                    if (ExteriorNode)
                    {
                        const FXmlNode* CompositeSurfaceNode = ExteriorNode->FindChildNode(TEXT("gml:CompositeSurface"));
                        if (CompositeSurfaceNode)
                        {
                            TArray<FVector> Vertices;
                            TArray<int32> Triangles; // Noch nicht wirklich impl

                            // Verarbeite alle gml:surfaceMember-Knoten also die einzelnen Waende oder Decken
                            for (const FXmlNode* SurfaceMemberNode : CompositeSurfaceNode->GetChildrenNodes())
                            {
                                const FXmlNode* PolygonNode = SurfaceMemberNode->FindChildNode(TEXT("gml:Polygon"));
                                if (PolygonNode)
                                {
                                    const FXmlNode* PolygonExteriorNode = PolygonNode->FindChildNode(TEXT("gml:exterior"));
                                    if (PolygonExteriorNode)
                                    {
                                        const FXmlNode* LinearRingNode = PolygonExteriorNode->FindChildNode(TEXT("gml:LinearRing"));
                                        if (LinearRingNode)
                                        {
                                            const FXmlNode* PosListNode = LinearRingNode->FindChildNode(TEXT("gml:posList"));
                                            if (PosListNode)
                                            {
                                                FString PosList = PosListNode->GetContent();
                                                TArray<FString> PosArray;
                                                PosList.ParseIntoArray(PosArray, TEXT(" "), true); // false damit " " auch drin ist
                                                UE_LOG(LogTemp, Log, TEXT("Hier PosArray anschauen"))

                                                    for (int32 i = 0; i < PosArray.Num()-3; i += 3) // Alle Punkte außer den letzten weil Kreis
                                                    {
                                                        FVector Vertex;
                                                        Vertex.X = FCString::Atof(*PosArray[i]);
                                                        Vertex.Y = FCString::Atof(*PosArray[i + 1]);
                                                        Vertex.Z = FCString::Atof(*PosArray[i + 2]);
                                                        Vertices.Add(Vertex);
                                                    }

                                                // Hier werden die Dreiecke für den Mesh-Aufbau definiert (Platzhalter
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
                            }

                            // Erstelle ein neues statisches Mesh in der Welt basierend auf den Vertices
                            int32 i1 = 0;
                            if(i1++ == 0) { // Vorerst soll nur ein Haus als Test erstellt werden
                            UWorld* World = GEditor->GetEditorWorldContext().World(); // Aktuelle Welt sollte das hier sein

                            if (World && Vertices.Num() > 0)
                            {
                                AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform());
                                if (NewActor)
                                {
                                    UProceduralMeshComponent* ProceduralMesh = NewObject<UProceduralMeshComponent>(NewActor);
                                    ProceduralMesh->RegisterComponent();

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
                                    };

                                    ProceduralMesh->CreateMeshSection(0, Vertices1, Triangles2, TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
                                    //NewActor->SetActorLabel(BuildingID);
                                    NewActor->SetRootComponent(ProceduralMesh);
                                }
                                    
                                }
                            }
                        }
                    }
                }
            }
        }
    } 

    





    UE_LOG(LogTemp, Log, TEXT("Finished processing CityGML file: %s"), *FilePath);

}

void FCityGMLImporterModule::CreateMeshFromPolygon(TArray<FVector>& Vertices, TArray<int32>& Triangles)
{
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) return;

    AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass());
    if (!MeshActor) return;

    UProceduralMeshComponent* ProceduralMesh = NewObject<UProceduralMeshComponent>(MeshActor);

    ProceduralMesh->CreateMeshSection(0, Vertices, Triangles, TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);

    MeshActor->SetRootComponent(ProceduralMesh);
    ProceduralMesh->RegisterComponent();
}




#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCityGMLImporterModule, CityGMLImporter)