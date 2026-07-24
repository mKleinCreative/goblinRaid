/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/IndexedCurve.h"
#include "Serialization/Archive.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"
#include "RoadLaneAttribute.generated.h"

namespace MetaRoad
{
	/** Maps a lateral alpha [0,1] across the whole road width, piecewise through R = 0:
	 *  Alpha=0 -> LeftR (left road edge), 0.5 -> 0 (reference line), 1 -> RightR (right road edge).
	 *  Single source of truth for the centre-lane anchor mapping used by GetCenterLaneROffset()
	 *  overrides and by the spline-mesh generator. */
	METAROAD_API double MapCenterLaneROffset(double Alpha, double LeftR, double RightR);
}

/**
 * Polymorphic base for all lane attribute values. Subclass this struct to define custom
 * per-lane metadata (e.g. speed limit, road mark profile, landscape parameters).
 *
 * Three virtual methods control how FRoadLaneAttribute evaluates a value at a given SOffset:
 *
 *  CanInterpolate() — evaluation mode:
 *    false (default): stepped curve — Evaluate(S) returns the value of the last key
 *      whose SOffset <= S. Use for discrete data (e.g. speed limits, mark profiles).
 *    true: interpolated curve — Evaluate(S) calls Interpolate() to blend between the
 *      two keys that surround S. Use for continuously varying data (e.g. landscape params).
 *
 *  Interpolate() — only called when CanInterpolate() == true. Blends `this` (earlier key)
 *    with Other (later key). Alpha is in [0,1]: 0 = at this key's SOffset, 1 = at Other's.
 *    Write the result into Out and return true. Return false to fall back to stepped.
 *    See FRoadLaneAttributeValueLandscape for a cubic-interpolation example.
 *
 *  GetKeyAlpha() — lateral position [0,1] within the lane width used when rendering
 *    this attribute in the editor and when computing the R-offset for BuildLinearApproximation():
 *      0.0 = inner lane edge (nearest to road center)
 *      0.5 = lane center (default)
 *      1.0 = outer lane edge
 *    FRoadLaneMark overrides this to 1.0 so markings are placed at the outer edge.
 */
USTRUCT()
struct METAROAD_API FRoadLaneAttributeValue
{
	GENERATED_USTRUCT_BODY()
public:

	virtual ~FRoadLaneAttributeValue() {}

	/** See class comment. Default: false (stepped evaluation). */
	virtual bool CanInterpolate() const { return false; }

	/** See class comment. Called only when CanInterpolate() == true.
	 *  Alpha [0,1]: 0 = at this key, 1 = at Other. Write result to Out, return true on success. */
	virtual bool Interpolate(const UScriptStruct* ValueType, const FRoadLaneAttributeValue* Other, float Alpha, FRoadLaneAttributeValue* Out) const { return false; }

	/** Returns true if this value type supports lateral (Alpha) editing via R-axis drag.
	 *  When true, SetAlphaFromROffset is called during HandleInputDelta. */
	virtual bool SupportsAlphaEditing() const { return false; }

	/** Lateral position [0,1] within the lane width used for rendering and R-offset computation.
	 *  0.0 = inner edge, 0.5 = center (default), 1.0 = outer edge. */
	virtual double GetKeyAlpha() const { return 0.5; }

	/** Directly set the lateral key alpha [0,1] (used by the numeric overlay editor).
	 *  Only meaningful when SupportsAlphaEditing() == true. Returns true if modified. */
	virtual bool SetKeyAlpha(double InAlpha) { return false; }

	/** R-offset of the anchor point for center lane (ZeroLaneIndex) drawing.
	 *  LeftR = leftmost road boundary (negative). RightR = rightmost road boundary (positive).
	 *  Default: 0.0 (road reference line). Types that place geometry across the road on the centre
	 *  lane (Curve in Fixed mode, Polygon) override this via MetaRoad::MapCenterLaneROffset(). */
	virtual double GetCenterLaneROffset(double LeftR, double RightR) const;

