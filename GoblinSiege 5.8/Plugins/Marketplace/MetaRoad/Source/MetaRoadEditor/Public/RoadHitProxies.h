/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "ComponentVisualizer.h"   // HComponentVisProxy, HHitProxy, DECLARE_HIT_PROXY, EMouseCursor
#include "RoadSplineComponent.h"    // URoadSplineComponent + MetaRoadTypes (ULaneConnection, URoadLaneAttributeDescriptor, MetaRoad::ZeroLaneIndex)

/**
 * All viewport hit proxies for the URoadSplineComponent editing hierarchy live here so the whole
 * picking taxonomy is visible in one place. Root: HComponentVisProxy -> HRoadSplineVisProxy ->
 * HRoadSectionVisProxy -> HRoadLaneVisProxy, with per-editor leaves below.
 * IMPLEMENT_HIT_PROXY for every type is in RoadHitProxies.cpp.
 *
 * Tool-local proxies (DrawRoadTool / IntersectionDrawTool) and the crosswalk proxy are intentionally
 * NOT here — they derive from HHitProxy/HComponentVisProxy directly and are private to their .cpp.
 */

// ─── Base hierarchy (spline / section / lane) ────────────────────────────────

struct METAROADEDITOR_API HRoadSplineVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadSplineVisProxy(const URoadSplineComponent* InComponent, EHitProxyPriority InPriority = HPP_Wireframe)
		: HComponentVisProxy(InComponent, InPriority)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};

struct METAROADEDITOR_API HRoadSectionVisProxy : public HRoadSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadSectionVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadSplineVisProxy(InComponent, InPriority)
		, SectionIndex(InSectionIndex)
	{}

	int SectionIndex;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

struct METAROADEDITOR_API HRoadLaneVisProxy : public HRoadSectionVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, int InLaneIndex, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadSectionVisProxy(InComponent, InSectionIndex, InPriority)
		, LaneIndex(InLaneIndex)
	{}

	int LaneIndex;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return LaneIndex == MetaRoad::ZeroLaneIndex ?  EMouseCursor::CardinalCross : EMouseCursor::Crosshairs;
	}
};

// ─── Looped fill area of a closed spline ─────────────────────────────────────

struct METAROADEDITOR_API HRoadLoopVisProxy : public HRoadSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLoopVisProxy(const URoadSplineComponent* InComponent, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadSplineVisProxy(InComponent, InPriority)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}
};

// ─── Spline Mode (FRoadSplineComponentVisualizer) ────────────────────────────

struct HRoadSplineKeyProxy : public HRoadSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadSplineKeyProxy(const URoadSplineComponent* InComponent, int32 InKeyIndex, EHitProxyPriority InPriority = HPP_Foreground)
		: HRoadSplineVisProxy(InComponent, InPriority)
		, KeyIndex(InKeyIndex)
	{
	}

	int32 KeyIndex;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

struct HRoadSplineSegmentProxy : public HRoadSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadSplineSegmentProxy(const URoadSplineComponent* InComponent, int32 InSegmentIndex, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadSplineVisProxy(InComponent, InPriority)
		, SegmentIndex(InSegmentIndex)
	{
	}

	int32 SegmentIndex;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

struct HRoadSplineTangentHandleProxy : public HRoadSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadSplineTangentHandleProxy(const URoadSplineComponent* InComponent, int32 InKeyIndex, bool bInArriveTangent, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadSplineVisProxy(InComponent, InPriority)
		, KeyIndex(InKeyIndex)
		, bArriveTangent(bInArriveTangent)
	{
	}

	int32 KeyIndex;
	bool bArriveTangent;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

// ─── Section Mode (FRoadSectionComponentVisualizer) ──────────────────────────

struct HRoadSectionKeyVisProxy : public HRoadLaneVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadSectionKeyVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, EHitProxyPriority InPriority = HPP_Foreground)
		: HRoadLaneVisProxy(InComponent, InSectionIndex, MetaRoad::ZeroLaneIndex, InPriority)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

// ─── Offset Mode (FRoadOffsetComponentVisualizer) ────────────────────────────

struct HRoadOffsetLineVisProxy : public HRoadSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadOffsetLineVisProxy(const URoadSplineComponent* InComponent, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadSplineVisProxy(InComponent, InPriority)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

struct HRoadOffsetKeyVisProxy : public HRoadSplineVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadOffsetKeyVisProxy(const URoadSplineComponent* InComponent, int OffsetKey, EHitProxyPriority InPriority = HPP_Foreground)
		: HRoadSplineVisProxy(InComponent, InPriority)
		, OffsetKey(OffsetKey)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}

	int OffsetKey;
};

