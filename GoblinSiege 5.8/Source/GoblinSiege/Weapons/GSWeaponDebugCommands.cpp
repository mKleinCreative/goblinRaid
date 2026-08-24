// GS.Weapon.* - the instruments for placing a weapon in a hand BY EYE.
// Written 2026-08-24 (#293).
//
// WHY: weapon placement is the one thing in this project that cannot be verified by measurement.
// A socket check, a distance-from-origin, a scale read-back - #100 proved all three pass on a sword
// gripped by the point. Placement is judged by looking at it, which means the only loop that works
// is change-a-number / look / change-it-again, and until now that loop did not exist:
//
//   - the offset lives in a UGSWeaponDataAsset field, which is editable in the editor but only read
//     when a weapon is equipped, so a change needed a full PIE restart to be seen;
//   - and once the ACF migration started it lived in a SECOND place as well, hand-copied onto the
//     weapon Blueprint, so editing the data asset changed our mesh and did nothing to the weapon
//     ACF was actually holding. Erika's bow is backwards on both paths and neither could be fixed
//     from the editor.
//
// #293 made the data asset the single source (UGSWeaponComponent::ApplyACFWeaponOffsets). These
// commands make it a LOOP: Dump to see what a weapon has, Set to try a value instantly without
// touching an asset, Reapply to pull an edited data asset onto a weapon already in somebody's hand.
//
// The intended workflow, all without leaving PIE:
//
//   GS.Weapon.Dump                          what is every weapon in the world using right now
//   GS.Weapon.Set Erika 10.6 -3.1 -61.4 0 180 0 1.25    try it - the bow turns round instantly
//   ... edit DA_Weapon_Erika.RangedMeshOffset in the editor to the value you settled on ...
//   GS.Weapon.Reapply                       pull the asset onto the live pawn and confirm
//
// Set is deliberately TEMPORARY. It writes to the live component and never to an asset, so nothing
// is saved by accident from a console - the value you like has to be typed into the data asset,
// which is also the thing that makes it survive.

#include "Weapons/GSWeaponComponent.h"
#include "Weapons/GSWeaponDataAsset.h"
#include "Combat/GSGameplayTags.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

namespace GSWeaponDebug
{
	/**
	 * The editor's Output Log console is not the PIE world, so a command that trusts the world it is
	 * handed silently does nothing when typed there - no pawn, no weapons, and no line saying why.
	 * GS.Raid.* was written twice for exactly this reason; this resolves PIE/Game explicitly so it
	 * does not matter which console the command is typed into.
	 */
	static UWorld* GameWorld(UWorld* Fallback)
	{
		if (GEngine)
		{
			for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
			{
				if ((Ctx.WorldType == EWorldType::PIE || Ctx.WorldType == EWorldType::Game) && Ctx.World())
				{
					return Ctx.World();
				}
			}
		}
		return Fallback;
	}

	/** Every weapon component in the world whose owner's name contains Filter (empty = all). */
	static TArray<UGSWeaponComponent*> FindWeaponComponents(UWorld* World, const FString& Filter)
	{
		TArray<UGSWeaponComponent*> Found;
		if (!World)
		{
			return Found;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor)
			{
				continue;
			}
			if (!Filter.IsEmpty() && !Actor->GetName().Contains(Filter))
			{
				continue;
			}
			if (UGSWeaponComponent* Weapon = Actor->FindComponentByClass<UGSWeaponComponent>())
			{
				Found.Add(Weapon);
			}
		}
		return Found;
	}

	static void Report(const FString& Line)
	{
		UE_LOG(LogTemp, Display, TEXT("[GS.Weapon] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 12.f, FColor::Cyan, FString::Printf(TEXT("[GS.Weapon] %s"), *Line));
		}
	}

	static FString Describe(const FTransform& T)
	{
		const FVector L = T.GetTranslation();
		const FRotator R = T.GetRotation().Rotator();
		const FVector S = T.GetScale3D();
		return FString::Printf(
			TEXT("loc (%.3f, %.3f, %.3f)  rot (pitch %.1f, yaw %.1f, roll %.1f)  scale %.3f"),
			L.X, L.Y, L.Z, R.Pitch, R.Yaw, R.Roll, S.X);
	}
}

