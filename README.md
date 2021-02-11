QuakeQuest
==========

Welcome to the only implementation of the Quake Engine for the Oculus Quest 1 and 2, using DarkPlaces as a base for this port.

The easiest way to install this on your Quest is using SideQuest, download SideQuest here:
https://sidequestvr.com/



IMPORTANT NOTE:
---------------

This is just an engine port, the apk does contain the shareware version of Quake, not the full game. If you wish to play the full game you must purchase it yourself (https://store.steampowered.com/app/2310/QUAKE/).

Copying the Full Game PAK files to your Oculus Quest
----------------------------------------------------
Copy the PAK files from the installed Quake game folder on your PC to the QuakeQuest/id1 folder on your Oculus Quest when it is connected to the PC. You have to have run QuakeQuest at least once for the folder to be created and if you don't see it when you connect your Quest to the PC you might have to restart the Quest.

This port DOES support mods, an excellent resource for finding out what you can do is here: https://www.reddit.com/r/quakegearvr/

Bear in mind that the above sub-reddit is for the Gear VR version, which is not dramatically different, but the folder in which game data/saves etc resides is now QuakeQuest instead of QGVR.


Controls:
---------

* Open the in-game menu with the left-controller menu button
* Left Thumbstick - locomotion
* Right Thumbstick Left/Right - Turn (if configured to do so in the options)
* Right Thumbstick Up/Down - Switch next/previous weapon
* A Button - Jump
* Y Button - Bring up the text input "keyboard"
* Dominant Hand Controller - Weapon orientation
* Dominant-Hand trigger - Fire
* Off-Hand Controller - Direction of movement (if configured in settings, otherwise HMD direction is used by default)
* Off-hand Trigger - Run
* Right-thumbstick click change the laser-sight mode 

Inputting Text:
---------------

This is cumbersome and rubbish, but until Oculus release their virtual keyboard implementation for Native apps, this is the best on offer:

- Press Y to bring up the "keyboard" and Y again to exit text entry mode
- Push left or right thumbstick to select the character in that location in the little diagram, selected character is shown for left right controller below the character layout diagram
- Press grip trigger on each controller to cycle through the available characters for that controller
- Press X to toggle SHIFT on and off
- Press Trigger on the appropriate controller to type the selected character (or select center character if no thumbstick direction is pushed)
- Press B to Delete characters
- Press A for Enter/Return


Things to note / FAQs:
----------------------
* The original soundtrack can work, you can find details here: https://www.reddit.com/r/quakegearvr/comments/7r9eri/got_the_musicsoundtrack_working/
* You can change the right-thumbstick turn mode in the Options -> Controller menu, but be warned possible nausea awaits
* You can change handed-ness (for you left handers) in the Controller settings menu
* By default the direction of movement is where the HMD is facing, this can be changed in the menu to the direction the off-hand controller is facing (strafe-tastic)
* You can change supersampling in the commandline.txt file, though by default it is already set to 1.3, you won't get much additional clarity increasing it more and may adversely affect performance

Known Issues:
-------------
* If you use dpmod, you know that it applies an extra weapon offset (to the right); I've tried and failed to correct it, so for now the weapon doesn't line up with the controller at all, though the laser sight is correct
