
"Headless NetHack" is a benchmark/test program based on NetHack 3.4.3. It can be 
used for testing compilers, code instrumenters and coverage tools. It is written in C.

My modifications to NetHack add the capability to record and playback "demo" files,
which contain sequences of commands entered by a player. In the recording mode,
"hnethack --record", you play NetHack in text mode as usual. In the playback mode,
"hnethack --play", the commands you entered during recording are replayed so that
your game session is reproduced.

In each mode, NetHack always runs with the same pseudo-random number seed, with
no "bones" or "save" files, and with a fixed set of configuration options. 


Jack Whitham, April 2016



# Requirements:

Because of the way that system calls have been redirected, you can only
build Headless NetHack on Linux. The recommended platform is Debian 8
running on AMD64. You will need to install at least the following packages:

    build-essential
    libncurses5
    libncurses5-dev
    bison
    flex

Unlike some of my other "Headless" programs, no additional downloads are required; the data
files are all distributed with the game.


# Instructions:

Run "./build.sh" to compile.

To play back a recording, enter the following:

    cd hackdir
    ./hnethack --play-quiet example.demo

After a short time, you should see the high score table, which will appear as
follows:

    You made the top ten list!

     No  Points     Name                                                   Hp [max]
      1      61869  magic-Bar-Hum-Mal-Cha died in The Dungeons of Doom on          
                    level 9 [max 12].  Killed by a white unicorn.           - [113]

To make a new recording, enter the following:
    cd hackdir
    ./hnethack --record my_recording.demo

Recording stops when you quit, win or die. It also stops when you save, but
save files can't be restored in Headless NetHack.

--play draws the game on screen as it plays (very quickly; this is only useful
if it crashes for some reason). --play-quiet does not display the game, but you
can still see that it ran as expected, because you'll see the high score after
the game ends.