// ----------------------------------------------------------------------------------- GS.Weapon.Dump
static FAutoConsoleCommandWithWorldAndArgs GSWeaponDumpCmd(
	TEXT("GS.Weapon.Dump"),
	TEXT("GS.Weapon.Dump [NameFilter] - every weapon in the world: which data asset drives it, which "
		 "field, the offset it is using, and whether ACF or our mesh path is holding it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* InWorld)
		{
			UWorld* World = GSWeaponDebug::GameWorld(InWorld);
			const FString Filter = Args.Num() > 0 ? Args[0] : FString();
			TArray<UGSWeaponComponent*> Components = GSWeaponDebug::FindWeaponComponents(World, Filter);
			if (Components.Num() == 0)
			{
				GSWeaponDebug::Report(FString::Printf(
					TEXT("no weapon components found%s. Is PIE running?"),
					Filter.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" matching '%s'"), *Filter)));
				return;
			}

			for (UGSWeaponComponent* Weapon : Components)
			{
				const AActor* Owner = Weapon->GetOwner();
				const UGSWeaponDataAsset* Data = Weapon->GetEquippedWeapon();
				if (!Data)
				{
					GSWeaponDebug::Report(FString::Printf(
						TEXT("%s: no weapon equipped"), *GetNameSafe(Owner)));
					continue;
				}

				const FGameplayTag Slot = Weapon->GetCurrentSlot();
				const bool bACF = Weapon->IsSlotOwnedByACF(Slot);
				GSWeaponDebug::Report(FString::Printf(
					TEXT("%s  slot=%s  data=%s  held by %s"),
					*GetNameSafe(Owner), *Slot.ToString(), *GetNameSafe(Data),
					bACF ? TEXT("ACF") : TEXT("our mesh path")));

				if (Slot == GSTags::WeaponSlot_Bow)
				{
					GSWeaponDebug::Report(FString::Printf(TEXT("    RangedMeshOffset  %s"),
						*GSWeaponDebug::Describe(Data->RangedMeshOffset)));
				}
				else
				{
					GSWeaponDebug::Report(FString::Printf(TEXT("    MeleeMeshOffset   %s"),
						*GSWeaponDebug::Describe(Data->MeleeMeshOffset)));
				}
			}
		}));

// ------------------------------------------------------------------------------------ GS.Weapon.Set
static FAutoConsoleCommandWithWorldAndArgs GSWeaponSetCmd(
	TEXT("GS.Weapon.Set"),
	TEXT("GS.Weapon.Set <NameFilter> <X> <Y> <Z> <Pitch> <Yaw> <Roll> [Scale] - try an offset on the "
		 "live weapon immediately. TEMPORARY: writes to the component, never to the asset. Put the "
		 "value you settle on into the weapon data asset to keep it."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* InWorld)
		{
			if (Args.Num() < 7)
			{
				GSWeaponDebug::Report(
					TEXT("usage: GS.Weapon.Set <NameFilter> <X> <Y> <Z> <Pitch> <Yaw> <Roll> [Scale]"));
				return;
			}

			UWorld* World = GSWeaponDebug::GameWorld(InWorld);
			const FString Filter = Args[0];
			const FVector Loc(FCString::Atof(*Args[1]), FCString::Atof(*Args[2]), FCString::Atof(*Args[3]));
			const FRotator Rot(FCString::Atof(*Args[4]), FCString::Atof(*Args[5]), FCString::Atof(*Args[6]));
			const float Scale = Args.Num() > 7 ? FCString::Atof(*Args[7]) : 1.f;
			const FTransform Offset(Rot, Loc, FVector(Scale));

			TArray<UGSWeaponComponent*> Components = GSWeaponDebug::FindWeaponComponents(World, Filter);
			if (Components.Num() == 0)
			{
				GSWeaponDebug::Report(FString::Printf(TEXT("nothing matches '%s'"), *Filter));
				return;
			}

			int32 Moved = 0;
			for (UGSWeaponComponent* Weapon : Components)
			{
				Moved += Weapon->DebugApplyOffsetToHeldWeapon(Offset);
			}

			GSWeaponDebug::Report(FString::Printf(
				TEXT("applied to %d mesh(es) on %d character(s): %s"),
				Moved, Components.Num(), *GSWeaponDebug::Describe(Offset)));
			GSWeaponDebug::Report(
				TEXT("TEMPORARY - type it into the weapon data asset to keep it."));
		}));

// -------------------------------------------------------------------------------- GS.Weapon.Reapply
static FAutoConsoleCommandWithWorldAndArgs GSWeaponReapplyCmd(
	TEXT("GS.Weapon.Reapply"),
	TEXT("GS.Weapon.Reapply [NameFilter] - re-read the weapon data assets and push their offsets onto "
		 "weapons already in hand. Edit the data asset in the editor with PIE running, then run this."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* InWorld)
		{
			UWorld* World = GSWeaponDebug::GameWorld(InWorld);
			const FString Filter = Args.Num() > 0 ? Args[0] : FString();
			TArray<UGSWeaponComponent*> Components = GSWeaponDebug::FindWeaponComponents(World, Filter);
			if (Components.Num() == 0)
			{
				GSWeaponDebug::Report(FString::Printf(
					TEXT("nothing matches '%s'"), Filter.IsEmpty() ? TEXT("(all)") : *Filter));
				return;
			}

			for (UGSWeaponComponent* Weapon : Components)
			{
				Weapon->DebugReapplyOffsets();
			}
			GSWeaponDebug::Report(FString::Printf(
				TEXT("re-applied data-asset offsets to %d character(s). GS.Weapon.Dump to read them back."),
				Components.Num()));
		}));
