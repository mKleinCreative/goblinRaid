// One line of the objective board: a type picture, a state marker, a name and a qualifier.
//
// Michael, 2026-08-26: "let's work on objective row icons." The art for these arrived with the UI
// kit (#310) and could not be used, because the whole list was a single TextBlock printing
// "[x] Houses 12 / 27" - there was nowhere for a picture to go.
//
// THIS WIDGET DECIDES NOTHING. Every judgement - which type collapses, what the denominator is,
// whether a row counts as in-progress - has already been made in UGSPlayerHUDWidget::BuildDisplayRows
// and arrives settled in an FGSObjectiveDisplayRow. A row that re-derived any of that would be a
// second place the rules live, and the two would drift.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Raid/GSRaidTypes.h"
#include "GSObjectiveRowWidget.generated.h"

UCLASS()
class GOBLINSIEGE_API UGSObjectiveRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Fill this row in. Textures are passed in rather than looked up, because the mapping from
	 * gameplay tag to picture is data on the HUD widget - one place to edit when a sixth burn type
	 * arrives, and the row stays ignorant of what a "house" is.
	 *
	 * Either texture may be null; the matching image just collapses, and the row still reads.
	 */
	void SetRow(const FGSObjectiveDisplayRow& Row, UTexture2D* TypeTexture, UTexture2D* StateTexture);

protected:
	/** The house / field / market / mill / statue picture. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UImage> TypeIcon;

	/** Empty recess, live flame, or charred X. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UImage> StateIcon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> LabelText;

	/** "12 / 27", "40%", "(bonus)". Collapses when the row has no qualifier, so the name does not
	 *  sit next to an empty gap. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> DetailText;

	/** Completed rows dim, so the eye lands on what is still to do rather than what is done. */
	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Row")
	FLinearColor DoneTint = FLinearColor(0.55f, 0.52f, 0.46f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "GoblinSiege|HUD|Row")
	FLinearColor ActiveTint = FLinearColor(1.f, 0.94f, 0.82f, 1.f);
};
