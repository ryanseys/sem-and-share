Run the make file called "Makefile" to compile sem_and_share.cpp into an executable called sem_and_share.

`
$ make
`

You can run the make file by calling the "make" command from the directory that the Makefile and sem_and_share.cpp are all in. Ensure that they are all in the same directory when you run the make command or you will obviously be unable to compile the files.

Alternatively, you can manually compile the file by running the following commands in the directories respectful of where the source (.cpp) file is located:

`
$ g++ sem_and_share.cpp -o sem_and_share
`

Then execute using:

`
$ ./sem_and_share
`
