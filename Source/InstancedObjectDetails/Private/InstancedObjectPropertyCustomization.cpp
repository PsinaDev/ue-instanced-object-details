// Copyright PsinaDev. All Rights Reserved.

#include "InstancedObjectPropertyCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailCustomNodeBuilder.h"
#include "PropertyHandle.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameplayTagContainer.h"
#include "UObject/UnrealType.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "InstancedObjectBPFilter"

#pragma region CategoryBuilder

namespace
{
	struct FCategoryNode
	{
		TArray<TSharedRef<IPropertyHandle>> Properties;
		TMap<FString, TSharedPtr<FCategoryNode>> Children;
		TArray<FString> ChildOrder;
	};

	/**
	 * Renders a category and its subtree using IDetailCustomNodeBuilder so that every leaf
	 * is added through IDetailChildrenBuilder::AddProperty. This preserves the underlying
	 * FObjectInstancePropertyNode chain that struct customizations relying on
	 * IPropertyHandle::AccessRawData / GetOuterObjects depend on (e.g. FGameplayTagQuery,
	 * FGameplayTagContainer, FInstancedStruct). Routing those leaves through
	 * IDetailGroup::AddPropertyRow corrupts the chain and AccessRawData returns no data.
	 */
	class FInstancedObjectCategoryBuilder : public IDetailCustomNodeBuilder
	{
	public:
		FInstancedObjectCategoryBuilder(FName InName, FText InDisplayName, TSharedRef<FCategoryNode> InNode)
			: NodeName(InName)
			, DisplayName(MoveTemp(InDisplayName))
			, Node(MoveTemp(InNode))
		{
		}

		virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override {}
		virtual void Tick(float DeltaTime) override {}
		virtual bool RequiresTick() const override { return false; }
		virtual bool InitiallyCollapsed() const override { return false; }
		virtual FName GetName() const override { return NodeName; }

		virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override
		{
			NodeRow
				.NameContent()
				[
					SNew(STextBlock)
					.Text(DisplayName)
					.Font(FAppStyle::Get().GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				];
		}

		virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override
		{
			for (const TSharedRef<IPropertyHandle>& Prop : Node->Properties)
			{
				ChildrenBuilder.AddProperty(Prop);
			}

			for (const FString& ChildName : Node->ChildOrder)
			{
				const TSharedPtr<FCategoryNode> ChildNode = Node->Children.FindRef(ChildName);
				if (!ChildNode.IsValid())
				{
					continue;
				}

				ChildrenBuilder.AddCustomBuilder(MakeShared<FInstancedObjectCategoryBuilder>(
					FName(*ChildName), FText::FromString(ChildName), ChildNode.ToSharedRef()));
			}
		}

	private:
		FName NodeName;
		FText DisplayName;
		TSharedRef<FCategoryNode> Node;
	};
}

#pragma endregion

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

	if (!CastField<FObjectPropertyBase>(Property))
	{
		return false;
	}

	// Re-grouping the Tag Query Editor's transient expression objects by category empties the expression body.
	TArray<UObject*> OuterObjects;
	PropertyHandle.GetOuterObjects(OuterObjects);
	for (const UObject* Outer : OuterObjects)
	{
		if (!Outer)
		{
			continue;
		}

		const UClass* OuterClass = Outer->GetClass();
		if (OuterClass->IsChildOf(UEditableGameplayTagQuery::StaticClass())
			|| OuterClass->IsChildOf(UEditableGameplayTagQueryExpression::StaticClass()))
		{
			return false;
		}
	}

