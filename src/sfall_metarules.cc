#include "sfall_metarules.h"

#include <math.h>
#include <string.h>

#include <ctype.h>

#include <string>

#include "combat.h"
#include "debug.h"
#include "game.h"
#include "game_dialog.h"
#include "game_mouse.h"
#include "interface.h"
#include "inventory.h"
#include "object.h"
#include "message.h"
#include "server_players.h"
#include "sfall_arrays.h"
#include "sfall_ext.h"
#include "sfall_hooks.h"
#include "sfall_ini.h"
#include "text_font.h"
#include "tile.h"
#include "window.h"
#include "worldmap.h"

namespace fallout {

typedef void(MetaruleHandler)(Program* program, int args);

// Simplified cousin of `SfallMetarule` from Sfall.
typedef struct MetaruleInfo {
    const char* name;
    MetaruleHandler* handler;
    int minArgs;
    int maxArgs;
} MetaruleInfo;

static void mf_car_gas_amount(Program* program, int args);
static void mf_combat_data(Program* program, int args);
static void mf_critter_inven_obj2(Program* program, int args);
static void mf_dialog_obj(Program* program, int args);
static void mf_get_cursor_mode(Program* program, int args);
static void mf_get_flags(Program* program, int args);
static void mf_get_object_data(Program* program, int args);
static void mf_get_text_width(Program* program, int args);
static void mf_intface_redraw(Program* program, int args);
static void mf_loot_obj(Program* program, int args);
static void mf_metarule_exist(Program* program, int args);
static void mf_outlined_object(Program* program, int args);
static void mf_set_cursor_mode(Program* program, int args);
static void mf_set_flags(Program* program, int args);
static void mf_set_ini_setting(Program* program, int args);
static void mf_set_outline(Program* program, int args);
static void mf_show_window(Program* program, int args);
static void mf_tile_refresh_display(Program* program, int args);
static void mf_add_extra_msg_file(Program* program, int args);
static void mf_add_iface_tag(Program* program, int args);
static void mf_floor2(Program* program, int args);
static void mf_get_ini_section(Program* program, int args);
static void mf_get_ini_sections(Program* program, int args);
static void mf_get_sfall_arg_at(Program* program, int args);
static void mf_message_box(Program* program, int args);
static void mf_real_dude_obj(Program* program, int args);
static void mf_set_dude_obj(Program* program, int args);
static void mf_set_iface_tag_text(Program* program, int args);
static void mf_set_object_data(Program* program, int args);

constexpr MetaruleInfo kMetarules[] = {
    { "car_gas_amount", mf_car_gas_amount, 0, 0 },
    { "combat_data", mf_combat_data, 0, 0 },
    { "critter_inven_obj2", mf_critter_inven_obj2, 2, 2 },
    { "dialog_obj", mf_dialog_obj, 0, 0 },
    { "get_cursor_mode", mf_get_cursor_mode, 0, 0 },
    { "get_flags", mf_get_flags, 1, 1 },
    { "get_object_data", mf_get_object_data, 2, 2 },
    { "get_text_width", mf_get_text_width, 1, 1 },
    { "intface_redraw", mf_intface_redraw, 0, 1 },
    { "loot_obj", mf_loot_obj, 0, 0 },
    { "metarule_exist", mf_metarule_exist, 1, 1 },
    { "outlined_object", mf_outlined_object, 0, 0 },
    { "set_cursor_mode", mf_set_cursor_mode, 1, 1 },
    { "set_flags", mf_set_flags, 2, 2 },
    { "set_ini_setting", mf_set_ini_setting, 2, 2 },
    { "set_outline", mf_set_outline, 2, 2 },
    { "show_window", mf_show_window, 0, 1 },
    { "tile_refresh_display", mf_tile_refresh_display, 0, 0 },
    // Restoration Project compatibility (sfall 4.x).
    { "add_extra_msg_file", mf_add_extra_msg_file, 1, 2 },
    { "add_iface_tag", mf_add_iface_tag, 0, 0 },
    { "floor2", mf_floor2, 1, 1 },
    { "get_ini_section", mf_get_ini_section, 2, 2 },
    { "get_ini_sections", mf_get_ini_sections, 1, 1 },
    { "get_sfall_arg_at", mf_get_sfall_arg_at, 1, 1 },
    { "message_box", mf_message_box, 1, 4 },
    { "real_dude_obj", mf_real_dude_obj, 0, 0 },
    { "set_dude_obj", mf_set_dude_obj, 1, 1 },
    { "set_iface_tag_text", mf_set_iface_tag_text, 3, 3 },
    { "set_object_data", mf_set_object_data, 3, 3 },
};

constexpr int kMetarulesMax = sizeof(kMetarules) / sizeof(kMetarules[0]);

void mf_car_gas_amount(Program* program, int args)
{
    programStackPushInteger(program, wmCarGasAmount());
}

void mf_combat_data(Program* program, int args)
{
    if (isInCombat()) {
        programStackPushPointer(program, combat_get_data());
    } else {
        programStackPushPointer(program, nullptr);
    }
}

void mf_critter_inven_obj2(Program* program, int args)
{
    int slot = programStackPopInteger(program);
    Object* obj = static_cast<Object*>(programStackPopPointer(program));

    switch (slot) {
    case 0:
        programStackPushPointer(program, critterGetArmor(obj));
        break;
    case 1:
        programStackPushPointer(program, critterGetItem2(obj));
        break;
    case 2:
        programStackPushPointer(program, critterGetItem1(obj));
        break;
    case -2:
        programStackPushInteger(program, obj->data.inventory.length);
        break;
    default:
        programFatalError("mf_critter_inven_obj2: invalid type");
    }
}

void mf_dialog_obj(Program* program, int args)
{
    if (GameMode::isInGameMode(GameMode::kDialog)) {
        programStackPushPointer(program, gGameDialogSpeaker);
    } else {
        programStackPushPointer(program, nullptr);
    }
}

void mf_get_cursor_mode(Program* program, int args)
{
    programStackPushInteger(program, gameMouseGetMode());
}

void mf_get_flags(Program* program, int args)
{
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    programStackPushInteger(program, object->flags);
}

void mf_get_object_data(Program* program, int args)
{
    size_t offset = static_cast<size_t>(programStackPopInteger(program));
    void* ptr = programStackPopPointer(program);

    if (offset % 4 != 0) {
        programFatalError("mf_get_object_data: bad offset %d", offset);
    }

    int value = *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(ptr) + offset);
    programStackPushInteger(program, value);
}

