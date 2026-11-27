# Changelog

## 0.3.0
- **Status colour**: override the built-in palette for the resting states (off, disarmed, closed, locked, home, idle) and
  the engaged states (on, armed, open, unlocked) — grey, green, blue or red. The alert states (triggered, arming,
  pending) keep their red or amber so an alarm always stands out.
- **Status icon**: show an icon in place of the dot, changing with the state — a separate icon for the resting, the
  engaged and the alert states, from Tessera's built-in set (a shield, a lock, a door, a light, a bell, an alarm, ...).
- Both default to how the tile looked before: the dot and the built-in colours, until you set them.

## 0.2.0
- **Prompt language**: the "tap again" prompt can be German ("Erneut drücken") instead of English.
- **Status size**: the state word can be large (fills the tile) or small (leaves more room).
- **Show the action below**: turn it off to hide the target's name and give the state the whole tile.
- All three default to how the tile looked before, so nothing changes until you set them.

## 0.1.0
- First version: a tile that shows one entity's state and, on a tap, switches or triggers a second entity.
- The tap can toggle, turn on, turn off or press the target; it reaches a script, a scene or a button too.
- Alarm, on/off, open/closed and locked states get their own word and colour; any other state shows as it is.
- **Ask before acting**: the first tap asks, the second acts, for a target such as an alarm.
