// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/UI/ItemSlotWidget.h"

void UItemSlotWidget::UpdateVisual(UTexture2D* Icon, int Quantity)
{
	if (Icon!=nullptr)
	{
		ItemIcon->SetBrushFromTexture(Icon);
	}
	
	if (Quantity <= 0)
	{
		ItemQuantity->SetText(FText::FromString(" "));
	}
	else
	{
		ItemQuantity->SetText(FText::Format(FText::FromString("{0}"), Quantity));
	}
}