	/** Called during lateral drag in FRoadAttributeComponentVisualizer::HandleInputDelta.
	 *  AnchorR = road-local R-offset of the dragged world position (after subtracting EvalROffset).
	 *  LaneInnerR/LaneOuterR are lane boundaries at the drag S position:
	 *    - ZeroLaneIndex: LaneInnerR = road LeftR (negative), LaneOuterR = road RightR (positive)
	 *    - Other lanes:   LaneInnerR = EvalLaneROffset(idx, S, 0.0), LaneOuterR = EvalLaneROffset(idx, S, 1.0)
	 *  Returns true if the value was modified. */
	virtual bool SetAlphaFromROffset(double AnchorR, double LaneInnerR, double LaneOuterR, bool bIsZeroLane);
};


/**
 * A single sample point in a FRoadLaneAttribute.
 * Keys are stored in FRoadLaneAttribute::Keys[] sorted by ascending SOffset.
 *
 * SOffset marks the arc-length position along the spline from which this key's
 * value becomes effective (stepped mode) or starts being blended (interpolated mode).
 * Value holds the concrete attribute data; its type must match FRoadLaneAttribute::ScriptStruct.
 */
USTRUCT(BlueprintType, Blueprintable)
struct METAROAD_API FRoadLaneAttributeKey
{
	GENERATED_USTRUCT_BODY()
public:

	FRoadLaneAttributeKey() = default;
	FRoadLaneAttributeKey(double InSOffset) : SOffset(InSOffset) {}
	virtual ~FRoadLaneAttributeKey() {}

	/** Arc-length distance from spline start at which this key takes effect. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = LaneAttribute)
	double SOffset = 0.0;

	template<typename AttributeType>
	const AttributeType& GetValue() const
	{
		return Value.Get<AttributeType>();
	}

	template<typename AttributeType>
	const AttributeType* GetValuePtr() const
	{
		return Value.GetPtr<AttributeType>();
	}

	template<typename AttributeType>
	AttributeType& GetValue()
	{
		return Value.GetMutable<AttributeType>();
	}

	template<typename AttributeType>
	AttributeType* GetValuePtr()
	{
		return Value.GetMutablePtr<AttributeType>();
	}


	bool operator < (const FRoadLaneAttributeKey& Other) const
	{
		return SOffset < Other.SOffset;
	}


//protected:
	/** The attribute value at this key. Must be an instance of FRoadLaneAttribute::ScriptStruct.
	 *  In stepped mode this value is returned as-is; in interpolated mode it is blended with
	 *  the next key's value by FRoadLaneAttributeValue::Interpolate(). */
	UPROPERTY(EditAnywhere, Category = LaneAttribute)
	TInstancedStruct<FRoadLaneAttributeValue> Value;

	//friend struct FRoadLaneAttribute;
};

/**
 * A typed, SOffset-keyed container for arbitrary per-lane metadata.
 * Behaves like a stepped curve: Evaluate(SOffset) returns the value of the last key at or
 * before the requested offset. If the value type overrides CanInterpolate(), the result is
 * blended between the surrounding keys instead.
 *
 * All keys must share the same UScriptStruct type (set via SetScriptStruct or the constructor).
 * See https://unrealdrive.readthedocs.io/en/latest/RoadModel.html#lane-attributes
 */
USTRUCT(BlueprintType)
struct METAROAD_API FRoadLaneAttribute 
{
	GENERATED_USTRUCT_BODY()

public:
	FRoadLaneAttribute() 
		: ScriptStruct(nullptr)
	{}
	FRoadLaneAttribute(UScriptStruct* InScriptStruct) 
		: ScriptStruct(InScriptStruct)
	{}

	FRoadLaneAttribute(const FRoadLaneAttribute& OtherCurve);

	/** Virtual destructor. */
	virtual ~FRoadLaneAttribute() { }

	
	/** Sets the underlying type for the curve, only possible when not containing any keys (see ::Reset) */
	void SetScriptStruct(const UScriptStruct* InScriptStruct);
	const UScriptStruct* GetScriptStruct() const { return ScriptStruct; }

	template<class T>
	bool IsChildOf() const
	{
		return IsValid(ScriptStruct) && ScriptStruct->IsChildOf<T>();
	}

	/** Returns true when ScriptStruct is set AND at least one key exists. */
	bool CanEvaluate() const;

