#include "Road/RoadBuilder.h"
#include "Elevation/HeightMapTable.h"
#include "Utils/Logging.h"
#include "EngineCode/EngineCode.h"
#include "LandscapeCombinatorStyle.h"

#include "Landscape.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeSplineControlPoint.h" 
#include "LandscapeSplinesComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SWindow.h"
#include "XmlFile.h"
#include "Misc/CString.h"
 	


#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"


FString FRoadBuilder::DetailsString()
{
	return XmlFilePath;
}

void FRoadBuilder::AddRoads(int WorldWidthKm, int WorldHeightKm, double ZScale, double WorldOriginX, double WorldOriginY)
{
	FMessageDialog::Open(EAppMsgType::Ok,
		FText::Format(
			LOCTEXT("LandscapeNotFound",
				"Landscape splines will now be added to Landscape {0}. You can monitor the progress in the Output Log. "
				"After the splines are added, you must go to\n"
				"Landscape Mode > Manage > Splines\n"
				"to manage the splines as usual. By selecting all control points or all segments, and then going to their Details panel, you can choose "
				"to paint a landscape layer for the roads, or you can add spline meshes to form your roads.\n"	
			),
			FText::FromString(LandscapeLabel)
		)
	);

	UWorld* World = GEditor->GetEditorWorldContext().World();
	double WorldWidthCm  = ((double) WorldWidthKm)  * 1000 * 100;
	double WorldHeightCm = ((double) WorldHeightKm) * 1000 * 100;

	if (!HMTable->Interfaces.Contains(LandscapeLabel))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("LandscapeNotFound", "Could not find Landscape '{0}' in the elevation interfaces."),
				FText::FromString(LandscapeLabel)
			)
		);
		return;
	}

	HMInterface *Interface = HMTable->Interfaces[LandscapeLabel];

	if (!Interface->Landscape) Interface->SetLandscapeFromLabel();

	ALandscape *Landscape = Interface->Landscape;
	if (!Landscape)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("LandscapeNotFound", "Could not find Landscape '{0}'."),
				FText::FromString(LandscapeLabel)
			)
		);
		return;
	}

	UE_LOG(LogLandscapeCombinator, Log, TEXT("Found Landscape %s"), *LandscapeLabel);

	ULandscapeSplinesComponent *LandscapeSplinesComponent = Landscape->GetSplinesComponent();
	if (!LandscapeSplinesComponent)
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Did not find a landscape splines component. Creating one."));
		Landscape->CreateSplineComponent();
		LandscapeSplinesComponent = Landscape->GetSplinesComponent();
	}
	else
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Found a landscape splines component"));
	}

	if (!LandscapeSplinesComponent)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("NoLandscapeSplinesComponent", "Could not create a landscape splines component for Landscape {0}."),
				FText::FromString(LandscapeLabel)
			)
		);
		return;
	}

	FXmlFile XmlFile = FXmlFile(XmlFilePath);
	if (!XmlFile.IsValid())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("InvalidXmlFile", "{0} is not a valid XML file."),
				FText::FromString(XmlFilePath)
			)
		);
		return;
	}
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Got a valid XML file, continuing."));

	FXmlNode* RootNode = XmlFile.GetRootNode();
	if (!RootNode)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("InvalidXmlFile2", "{0} is not a valid XML file (no root node)."),
				FText::FromString(XmlFilePath)
			)
		);
		return;
	}

	int32 NumNodes = RootNode->GetChildrenNodes().FilterByPredicate([](FXmlNode* XmlNode) { return XmlNode->GetTag().Equals("node"); }).Num();
	int32 Precision = 100;
	bool Stop = true;

	do
	{
		Stop = true; // will remain true if user closes the window with the close button, but becomes true with the OK button
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Asking road precision to the user."));

		TSharedRef<SWindow> Window = SNew(SWindow)
			.SizingRule(ESizingRule::Autosized)
			.AutoCenter(EAutoCenter::PrimaryWorkArea)
			.Title(LOCTEXT("RoadPrecision", "Choose road precision (%)"));

		Window->SetContent(
			SNew(SBox).Padding(FMargin(30, 30, 30, 30))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(
						FText::Format(
							LOCTEXT("RoadPrecisionMessage",
								"There are {0} nodes in your roads. What percent do you want to keep?\n\nPlease enter an integer between 1% and 100% (100% to keep all the road points)."),
							FText::AsNumber(NumNodes)
						)
					).Font(FLandscapeCombinatorStyle::RegularFont())
				]
				+SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 20))
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot().AutoWidth().VAlign(EVerticalAlignment::VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("RoadPrecision", "Road Precision: ")).Font(FLandscapeCombinatorStyle::RegularFont())
					]
					+SHorizontalBox::Slot().AutoWidth().VAlign(EVerticalAlignment::VAlign_Center)
					[
						SNew(SEditableTextBox)
						.Text(FText::AsNumber(100))
						.Font(FLandscapeCombinatorStyle::RegularFont())
						.OnTextCommitted_Lambda([&Precision](const FText &NewText, ETextCommit::Type) { Precision = FCString::Atoi(*NewText.ToString()); } )
						.OnTextChanged_Lambda([&Precision](const FText &NewText) { Precision = FCString::Atoi(*NewText.ToString()); } )
					]
					+SHorizontalBox::Slot().AutoWidth().VAlign(EVerticalAlignment::VAlign_Center)
					[
						SNew(STextBlock).Text(FText::FromString("%")).Font(FLandscapeCombinatorStyle::RegularFont())
					]
				]
				+SVerticalBox::Slot().AutoHeight().HAlign(EHorizontalAlignment::HAlign_Center)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot().AutoWidth().HAlign(EHorizontalAlignment::HAlign_Center)
					[
						SNew(SButton)
						.IsEnabled_Lambda([&Precision]() { return Precision > 0 && Precision <= 100; })
						.OnClicked_Lambda([&Window, &Stop]()->FReply {
							Window->RequestDestroyWindow();
							Stop = false;
							return FReply::Handled();
						})
						[
							SNew(STextBlock).Font(FLandscapeCombinatorStyle::RegularFont()).Text(FText::FromString(" Ok "))
						]
					]
				]
			]
		);
		GEditor->EditorAddModalWindow(Window);
	}
	while ((Precision <= 0 || Precision > 100) && !Stop);

	if (Stop)
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Not adding roads as user closed the road precision window."));
		return;
	}
	
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Road precision: %d"), Precision);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Number of nodes: %d"), NumNodes);

	const FXmlNode* Node = RootNode->FindChildNode(FString("node"));

	if (!Node)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("InvalidXmlFile3", "{0} is not a valid XML file (no node)."),
				FText::FromString(XmlFilePath)
			)
		);
		return;
	}

	LandscapeSplinesComponent->Modify();
	
	FCollisionQueryParams CollisionQueryParams = Interface->CustomCollisionQueryParams(World);
	TMap<FString, ULandscapeSplineControlPoint*> Points;
	TMap<FString, FVector3d> PointCoordinates;

	double Time0 = FPlatformTime::Seconds();

	while (Node->GetTag().Equals("node"))
	{
		FString id = Node->GetAttribute("id");
		double longitude = FCString::Atod(*(Node->GetAttribute("lon")));
		double lattitude = FCString::Atod(*(Node->GetAttribute("lat")));
		double x = (longitude - WorldOriginX) * WorldWidthCm / 360;
		double y = (WorldOriginY - lattitude) * WorldHeightCm / 180;
		double z = Interface->GetZ(World, CollisionQueryParams, x, y);
		if (z > 0) PointCoordinates.Add(id, { x, y, z });
		Node = Node->GetNextNode();
	}

	double Time1 = FPlatformTime::Seconds();

	UE_LOG(LogLandscapeCombinator, Log, TEXT("Computed coordinates of %d points in %f seconds"), NumNodes, Time1 - Time0);


	auto AddPoint = [&Points, &PointCoordinates, Interface, World, LandscapeSplinesComponent, this](FString Ref) {
		if (Points.Contains(Ref)) return;
		else if (!PointCoordinates.Contains(Ref)) return;
		{
			double x = PointCoordinates[Ref][0];
			double y = PointCoordinates[Ref][1];
			double z = PointCoordinates[Ref][2];
			FVector Location = FVector(x, y, z);

			FVector LocalLocation = LandscapeSplinesComponent->GetComponentToWorld().InverseTransformPosition(Location);
			ULandscapeSplineControlPoint* ControlPoint = EngineCode::AddControlPoint(LandscapeSplinesComponent, LocalLocation);
			//ControlPoint->LayerName = "Road";
			//ControlPoint->Width = RoadWidth * 50; // half-width in cm
			ControlPoint->Width = 250; // half-width in cm
			ControlPoint->SideFalloff = 200;
			Points.Add(Ref, ControlPoint);
		}
	};

	while (Node && Node->GetTag().Equals("way"))
	{
		const FXmlNode* RefNode1 = Node->FindChildNode(FString("nd"));
		if (!RefNode1) break;

		const FXmlNode* RefNode2 = RefNode1->GetNextNode();
			 
		while (RefNode2 && RefNode2->GetTag().Equals("nd"))
		{
			FString Ref1 = RefNode1->GetAttribute("ref");
			FString Ref2 = RefNode2->GetAttribute("ref");
			const FXmlNode* RefNode3 = RefNode2->GetNextNode();
			
			// if the first point is not on the landscape, we go to next point
			if (!PointCoordinates.Contains(Ref1))
			{
				RefNode1 = RefNode2;
				RefNode2 = RefNode3;
			}
			// if the second point is not on the landscape, we skip it
			else if (!PointCoordinates.Contains(Ref2))
			{
				RefNode2 = RefNode3;
			}
			// we can also skip node RefNode2 if there exists a next node (RefNode3) on the landscape (and the precision is low enough)
			else if (RefNode3 && RefNode3->GetTag().Equals("nd") && PointCoordinates.Contains(RefNode3->GetAttribute("ref")) && Precision < 100 && FMath::RandRange(1, 100) > Precision)
			{
				RefNode2 = RefNode3;
			}
			else
			{
				AddPoint(Ref1);
				AddPoint(Ref2);
				if (Points.Contains(Ref1) && Points.Contains(Ref2))
				{
					ULandscapeSplineSegment* Segment = EngineCode::AddSegment(Points[Ref1], Points[Ref2], true, true);
					//Segment->LayerName = "Road";
					RefNode1 = RefNode2;
					RefNode2 = RefNode3;
				}
				else
				{
					UE_LOG(LogLandscapeCombinator, Error, TEXT("Point not found. This case shouldn't happen, but we will continue anyway."));
					RefNode1 = RefNode2;
					RefNode2 = RefNode3;
				}


			}
		}
		Node = Node->GetNextNode();
	}

	double Time2 = FPlatformTime::Seconds();

	UE_LOG(LogLandscapeCombinator, Log, TEXT("Added %d segments in %f seconds."), LandscapeSplinesComponent->GetSegments().Num(), Time2 - Time1);
			
	LandscapeSplinesComponent->AttachToComponent(Landscape->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	LandscapeSplinesComponent->MarkRenderStateDirty();
	LandscapeSplinesComponent->PostEditChange();

	double Time3 = FPlatformTime::Seconds();

	UE_LOG(LogLandscapeCombinator, Log, TEXT("Landscape spline post edit change took %f seconds."), Time3 - Time2);
}

#undef LOCTEXT_NAMESPACE