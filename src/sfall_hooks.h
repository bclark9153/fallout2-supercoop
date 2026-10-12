#ifndef FALLOUT_SFALL_HOOKS_H_
#define FALLOUT_SFALL_HOOKS_H_

#include "interpreter.h"
#include "obj_types.h"

namespace fallout {

// sfall hook scripts (register_hook_proc and friends), reimplemented for the
// hooks the Restoration Project and its bundled mods use. Hook ids match
// sfall's HOOK_* constants (sfall.h).
enum SfallHookId {
    SFALL_HOOK_COMBATDAMAGE = 5,
    SFALL_HOOK_USEOBJON = 8,
    SFALL_HOOK_USEOBJ = 18,
    SFALL_HOOK_KEYPRESS = 19,
    SFALL_HOOK_INVENTORYMOVE = 24,
    SFALL_HOOK_INVENWIELD = 25,
    SFALL_HOOK_ADJUSTFID = 26,
    SFALL_HOOK_COMBATTURN = 27,
    SFALL_HOOK_SETGLOBALVAR = 29,
    SFALL_HOOK_GAMEMODECHANGE = 31,
    SFALL_HOOK_CANUSEWEAPON = 48,
    SFALL_HOOK_COUNT = 64,
};

void sfallHooksRegisterOpcodes();

// Drop every registration (global scripts are being unloaded).
void sfallHooksReset();
// Drop the registrations a program made (that program is being freed).
void sfallHooksRemoveProgram(Program* program);

bool sfallHookHasScripts(int hookId);
ProgramValue sfallHookGetArgAt(int index);

// Call sites. Each returns the engine-facing result described next to it and
// is a cheap no-op when no script registered the hook.

// HOOK_USEOBJON: returns -1 for "use engine handler", else the override.
int sfallHookUseObjOn(Object* target, Object* user, Object* item);
// HOOK_USEOBJ: returns -1 for "use engine handler", else the override.
int sfallHookUseObj(Object* user, Object* item);
// HOOK_INVENWIELD: returns true when the engine handler should run.
bool sfallHookInvenWield(Object* critter, Object* item, int slot, bool wield, bool remove);
// HOOK_CANUSEWEAPON: returns the (possibly overridden) result.
bool sfallHookCanUseWeapon(bool result, Object* critter, Object* weapon, int hitMode);
// HOOK_INVENTORYMOVE: returns -1 for "use engine handler", else cancel.
int sfallHookInventoryMove(int slot, Object* item, Object* replaced);
// HOOK_COMBATDAMAGE: may rewrite the attack's damage/flags/knockback.
struct Attack;
void sfallHookCombatDamage(Attack* attack, int rounds, int multiplier);
// HOOK_GAMEMODECHANGE
void sfallHookGameModeChange(int exitEvent, int previousMode);
// HOOK_SETGLOBALVAR: returns the value to store.
int sfallHookSetGlobalVar(int index, int value);

} // namespace fallout

#endif /* FALLOUT_SFALL_HOOKS_H_ */
