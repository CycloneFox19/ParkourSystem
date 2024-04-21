// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParkourSystemCharacter.h"
#include "ParkourSystemProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// AParkourSystemCharacter

AParkourSystemCharacter::AParkourSystemCharacter()
	: CurrentMovementMode(EMovementMode::MOVE_None)
	, PreviousMovementMode(EMovementMode::MOVE_None)
	, CurrentParkourMode(EParkourMode::EPM_None)
	, PrevParkourMode(EParkourMode::EPM_None)
	, bCanSprint(false)
	, bCanSlide(false)
	, bCanDoubleJump(true)
	, VerticalJumpForce(450.f)
	, HorizontalJumpForce(100.f)
	, SprintSpeed(1000.f)
	, SprintJumpForce(200.f)
	, bIsSprintQueued(false)
	, bIsSlideQueued(false)
	, CrouchCapsuleHalfHeight(35.f)
	, CrouchCameraZOffset(60.f)
	, SlideSpeed(1000.f)
	, SlideForceMultiplier(100.f)
{
	// Character doesnt have a rifle at start
	bHasRifle = false;
	
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);
		
	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-10.f, 0.f, 60.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	//Mesh1P->SetRelativeRotation(FRotator(0.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));
}

void AParkourSystemCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	// Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	DefaultWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
	DefaultGroundFriction = GetCharacterMovement()->GroundFriction;
	DefaultBrakingDeceleration = GetCharacterMovement()->BrakingDecelerationWalking;

	StandingCapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	StandingCameraZOffset = GetFirstPersonCameraComponent()->GetRelativeLocation().Z;

	CurrentMovementMode = GetCharacterMovement()->MovementMode;
}

void AParkourSystemCharacter::Tick(float DeltaTime)
{
	// Call the base class
	Super::Tick(DeltaTime);

	if (bCanSprint)
	{
		SprintUpdate();
	}

	if (bCanSlide)
	{
		SlideUpdate();
	}
	
	CrouchUpdate();
}

void AParkourSystemCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
}

// Jump(Including Double Jumping)
void AParkourSystemCharacter::Jump()
{
	if (!CanStand())
	{
		return;
	}

	// Jump Events
	CrouchJump();
	SprintJump();

	Super::Jump();

	/* if (PrevParkourMode == EParkourMode::EPM_Sprint || bIsSprintQueued)
	{
		const FVector SprintJumpThrust = FVector(SprintJumpForce * GetVelocity().X, SprintJumpForce * GetVelocity().Y, 0.f);
		LaunchCharacter(SprintJumpThrust, false, false);
	} */

	if (bCanDoubleJump && GetCharacterMovement()->IsFalling())
	{
		const FVector JumpVelocity = { GetActorForwardVector().X * HorizontalJumpForce, GetActorForwardVector().Y * HorizontalJumpForce, VerticalJumpForce};
		LaunchCharacter(JumpVelocity, false, true);

		bCanDoubleJump = false;
	}
}

// Called When Player Starts Sprinting
void AParkourSystemCharacter::SprintStart()
{
	CrouchEnd();

	SlideEnd();

	if (CanSprint())
	{
		if (SetParkourMode(EParkourMode::EPM_Sprint))
		{
			GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
			EnableSprint();
			bIsSprintQueued = false;
			bIsSlideQueued = false;
		}
	}
}

// Called When Player Stops Sprinting
void AParkourSystemCharacter::SprintEnd()
{
	if (CurrentParkourMode == EParkourMode::EPM_Sprint && SetParkourMode(EParkourMode::EPM_None))
	{
		DisableSprint();

		GetWorldTimerManager().SetTimer(SprintTimerHandle, this, &AParkourSystemCharacter::EnableSprint, 0.1f);
	}
}

// Check If Player Can Sprint
bool AParkourSystemCharacter::CanSprint() const
{
	if (!GetCharacterMovement())
	{
		return false;
	}

	return CurrentParkourMode == EParkourMode::EPM_None && GetCharacterMovement()->IsWalking();
}

