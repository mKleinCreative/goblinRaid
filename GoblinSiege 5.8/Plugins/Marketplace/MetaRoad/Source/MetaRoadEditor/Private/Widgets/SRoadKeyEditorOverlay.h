/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SNumericEntryBox.h"

class SLevelViewport;

namespace RoadKeyOverlay
{
	/** Build a single labeled numeric row (STextBlock + SNumericEntryBox<double>) for the overlay.
	 *  Each visualizer composes its rows from this helper; the overlay container stays type-agnostic. */
	TSharedRef<SWidget> MakeNumericRow(
		FText Label,
		TAttribute<TOptional<double>> Get,
		SNumericEntryBox<double>::FOnValueCommitted OnCommitted,
		TAttribute<bool> IsEnabled = true,
		TOptional<double> MinValue = TOptional<double>(),
		TOptional<double> MaxValue = TOptional<double>());
}

/**
 * Reusable floating overlay container for the road component visualizers.
 *
 * Content-agnostic: it only provides the draggable frame (built on the engine
 * SDraggableBoxOverlay, same style as STransformGizmoNumericalUIOverlay) and an
 * optional header. The concrete editable fields (of any widget type) are built by
 * each visualizer and passed in through the default content slot.
 */
class SRoadKeyEditorOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRoadKeyEditorOverlay) {}
		/** Optional header text drawn above the content (may be bound for dynamic headers). */
		SLATE_ATTRIBUTE(FText, HeaderText)
		/** Visibility of the whole overlay. The owning visualizer binds this to a lambda
		 *  returning SelfHitTestInvisible when a key is selected, Collapsed otherwise. */
		SLATE_ATTRIBUTE(EVisibility, OverlayVisibility)
		/** The editable fields, built individually by each visualizer. */
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** Draggable box (engine widget that exposes the drag-complete event). */
	TSharedPtr<class SDraggableBox> DraggableBox;
	/** Full-viewport container the draggable box is positioned within. */
	TSharedPtr<class SVerticalBox> ContainingBox;

	/** Current box position (distance from right / from bottom edge), driving the slot padding.
	 *  Anchored to the bottom-right corner. */
	float PaddingRight = 0.f;
	float PaddingBottom = 0.f;

	/** Persisted position, shared across all overlay instances so it survives widget recreation
	 *  (e.g. switching between visualizers). Updated on every drag. Defaults to a 16px bottom-right inset. */
	static float SavedPaddingRight;
	static float SavedPaddingBottom;
};

/**
 * Lifecycle helper that owns one SRoadKeyEditorOverlay and manages adding/removing it
 * to the active level viewport overlay. Held by each visualizer; content-agnostic.
 */
class FRoadKeyOverlayController
{
public:
	~FRoadKeyOverlayController();

	/** Build the overlay widget once with the given content, header and visibility binding. */
	void Setup(TSharedRef<SWidget> Content, TAttribute<FText> Header, TAttribute<EVisibility> OverlayVisibility);

	/** Add the overlay to the active level viewport (idempotent; re-adds it on viewport change).
	 *  Safe to call every frame from DrawVisualization. */
	void EnsureAddedToActiveViewport();

	/** Remove the overlay from its viewport (the widget itself is kept for reuse). Idempotent. */
	void Shutdown();

private:
	TSharedPtr<SRoadKeyEditorOverlay> Overlay;
	TWeakPtr<SLevelViewport> AddedViewport;
};
