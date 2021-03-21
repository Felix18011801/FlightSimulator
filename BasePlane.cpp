// Fill out your copyright notice in the Description page of Project Settings.


#include "BasePlane.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"

ABasePlane::ABasePlane()
{
	PrimaryActorTick.bCanEverTick = true;

	PlaneBodyComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PlaneBody"));
	SetRootComponent(PlaneBodyComp);

	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArmComp->bUsePawnControlRotation = true;
	SpringArmComp->SetupAttachment(RootComponent);

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComp->SetupAttachment(SpringArmComp, USpringArmComponent::SocketName);

}

void ABasePlane::BeginPlay()
{
	Super::BeginPlay();
}


void ABasePlane::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	fDeltaTime = DeltaTime;

	PlaneBodyComp->SetAllBodiesPhysicsBlendWeight(0.0f, false);
	RunPhysicsFunction();
	ApplyGravity(true);

	CurrentFlaps = FMath::FInterpConstantTo(CurrentFlaps, TargetFlaps, fDeltaTime, 2);

	int AktuelleFlapsExtend_Index1 = FMath::FloorToInt(CurrentFlaps);
	int AktuelleFlapsExtend_Index2 = FMath::CeilToInt(CurrentFlaps);

	if(AktuelleFlapsExtend_Index1 < FlapsExtend.Num() && AktuelleFlapsExtend_Index2 < FlapsExtend.Num())
		AktuelleFlapsExtend = FMath::Lerp(FlapsExtend[AktuelleFlapsExtend_Index1], FlapsExtend[AktuelleFlapsExtend_Index2], FMath::Fmod(CurrentFlaps, 1.0f));

	CurrentSpoilers = FMath::FInterpConstantTo(CurrentSpoilers, TargetSpoilers, fDeltaTime, 2);

	int AktuelleSpoilerExtend_Index1 = FMath::FloorToInt(CurrentSpoilers);
	int AktuelleSpoilerExtend_Index2 = FMath::CeilToInt(CurrentSpoilers);

	if (AktuelleSpoilerExtend_Index1 < SpoilerExtend.Num() && AktuelleSpoilerExtend_Index2 < SpoilerExtend.Num())
		AktuelleSpoilerExtend = FMath::Lerp(SpoilerExtend[AktuelleSpoilerExtend_Index1], SpoilerExtend[AktuelleSpoilerExtend_Index2], FMath::Fmod(CurrentSpoilers, 1.0f));

	const FRotator NewRotation(0.0f, 0.0f, Windrichtung);
	Wind = NewRotation.RotateVector(FVector(Windstaerke * 100.0f, 0.0f, 0.0f));

	float GetMovementSpeed;
	FVector GetMovementMoveDirection;
	FVector GetMovementUpDirection;
	float GetMovementRot;
	GetMovement(PlaneBodyComp->GetComponentRotation(), (PlaneBodyComp->GetPhysicsLinearVelocity() - Wind), GetMovementSpeed, GetMovementMoveDirection, GetMovementUpDirection, GetMovementRot);

	GeschwindigkeitVonVorne = GetMovementSpeed;
	Hoehe = GetActorLocation().Z / 100.0f;
	PlaneRotation = GetMovementRot;
}

void ABasePlane::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ABasePlane::CalculateLuftdichteHoehe()
{
	LuftdichteHoehe = (1 - (((VertikalerTemperaturgradient * Hoehe) / FMath::Pow(TemperaturMeeresspiegel, MolareMasseLuft)) * Gravitationskonstante * VertikalerTemperaturgradient) / UniverselleGaskonstante) * LuftdichteMeereshoehe;
}

void ABasePlane::ClaculateLuftdichteFlugzeug()
{
	LuftdichteFlugzeug = (1 - (((VertikalerTemperaturgradient * Hoehe) / FMath::Pow(TemperaturMeeresspiegel, MolareMasseLuft)) * Gravitationskonstante * VertikalerTemperaturgradient) / UniverselleGaskonstante) * ((Staudruck * MolareMasseLuft) / (UniverselleGaskonstante * TemperaturMeeresspiegel));
}

void ABasePlane::CalculateStaudruck()
{
	Staudruck = ((LuftdichteHoehe / 2) * FMath::Sqrt(GeschwindigkeitVonVorne)) + LuftdruckMeereshoehe;
}