	return true;
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
	TWeakPtr<IPropertyHandle> WeakHandle = PropertyHandle;

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
				.OnClicked_Lambda([WeakHandle]() -> FReply
				{
					TSharedPtr<IPropertyHandle> Handle = WeakHandle.Pin();
					if (!Handle.IsValid())
					{
						return FReply::Handled();
					}

					UObject* Value = nullptr;
					Handle->GetValue(Value);
					if (!Value)
					{
						return FReply::Handled();
					}

					UClass* Class = Value->GetClass();
					UObject* Blueprint = Class ? Class->ClassGeneratedBy : nullptr;
					if (Blueprint)
					{
						FContentBrowserModule& ContentBrowserModule =
							FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
						ContentBrowserModule.Get().SyncBrowserToAssets({ FAssetData(Blueprint) });
					}

					return FReply::Handled();
				})
				.ToolTipText(LOCTEXT("BrowseToBlueprint", "Browse to Blueprint in Content Browser"))
				.Visibility_Lambda([WeakHandle]() -> EVisibility
				{
					TSharedPtr<IPropertyHandle> Handle = WeakHandle.Pin();
					if (!Handle.IsValid())
					{
						return EVisibility::Collapsed;
					}

					UObject* Value = nullptr;
					Handle->GetValue(Value);
					if (!Value)
					{
						return EVisibility::Collapsed;
					}

					UClass* Class = Value->GetClass();
					const bool bIsBlueprintClass =
						Cast<UBlueprintGeneratedClass>(Class) != nullptr
						&& Class->ClassGeneratedBy != nullptr;

					return bIsBlueprintClass ? EVisibility::Visible : EVisibility::Collapsed;
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
	const bool bTemplateContext = ResolveTemplateContext(*PropertyHandle);

	TArray<TSharedRef<IPropertyHandle>> LeafProperties;
	GatherLeafProperties(PropertyHandle, LeafProperties, bTemplateContext);

	const TSharedRef<FCategoryNode> Root = MakeShared<FCategoryNode>();
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

		FCategoryNode* Current = &Root.Get();
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

	// Root-level (uncategorized) leaves first.
	for (const TSharedRef<IPropertyHandle>& Prop : Root->Properties)
	{
		ChildBuilder.AddProperty(Prop);
	}

	// Sub-categories rendered through IDetailCustomNodeBuilder. Going through IDetailGroup
	// would re-route leaves via IDetailGroup::AddPropertyRow, breaking the property-node
	// chain that struct customizations like FGameplayTagQueryCustomization rely on
	// (AccessRawData / GetOuterObjects would return nothing and the value would not display).
	for (const FString& ChildName : Root->ChildOrder)
	{
		const TSharedPtr<FCategoryNode> ChildNode = Root->Children.FindRef(ChildName);
		if (!ChildNode.IsValid())
		{
			continue;
		}

		ChildBuilder.AddCustomBuilder(MakeShared<FInstancedObjectCategoryBuilder>(
			FName(*ChildName), FText::FromString(ChildName), ChildNode.ToSharedRef()));
	}
}

#pragma endregion

#pragma region Helpers

bool FInstancedObjectPropertyCustomization::ShouldShowProperty(const FProperty* Property, bool bTemplateContext)
{
	if (!Property)
	{
		return false;
	}

	if (!Property->HasAnyPropertyFlags(CPF_Edit))
	{
		return false;
	}

	// CPF_DisableEditOnInstance is already filtered by PropertyEditorHelpers::ShouldBeVisible when these child nodes are built; CPF_DisableEditOnTemplate is not.
	if (bTemplateContext && Property->HasAnyPropertyFlags(CPF_DisableEditOnTemplate))
	{
		return false;
	}

	return true;
}

void FInstancedObjectPropertyCustomization::GatherLeafProperties(
	TSharedRef<IPropertyHandle> ParentHandle,
	TArray<TSharedRef<IPropertyHandle>>& OutProperties,
	bool bTemplateContext)
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
			GatherLeafProperties(ChildHandle.ToSharedRef(), OutProperties, bTemplateContext);
			continue;
		}

		if (!ShouldShowProperty(ChildProp, bTemplateContext))
		{
			continue;
		}

		OutProperties.Add(ChildHandle.ToSharedRef());
	}
}

bool FInstancedObjectPropertyCustomization::ResolveTemplateContext(const IPropertyHandle& PropertyHandle)
{
	// IsAsset() is not template context: Enhanced Input declares its modifier settings EditInstanceOnly inside UDataAsset instances.
	UObject* Value = nullptr;
	if (PropertyHandle.GetValue(Value) == FPropertyAccess::Success && Value)
	{
		return Value->IsTemplate(RF_ClassDefaultObject | RF_ArchetypeObject);
	}

	TArray<UObject*> OuterObjects;
	PropertyHandle.GetOuterObjects(OuterObjects);
	for (const UObject* Outer : OuterObjects)
	{
		if (Outer && Outer->IsTemplate(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			return true;
		}
	}

	return false;
}

#pragma endregion

#undef LOCTEXT_NAMESPACE