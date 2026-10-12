#include "sfall_hooks.h"

#include <algorithm>
#include <vector>

#include "combat_defs.h"
#include "debug.h"
#include "interpreter.h"
#include "sfall_global_scripts.h"

namespace fallout {

namespace {

constexpr int kMaxHookArgs = 16;
constexpr int kMaxHookRets = 8;
constexpr int kMaxHookDepth = 8;

struct HookHandler {
    Program* program;
    int procedureIndex;
};

// Handlers per hook id, in sfall's storage order. sfall runs them from the
// back (newest registration first); register_hook_proc_spec inserts at the
// front, so a "spec" handler runs last and has the final say.
std::vector<HookHandler> gHooks[SFALL_HOOK_COUNT];

struct HookFrame {
    int hookId;
    ProgramValue args[kMaxHookArgs];
    int argCount;
    int argCursor; // get_sfall_arg position for the handler being run
    ProgramValue rets[kMaxHookRets];
    int retCount; // max over handlers, as in sfall (cRet)
    int handlerRetCount; // set_sfall_return position for the current handler
};

std::vector<HookFrame> gFrames;

HookFrame* currentFrame()
{
    return gFrames.empty() ? nullptr : &gFrames.back();
}

bool hookIdValid(int hookId)
{
    return hookId >= 0 && hookId < SFALL_HOOK_COUNT;
}

// Values crossing between programs must not be strings: a string value is an
// offset into the program that pushed it. Hook args here are ints/pointers.
ProgramValue sanitize(const ProgramValue& value)
{
    if (value.isString()) {
        return ProgramValue(0);
    }
    return value;
}

// Runs every handler for |hookId| with |args|. Returns false (and runs
// nothing) when there are no handlers, the hook is already running higher up
// the stack (sfall forbids recursion), or the depth limit is reached.
bool runHook(int hookId, const ProgramValue* args, int argCount, HookFrame& out)
{
    if (!hookIdValid(hookId) || gHooks[hookId].empty()) {
        return false;
    }

    for (const auto& frame : gFrames) {
        if (frame.hookId == hookId) {
            return false;
        }
    }
    if (static_cast<int>(gFrames.size()) >= kMaxHookDepth) {
        debugPrint("[SFALL] hook %d not run: depth limit\n", hookId);
        return false;
    }

    gFrames.emplace_back();
    HookFrame* frame = &gFrames.back();
    frame->hookId = hookId;
    frame->argCount = std::min(argCount, kMaxHookArgs);
    for (int index = 0; index < frame->argCount; index++) {
        frame->args[index] = args[index];
    }
    frame->retCount = 0;

    // Copy: a handler may register/unregister while we iterate.
    std::vector<HookHandler> handlers = gHooks[hookId];
    for (int index = static_cast<int>(handlers.size()) - 1; index >= 0; index--) {
        frame = &gFrames.back();
        frame->argCursor = 0;
        frame->handlerRetCount = 0;
        _executeProcedure(handlers[index].program, handlers[index].procedureIndex);
    }

    out = gFrames.back();
    gFrames.pop_back();
    return true;
}

void registerHook(Program* program, int hookId, int procedureIndex, bool spec)
{
    if (!hookIdValid(hookId)) {
        return;
    }

    auto& handlers = gHooks[hookId];
    for (auto it = handlers.begin(); it != handlers.end(); ++it) {
        if (it->program == program) {
            if (procedureIndex == 0) {
                handlers.erase(it); // register_hook_proc(id, 0) unregisters
            }
            return;
        }
    }
    if (procedureIndex == 0) {
        return;
    }

    if (!sfall_gl_scr_is_global(program)) {
        debugPrint("[SFALL] %s: register_hook_proc ignored (not a global script)\n", program->name);
        return;
    }

    HookHandler handler { program, procedureIndex };
    if (spec) {
        handlers.insert(handlers.begin(), handler);
    } else {
        handlers.push_back(handler);
    }
    debugPrint("[SFALL] %s registered for hook %d\n", program->name, hookId);
}

// register_hook_proc(id, proc)
void op_register_hook_proc(Program* program)
{
    int procedureIndex = programStackPopInteger(program);
    int hookId = programStackPopInteger(program);
    registerHook(program, hookId, procedureIndex, false);
}

// register_hook_proc_spec(id, proc)
void op_register_hook_proc_spec(Program* program)
{
    int procedureIndex = programStackPopInteger(program);
    int hookId = programStackPopInteger(program);
    registerHook(program, hookId, procedureIndex, true);
}

// register_hook(id): old form, the script's "start" procedure is the handler.
void op_register_hook(Program* program)
{
    int hookId = programStackPopInteger(program);
    int procedureIndex = programFindProcedure(program, "start");
    if (procedureIndex > 0) {
        registerHook(program, hookId, procedureIndex, false);
    }
}

// get_sfall_arg
void op_get_sfall_arg(Program* program)
{
    HookFrame* frame = currentFrame();
    if (frame == nullptr || frame->argCursor >= frame->argCount) {
        programStackPushInteger(program, 0);
        return;
    }
    programStackPushValue(program, frame->args[frame->argCursor++]);
}

// set_sfall_return(value)
void op_set_sfall_return(Program* program)
{
    ProgramValue value = sanitize(programStackPopValue(program));
    HookFrame* frame = currentFrame();
    if (frame == nullptr) {
        return;
    }
    if (frame->handlerRetCount < kMaxHookRets) {
        frame->rets[frame->handlerRetCount++] = value;
        if (frame->handlerRetCount > frame->retCount) {
            frame->retCount = frame->handlerRetCount;
        }
    }
}

// set_sfall_arg(index, value)
void op_set_sfall_arg(Program* program)
{
    ProgramValue value = sanitize(programStackPopValue(program));
    int index = programStackPopInteger(program);
    HookFrame* frame = currentFrame();
    if (frame != nullptr && index >= 0 && index < frame->argCount) {
        frame->args[index] = value;
    }
}

// init_hook: 1 while a hook runs for the first time after load; sfall scripts
// use it in hs_*.int. Global-script handlers never see 1.
void op_init_hook(Program* program)
{
    programStackPushInteger(program, 0);
}

ProgramValue ptrValue(Object* object)
{
    return ProgramValue(object);
}

} // namespace

void sfallHooksRegisterOpcodes()
{
    interpreterRegisterOpcode(0x81E4, op_get_sfall_arg);
    interpreterRegisterOpcode(0x81E5, op_set_sfall_return);
    interpreterRegisterOpcode(0x81EA, op_init_hook);
    interpreterRegisterOpcode(0x8207, op_register_hook);
    interpreterRegisterOpcode(0x823D, op_set_sfall_arg);
    interpreterRegisterOpcode(0x8262, op_register_hook_proc);
    interpreterRegisterOpcode(0x827D, op_register_hook_proc_spec);
}

void sfallHooksReset()
{
    for (auto& handlers : gHooks) {
        handlers.clear();
    }
}

void sfallHooksRemoveProgram(Program* program)
{
    for (auto& handlers : gHooks) {
        handlers.erase(std::remove_if(handlers.begin(), handlers.end(), [program](const HookHandler& handler) {
            return handler.program == program;
        }),
            handlers.end());
    }
}

bool sfallHookHasScripts(int hookId)
{
    return hookIdValid(hookId) && !gHooks[hookId].empty();
}

ProgramValue sfallHookGetArgAt(int index)
{
    HookFrame* frame = currentFrame();
    if (frame == nullptr || index < 0 || index >= frame->argCount) {
        return ProgramValue(0);
    }
    return frame->args[index];
}

int sfallHookUseObjOn(Object* target, Object* user, Object* item)
{
    if (!sfallHookHasScripts(SFALL_HOOK_USEOBJON)) {
        return -1;
    }
    ProgramValue args[] = { ptrValue(target), ptrValue(user), ptrValue(item) };
    HookFrame result;
    if (!runHook(SFALL_HOOK_USEOBJON, args, 3, result) || result.retCount < 1) {
        return -1;
    }
    return result.rets[0].asInt();
}

int sfallHookUseObj(Object* user, Object* item)
{
    if (!sfallHookHasScripts(SFALL_HOOK_USEOBJ)) {
        return -1;
    }
    ProgramValue args[] = { ptrValue(user), ptrValue(item) };
    HookFrame result;
    if (!runHook(SFALL_HOOK_USEOBJ, args, 2, result) || result.retCount < 1) {
        return -1;
    }
    return result.rets[0].asInt();
}

bool sfallHookInvenWield(Object* critter, Object* item, int slot, bool wield, bool remove)
{
    if (!sfallHookHasScripts(SFALL_HOOK_INVENWIELD)) {
        return true;
    }
    ProgramValue args[] = { ptrValue(critter), ptrValue(item), ProgramValue(slot), ProgramValue(wield ? 1 : 0), ProgramValue(remove ? 1 : 0) };
    HookFrame result;
    if (!runHook(SFALL_HOOK_INVENWIELD, args, 5, result)) {
        return true;
    }
    return result.retCount == 0 || result.rets[0].asInt() == -1;
}

bool sfallHookCanUseWeapon(bool engineResult, Object* critter, Object* weapon, int hitMode)
{
    if (!sfallHookHasScripts(SFALL_HOOK_CANUSEWEAPON)) {
        return engineResult;
    }
    ProgramValue args[] = { ptrValue(critter), ptrValue(weapon), ProgramValue(hitMode), ProgramValue(engineResult ? 1 : 0) };
    HookFrame result;
    if (!runHook(SFALL_HOOK_CANUSEWEAPON, args, 4, result) || result.retCount < 1) {
        return engineResult;
    }
    return result.rets[0].asInt() != 0;
}

int sfallHookInventoryMove(int slot, Object* item, Object* replaced)
{
    if (!sfallHookHasScripts(SFALL_HOOK_INVENTORYMOVE)) {
        return -1;
    }
    ProgramValue args[] = { ProgramValue(slot), ptrValue(item), ptrValue(replaced) };
    HookFrame result;
    if (!runHook(SFALL_HOOK_INVENTORYMOVE, args, 3, result) || result.retCount < 1) {
        return -1;
    }
    return result.rets[0].asInt();
}

void sfallHookCombatDamage(Attack* attack, int rounds, int multiplier)
{
    if (!sfallHookHasScripts(SFALL_HOOK_COMBATDAMAGE)) {
        return;
    }
    ProgramValue attackValue;
    attackValue.opcode = VALUE_TYPE_PTR;
    attackValue.pointerValue = attack;

    ProgramValue args[] = {
        ptrValue(attack->defender),
        ptrValue(attack->attacker),
        ProgramValue(attack->defenderDamage),
        ProgramValue(attack->attackerDamage),
        ProgramValue(attack->defenderFlags),
        ProgramValue(attack->attackerFlags),
        ptrValue(attack->weapon),
        ProgramValue(attack->defenderHitLocation),
        ProgramValue(multiplier),
        ProgramValue(rounds),
        ProgramValue(attack->defenderKnockback),
        ProgramValue(attack->hitMode),
        attackValue,
    };
    HookFrame result;
    if (!runHook(SFALL_HOOK_COMBATDAMAGE, args, 13, result) || result.retCount < 1) {
        return;
    }

    attack->defenderDamage = result.rets[0].asInt();
    if (result.retCount > 1) {
        attack->attackerDamage = result.rets[1].asInt();
    }
    if (result.retCount > 2) {
        attack->defenderFlags = result.rets[2].asInt();
    }
    if (result.retCount > 3) {
        attack->attackerFlags = result.rets[3].asInt();
    }
    if (result.retCount > 4) {
        attack->defenderKnockback = result.rets[4].asInt();
    }
    // sfall: an instadeath critical never ends at zero damage.
    if ((attack->defenderFlags & DAM_DEAD) != 0 && attack->defenderDamage <= 0) {
        attack->defenderDamage = 1;
    }
}

void sfallHookGameModeChange(int exitEvent, int previousMode)
{
    if (!sfallHookHasScripts(SFALL_HOOK_GAMEMODECHANGE)) {
        return;
    }
    ProgramValue args[] = { ProgramValue(exitEvent), ProgramValue(previousMode) };
    HookFrame result;
    runHook(SFALL_HOOK_GAMEMODECHANGE, args, 2, result);
}

int sfallHookSetGlobalVar(int index, int value)
{
    if (!sfallHookHasScripts(SFALL_HOOK_SETGLOBALVAR)) {
        return value;
    }
    ProgramValue args[] = { ProgramValue(index), ProgramValue(value) };
    HookFrame result;
    if (!runHook(SFALL_HOOK_SETGLOBALVAR, args, 2, result) || result.retCount < 1) {
        return value;
    }
    return result.rets[0].asInt();
}

} // namespace fallout
