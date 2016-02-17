/* CSci4061 S2016 Assignment 1							*
 * login: biasc007										*
 * date: 02/17/16										*
 * name: Caleb Biasco, Dennis Ehrhardt, Meghan Jonas	*
 * id: biasc007, ehrh0023, jonas050 					*/

Our make4061 program will take a Makefile that follows the guidelines described in the lab manual (e.g. has a minimum of 10 targets, no cycles). It can also take the flags -f, -n, -B, and -m which behave in a similar fashion to the GNU make program, and a particular target to be built can be specified as a parameter.

To compile the make4061 program, use 'make' in the same directory as the main.c, util.h, util.c, and corresponding makefile. This will output a file named 'make4061'.

To use the make4061 program from the shell, use './make4061' followed any extra parameters you want to use, like './make4061 -f new_file_path' or './make4061 Makefile2 -B'. If you do not specify a target, then the make4061 program will build the first target found in the makefile.

Note that the make4061 program MUST be in the same directory as the files indicated in the makefile in order to build with them, similarly to the GNU make program (you can use a makefile from any path using the -f flag, though).