void ABasePlane::CalculateLuftdichteMeereshoehe()
{
	LuftdichteMeereshoehe = (LuftdruckMeereshoehe * MolareMasseLuft) / (UniverselleGaskonstante * TemperaturMeeresspiegel);
}

void ABasePlane::RunPhysicsFunction()
{
	CalculateStaudruck();
	CalculateLuftdichteMeereshoehe();
	CalculateLuftdichteHoehe();
	ClaculateLuftdichteFlugzeug();
}

void ABasePlane::ApplyGravity(bool bShouldApply)
{
	if (bShouldApply)
	{
		FVector Force = (LeerGewichtFlugzeug + LeftFuelTank + RightFuelTank + Payload) * FVector(0.0f, 0.0f, -981.0f);
		PlaneBodyComp->AddForce(Force, NAME_None, false);
	}
}

float ABasePlane::GetLuftdichte(FVector A, FRotator B)
{
	if(LuftwiderstandPlaneCurve)
		return FMath::Clamp(FMath::Lerp(LuftdichteHoehe, LuftdichteFlugzeug, LuftwiderstandPlaneCurve->GetFloatValue((B.UnrotateVector(A - GetActorLocation()).X / 100) / PlaneHalfLength)), LuftdichteHoehe, LuftdichteFlugzeug);
	
	return 0.0f;
}

void ABasePlane::GetMovement(FRotator Rotation, FVector Velocity, float & Speed, FVector & MoveDirection, FVector & UpMovement, float & Rot)
{
	FVector ForwardVector = FRotationMatrix(Rotation).GetScaledAxis(EAxis::X);
	FVector UpVector = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Z);
	FVector RightVector = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Y);

	bool SelectConditions = Velocity.Size() > 20;

	FVector AddedDotProducts = ForwardVector * FVector::DotProduct(ForwardVector, Velocity / 100) + (UpVector * FVector::DotProduct(UpVector, Velocity / 100));
	float InverseTanOfDotProducts =  FMath::Atan2(FVector::DotProduct(UpVector, Velocity / 100), FVector::DotProduct(ForwardVector, Velocity / 100));

	//return
	Speed = SelectConditions ? AddedDotProducts.Size() : 0.0f;

	AddedDotProducts.Normalize();
	MoveDirection = SelectConditions ? AddedDotProducts : FVector(0, 0, 0);
	UpMovement = SelectConditions ? AddedDotProducts.RotateAngleAxis(-90, RightVector) : FVector(0, 0, 0);
	Rot = SelectConditions ? (0.0f - InverseTanOfDotProducts) : 0.0f;
}

float ABasePlane::BodenEffekt(float HoeheUeberGrund, float Spannweite)
{
	return FMath::Sqrt(HoeheUeberGrund / (Spannweite * 8.0f)) / (FMath::Sqrt(HoeheUeberGrund / (Spannweite * 8.0f)) + 1);
}

void ABasePlane::GetWingLoc(FName SocketName, bool AuftriebVerschieben, FVector & Location, FRotator & Rotation)
{
	FVector BoneLocation = PlaneBodyComp->GetSocketLocation(PlaneBodyComp->GetSocketBoneName(SocketName));
	FRotator BoneRotation = PlaneBodyComp->GetSocketRotation(PlaneBodyComp->GetSocketBoneName(SocketName));

	FName EndSocketName = *FString::Printf(TEXT("%s%s"), *SocketName.ToString(), TEXT("_end"));
	FVector BoneEndLocation = PlaneBodyComp->GetSocketLocation(PlaneBodyComp->GetSocketBoneName(EndSocketName));

	if (ArrayElemAuftrieb < PrevRotation.Num() && LiftCurve)
	{
		float LerpAlpha = FMath::Clamp(LiftCurve->GetFloatValue(PrevRotation[ArrayElemAuftrieb]) / 1.5f, 0.0f, 1.0f);
		FVector LerpOutput = FMath::Lerp(BoneLocation, BoneEndLocation, LerpAlpha);

		Location = AuftriebVerschieben ? LerpOutput : BoneLocation;
		Rotation = BoneRotation;
	}
}

