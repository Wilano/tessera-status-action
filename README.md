# Status & action

One tile that shows the state of an entity and, on a tap, switches or triggers **a second, different** entity. Made for
a wall panel where the alarm's state is on the glass and a tap runs the script that arms or disarms it, but it works for
any pair: a lock's state with a tap on the porch light, a sensor with a tap on an evening scene.

Tessera's own tiles can act only on the entity they show; this tile keeps the two apart, so the thing you watch and the
thing you touch need not be the same.

- **Status entity**: what the tile shows. An alarm panel, a sensor, a binary sensor, a switch, a light, a lock, a cover
  or an input boolean. Its state becomes a word and a colour: an alarm green when disarmed, blue when armed, amber while
  it counts down, red when triggered; on/off, open/closed, locked/unlocked for the rest; any other state as it is, with
  a sensor's unit.
- **Tap acts on**: the second entity. A script, a switch, an input boolean, a light, a scene, a button, an automation or
  a fan.
- **What a tap does**: toggle it, turn it on, turn it off, or press it (for a button). The default is toggle.
- **Ask before acting**: turn it on and the first tap asks "Tap again", the second acts — a guard against an accidental
  tap on something that matters, such as an alarm.

The tile reads and writes through Home Assistant only: it watches the status entity and calls the action on the target.
It holds no secret and reaches no web service.

## Set up

1. Add the plugin to a screen in the Tessera app. The screen builds once.
2. In Layout, place the **Status & action** tile from the library's Plugins group.
3. In the inspector, pick the **Status entity** to show and the entity a tap should **act on**, choose **What a tap
   does**, and turn on **Ask before acting** for a target such as an alarm.

## Good to know

- The colours follow the screen's light and dark look; nothing is a fixed colour.
- A tap on a target the integration runs as a script or scene shows no state change of its own; the status entity you
  chose is what tells you the result.
- Leave the target empty and the tile is a plain status display; a tap then does nothing.
