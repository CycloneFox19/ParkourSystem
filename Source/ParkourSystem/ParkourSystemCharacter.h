// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "ParkourMode.h"
#include "ParkourSystemCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class AParkourSystemCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	USkeletalMeshComponent* Mesh1P;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* MoveAction;
	
public:
	AParkourSystemCharacter();

protected:
	virtual void BeginPlay();

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

public:
	virtual void Landed(const FHitResult& Hit) override;

public:
		
	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;

	/** Bool for AnimBP to switch to another animation set */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Weapon)
	bool bHasRifle;

	/** Setter to set the bool */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	void SetHasRifle(bool bNewHasRifle);

	/** Getter for the bool */
	UFUNCTION(BlueprintCallable, Category = Weapon)
	bool GetHasRifle();

protected:
	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:
	/** MovementMode Functions and Variables */

	EMovementMode CurrentMovementMode;
	EMovementMode PreviousMovementMode;
	
	// Called When Movement Mode is Changed
	virtual void OnMovementModeChanged(EMovementMode InPrevMovementMode, uint8 PreviousCustomMode) override;

protected:
	/** ParkourMode Functions and Variables */

	// Set ParkourMode and Do Additional Process
	//! @retval true ParkourMode was Changed
	//! @retval false ParkourMode was not Changed
	bool SetParkourMode(EParkourMode InNewParkourMode);

	// Reset Parameters Changed In Parkour Action
	void ResetMovement();

	EParkourMode CurrentParkourMode;
	EParkourMode PrevParkourMode;

protected:
	/** Functions and Variables Related to Enabling and Disabling Parkour */

	// If Character Can Sprint
	bool bCanSprint;

	// TimerHandle for Sprint
	FTimerHandle SprintTimerHandle;

	// Enable Sprint
	void EnableSprint();

	// Disable Sprint
	void DisableSprint();

	// If Character Can Slide
	bool bCanSlide;

	// TimerHandle for Slide
	FTimerHandle SlideTimerHandle;

	// Enable Slide
	void EnableSlide();

	// Disable Slide
	void DisableSlide();

protected:
	/** Common Functions among Various Parkour Movement */

	// Check If the Character Intends to Move Forward
	bool ForwardInput() const;

	// Check If Sprint or Slide is Queued, and Start Respective Action
	void CheckQueues();

public:
	/** Variables and Functions Related To Jump */

	// If Player Can Jump Twice
	bool bCanDoubleJump;

	// Upward Force of Jumping
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Jump)
	float VerticalJumpForce;

	// Horizontal Force of Jumping
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Jump)
	float HorizontalJumpForce;

	// Jump(Including Double Jumping)
	virtual void Jump() override;

public:
	/** Variables and Functions Related To Sprint */

	// Input Action for Sprint
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputAction* SprintAction;

	// Speed of Sprint
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sprint)
	float SprintSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sprint)
	float SprintJumpForce;

	float DefaultWalkSpeed;

	// Whether Sprint is Queued
	bool bIsSprintQueued;

	// Start Sprint
	void SprintStart();

	// Finish Sprint
	void SprintEnd();

	// Check If Player Can Sprint
	bool CanSprint() const;

	// Update and Check the State
	void SprintUpdate();

	// Fired When Sprint Key was Pressed
	void Sprint();

	// Process When Player Jumps or Falls While Sprinting
	void SprintJump();

public:
	/** Variables and Functions Related To Crouch */

	// Input Action for Crouch and Slide
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	UInputAction* CrouchAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Crouch)
	float CrouchCapsuleHalfHeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Crouch)
	float CrouchCameraZOffset;

	float StandingCapsuleHalfHeight;

	float StandingCameraZOffset;

	// Start Crouch
	void CrouchStart();

	// Finish Crouch
	void CrouchEnd();

	// Called Every Frame with Regard to Crouching, Mainly so that Parameters get updated
	void CrouchUpdate();

	// Processing with Regard to Jumping While Crouching
	void CrouchJump();

	// Check If Player Can Stand up
	bool CanStand() const;

	// Fired When Crouch Key was Pressed
	void CrouchSlideKeyPressed();

	// Toggle Crouch and Standing
	void CrouchToggle();

public:
	/** Variables and Functions Related to Slide */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Slide)
	float SlideSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Slide)
	float SlideForceMultiplier;

	// Ground Friction by Default
	float DefaultGroundFriction;

	// Braking Deceleration by Default
	float DefaultBrakingDeceleration;

	// If Sliding is Queued
	bool bIsSlideQueued;

	// Start Slide
	void SlideStart();

	// Finish Slide
	void SlideEnd();

	// Called Every Frame with Regard to Sliding
	void SlideUpdate();

	// Check If Player Can Slide
	bool CanSlide();

	// Called When Jump Key was Pressed While Sliding
	void SlideJump();

	// Calculate the Influence of Slope on Slide
	FVector CalculateFloorInfluenceVector(const FVector& FloorNormal) const;

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

public:
	/** Returns Mesh1P subobject **/
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

};