	/** Evaluate the curve keys into a temporary value container */
	template<typename AttributeType>
	AttributeType Evaluate(double SOffset, const AttributeType& Default = {}) const
	{
		AttributeType EvaluatedValue = Default;
		EvaluateToPtr(AttributeType::StaticStruct(), SOffset, &EvaluatedValue);
		return EvaluatedValue;
	}

	/** Evaluate the curve keys into a temporary value container */
	void Evaluate(double SOffset, TInstancedStruct<FRoadLaneAttributeValue>& Target) const
	{
		Target.InitializeAsScriptStruct(ScriptStruct, nullptr);
		EvaluateToPtr(ScriptStruct, SOffset, Target.GetMutablePtr<FRoadLaneAttributeValue>());
	}

	/** Check whether this curve has any data or not */
	bool HasAnyData() const;

	/** Removes all key data */
	void Reset();

	/** Const iterator for the keys, so the indices and handles stay valid */
	TArray<FRoadLaneAttributeKey>::TConstIterator GetKeyIterator() const;

	/** Add a new typed key to the curve with the supplied SOffset and Value. */
	template<typename AttributeType>
	int AddTypedKey(double InSOffset, const AttributeType& InValue)
	{
		check(AttributeType::StaticStruct() == ScriptStruct); 
		return AddKey(InSOffset, &InValue);
	}

	/** Finds the key at InSOffset, and updates its typed value. If it can't find the key within the KeySOffsetTolerance, it adds one at that SOffset */
	template<typename AttributeType>
	int UpdateOrAddTypedKey(double InSOffset, const AttributeType& InValue, double KeySOffsetTolerance = UE_KINDA_SMALL_NUMBER)
	{
		check(AttributeType::StaticStruct() == ScriptStruct);
		return UpdateOrAddKey(InSOffset, &InValue, KeySOffsetTolerance);
	}

	/** Finds the key at InSOffset, and updates its typed value. If it can't find the key within the KeySOffsetTolerance, it adds one at that SOffset */
	int UpdateOrAddTypedKey(double InSOffset, const void* InValue, const UScriptStruct* ValueType, double KeySOffsetTolerance = UE_KINDA_SMALL_NUMBER)
	{
		check(ValueType == ScriptStruct);
		return UpdateOrAddKey(InSOffset, InValue, KeySOffsetTolerance);
	}
			

	/** Finds the key at KeySOffset and returns its handle. If it can't find the key within the KeySOffsetTolerance, it will return an invalid handle */
	int FindKey(double KeySOffset, double KeySOffsetTolerance = UE_KINDA_SMALL_NUMBER) const;

	/** Gets the handle for the last key which is at or before the SOffset requested.  If there are no keys at or before the requested SOffset, an invalid handle is returned. */
	int FindKeyBeforeOrAt(double KeySOffset) const;

	/** Removes keys whose removal does not change Evaluate() output at any SOffset (zero-tolerance). */
	void RemoveRedundantKeys();

	void Trim(double S0, double S1);

public:
	/** The keys, ordered by SOffset */
	UPROPERTY(EditAnywhere, Category = "Custom Attributes")
	TArray<FRoadLaneAttributeKey> Keys;

protected:
	/** Evaluate the curve keys into the provided memory (should be appropriatedly sized) */
	void EvaluateToPtr(const UScriptStruct* InScriptStruct, double SOffset, FRoadLaneAttributeValue* InOutDataPtr) const;

	/** Finds the key at InSOffset, and updates its typed value. If it can't find the key within the KeySOffsetTolerance, it adds one at that SOffset */
	int UpdateOrAddKey(double InSOffset, const void* InStructMemory, double KeySOffsetTolerance = UE_KINDA_SMALL_NUMBER);

	/** Add a new raw memory key (should be appropriately sized) to the curve with the supplied SOffset and Value. */
	int AddKey(double InSOffset, const void* InStructMemory);

protected:

	/** The UScriptStruct describing the value type for all keys in this attribute.
	 *  Must be set before adding keys; cannot be changed while keys exist (call Reset() first). */
	UPROPERTY(EditAnywhere, Category = "Custom Attributes")
	TObjectPtr<const UScriptStruct> ScriptStruct;

};