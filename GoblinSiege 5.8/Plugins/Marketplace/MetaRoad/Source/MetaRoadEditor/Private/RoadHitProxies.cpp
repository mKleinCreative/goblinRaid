/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#include "RoadHitProxies.h"

// Base hierarchy
IMPLEMENT_HIT_PROXY(HRoadSplineVisProxy, HComponentVisProxy);
IMPLEMENT_HIT_PROXY(HRoadSectionVisProxy, HRoadSplineVisProxy);
IMPLEMENT_HIT_PROXY(HRoadLaneVisProxy, HRoadSectionVisProxy);

// Looped fill area
IMPLEMENT_HIT_PROXY(HRoadLoopVisProxy, HRoadSplineVisProxy);

// Spline Mode
IMPLEMENT_HIT_PROXY(HRoadSplineKeyProxy, HRoadSplineVisProxy);
IMPLEMENT_HIT_PROXY(HRoadSplineSegmentProxy, HRoadSplineVisProxy);
IMPLEMENT_HIT_PROXY(HRoadSplineTangentHandleProxy, HRoadSplineVisProxy);

// Section Mode
IMPLEMENT_HIT_PROXY(HRoadSectionKeyVisProxy, HRoadLaneVisProxy);

// Offset Mode
IMPLEMENT_HIT_PROXY(HRoadOffsetLineVisProxy, HRoadSplineVisProxy);
IMPLEMENT_HIT_PROXY(HRoadOffsetKeyVisProxy, HRoadSplineVisProxy);
IMPLEMENT_HIT_PROXY(HRoadOffsetTangentVisProxy, HRoadOffsetKeyVisProxy);

// Width Mode
IMPLEMENT_HIT_PROXY(HRoadLaneWidthSegmentVisProxy, HRoadLaneVisProxy);
IMPLEMENT_HIT_PROXY(HRoadLaneWidthKeyVisProxy, HRoadLaneWidthSegmentVisProxy);
IMPLEMENT_HIT_PROXY(HRoadLaneWidthTangentVisProxy, HRoadLaneWidthKeyVisProxy);

// Attribute Mode
IMPLEMENT_HIT_PROXY(HRoadLaneAttributeVisProxy, HRoadLaneVisProxy);
IMPLEMENT_HIT_PROXY(HRoadLaneAttributeSegmentVisProxy, HRoadLaneAttributeVisProxy);
IMPLEMENT_HIT_PROXY(HRoadLaneAttributeKeyVisProxy, HRoadLaneAttributeSegmentVisProxy);

// Lane connection handle
IMPLEMENT_HIT_PROXY(HRoadLaneConnectionProxy, HRoadLaneVisProxy);
