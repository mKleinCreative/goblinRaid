/*
 * Copyright (c) 2026 Ivan Zhukov. All Rights Reserved.
 * Email: ivzhuk7@gmail.com
 */

#pragma once

#include "ModelingOperators.h"

namespace MetaRoad
{
	/**
	 * TLambdaGenericDataFactory<ResultDataType>
	 *
	 * Adapts a TFunction closure into an IGenericDataOperatorFactory. Intended to be created
	 * with MakeUnique and passed to URoadGraphBackgroundCompute::SetupWithOwnedFactory() (or any
	 * other generic-data compute), which takes ownership and keeps the raw pointer alive for the
	 * compute source.
	 *
	 * Usage:
	 *   auto Factory = MakeUnique<TLambdaGenericDataFactory<FMyData>>(
	 *       [Scope, WProps]() -> TUniquePtr<TGenericDataOperator<FMyData>>
	 *       {
	 *           auto Op = MakeUnique<FMyOp>();
	 *           Op->BaseData = Scope.Pin()->GetTriangulationResult();
	 *           return Op;
	 *       });
	 *   Compute->SetupWithOwnedFactory(Tool, MoveTemp(Factory), Scope);
	 */
	template<typename ResultDataType>
	class TLambdaGenericDataFactory : public UE::Geometry::IGenericDataOperatorFactory<ResultDataType>
	{
		TFunction<TUniquePtr<UE::Geometry::TGenericDataOperator<ResultDataType>>()> Fn;
	public:
		TLambdaGenericDataFactory(TFunction<TUniquePtr<UE::Geometry::TGenericDataOperator<ResultDataType>>()> InFn)
			: Fn(MoveTemp(InFn)) {}
		TUniquePtr<UE::Geometry::TGenericDataOperator<ResultDataType>> MakeNewOperator() override { return Fn(); }
	};
}