void ABasePlane::WingFunction(
	FName SocketName,
	bool bIsFirst,
	float Fluegelflaeche,
	float Anstellwinkel,
	float Steigung_CL_Curve,
	float StatischerWiderstandswert,
	float WiderStandsMultiplier,
	float StallRotation,
	float BodeneffektMultiplier,
	TArray<float> MasAuftriebswert,
	TArray<float> Luftwiderstand,
	TArray<float> RelativeUse,
	float Spannweite,
	bool AuftriebVerschiebung)
{
	float LocalAuftrieb = 0.0f;
	float LocalLuftwiderstand = 0.0f;

	if (bIsFirst)
	{
		ArrayElemAuftrieb = 0;
	}
	else
	{
		ArrayElemAuftrieb++;
	}

	FVector GetWingLocLocation;
	FRotator GetWingLocRotation;
	GetWingLoc(SocketName, AuftriebVerschiebung, GetWingLocLocation, GetWingLocRotation);

	FHitResult OutHit;
	if (BodeneffektMultiplier != 0)
	{
		FVector Start = GetWingLocLocation;

		FVector GetWingLocVectorUp = FRotationMatrix(GetWingLocRotation).GetScaledAxis(EAxis::Z);
		FVector End = (GetWingLocVectorUp * -100000000.0f) + GetWingLocLocation;

		FCollisionQueryParams Params;

		Params.bTraceComplex = false;
		Params.AddIgnoredActor(this);

		FCollisionResponseParams ResponseParam;

		GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECollisionChannel::ECC_Visibility, Params, ResponseParam);
	}

	FVector LinearPhysics = PlaneBodyComp->GetPhysicsLinearVelocityAtPoint(GetWingLocLocation, NAME_None);

	float GetMovementSpeed;
	FVector GetMovementMoveDirection;
	FVector GetMovementUpDirection;
	float GetMovementRot;
	GetMovement(GetWingLocRotation, (LinearPhysics - Wind), GetMovementSpeed, GetMovementMoveDirection, GetMovementUpDirection, GetMovementRot);

	CPP_SetArrayElement(PrevRotation, ArrayElemAuftrieb, GetMovementRot, true);

	for (int i = 0; i < PrevRotation.Num(); i++)
	{
		float f = PrevRotation[i];

		if (i < MasAuftriebswert.Num() && i < Luftwiderstand.Num() && i < RelativeUse.Num())
		{
			LocalAuftrieb = (MasAuftriebswert[i] * RelativeUse[i]) + LocalAuftrieb;
			LocalLuftwiderstand = (Luftwiderstand[i] * RelativeUse[i]) + LocalLuftwiderstand;
		}
		
	}

	float LuftdichteReturnValue = GetLuftdichte(GetWingLocLocation, GetWingLocRotation);

	if (LiftCurve)
	{
		float AuftriebVar1 = (LiftCurve->GetFloatValue(GetMovementRot + Anstellwinkel / (StallRotation / 16)) * ((StallRotation / 16) * Steigung_CL_Curve)) + LocalAuftrieb;
		float AuftriebVar2 = OutHit.bBlockingHit ? (((((BodenEffekt(OutHit.Distance / 100, Spannweite) * BodeneffektMultiplier) + (1 - BodeneffektMultiplier)) + 1) * -1.0f) + 1) : 1.0f;

		float CalculatedAuftrieb = 0.5f * LuftdichteReturnValue * FMath::Sqrt(GetMovementSpeed) * Fluegelflaeche * AuftriebVar1 * AuftriebVar2;



		if (LuftwiderstandCurve && DragCurve)
		{
			float LuftwiderstandVar1_1 = StatischerWiderstandswert + LocalLuftwiderstand + (DragCurve->GetFloatValue(GetMovementRot + Anstellwinkel) * WiderStandsMultiplier);
			float LuftwiderstandVar1_2 = OutHit.bBlockingHit ? ((BodenEffekt(OutHit.Distance / 100, Spannweite) * BodeneffektMultiplier) + (1 - BodeneffektMultiplier)) : 1.0f;
			float LuftwiderstandVar1 = LuftwiderstandVar1_1 * LuftwiderstandVar1_2 * LuftwiderstandCurve->GetFloatValue(GeschwindigkeitVonVorne / 343);
	
			float CalculatedLuftwiderstand = 0.5f * LuftdichteReturnValue * FMath::Sqrt(GetMovementSpeed) * Fluegelflaeche * LuftwiderstandVar1;
	
	
			PlaneBodyComp->AddForceAtLocation((GetMovementUpDirection * CalculatedAuftrieb) + (GetMovementMoveDirection * CalculatedLuftwiderstand) * 100.0f, GetWingLocLocation, NAME_None);
		}
	}
}

