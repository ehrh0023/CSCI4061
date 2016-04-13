/* CSci4061 Assignment 3
 * Name: Caleb Biasco, Dennis Ehrhardt, Meghan Jonas
 * X500: biasc007, ehrh0023, jonas050 */

We modified application.c in order to fix a bug that would happen due to signals interrupting the scanf() call. All we added was a while loop around scaf() that also checks if the errno is equal to EINTR. That way, if a signal interrupt happened, scanf() can be recalled. errno is reset to 0 at the beginning of the application while loop and inside of the scanf() while loop in order to prevent lost inputs.