// Called Every Frame While Sprinting
void AParkourSystemCharacter::SprintUpdate()
{
	if (CurrentParkourMode == EParkourMode::EPM_Sprint && !ForwardInput())
	{
		SprintEnd();
	}
}

// Fired When Sprint Key was Pressed
void AParkourSystemCharacter::Sprint()
{
	if (CurrentParkourMode == EParkourMode::EPM_Sprint)
	{
		SprintEnd();
	}
	else if (CurrentParkourMode == EParkourMode::EPM_None || CurrentParkourMode == EParkourMode::EPM_Crouch)
	{
		SprintStart();
	}
}

// Process When Player Jumps or Falls While Sprinting
void AParkourSystemCharacter::SprintJump()
{
	if (CurrentParkourMode == EParkourMode::EPM_Sprint)
	{
		SprintEnd();
		bIsSprintQueued = true;
	}
}

// Called When Player Starts Crouching
void AParkourSystemCharacter::CrouchStart()
{
	if (CurrentParkourMode == EParkourMode::EPM_None)
	{
		SetParkourMode(EParkourMode::EPM_Crouch);

		GetCharacterMovement()->MaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeedCrouched;
		bIsSprintQueued = false;
		bIsSlideQueued = false;
	}
}

// Called When Player Finishes Crouching
void AParkourSystemCharacter::CrouchEnd()
{
	if (CurrentParkourMode == EParkourMode::EPM_Crouch && CanStand())
	{
		SetParkourMode(EParkourMode::EPM_None);

		GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed;
		bIsSprintQueued = false;
		bIsSlideQueued = false;
	}
}

// Called Every Frame with Regard to Crouching
void AParkourSystemCharacter::CrouchUpdate()
{
	float CurrentHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector CurrentOffset = GetFirstPersonCameraComponent()->GetRelativeLocation();

	if (CurrentParkourMode == EParkourMode::EPM_Crouch || CurrentParkourMode == EParkourMode::EPM_Slide)
	{
		CurrentHalfHeight = FMath::FInterpTo(
			CurrentHalfHeight, 
			CrouchCapsuleHalfHeight, 
			UGameplayStatics::GetWorldDeltaSeconds(this), 
			10.f);

		CurrentOffset.Z = FMath::FInterpTo(
			CurrentOffset.Z,
			CrouchCameraZOffset,
			UGameplayStatics::GetWorldDeltaSeconds(this),
			10.f);
	}
	else
	{
		CurrentHalfHeight = FMath::FInterpTo(
			CurrentHalfHeight,
			StandingCapsuleHalfHeight,
			UGameplayStatics::GetWorldDeltaSeconds(this),
			10.f);

		CurrentOffset.Z = FMath::FInterpTo(
			CurrentOffset.Z,
			StandingCameraZOffset,
			UGameplayStatics::GetWorldDeltaSeconds(this),
			10.f);
	}

	GetCapsuleComponent()->SetCapsuleHalfHeight(CurrentHalfHeight);
	GetFirstPersonCameraComponent()->SetRelativeLocation(CurrentOffset);
}

void AParkourSystemCharacter::CrouchJump()
{
	if (CurrentParkourMode == EParkourMode::EPM_Crouch)
	{
		CrouchEnd();
	}
}

bool AParkourSystemCharacter::CanStand() const
{
	const FVector TraceStart = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector TraceEnd = TraceStart + FVector(0.f, 0.f, 2.f * StandingCapsuleHalfHeight);

	FHitResult HitResult;
	return !GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECollisionChannel::ECC_Visibility);
}

// Fired When Crouch/Slide Key was Pressed
void AParkourSystemCharacter::CrouchSlideKeyPressed()
{
	// Cancel Parkour

	if (CanSlide())
	{
		if (GetCharacterMovement()->IsWalking())
		{
			SlideStart();
		}
		else
		{
			bIsSlideQueued = true;
		}
	}
	else
	{
		CrouchToggle();
	}
}

