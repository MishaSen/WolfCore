#include "Core/WolfFunctionLibrary.h"


FTransform UWolfFunctionLibrary::GetProjectedTransform(const UAnimInstance* AnimInstance, UAnimMontage* Montage,
	float ScrubPosition, const FTransform& BaseTransform)
{
	if (!Montage || !AnimInstance) return BaseTransform;

	const auto RootMotionDelta = ExtractRootMotionAtTime(Montage, ScrubPosition);
	return RootMotionDelta * BaseTransform;
}

FTransform UWolfFunctionLibrary::ExtractRootMotionAtTime(const UAnimMontage* Montage, float TimePosition)
{
	if (!Montage) return FTransform::Identity;
	return Montage->ExtractRootMotionFromRange(0.f, TimePosition, FAnimExtractContext());
}