void mf_get_text_width(Program* program, int args)
{
    const char* string = programStackPopString(program);
    programStackPushInteger(program, fontGetStringWidth(string));
}

void mf_intface_redraw(Program* program, int args)
{
    if (args == 0) {
        interfaceBarRefresh();
    } else {
        // TODO: Incomplete.
        programFatalError("mf_intface_redraw: not implemented");
    }

    programStackPushInteger(program, -1);
}

void mf_loot_obj(Program* program, int args)
{
    if (GameMode::isInGameMode(GameMode::kInventory)) {
        programStackPushPointer(program, inven_get_current_target_obj());
    } else {
        programStackPushPointer(program, nullptr);
    }
}

void mf_metarule_exist(Program* program, int args)
{
    const char* metarule = programStackPopString(program);

    for (int index = 0; index < kMetarulesMax; index++) {
        if (strcmp(kMetarules[index].name, metarule) == 0) {
            programStackPushInteger(program, 1);
            return;
        }
    }

    programStackPushInteger(program, 0);
}

void mf_outlined_object(Program* program, int args)
{
    programStackPushPointer(program, gmouse_get_outlined_object());
}

void mf_set_cursor_mode(Program* program, int args)
{
    int mode = programStackPopInteger(program);
    gameMouseSetMode(mode);
    programStackPushInteger(program, -1);
}

void mf_set_flags(Program* program, int args)
{
    int flags = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));

    object->flags = flags;

    programStackPushInteger(program, -1);
}

void mf_set_ini_setting(Program* program, int args)
{
    ProgramValue value = programStackPopValue(program);
    const char* triplet = programStackPopString(program);

    if (value.isString()) {
        const char* stringValue = programGetString(program, value.opcode, value.integerValue);
        if (!sfall_ini_set_string(triplet, stringValue)) {
            debugPrint("set_ini_setting: unable to write '%s' to '%s'",
                stringValue,
                triplet);
        }
    } else {
        int integerValue = value.asInt();
        if (!sfall_ini_set_int(triplet, integerValue)) {
            debugPrint("set_ini_setting: unable to write '%d' to '%s'",
                integerValue,
                triplet);
        }
    }

    programStackPushInteger(program, -1);
}

void mf_set_outline(Program* program, int args)
{
    int outline = programStackPopInteger(program);
    Object* object = static_cast<Object*>(programStackPopPointer(program));
    object->outline = outline;
    programStackPushInteger(program, -1);
}

void mf_show_window(Program* program, int args)
{
    if (args == 0) {
        _windowShow();
    } else if (args == 1) {
        const char* windowName = programStackPopString(program);
        if (!_windowShowNamed(windowName)) {
            debugPrint("show_window: window '%s' is not found", windowName);
        }
    }

    programStackPushInteger(program, -1);
}

void mf_tile_refresh_display(Program* program, int args)
{
    tileWindowRefresh();
    programStackPushInteger(program, -1);
}

// add_extra_msg_file(name[, number]): loads text\<lang>\game\<name> as an
// extra message list and returns its id for message_str_game. The same file
// returns the same id (global scripts re-run their start proc on every load).
void mf_add_extra_msg_file(Program* program, int args)
{
    if (args == 2) {
        programStackPopValue(program); // deprecated number argument, ignored as in sfall
    }
    const char* name = programStackPopString(program);

    std::string path = std::string("game\\") + name;
    for (auto& ch : path) {
        ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
    }

    int existing = sfallExtraMsgFileGet(path.c_str());
    if (existing > 0) {
        programStackPushInteger(program, existing);
        return;
    }

    int id = messageListRepositoryAddExtra(0, path.c_str());
    if (id > 0) {
        sfallExtraMsgFileSet(path.c_str(), id);
    } else {
        debugPrint("%s: add_extra_msg_file('%s') failed (%d)\n", program->name, name, id);
    }
    programStackPushInteger(program, id);
}

