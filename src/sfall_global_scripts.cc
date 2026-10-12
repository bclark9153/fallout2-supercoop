#include "sfall_global_scripts.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "db.h"
#include "input.h"
#include "platform_compat.h"
#include "sfall_hooks.h"
#include "scripts.h"
#include "server_loop.h"
#include "server_players.h"
#include "sfall_config.h"

namespace fallout {

struct GlobalScript {
    Program* program = nullptr;
    int procs[SCRIPT_PROC_COUNT] = { 0 };
    int repeat = 0;
    int count = 0;
    int mode = 0;
    bool once = true;

    // Co-op (dedicated server). sfall global scripts were written for one
    // player: their timers act on dude_obj. A PER-PLAYER script's timer runs
    // once for every player in the world, as that player. Any other script's
    // timer runs as its OWNER: the last player whose event (hook) made it change
    // the world — so the player who became addicted is the one the addiction
    // timer later shakes and relieves. No owner yet = the host, as before.
    bool perPlayer = false;
    int ownerActorId = -1;
};

struct GlobalScriptsState {
    std::vector<std::string> paths;
    std::vector<GlobalScript> globalScripts;
};

// Scripts whose repeating body only reads and writes "the player's" own state.
// Overridable from ddraw.ini: [Coop] PerPlayerGlobalScripts=name,name,...
static constexpr const char* kDefaultPerPlayerGlobalScripts = "gl_k_crippl,gl_k_fipswd";

static bool sfall_gl_scr_name_in_list(const std::string& path, const char* list)
{
    // path is "scripts\\gl_k_crippl.int"; compare the bare name.
    size_t slash = path.find_last_of("\\/");
    std::string name = path.substr(slash == std::string::npos ? 0 : slash + 1);
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) {
        name.resize(dot);
    }

    const char* curr = list;
    while (curr != nullptr && *curr != '\0') {
        const char* end = strchr(curr, ',');
        size_t len = end != nullptr ? static_cast<size_t>(end - curr) : strlen(curr);
        while (len > 0 && (*curr == ' ' || *curr == '\t')) {
            curr++;
            len--;
        }
        while (len > 0 && (curr[len - 1] == ' ' || curr[len - 1] == '\t')) {
            len--;
        }
        if (len == name.size() && compat_strnicmp(curr, name.c_str(), len) == 0) {
            return true;
        }
        curr = end != nullptr ? end + 1 : nullptr;
    }
    return false;
}

// Runs one global script procedure the way a co-op world needs it (see
// GlobalScript). Off a dedicated server this is a plain _executeProcedure.
static void sfall_gl_scr_run_proc(GlobalScript& scr, int proc)
{
    if (!serverDedicatedActive() || playerActorCount() < 2) {
        _executeProcedure(scr.program, proc);
        return;
    }

    if (scr.perPlayer) {
        // Snapshot the roster first: a timer body may change it (a death).
        std::vector<Object*> actors;
        for (int slot = 0; slot < playerActorCount(); slot++) {
            Object* actor = playerActorAt(slot);
            if (actor != nullptr && playerActorOnline(slot)) {
                actors.push_back(actor);
            }
        }
        for (Object* actor : actors) {
            ServerActorScope scope(actor);
            _executeProcedure(scr.program, proc);
        }
        return;
    }

    Object* owner = nullptr;
    if (scr.ownerActorId != -1) {
        for (int slot = 0; slot < playerActorCount(); slot++) {
            Object* actor = playerActorAt(slot);
            if (actor != nullptr && actor->id == scr.ownerActorId && playerActorOnline(slot)) {
                owner = actor;
                break;
            }
        }
    }
    ServerActorScope scope(owner); // null owner = no scope = the host
    _executeProcedure(scr.program, proc);
}

static GlobalScriptsState* state = nullptr;

bool sfall_gl_scr_init()
{
    state = new (std::nothrow) GlobalScriptsState();
    if (state == nullptr) {
        return false;
    }

    char* paths;
    configGetString(&gSfallConfig, SFALL_CONFIG_SCRIPTS_KEY, SFALL_CONFIG_GLOBAL_SCRIPT_PATHS, &paths);

    char* curr = paths;
    while (curr != nullptr && *curr != '\0') {
        char* end = strchr(curr, ',');
        if (end != nullptr) {
            *end = '\0';
        }

        char drive[COMPAT_MAX_DRIVE];
        char dir[COMPAT_MAX_DIR];
        compat_splitpath(curr, drive, dir, nullptr, nullptr);

        char** files;
        int filesLength = fileNameListInit(curr, &files, 0, 0);
        if (filesLength != 0) {
            for (int index = 0; index < filesLength; index++) {
                char path[COMPAT_MAX_PATH];
                compat_makepath(path, drive, dir, files[index], nullptr);

                state->paths.push_back(std::string { path });
            }

            fileNameListFree(&files, 0);
        }

        if (end != nullptr) {
            *end = ',';
            curr = end + 1;
        } else {
            curr = nullptr;
        }
    }

    std::sort(state->paths.begin(), state->paths.end());

    return true;
}

