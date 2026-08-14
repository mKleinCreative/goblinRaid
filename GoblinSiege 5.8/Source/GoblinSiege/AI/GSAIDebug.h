// One log channel for AI combat DECISIONS, as opposed to their outcomes.
//
// GS.Combat.LogDamage already answers "what happened to this hit". It cannot answer the question
// that actually costs the evenings: "why did he not block?" - which has at least six
// indistinguishable causes (no target, out of range, still on cooldown, the roll failed, the
// telegraph was never seen, StartBlocking refused because the guard was already broken). Each looks
// exactly like the others from the outside: a defender standing there getting hit.
//
// Same reasoning as GS.Combat.LogHitReact, and the same shape. Declared in a header rather than
// living as a file-static because three separate translation units make these decisions
// (BTService_AcquireTarget, BTTask_Block, BTTask_MeleeAttack) and three private copies of one
// switch is how a toggle ends up half-working. The definitions are in Combat/GSDebugCommands.cpp,
// which already owns this project's debug plumbing and the GS.PlayerView toggle table.
#pragma once

#include "CoreMinimal.h"

class AActor;

namespace GSAIDebug
{
	/** True when GS.Combat.LogAI is non-zero. Call this before building an expensive message - the
	 *  logging here sits inside BT node ticks, and a formatted FString per agent per tick is not
	 *  free even when the log call itself is discarded. */
	GOBLINSIEGE_API bool IsLogging();

	/** Emits "[GS.AI] <actor>: <message>". Who may be null. Cheap no-op when logging is off, but
	 *  prefer guarding the call site with IsLogging() when the message costs anything to build. */
	GOBLINSIEGE_API void Log(const AActor* Who, const FString& Message);
}
