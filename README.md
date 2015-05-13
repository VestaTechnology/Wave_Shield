#Adafruit Arduino Wave Shield
This project lets you use the Adafruit Arduino Wave Shield with a Mercury 18!  Add music to your Mercury projects with this useful shield.  This project requires an Adafruit Wave Shield, FAT formatted SD card, and audio saved as single-channel, 22kHz, 16-bit wav files.

__First of all, documentation and recognition is needed.__

* This project is for an Arduino Shield developed by [Adafruit], buy it [here][purchase]!
	* Mercury is Arduino-pin compatible so the shield will work, it just needs some Mercury specific software to drive it (this project :smiley:)!
	* Adafruit has a [wonderful site][learn] full of indispensable information about their shield.  We assembled our shield based on [their instructions][assemble].
* This project makes use of a FAT file system module, [Petit FatFs][pFAT], developed by ChaN
	* see the [Application Note] for general info and license
* Here is a [sweet tutorial][SDTutorial] that does a good job of describing the FAT file system on an SD card, thanks [Joonas Pihlajamaa].
* Here is a specification of the [wave file format][WAV Spec], thank you Prof. Peter Kabal
* I used [SoX] to convert my music into single-channel, 22kHz, 16-bit wav files.
	* Here is a [tutorial][sox tutorial] from the Adafruit forums. On Windows be sure to add Sox to your system PATH after downloading it.
	* I used this command (in Windows 7 cmd.exe) with sox v14.4.1  `sox <infile.xxx> -b 16 <outfile.wav> channels 1 rate 22050`

[Adafruit]: https://www.adafruit.com/
[purchase]: http://www.adafruit.com/products/94?&main_page=product_info&cPath=17_21&products_id=94
[learn]: https://learn.adafruit.com/adafruit-wave-shield-audio-shield-for-arduino
[assemble]: https://learn.adafruit.com/adafruit-wave-shield-audio-shield-for-arduino/solder
[pFAT]: http://elm-chan.org/fsw/ff/00index_p.html
[Application Note]: http://elm-chan.org/fsw/ff/pf/appnote.html
[SDTutorial]: http://codeandlife.com/2012/04/02/simple-fat-and-sd-tutorial-part-1/
[Joonas Pihlajamaa]: http://joonaspihlajamaa.com/about.html
[WAV Spec]: http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
[SoX]: http://sox.sourceforge.net/
[sox tutorial]: http://forums.adafruit.com/viewtopic.php?p=29636

##Making Sense of the Project
This project will play all the correctly formatted music files in the root directory of the SD card.  First, load your music files onto a freshly formatted SD card.  It is highly recommend to use the [official formatter] released by the SD Association.  Then, clone this project, or download the [.zip][wavShieldZIP] and open it in MPLAB X. Plug in your SD card and attach your wave shield to your Mercury.  _Make and Program_ or _Debug Project_ in MPLAB X.  The project will print notifications and errors over the COM port.  If you need help opening and debugging a project in MPLAB X, see our [Quick Start Guide][QS].

[official formatter]: https://www.sdcard.org/downloads/formatter_4/index.html
[wavShieldZIP]: https://github.com/VestaTechnology/Wave_Shield/archive/master.zip
[QS]: https://github.com/VestaTechnology/Onboard_Mercury_18/blob/master/README.md

This project uses the common header mercury18.h.  It looks for the header file one directory up from the project directory.  You can download mercury18.h from our [Onboard_Mercury_18][mer18] repository.  It is located in the folder Common.

[mer18]: https://github.com/VestaTechnology/Onboard_Mercury_18

###Project Architecture
__DiskIO__ is the low level disk I/O module of Petit FatFs that is processor specific.  This was written by Vesta Technology specifically for use with a Mercury 18, though it may work, or at least serve as a guide, for any PIC18F66K90 board.  It contains functions for initializing and communicating with an SD card.

__PFF__ is the Petit FatFs module provided by ChaN.  It contains functions to mount a file system, navigate it, and read and write files.  This is not processor specific and relies on the DiskIO module to send and receive commands.  Its functionality can be configured in __pffconf.h__.

__integer.h__ is another header file for Petit FatFs configuration.  It accounts for differences in variable lengths on different processors.  It is configured for the PIC18F66K90 on the Mercury 18.

__WaveReader__
This was written by Vesta Technology to read wave files and interface with the Wave shield.  This module is called by __main.c__ to play the SD card's root directory.  It has functions to open and check the format of wave files, and initiate the playing sequence.  It also contains the Interrupt Service Routine that sends data to the DAC.  Each function is documented in the code if you're interested in learning more about them.  
This is also where you would start tinkering to add functionality to this project.  Maybe you want a play/pause button, or you want to play a sound when you detect something is nearby.  The possibilities are endless! See Adafruit's [Examples] for more ideas.  Each example links source code that can serve as a guide for modifying this project.

Have fun, be creative, and share what you do with this project!  Submit a pull request!  As always, you can submit an issue, [contact us][contact] or send us an [email][mail] if you need help or have questions.

[Examples]: https://learn.adafruit.com/adafruit-wave-shield-audio-shield-for-arduino/examples

[contact]: https://www.vestatech.com/support/contact-us/
[mail]: mailto:support@vestatech.com?subj=Github/Mercury
