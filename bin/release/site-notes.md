<!-- What changed on tic80.com since the previous client release. The site's
     own work has no pull request in this repository, so the summary above
     never mentions it — generate-notes.sh prints this file verbatim after
     the summary. Rewrite it for every release; when nothing site-side has
     changed, empty (or delete) it and the section disappears. The comment
     itself does not render. -->

## On tic80.com

The site moved on too:

- **A WebGPU player.** In browsers that support WebGPU, a game page now loads
  a second player build that uses it; everywhere else the existing WebGL one
  still runs. Nothing to switch on.
- **Your saved game gets its own name and icon on iOS.** Adding the player page
  to the home screen on an iPhone or iPad names the app after the game and
  gives it the game's cover as the icon.
- **Cartridge names are unique per account.** Two live cartridges of yours can
  no longer share a name; the older duplicates were renamed with their id, and
  old `/play/{id}` links redirect to the game's own page.
- **Forth cartridges are no longer accepted.** The engine dropped pForth from
  its release builds, so an upload of a Forth cartridge is refused.
