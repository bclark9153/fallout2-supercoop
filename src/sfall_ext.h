#ifndef FALLOUT_SFALL_EXT_H_
#define FALLOUT_SFALL_EXT_H_

#include "obj_types.h"
#include <vector>

namespace fallout {

// State behind the sfall opcodes added for Restoration Project compatibility:
// interface tags, fake perks, AI no-burst flags, the in-memory sfall file
// system and the reg_anim combat check. Reset on every new game / load.

void sfallExtReset();

// reg_anim_combat_check: false lets reg_anim_* run during combat.
extern bool gSfallRegAnimCombatCheck;

// set_critter_burst_disable
void sfallSetCritterNoBurst(Object* critter, bool on);
bool sfallCritterNoBurst(Object* critter);

// show_iface_tag / hide_iface_tag / is_iface_tag_active / add_iface_tag /
// set_iface_tag_text. State only: the dedicated server has no interface bar,
// and the co-op client does not draw custom tags yet.
// Tags act on dude_obj (the acting player in co-op). 0/3/4 are the player's
// sneak/level/addict flags; 1/2 read poison/radiation; 5+ are custom boxes,
// kept per player and streamed to that player's client.
void sfallIfaceTagShow(int tag);
void sfallIfaceTagHide(int tag);
bool sfallIfaceTagActive(int tag);
int sfallIfaceTagAdd();
void sfallIfaceTagSetText(int tag, const char* text, int color);
// Custom tags (5+) active for |actor|, ascending; empty when none.
const std::vector<int>& sfallIfaceCustomTagsFor(Object* actor);
// Client: replace |actor|'s custom tags with what the server sent.
void sfallIfaceCustomTagsSet(Object* actor, const int* tags, int count);
// Server: send every actor's custom tags (a join / rebaseline).
void sfallIfaceCustomTagsEmitAll();

// set_fake_perk (recorded; not shown on the character screen yet).
void sfallSetFakePerk(const char* name, int level, int image, const char* desc);
int sfallFakePerkLevel(const char* name);

// add_extra_msg_file: message list id already loaded for a path (0 if none).
// Cleared on reset, together with the engine's temporary message lists.
int sfallExtraMsgFileGet(const char* path);
void sfallExtraMsgFileSet(const char* path, int id);

// fs_* : sfall's in-memory file system. Files are copies of game files that a
// script can patch; they are not yet served back through the data layer.
int sfallFsCopy(const char* path, const char* source);
int sfallFsFind(const char* path);
void sfallFsSeek(int id, int pos);
int sfallFsReadByte(int id);
int sfallFsReadShort(int id);
int sfallFsReadInt(int id);
void sfallFsWriteByte(int id, int value);
void sfallFsWriteShort(int id, int value);
void sfallFsWriteInt(int id, int value);
int sfallFsSize(int id);
int sfallFsPos(int id);

} // namespace fallout

#endif /* FALLOUT_SFALL_EXT_H_ */