void ABasePlane::EngineFunction(
	FName SocketName,
	bool bIsFirst,
	float MaxThurst,
	float Stroemungverhaeltnis,
	float SpeedbeeinflussungFan,
	float SpeedbeeinflussungKern,
	bool bGegenschub,
	float Gegenschubwinkel,
	float A,
	float B,
	float K,
	float R,
	float Km_1,
	float Km_2,
	float MinFuelKonsumption,
	float MaxFuelKonsumption,
	float FuelExponential,
	UPARAM(ref) float & FuelTank,
	float TrottleInput,
	float RelDrehIdle,
	float Startdauer)
{
	if (bIsFirst)
	{
		ArrayElemEngine = 0;
	}
	else
	{
		ArrayElemEngine++;
	}

	if (FuelTank <= 0)
	{
		CPP_SetArrayElement(EngineOn, ArrayElemEngine, false, true);
	}

	if(TrottleReversePercent.Num() > 0)
		CPP_SetArrayElement(TrottleReversePercent, ArrayElemEngine, FMath::FInterpConstantTo(TrottleReversePercent[ArrayElemEngine], TrottleReverse ? 1.0f : 0.0f, fDeltaTime, 1), true);

	if (ArrayElemEngine < EngineOn.Num() && ArrayElemEngine < EngineStartingProgress.Num() && ArrayElemEngine < RelDrehNiedrigdruckwelle.Num() && EngineStartProcessCurve)
	{
		float NewEngineStartingProgressValue = EngineOn[ArrayElemEngine] ? FMath::Clamp((EngineStartingProgress[ArrayElemEngine]) + ((1 / Startdauer) * fDeltaTime * (EngineOn[ArrayElemEngine] ? 1.0f : -1.0f)), 0.0f, 1.0f) : 0.0f;
		CPP_SetArrayElement(EngineStartingProgress, ArrayElemEngine, NewEngineStartingProgressValue, true);

		float CalculateddrehzahlAghaengigkeitThrottle = FMath::Lerp(RelDrehIdle, 0.0f, TrottleInput);

		float CalculatedStartingProcess = (EngineStartingProgress[ArrayElemEngine] == 1.0f) ? CalculateddrehzahlAghaengigkeitThrottle : ((EngineStartingProgress[ArrayElemEngine] == 0.0f) ? 0.0f : FMath::Lerp(0.0f, RelDrehIdle, EngineStartProcessCurve->GetFloatValue(EngineStartingProgress[ArrayElemEngine])));

		float CalculatedInterpolationSpeed_Var1 = FMath::Abs(EngineStartingProgress[ArrayElemEngine] - CalculatedStartingProcess);
		float CalculatedInterpolationSpeed_Var2_1 = (((EngineStartingProgress[ArrayElemEngine] >= 0.3f && EngineStartingProgress[ArrayElemEngine] <= 0.95f) ? 1.0f : 0.0f) + 1.0f);
		float CalculatedInterpolationSpeed_Var2_2 = ((EngineStartingProgress[ArrayElemEngine] == 0.0f) ? 1.0f : 0.0f * 5.0f);
		float CalculatedInterpolationSpeed = CalculatedInterpolationSpeed_Var1 * CalculatedInterpolationSpeed_Var2_1 * CalculatedInterpolationSpeed_Var2_2;

		CPP_SetArrayElement(RelDrehNiedrigdruckwelle, ArrayElemEngine, FMath::FInterpConstantTo(RelDrehNiedrigdruckwelle[ArrayElemEngine], CalculatedStartingProcess, FMath::Abs(fDeltaTime), CalculatedInterpolationSpeed), true);

		float CalculatedSRelativeDrehzahlHochdruckwelle = (RelDrehNiedrigdruckwelle[ArrayElemEngine] * A) + B;

		float CalculatedSMassenflussKerntriebwerk = Km_1 * CalculatedSRelativeDrehzahlHochdruckwelle * (LuftdichteFlugzeug / 1.205f);

		float CalculatedStroemungsverhaeltnis = ((Stroemungverhaeltnis - 1.0f) * RelDrehNiedrigdruckwelle[ArrayElemEngine]) + 1.0f;

		float CalculatedLuftgeschwindigkeitAustrittKerntriebwerk_Var1 = (FMath::Exp(RelDrehNiedrigdruckwelle[ArrayElemEngine] / R) - 1.0f) * K * MaxThurst;
		float CalculatedLuftgeschwindigkeitAustrittKerntriebwerk_Var2_1 = (((RelDrehNiedrigdruckwelle[ArrayElemEngine] * A) + B) * Km_1);
		float CalculatedLuftgeschwindigkeitAustrittKerntriebwerk_Var2_2 = RelDrehNiedrigdruckwelle[ArrayElemEngine] * Km_2 * CalculatedStroemungsverhaeltnis;
		float CalculatedLuftgeschwindigkeitAustrittKerntriebwerk_Var2 = CalculatedLuftgeschwindigkeitAustrittKerntriebwerk_Var2_1 + CalculatedLuftgeschwindigkeitAustrittKerntriebwerk_Var2_2;
		float CalculatedLuftgeschwindigkeitAustrittKerntriebwerk = CalculatedLuftgeschwindigkeitAustrittKerntriebwerk_Var1 / CalculatedLuftgeschwindigkeitAustrittKerntriebwerk_Var2;

		float CalculatedLuftgeschwindigkeitAustrittFan = CalculatedStroemungsverhaeltnis * CalculatedLuftgeschwindigkeitAustrittKerntriebwerk;

		float CalculatedMassenflussFan = RelDrehNiedrigdruckwelle[ArrayElemEngine] * Km_2 * (LuftdichteFlugzeug / 1.205f);

		float CalculatedSchubfan = FMath::Clamp((CalculatedLuftgeschwindigkeitAustrittFan - (GeschwindigkeitVonVorne / SpeedbeeinflussungFan)) * CalculatedMassenflussFan, 0.0f, 10000000000.0f);

		float CalculatedSchubKerntriebwerk = FMath::Clamp((CalculatedLuftgeschwindigkeitAustrittKerntriebwerk - (GeschwindigkeitVonVorne / SpeedbeeinflussungKern)) * CalculatedSMassenflussKerntriebwerk, 0.0f, 10000000000.0f);

		float ThrottleReverse_LerpA = CalculatedSchubKerntriebwerk + CalculatedSchubfan;
		float ThrottleReverse_LerpB = CalculatedSchubKerntriebwerk - (FMath::Cos(Gegenschubwinkel) * CalculatedSchubfan);
		float ThrottleReverse_LerpAlpha = RelDrehNiedrigdruckwelle[ArrayElemEngine] * (bGegenschub ? 1.0f : 0.0f);

		float ThrottleReverse = FMath::Lerp(ThrottleReverse_LerpA, ThrottleReverse_LerpB, ThrottleReverse_LerpAlpha);

		FVector SocketNameForwardVector = FRotationMatrix(PlaneBodyComp->GetSocketRotation(PlaneBodyComp->GetSocketBoneName(SocketName))).GetScaledAxis(EAxis::X);
		PlaneBodyComp->AddForceAtLocation(SocketNameForwardVector * (ThrottleReverse * 100), PlaneBodyComp->GetSocketLocation(PlaneBodyComp->GetSocketBoneName(SocketName)));

		float FuelKonsumpten = FMath::Clamp(FuelTank - (FMath::Lerp(0.0f, FMath::Lerp(MinFuelKonsumption, MaxFuelKonsumption, FMath::Pow((ThrottleReverse_LerpA / MaxThurst), (1 / FuelExponential))), EngineStartingProgress[ArrayElemEngine]) * fDeltaTime), 0.0f, 13399722918938673152.0f);

		FuelTank = FuelKonsumpten;

	}
}

template<typename T>
void ABasePlane::CPP_SetArrayElement(TArray<T> & Array, int index, T Value, bool bSizeToFit)
{
	if (bSizeToFit && index >= Array.Num())
	{
		Array.SetNum(index + 1);
	}

	Array[index] = Value;
}