#include "sfall_ext.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "db.h"
#include "debug.h"
#include "object.h"
#include "platform_compat.h"
#include <algorithm>
#include "critter.h"
#include "interface.h"
#include "presenter.h"
#include "server_loop.h"
#include "server_players.h"

namespace fallout {

bool gSfallRegAnimCombatCheck = true;

// Vanilla interface-bar tags are 0..4 (sneak, poisoned, radiated, level,
// addict); sfall numbers custom tags from 5. add_iface_tag hands out the next.
static constexpr int kFirstCustomIfaceTag = 5;
static constexpr int kMaxIfaceTag = 126;

static constexpr int kRadiationTagThreshold = 65; // interface.cc RADATION_INDICATOR_THRESHOLD

static std::unordered_set<int> gNoBurstCritterIds;
// Custom tags per actor object id, ascending.
static std::unordered_map<int, std::vector<int>> gCustomIfaceTags;
static int gNextCustomIfaceTag = kFirstCustomIfaceTag;

struct FakePerk {
    std::string name;
    int level;
    int image;
    std::string desc;
};
static std::vector<FakePerk> gFakePerks;

struct FsFile {
    std::string name;
    std::vector<unsigned char> data;
    size_t pos = 0;
};
static std::vector<FsFile> gFsFiles;

static std::unordered_map<std::string, int> gExtraMsgFiles;

void sfallExtReset()
{
    gSfallRegAnimCombatCheck = true;
    gNoBurstCritterIds.clear();
    gCustomIfaceTags.clear();
    gNextCustomIfaceTag = kFirstCustomIfaceTag;
    gFakePerks.clear();
    gExtraMsgFiles.clear();
    // sfall keeps fs files for the whole session (global scripts copy them
    // once in their start procedure and look them up by name afterwards).
}

void sfallSetCritterNoBurst(Object* critter, bool on)
{
    if (critter == nullptr) {
        return;
    }
    if (on) {
        gNoBurstCritterIds.insert(critter->id);
    } else {
        gNoBurstCritterIds.erase(critter->id);
    }
}

bool sfallCritterNoBurst(Object* critter)
{
    return critter != nullptr && !gNoBurstCritterIds.empty() && gNoBurstCritterIds.count(critter->id) != 0;
}

// sfall tags 0/3/4 are the player's own flags (proto critterFlags bits),
// which the engine already keeps per player and streams with the sheet row.
static int ifaceTagDudeState(int tag)
{
    switch (tag) {
    case 0:
        return DUDE_STATE_SNEAKING;
    case 3:
        return DUDE_STATE_LEVEL_UP_AVAILABLE;
    case 4:
        return DUDE_STATE_ADDICTED;
    }
    return -1;
}

static void ifaceTagsChanged(Object* actor)
{
    if (serverDedicatedActive()) {
        const std::vector<int>& tags = sfallIfaceCustomTagsFor(actor);
        presenter()->ifaceTags(actor->netId, tags.data(), static_cast<int>(tags.size()));
    } else {
        indicatorBarRefresh();
    }
}

void sfallIfaceTagShow(int tag)
{
    Object* actor = gDude;
    if (actor == nullptr) {
        return;
    }

    int state = ifaceTagDudeState(tag);
    if (state != -1) {
        if (!dudeHasState(state, actor)) {
            dudeEnableState(state, actor);
            if (!serverDedicatedActive()) {
                indicatorBarRefresh();
            }
        }
        return;
    }

    if (tag < kFirstCustomIfaceTag || tag > kMaxIfaceTag) {
        return;
    }

    std::vector<int>& tags = gCustomIfaceTags[actor->id];
    auto it = std::lower_bound(tags.begin(), tags.end(), tag);
    if (it == tags.end() || *it != tag) {
        tags.insert(it, tag);
        ifaceTagsChanged(actor);
    }
}

void sfallIfaceTagHide(int tag)
{
    Object* actor = gDude;
    if (actor == nullptr) {
        return;
    }

    int state = ifaceTagDudeState(tag);
    if (state != -1) {
        if (dudeHasState(state, actor)) {
            dudeDisableState(state, actor);
            if (!serverDedicatedActive()) {
                indicatorBarRefresh();
            }
        }
        return;
    }

    auto entry = gCustomIfaceTags.find(actor->id);
    if (entry == gCustomIfaceTags.end()) {
        return;
    }
    auto it = std::lower_bound(entry->second.begin(), entry->second.end(), tag);
    if (it != entry->second.end() && *it == tag) {
        entry->second.erase(it);
        ifaceTagsChanged(actor);
    }
}

bool sfallIfaceTagActive(int tag)
{
    Object* actor = gDude;
    if (actor == nullptr) {
        return false;
    }

    int state = ifaceTagDudeState(tag);
    if (state != -1) {
        return dudeHasState(state, actor);
    }
    if (tag == 1) {
        return critterGetPoison(actor) > 0;
    }
    if (tag == 2) {
        return critterGetRadiation(actor) > kRadiationTagThreshold;
    }

    const std::vector<int>& tags = sfallIfaceCustomTagsFor(actor);
    return std::binary_search(tags.begin(), tags.end(), tag);
}

const std::vector<int>& sfallIfaceCustomTagsFor(Object* actor)
{
    static const std::vector<int> kNone;
    if (actor == nullptr) {
        return kNone;
    }
    auto it = gCustomIfaceTags.find(actor->id);
    return it != gCustomIfaceTags.end() ? it->second : kNone;
}

void sfallIfaceCustomTagsSet(Object* actor, const int* tags, int count)
{
    if (actor == nullptr) {
        return;
    }
    std::vector<int> sorted(tags, tags + count);
    std::sort(sorted.begin(), sorted.end());
    gCustomIfaceTags[actor->id] = std::move(sorted);
    indicatorBarRefresh();
}

void sfallIfaceCustomTagsEmitAll()
{
    for (int slot = 0; slot < playerActorCount(); slot++) {
        Object* actor = playerActorAt(slot);
        if (actor != nullptr) {
            const std::vector<int>& tags = sfallIfaceCustomTagsFor(actor);
            presenter()->ifaceTags(actor->netId, tags.data(), static_cast<int>(tags.size()));
        }
    }
}

int sfallIfaceTagAdd()
{
    if (gNextCustomIfaceTag > kMaxIfaceTag) {
        return -1;
    }
    return gNextCustomIfaceTag++;
}

void sfallIfaceTagSetText(int tag, const char* text, int color)
{
    debugPrint("sfall: iface tag %d text '%s' color %d (not drawn in co-op yet)\n", tag, text != nullptr ? text : "", color);
}

void sfallSetFakePerk(const char* name, int level, int image, const char* desc)
{
    if (name == nullptr) {
        return;
    }
    for (auto it = gFakePerks.begin(); it != gFakePerks.end(); ++it) {
        if (compat_stricmp(it->name.c_str(), name) == 0) {
            if (level <= 0) {
                gFakePerks.erase(it);
            } else {
                it->level = level;
                it->image = image;
                it->desc = desc != nullptr ? desc : "";
            }
            return;
        }
    }
    if (level > 0) {
        gFakePerks.push_back(FakePerk { name, level, image, desc != nullptr ? desc : "" });
    }
}

int sfallExtraMsgFileGet(const char* path)
{
    auto it = gExtraMsgFiles.find(path);
    return it != gExtraMsgFiles.end() ? it->second : 0;
}

void sfallExtraMsgFileSet(const char* path, int id)
{
    gExtraMsgFiles[path] = id;
}

int sfallFakePerkLevel(const char* name)
{
    for (auto& perk : gFakePerks) {
        if (compat_stricmp(perk.name.c_str(), name) == 0) {
            return perk.level;
        }
    }
    return 0;
}

static FsFile* fsGet(int id)
{
    if (id < 0 || static_cast<size_t>(id) >= gFsFiles.size()) {
        return nullptr;
    }
    return &gFsFiles[id];
}

int sfallFsFind(const char* path)
{
    for (size_t i = 0; i < gFsFiles.size(); i++) {
        if (compat_stricmp(gFsFiles[i].name.c_str(), path) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int sfallFsCopy(const char* path, const char* source)
{
    int existing = sfallFsFind(path);
    if (existing != -1) {
        return existing;
    }

    File* stream = fileOpen(source, "rb");
    if (stream == nullptr) {
        return -1;
    }

    FsFile file;
    file.name = path;
    int size = fileGetSize(stream);
    if (size > 0) {
        file.data.resize(static_cast<size_t>(size));
        fileRead(file.data.data(), 1, file.data.size(), stream);
    }
    fileClose(stream);

    gFsFiles.push_back(std::move(file));
    return static_cast<int>(gFsFiles.size() - 1);
}

void sfallFsSeek(int id, int pos)
{
    FsFile* file = fsGet(id);
    if (file != nullptr && pos >= 0 && static_cast<size_t>(pos) <= file->data.size()) {
        file->pos = static_cast<size_t>(pos);
    }
}

// sfall's fs_read/write helpers are big-endian, like the game's own files.
static bool fsCanAccess(FsFile* file, size_t count)
{
    return file != nullptr && file->pos + count <= file->data.size();
}

int sfallFsReadByte(int id)
{
    FsFile* file = fsGet(id);
    if (!fsCanAccess(file, 1)) {
        return 0;
    }
    return file->data[file->pos++];
}

int sfallFsReadShort(int id)
{
    FsFile* file = fsGet(id);
    if (!fsCanAccess(file, 2)) {
        return 0;
    }
    short value = static_cast<short>((file->data[file->pos] << 8) | file->data[file->pos + 1]);
    file->pos += 2;
    return value;
}

int sfallFsReadInt(int id)
{
    FsFile* file = fsGet(id);
    if (!fsCanAccess(file, 4)) {
        return 0;
    }
    unsigned char* p = &file->data[file->pos];
    file->pos += 4;
    return static_cast<int>((static_cast<unsigned int>(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}

void sfallFsWriteByte(int id, int value)
{
    FsFile* file = fsGet(id);
    if (fsCanAccess(file, 1)) {
        file->data[file->pos++] = static_cast<unsigned char>(value);
    }
}

void sfallFsWriteShort(int id, int value)
{
    FsFile* file = fsGet(id);
    if (fsCanAccess(file, 2)) {
        file->data[file->pos++] = static_cast<unsigned char>((value >> 8) & 0xFF);
        file->data[file->pos++] = static_cast<unsigned char>(value & 0xFF);
    }
}

void sfallFsWriteInt(int id, int value)
{
    FsFile* file = fsGet(id);
    if (fsCanAccess(file, 4)) {
        for (int shift = 24; shift >= 0; shift -= 8) {
            file->data[file->pos++] = static_cast<unsigned char>((value >> shift) & 0xFF);
        }
    }
}

int sfallFsSize(int id)
{
    FsFile* file = fsGet(id);
    return file != nullptr ? static_cast<int>(file->data.size()) : 0;
}

int sfallFsPos(int id)
{
    FsFile* file = fsGet(id);
    return file != nullptr ? static_cast<int>(file->pos) : 0;
}

} // namespace fallout