struct HRoadOffsetTangentVisProxy : public HRoadOffsetKeyVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadOffsetTangentVisProxy(const URoadSplineComponent* InComponent, int OffsetKey, bool bInArriveTangent, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadOffsetKeyVisProxy(InComponent, OffsetKey, InPriority)
		, bArriveTangent(bInArriveTangent)
	{
	}

	bool bArriveTangent;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

// ─── Width Mode (FRoadWidthComponentVisualizer) ──────────────────────────────

struct HRoadLaneWidthSegmentVisProxy : public HRoadLaneVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneWidthSegmentVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, int InLaneIndex, int InWidthIndex, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadLaneVisProxy(InComponent, InSectionIndex, InLaneIndex, InPriority)
		, WidthIndex(InWidthIndex)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}

	int WidthIndex;
};

struct HRoadLaneWidthKeyVisProxy : public HRoadLaneWidthSegmentVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneWidthKeyVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, int InLaneIndex, int InWidthIndex, EHitProxyPriority InPriority = HPP_Foreground)
		: HRoadLaneWidthSegmentVisProxy(InComponent, InSectionIndex, InLaneIndex, InWidthIndex, InPriority)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

struct HRoadLaneWidthTangentVisProxy : public HRoadLaneWidthKeyVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneWidthTangentVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, int InLaneIndex, int InWidthIndex, bool bInArriveTangent, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadLaneWidthKeyVisProxy(InComponent, InSectionIndex, InLaneIndex, InWidthIndex, InPriority)
		, bArriveTangent(bInArriveTangent)
	{
	}

	bool bArriveTangent;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

// ─── Attribute Mode (FRoadAttributeComponentVisualizer) ──────────────────────

struct HRoadLaneAttributeVisProxy : public HRoadLaneVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneAttributeVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, int InLaneIndex, const TSubclassOf<URoadLaneAttributeDescriptor>& InAttributeDescriptor, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadLaneVisProxy(InComponent, InSectionIndex, InLaneIndex, InPriority)
		, AttributeDescriptor(InAttributeDescriptor)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
	const TSubclassOf<URoadLaneAttributeDescriptor> AttributeDescriptor;
};

struct HRoadLaneAttributeSegmentVisProxy : public HRoadLaneAttributeVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneAttributeSegmentVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, int InLaneIndex, const TSubclassOf<URoadLaneAttributeDescriptor>& InAttributeDescriptor, int InAttributeIndex, EHitProxyPriority InPriority = HPP_Wireframe)
		: HRoadLaneAttributeVisProxy(InComponent, InSectionIndex, InLaneIndex, InAttributeDescriptor, InPriority)
		, AttributeIndex(InAttributeIndex)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}

	int AttributeIndex;
};

struct HRoadLaneAttributeKeyVisProxy : public HRoadLaneAttributeSegmentVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneAttributeKeyVisProxy(const URoadSplineComponent* InComponent, int InSectionIndex, int InLaneIndex, const TSubclassOf<URoadLaneAttributeDescriptor>& InAttributeDescriptor, int InAttributeIndex, EHitProxyPriority InPriority = HPP_Foreground)
		: HRoadLaneAttributeSegmentVisProxy(InComponent, InSectionIndex, InLaneIndex, InAttributeDescriptor, InAttributeIndex, InPriority)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

// ─── Lane connection handle (RoadUtils) ──────────────────────────────────────

struct HRoadLaneConnectionProxy : public HRoadLaneVisProxy
{
	DECLARE_HIT_PROXY();

	HRoadLaneConnectionProxy(ULaneConnection* InConnection, int InSectionIndex, int InLaneIndex, EHitProxyPriority InPriority = HPP_UI)
		: HRoadLaneVisProxy(InConnection->GetOwnedRoadSplineChecked(), InSectionIndex, InLaneIndex, InPriority)
		, Connection(InConnection)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Hand;
	}

	TWeakObjectPtr<ULaneConnection> Connection;
};