void mf_add_iface_tag(Program* program, int args)
{
    programStackPushInteger(program, sfallIfaceTagAdd());
}

void mf_floor2(Program* program, int args)
{
    ProgramValue value = programStackPopValue(program);
    programStackPushInteger(program, static_cast<int>(floorf(value.asFloat())));
}

// get_ini_section(file, section) -> temp associative array of key -> value.
void mf_get_ini_section(Program* program, int args)
{
    const char* sectionName = programStackPopString(program);
    const char* fileName = programStackPopString(program);

    ArrayId arrayId = CreateTempArray(-1, SFALL_ARRAYFLAG_ASSOC);

    Config config;
    if (sfall_ini_load(fileName, &config)) {
        int sectionIndex = dictionaryGetIndexByKey(&config, sectionName);
        if (sectionIndex != -1) {
            ConfigSection* section = reinterpret_cast<ConfigSection*>(config.entries[sectionIndex].value);
            for (int index = 0; index < section->entriesLength; index++) {
                DictionaryEntry* entry = &(section->entries[index]);
                char** value = reinterpret_cast<char**>(entry->value);
                SetArrayAssocString(arrayId, entry->key, *value != nullptr ? *value : "");
            }
        }
        configFree(&config);
    }

    programStackPushInteger(program, static_cast<int>(arrayId));
}

// get_ini_sections(file) -> temp array of section names.
void mf_get_ini_sections(Program* program, int args)
{
    const char* fileName = programStackPopString(program);

    Config config;
    if (!sfall_ini_load(fileName, &config)) {
        programStackPushInteger(program, static_cast<int>(CreateTempArray(0, 0)));
        return;
    }

    ArrayId arrayId = CreateTempArray(config.entriesLength, 0);
    for (int index = 0; index < config.entriesLength; index++) {
        SetArrayIndexString(arrayId, index, config.entries[index].key);
    }
    configFree(&config);

    programStackPushInteger(program, static_cast<int>(arrayId));
}

void mf_get_sfall_arg_at(Program* program, int args)
{
    int index = programStackPopInteger(program);
    ProgramValue value = sfallHookGetArgAt(index);
    programStackPushValue(program, value);
}

// message_box(text[, flags, color1, color2]). The dedicated server has nobody
// to show a dialog to: log it, broadcast nothing, answer "yes" (1).
void mf_message_box(Program* program, int args)
{
    for (int index = 1; index < args; index++) {
        programStackPopValue(program);
    }
    const char* text = programStackPopString(program);
    debugPrint("%s: message_box: %s\n", program->name, text);
    programStackPushInteger(program, 1);
}

// real_dude_obj: party control never swaps the dude here, so it is dude_obj.
void mf_real_dude_obj(Program* program, int args)
{
    programStackPushPointer(program, scriptContextDude(program));
}

// set_dude_obj: sfall's party control (take over a companion). Swapping the
// global dude would hand one player's controls to every client; refused.
void mf_set_dude_obj(Program* program, int args)
{
    programStackPopValue(program);
    debugPrint("%s: set_dude_obj refused (not supported in co-op)\n", program->name);
    programStackPushInteger(program, -1);
}

void mf_set_iface_tag_text(Program* program, int args)
{
    int color = programStackPopInteger(program);
    const char* text = programStackPopString(program);
    int tag = programStackPopInteger(program);
    sfallIfaceTagSetText(tag, text, color);
    programStackPushInteger(program, -1);
}

void mf_set_object_data(Program* program, int args)
{
    int value = programStackPopInteger(program);
    size_t offset = static_cast<size_t>(programStackPopInteger(program));
    void* ptr = programStackPopPointer(program);

    if (ptr == nullptr || offset % 4 != 0 || offset >= sizeof(Object)) {
        programFatalError("mf_set_object_data: bad offset %d", static_cast<int>(offset));
    }

    *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(ptr) + offset) = value;
    programStackPushInteger(program, -1);
}

void sfall_metarule(Program* program, int args)
{
    static ProgramValue values[6];

    for (int index = 0; index < args; index++) {
        values[index] = programStackPopValue(program);
    }

    const char* metarule = programStackPopString(program);

    for (int index = 0; index < args; index++) {
        programStackPushValue(program, values[index]);
    }

    int metaruleIndex = -1;
    for (int index = 0; index < kMetarulesMax; index++) {
        if (strcmp(kMetarules[index].name, metarule) == 0) {
            metaruleIndex = index;
            break;
        }
    }

    if (metaruleIndex == -1) {
        programFatalError("op_sfall_func: '%s' is not implemented", metarule);
    }

    if (args < kMetarules[metaruleIndex].minArgs || args > kMetarules[metaruleIndex].maxArgs) {
        programFatalError("op_sfall_func: '%s': invalid number of args", metarule);
    }

    kMetarules[metaruleIndex].handler(program, args);
}

} // namespace fallout