// Toggle Crouch and Standing
void AParkourSystemCharacter::CrouchToggle()
{
	if (CurrentParkourMode == EParkourMode::EPM_None)
	{
		CrouchStart();
	}
	else if (CurrentParkourMode == EParkourMode::EPM_Crouch)
	{
		CrouchEnd();
	}
}

// Start Sliding
void AParkourSystemCharacter::SlideStart()
{
	if (CanSlide() && GetCharacterMovement()->IsWalking())
	{
		SprintEnd();
		
		SetParkourMode(EParkourMode::EPM_Slide);
		
		// Slide Mechanism
		GetCharacterMovement()->GroundFriction = 0.f;
		GetCharacterMovement()->MaxWalkSpeed = 0.f;
		GetCharacterMovement()->BrakingDecelerationWalking = 1000.f;

		const FVector FloorNormal = GetCharacterMovement()->CurrentFloor.HitResult.Normal;
		const FVector SlideDirection = FVector::CrossProduct(GetActorRightVector(), FloorNormal).GetSafeNormal();

		GetCharacterMovement()->AddImpulse(SlideSpeed * SlideDirection, true);

		EnableSlide();
		bIsSprintQueued = false;
		bIsSlideQueued = false;
	}
}

// Finish Sliding
void AParkourSystemCharacter::SlideEnd()
{
	if (CurrentParkourMode == EParkourMode::EPM_Slide)
	{
		if (SetParkourMode(EParkourMode::EPM_Crouch))
		{
			DisableSlide();
		}
	}
}

// Called Every Frame, with Regard to Sliding
void AParkourSystemCharacter::SlideUpdate()
{
	if (CurrentParkourMode == EParkourMode::EPM_Slide)
	{
		if (GetCharacterMovement()->Velocity.Length() < 35.f)
		{
			SlideEnd();
		}

		const FVector FloorNormal = GetCharacterMovement()->CurrentFloor.HitResult.Normal;
		const FVector ForceDirection = FVector::CrossProduct(FloorNormal, FVector::CrossProduct(FloorNormal, FVector::UpVector)).GetSafeNormal();

		UE_LOG(LogTemp, Warning, TEXT("%s"), *ForceDirection.ToString());

		GetCharacterMovement()->AddForce(ForceDirection * SlideSpeed * SlideForceMultiplier);
		if (GetCharacterMovement()->Velocity.Length() > SlideSpeed)
		{
			GetCharacterMovement()->Velocity = SlideSpeed * GetCharacterMovement()->Velocity.GetSafeNormal();
		}
	}
}

// Check If Player Can Slide
bool AParkourSystemCharacter::CanSlide()
{
	const bool bSprintFactors = CurrentParkourMode == EParkourMode::EPM_Sprint || bIsSprintQueued;
	return ForwardInput() && bSprintFactors;
}

void AParkourSystemCharacter::SlideJump()
{
	if (CurrentParkourMode == EParkourMode::EPM_Slide)
	{
		SlideEnd();
	}
}

// Compute the Influence of Slope
FVector AParkourSystemCharacter::CalculateFloorInfluenceVector(const FVector& FloorNormal) const
{
	if (FloorNormal.Equals(FVector::ZAxisVector))
	{
		return FVector::ZeroVector;
	}
	else
	{
		const float FloorInfluenceStrength = FMath::Clamp(1.f - FVector::DotProduct(FloorNormal, FVector::ZeroVector), 0.f, 1.f);
		const FVector FloorInfluenceVector = FloorInfluenceStrength * FVector::CrossProduct(FloorNormal, FVector::CrossProduct(FloorNormal, FVector::ZAxisVector));
		return FloorInfluenceVector;
	}
}

//////////////////////////////////////////////////////////////////////////// Input
void AParkourSystemCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AParkourSystemCharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AParkourSystemCharacter::Move);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AParkourSystemCharacter::Look);

		// Sprinting
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AParkourSystemCharacter::Sprint);

		// Crouching and Sliding
		EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &AParkourSystemCharacter::CrouchSlideKeyPressed);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input Component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}


void AParkourSystemCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr && CurrentParkourMode != EParkourMode::EPM_Slide)
	{
		// add movement 
		AddMovementInput(GetActorForwardVector(), MovementVector.Y);
		AddMovementInput(GetActorRightVector(), MovementVector.X);
	}
}

void AParkourSystemCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

// Called When MovementMode is Changed
void AParkourSystemCharacter::OnMovementModeChanged(EMovementMode InPrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(InPrevMovementMode, PreviousCustomMode);

	if (!GetCharacterMovement())
	{
		return;
	}

	PreviousMovementMode = InPrevMovementMode;
	CurrentMovementMode = GetCharacterMovement()->MovementMode;

	if (PreviousMovementMode == EMovementMode::MOVE_Walking && CurrentMovementMode == EMovementMode::MOVE_Falling)
	{
		SprintJump();
		// End Events
		SprintEnd();
		SlideEnd();
		// Open Gates
	}
	else if (PreviousMovementMode == EMovementMode::MOVE_Falling && CurrentMovementMode == EMovementMode::MOVE_Walking)
	{
		bCanDoubleJump = true;
		CheckQueues();
	}
}

// Set ParkourMode
bool AParkourSystemCharacter::SetParkourMode(EParkourMode InNewParkourMode)
{
	if (InNewParkourMode == CurrentParkourMode)
	{
		return false;
	}
	else
	{
		PrevParkourMode = CurrentParkourMode;
		CurrentParkourMode = InNewParkourMode;

		ResetMovement();
		return true;
	}
}

// Reset Parameters
void AParkourSystemCharacter::ResetMovement()
{
	if (CurrentParkourMode == EParkourMode::EPM_None || CurrentParkourMode == EParkourMode::EPM_Crouch)
	{
		if (CurrentParkourMode == EParkourMode::EPM_Crouch)
		{
			GetCharacterMovement()->MaxWalkSpeed = GetCharacterMovement()->MaxWalkSpeedCrouched;
		}
		else
		{
			GetCharacterMovement()->MaxWalkSpeed = DefaultWalkSpeed;
		}

		GetCharacterMovement()->GroundFriction = DefaultGroundFriction;
		GetCharacterMovement()->BrakingDecelerationWalking = DefaultBrakingDeceleration;
		GetCharacterMovement()->SetPlaneConstraintEnabled(false);

		bool bToWalking = PrevParkourMode == EParkourMode::EPM_None
			|| PrevParkourMode == EParkourMode::EPM_Sprint
			|| PrevParkourMode == EParkourMode::EPM_Crouch
			|| PrevParkourMode == EParkourMode::EPM_Slide;
		if (bToWalking)
		{
			GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
		}
	}
}

// Enable Sprint
void AParkourSystemCharacter::EnableSprint()
{
	bCanSprint = true;
}

// Disable Sprint
void AParkourSystemCharacter::DisableSprint()
{
	bCanSprint = false;
}

void AParkourSystemCharacter::EnableSlide()
{
	bCanSlide = true;
}

void AParkourSystemCharacter::DisableSlide()
{
	bCanSlide = false;
}

// Check If Input Vector is Directed Forward
bool AParkourSystemCharacter::ForwardInput() const
{
	if (!GetCharacterMovement())
	{
		return false;
	}

	const float Cosine = FVector::DotProduct(GetActorForwardVector(), GetCharacterMovement()->GetLastInputVector());
	return Cosine > 0.f;
}

// Check If Sprint or Slide Queued, and Start Respective Action
void AParkourSystemCharacter::CheckQueues()
{
	if (bIsSlideQueued)
	{
		SlideStart();
	}
	else if (bIsSprintQueued)
	{
		SprintStart();
	}
}

void AParkourSystemCharacter::SetHasRifle(bool bNewHasRifle)
{
	bHasRifle = bNewHasRifle;
}

bool AParkourSystemCharacter::GetHasRifle()
{
	return bHasRifle;
}