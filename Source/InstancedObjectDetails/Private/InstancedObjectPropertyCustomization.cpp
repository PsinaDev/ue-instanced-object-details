// Copyright PsinaDev. All Rights Reserved.

#include "InstancedObjectPropertyCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/UnrealType.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "InstancedObjectBPFilter"

#pragma region Identifier

bool FInstancedObjectPropertyIdentifier::IsPropertyTypeCustomized(const IPropertyHandle& PropertyHandle) const
{
	const FProperty* Property = PropertyHandle.GetProperty();
	if (!Property)
	{
		return false;
	}

	if (!Property->HasAllPropertyFlags(CPF_InstancedReference))
	{
		return false;
	}

	return CastField<FObjectPropertyBase>(Property) != nullptr;
}

#pragma endregion

#pragma region Customization

TSharedRef<IPropertyTypeCustomization> FInstancedObjectPropertyCustomization::MakeInstance()
{
	return MakeShared<FInstancedObjectPropertyCustomization>();
}

void FInstancedObjectPropertyCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	CachedPropertyHandle = PropertyHandle;

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				PropertyHandle->CreatePropertyValueWidget(false)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked_Lambda([this]() -> FReply
				{
					BrowseToBlueprint();
					return FReply::Handled();
				})
				.ToolTipText(LOCTEXT("BrowseToBlueprint", "Browse to Blueprint in Content Browser"))
				.Visibility_Lambda([this]() -> EVisibility
				{
					return IsCurrentValueBlueprintClass()
						? EVisibility::Visible
						: EVisibility::Collapsed;
				})
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.BrowseContent"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		];
}

void FInstancedObjectPropertyCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TArray<TSharedRef<IPropertyHandle>> LeafProperties;
	GatherLeafProperties(PropertyHandle, LeafProperties);

	// Build category tree from |-delimited paths.
	struct FCategoryNode
	{
		TArray<TSharedRef<IPropertyHandle>> Properties;
		TMap<FString, TSharedPtr<FCategoryNode>> Children;
		TArray<FString> ChildOrder;
	};

	TSharedPtr<FCategoryNode> Root = MakeShared<FCategoryNode>();
	bool bHasMultipleCategories = false;

	for (const TSharedRef<IPropertyHandle>& Child : LeafProperties)
	{
		FString CategoryPath;
		if (const FProperty* Prop = Child->GetProperty())
		{
			CategoryPath = Prop->GetMetaData(TEXT("Category"));
		}

		if (CategoryPath.IsEmpty())
		{
			Root->Properties.Add(Child);
			continue;
		}

		bHasMultipleCategories = true;

		// Walk |-delimited segments to find or create the target node.
		TArray<FString> Segments;
		CategoryPath.ParseIntoArray(Segments, TEXT("|"), true);

		FCategoryNode* Current = Root.Get();
		for (const FString& Segment : Segments)
		{
			TSharedPtr<FCategoryNode>& ChildNode = Current->Children.FindOrAdd(Segment);
			if (!ChildNode.IsValid())
			{
				ChildNode = MakeShared<FCategoryNode>();
				Current->ChildOrder.Add(Segment);
			}
			Current = ChildNode.Get();
		}

		Current->Properties.Add(Child);
	}

	// No categories at all: flat output.
	if (!bHasMultipleCategories && Root->Children.Num() == 0)
	{
		for (const TSharedRef<IPropertyHandle>& Child : LeafProperties)
		{
			ChildBuilder.AddProperty(Child);
		}
		return;
	}

	// Recursive lambda to emit groups.
	TFunction<void(FCategoryNode*, IDetailChildrenBuilder*, IDetailGroup*)> EmitNode;
	EmitNode = [&EmitNode](FCategoryNode* Node, IDetailChildrenBuilder* Builder, IDetailGroup* ParentGroup)
	{
		// Properties at this level.
		for (const TSharedRef<IPropertyHandle>& Prop : Node->Properties)
		{
			if (ParentGroup)
			{
				ParentGroup->AddPropertyRow(Prop);
			}
			else
			{
				Builder->AddProperty(Prop);
			}
		}

		// Sub-categories.
		for (const FString& ChildName : Node->ChildOrder)
		{
			FCategoryNode* ChildNode = Node->Children[ChildName].Get();
			const FName GroupName(*ChildName);
			const FText DisplayName = FText::FromString(ChildName);

			IDetailGroup* NewGroup;
			if (ParentGroup)
			{
				NewGroup = &ParentGroup->AddGroup(GroupName, DisplayName);
			}
			else
			{
				NewGroup = &Builder->AddGroup(GroupName, DisplayName);
			}

			EmitNode(ChildNode, Builder, NewGroup);
		}
	};

	EmitNode(Root.Get(), &ChildBuilder, nullptr);
}

