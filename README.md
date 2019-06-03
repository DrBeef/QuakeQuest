QuakeQuest
==========

Welcome to the first (and only I think!) 6DoF implementation of Quake, using the DarkPlaces Engine.

I am quite pleased with how this has turned out. Getting this to work as a 6DoF game was a bit of a faff, and it isn't perfect (see known issues), but I think you'll agree it's good enough and also very good fun to play.

This requires sideloading in order to play on your Oculus Quest.

Things to note / FAQs:
----------------------
* You are the weapon.. to make it truly 6DoF the location of the weapon is what the engine understands to be the player. So you can peek round corners and enemies won't spot you, but if you poke the gun round to shoot, they'll see you.
* Open the in-game menu with the left-controller menu button
* The original soundtrack can work, you can find details here: 
* You can change the right-thumbstick turn mode in the Options -> Controller menu, but be warned possible nausea awaits
* You can change the laser-sight mode using the right-thumbstick click
* The off-hand trigger will make you run
* Switch weapon next/prev using the controller grip buttons
* By default the direction of movement is where the HMD is facing, this can be changed in the menu to the direction the off-hand controller is facing (strafe-tastic)
* You can change supersampling in the commandline.txt file, though by default it is already set to 1.3, you won't get much additional clarity increasing it more and may adversely affect performance
* A number of the controller buttons are currently unmapped; future updates may give them function (see future to-dos on console commands for example)

Known Issues:
-------------
* If you use dpmod, you know that it applies an extra weapon offset (to the right); I've tried and failed to correct it, so for now the weapon doesn't line up with the controller at all, though the laser sight is correct
* Some slight movement when moving weapon around (this is due to how I had to implement 6DoF) however you won't notice this in the heat of battle, only when you stand still marveling at the true 6DoF weapon in your hand
* Orientation when headset looking near poles (north or south) doesn't work very well at all (Quaternion to Euler Angle issue) - if anyone knows what's going on there I'd appreciate some help!, it was like this on the GearVR and not many people complained about it, but it bothers me
* Laser Sight out of alignment when very close to wall/object
* No way to type in console commands at the moment (see future to-dos)
* The axe and arm are way out of alignment with the controller (see future to-dos)

Future To-Dos:
--------------
* Apply weapon transformation to line up weapons with controller better
* Add a way to choose and run console commands from a file of them
* Add an off-hand world entity - such as Flashlight or the HUD
* Left-handed mode needs to have the axe model reflected and a mod created so it uses the left-hand model rather than the right

Building:
---------

I will add details on how to build in a future update. For a start you need:

* Android Developer Studio
* Oculus Mobile SDK 1.23.0
* The QuakeQuest folder should be below VrSamples in the extracted SDK
