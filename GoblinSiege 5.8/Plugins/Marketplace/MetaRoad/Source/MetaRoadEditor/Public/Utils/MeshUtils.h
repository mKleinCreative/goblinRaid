/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

using namespace UE::Geometry;

namespace MeshUtils 
{
	METAROADEDITOR_API void AppendMesh(FDynamicMesh3& Dst, FDynamicMesh3& Src);
	METAROADEDITOR_API void EnableDefaultAttributes(FDynamicMesh3& DynamicMesh, bool bNormals, bool bColors, bool bMaterialIDs, bool TriangleGroups, int NumUVs);
	METAROADEDITOR_API TArray<int32> GetGroupTries(const FDynamicMesh3& Mesh, int GroupID);
	METAROADEDITOR_API int FindMeshSections(const FDynamicMesh3& Mesh, const TArray<int32>& TargetTIDs, TArray<int32>& OutSections);
	METAROADEDITOR_API TSet<int> GetAllGroups(const FDynamicMesh3& Mesh);
	METAROADEDITOR_API double GetGroupArea(const FDynamicMesh3& Mesh, int GroupID);
	METAROADEDITOR_API TSet<int> GetGroupNeighbours(const FDynamicMesh3& Mesh, int GroupID, const TArray<int32>& TIDs);
	METAROADEDITOR_API int FindMinAreaGroupNeighbour(const FDynamicMesh3& Mesh, int GroupID, const TArray<int32>& TIDs);
	METAROADEDITOR_API void ChangeGroup(FDynamicMesh3& Mesh, int OldGroupId, int NewGroupID);
	METAROADEDITOR_API void MergeGroupByArea(FDynamicMesh3& Mesh, double MergeSectionsAreaThreshold);
	METAROADEDITOR_API int SplitMeshGroupsBySections(UE::Geometry::FDynamicMesh3& Mesh);



} // MeshUtils