#pragma endregion

#pragma region Helpers

bool FInstancedObjectPropertyCustomization::IsCurrentValueBlueprintClass() const
{
	if (!CachedPropertyHandle.IsValid())
	{
		return false;
	}

	UObject* CurrentValue = nullptr;
	CachedPropertyHandle->GetValue(CurrentValue);

	if (!CurrentValue)
	{
		return false;
	}

	UClass* Class = CurrentValue->GetClass();
	return Cast<UBlueprintGeneratedClass>(Class) != nullptr
		&& Class->ClassGeneratedBy != nullptr;
}

void FInstancedObjectPropertyCustomization::BrowseToBlueprint() const
{
	if (!CachedPropertyHandle.IsValid())
	{
		return;
	}

	UObject* CurrentValue = nullptr;
	CachedPropertyHandle->GetValue(CurrentValue);
	if (!CurrentValue)
	{
		return;
	}

	UClass* Class = CurrentValue->GetClass();
	UObject* Blueprint = Class ? Class->ClassGeneratedBy : nullptr;
	if (!Blueprint)
	{
		return;
	}

	TArray<FAssetData> Assets;
	Assets.Add(FAssetData(Blueprint));

	FContentBrowserModule& ContentBrowserModule =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().SyncBrowserToAssets(Assets);
}

bool FInstancedObjectPropertyCustomization::ShouldShowProperty(const FProperty* Property)
{
	if (!Property)
	{
		return false;
	}

	// Must be editable to appear in Details panel.
	if (!Property->HasAllPropertyFlags(CPF_Edit))
	{
		return false;
	}

	// Instanced objects are instances by definition — skip DisableEditOnInstance properties.
	if (Property->HasAllPropertyFlags(CPF_DisableEditOnInstance))
	{
		return false;
	}

	return true;
}

void FInstancedObjectPropertyCustomization::GatherLeafProperties(
	TSharedRef<IPropertyHandle> ParentHandle,
	TArray<TSharedRef<IPropertyHandle>>& OutProperties)
{
	const FProperty* ParentProp = ParentHandle->GetProperty();

	uint32 NumChildren = 0;
	ParentHandle->GetNumChildren(NumChildren);

	for (uint32 Index = 0; Index < NumChildren; ++Index)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = ParentHandle->GetChildHandle(Index);
		if (!ChildHandle.IsValid() || !ChildHandle->IsValidHandle())
		{
			continue;
		}

		const FProperty* ChildProp = ChildHandle->GetProperty();

		// Intermediate node: no property (category), or same FProperty as parent (object node).
		const bool bIsIntermediateNode =
			!ChildProp ||
			(ParentProp && ChildProp == ParentProp);

		if (bIsIntermediateNode)
		{
			GatherLeafProperties(ChildHandle.ToSharedRef(), OutProperties);
			continue;
		}

		// Filter by editability: CPF_Edit required, CPF_DisableEditOnInstance excluded.
		if (!ShouldShowProperty(ChildProp))
		{
			continue;
		}

		OutProperties.Add(ChildHandle.ToSharedRef());
	}
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