void sfall_gl_scr_reset()
{
    if (state != nullptr) {
        sfall_gl_scr_remove_all();
    }
}

void sfall_gl_scr_exit()
{
    if (state != nullptr) {
        sfall_gl_scr_remove_all();

        delete state;
        state = nullptr;
    }
}

void sfall_gl_scr_exec_start_proc()
{
    char* perPlayerList = nullptr;
    if (!configGetString(&gSfallConfig, "Coop", "PerPlayerGlobalScripts", &perPlayerList) || perPlayerList == nullptr) {
        perPlayerList = const_cast<char*>(kDefaultPerPlayerGlobalScripts);
    }

    for (auto& path : state->paths) {
        Program* program = programCreateByPath(path.c_str());
        if (program != nullptr) {
            GlobalScript scr;
            scr.program = program;
            scr.perPlayer = sfall_gl_scr_name_in_list(path, perPlayerList);

            for (int action = 0; action < SCRIPT_PROC_COUNT; action++) {
                scr.procs[action] = programFindProcedure(program, gScriptProcNames[action]);
            }

            state->globalScripts.push_back(std::move(scr));

            _interpret(program, -1);
        }
    }

    tickersAdd(sfall_gl_scr_process_input);
}

void sfall_gl_scr_remove_all()
{
    tickersRemove(sfall_gl_scr_process_input);

    // Hook registrations point at these programs.
    sfallHooksReset();

    for (auto& scr : state->globalScripts) {
        programFree(scr.program);
    }

    state->globalScripts.clear();
}

void sfall_gl_scr_exec_map_update_scripts(int action)
{
    for (auto& scr : state->globalScripts) {
        if (scr.mode == 0 || scr.mode == 3) {
            if (scr.procs[action] != -1) {
                _executeProcedure(scr.program, scr.procs[action]);
            }
        }
    }
}

static void sfall_gl_scr_process_simple(int mode1, int mode2, int frames = 1)
{
    for (auto& scr : state->globalScripts) {
        if (scr.repeat != 0 && (scr.mode == mode1 || scr.mode == mode2)) {
            scr.count += frames;
            if (scr.count >= scr.repeat) {
                sfall_gl_scr_run_proc(scr, scr.procs[SCRIPT_PROC_START]);
                scr.count = 0;
            }
        }
    }
}

void sfall_gl_scr_process_main()
{
    sfall_gl_scr_process_simple(0, 3);
}

void sfall_gl_scr_process_input()
{
    sfall_gl_scr_process_simple(1, 1);
}

void sfall_gl_scr_process_worldmap()
{
    sfall_gl_scr_process_simple(2, 3);
}

// Dedicated server: one beat stands for |frames| rendered frames of the
// client's loop (sfall counts set_global_script_repeat in frames).
void sfall_gl_scr_process_main_frames(int frames)
{
    if (state != nullptr) {
        sfall_gl_scr_process_simple(0, 3, frames);
    }
}

void sfall_gl_scr_process_worldmap_frames(int frames)
{
    if (state != nullptr) {
        sfall_gl_scr_process_simple(2, 3, frames);
    }
}

static GlobalScript* sfall_gl_scr_map_program_to_scr(Program* program)
{
    auto it = std::find_if(state->globalScripts.begin(),
        state->globalScripts.end(),
        [&program](const GlobalScript& scr) {
            return scr.program == program;
        });
    return it != state->globalScripts.end() ? &(*it) : nullptr;
}

void sfall_gl_scr_set_repeat(Program* program, int frames)
{
    GlobalScript* scr = sfall_gl_scr_map_program_to_scr(program);
    if (scr != nullptr) {
        scr->repeat = frames;
    }
}

void sfall_gl_scr_set_type(Program* program, int type)
{
    if (type < 0 || type > 3) {
        return;
    }

    GlobalScript* scr = sfall_gl_scr_map_program_to_scr(program);
    if (scr != nullptr) {
        scr->mode = type;
    }
}

void sfall_gl_scr_set_owner(Program* program, Object* actor)
{
    GlobalScript* scr = state != nullptr ? sfall_gl_scr_map_program_to_scr(program) : nullptr;
    if (scr != nullptr && actor != nullptr && !scr->perPlayer) {
        scr->ownerActorId = actor->id;
    }
}

bool sfall_gl_scr_is_per_player(Program* program)
{
    GlobalScript* scr = state != nullptr ? sfall_gl_scr_map_program_to_scr(program) : nullptr;
    return scr != nullptr && scr->perPlayer;
}

bool sfall_gl_scr_is_global(Program* program)
{
    return state != nullptr && sfall_gl_scr_map_program_to_scr(program) != nullptr;
}

bool sfall_gl_scr_is_loaded(Program* program)
{
    GlobalScript* scr = sfall_gl_scr_map_program_to_scr(program);
    if (scr != nullptr) {
        if (scr->once) {
            scr->once = false;
            return true;
        }

        return false;
    }

    // Not a global script.
    return true;
}

void sfall_gl_scr_update(int burstSize)
{
    for (auto& scr : state->globalScripts) {
        _interpret(scr.program, burstSize);
    }
}

} // namespace fallout
