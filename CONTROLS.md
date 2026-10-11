# Play Minecraft × Doom

Use **Minecraft x Doom** or **Minecraft x Doom II** on your desktop. If Minecraft is closed, the shortcut opens its official launcher: select **Minecraft × Doom**, click Play, and leave that Minecraft window running. Return to the Doom window. Close Doom before switching to the other desktop launcher. Minecraft authentication stays in the official launcher.

Bow arrows now hit Doom enemies and stick into Doom surfaces; pearls land on Doom geometry and teleport you. Spears enchanted with Lunge propel you along your current view. Give the spear its normal attack cooldown after switching slots before jabbing. Bow damage is scaled for Doom health.

Worn Minecraft armor now reduces normal Doom damage by 4% per armor point, up to 80% for full diamond/netherite armor. This is bridge balancing; toughness, Protection enchantments, armor durability and shield blocking are not yet mapped to Doom damage. Armor-bypassing attacks and telefrags retain Doom's rules.

A **Totem of Undying held in the main hand or offhand** saves you from a normal lethal Doom hit and consumes one totem. It plays Minecraft's pop animation/effects, restores half a heart, and gives brief protection plus four absorption hearts. A totem elsewhere in your inventory does not activate. Doom telefrags/forced kills retain their bypass rules.

- **F8:** boss destinations for the current game. Arrow keys and Enter select a fight. This starts a fresh copy of the selected level and its required difficulty; selecting the same fight again after death returns to the arena. Save a campaign before travelling.
- **F6:** refill explosive gear. Also available in the F8 menu. Replaces hotbar slots 3, 5, 6, 9 and the offhand with a loaded firework crossbow, TNT, flint and steel, and rockets.
- **3:** crossbow. Right-click fires; hold right-click to reload from the offhand rockets, release, then fire again.
- **5:** TNT, right-click to place. **6:** flint and steel, right-click the TNT to ignite. Stand back.
- **8:** building blocks. Right-click places; left-click mines or attacks.
- **WASD:** movement; mouse: look; Space: jump; Shift: sneak; Ctrl: sprint; 1–9/wheel: hotbar, unless rebound in Minecraft.
- **F7:** toggle the Minecraft display/bridge. Escape opens Doom's menu. After death, hearts show zero; click either mouse button, press your Minecraft Jump key, **Enter**, or **G** to restart the level. Movement and position reset on respawn; Minecraft inventory is retained.
- **E:** Minecraft inventory (or your rebound inventory key). Click/drag items, shift-click, and hover an item then press its hotbar key to swap it. Escape or your inventory key closes it. **G:** Doom doors/switches.
- **F:** swap main hand/offhand (or your rebound swap key). Both hands display normally. Movement, hotbar selection, attack/use, pick, drop, chat, and inventory use your Minecraft bindings. Your normal Minecraft keybinds were imported once; later changes in the Doom guest's Minecraft Controls update Doom automatically. Mouse wheel still scrolls the hotbar.
- **Hold F3 and tap F4:** toggle Creative/Survival directly. Creative also protects against Doom damage. Double-tap your jump key to start/stop Creative flying; hold jump to ascend, sneak to descend. Survival falls normally. Minecraft controls walking, sprinting, sneaking, jumping and flight; Doom checks movement against its map.
- **F9:** equip an elytra. An existing chest item is moved into inventory if there is room; if there is no room, equip is skipped. Jump while airborne in Survival to start gliding, aim with the mouse, and use a firework rocket to boost. Flight rockets keep vanilla acceleration and do not create a Doom blast while attached to you. Use a tall/open area such as E2M8 for flying.
- **Lunge spears:** left-click to jab/lunge after the attack cooldown. Survival requires at least 7/20 hunger (3.5 food icons); eat when low. The food bar shows Minecraft's actual hunger, since Doom has no stamina bar.
- **Golden absorption hearts:** golden apples, enchanted golden apples and totems provide temporary extra health. Doom damage consumes gold hearts first, then red hearts. Minecraft controls their normal duration and refresh behavior; regeneration heals red hearts separately.

Boss travel includes Doom E1M8's two Barons; all Cyberdemon/Spider encounters present in the owned Doom and Doom II maps; and Doom II MAP30's real Icon of Sin arena. E4M7 and MAP06 use hard difficulty because those bosses are absent from the lower setting. No expansions were included.

TNT uses native Doom splash damage (256 damage, 192-unit radius). Fireworks use a native rocket explosion (128 damage/radius); crossbow hits also apply Doom rocket impact damage. Doom's original Cyberdemon/Spider splash immunity remains: shoot them directly with the firework crossbow.

This is a local prototype. The camera presents both games from the same captured pose; two games still add some input latency and map streaming can briefly delay Minecraft. Floors/ceilings supply exact sampled heights for player movement, while voxel/wall proxies remain approximate; Doom resolves blocked moves and checks flight in short steps. Doors/platforms refresh collision even while standing still. Long campaign/save-load stability has not been exhaustively tested.

Source and build notes: `doomcraft-review`, with separate Doom/Doom II configs and saves under `local`. Your Steam WADs remain in the Steam installation.
