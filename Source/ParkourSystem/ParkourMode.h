#pragma once

UENUM(BlueprintType)
enum class EParkourMode : uint8
{
	EPM_None UMETA(DisplayName = "None"),
	EPM_Sprint UMETA(DisplayName = "Sprint"),
	EPM_Crouch UMETA(DisplayName = "Crouch"),
	EPM_Slide UMETA(DisplayName = "Slide")
};
