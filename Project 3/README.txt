/* CSci4061 Assignment 3
 * Name: Caleb Biasco, Dennis Ehrhardt, Meghan Jonas
 * X500: biasc007, ehrh0023, jonas050 */

IMPORTANT NOTE:
We modified application.c in order to fix a bug that would happen due to signals interrupting the scanf() call. All we added was a while loop around scaf() that also checks if the errno is equal to EINTR. That way, if a signal interrupt happened, scanf() can be recalled. errno is reset to 0 at the beginning of the application while loop and inside of the scanf() while loop in order to prevent lost inputs.
 
TO COMPILE AND RUN THE APPLICATION:
Extract assignment3_groupG.tar into a directory of your choice and run `make` in that directory. To run the application, enter the following into the command line:

`./application <process name> <mailbox key> <window size> <max delay> <timeout> <drop rate>`

process name: Any string not used by another process as a name
mailbox key: Any integer that is not used by any other message queues
window size: An integer greater than 0 that denotes how many packets can be simultaneously being transferred between processes
max delay: maximum delay in sending an ACK (in seconds)
timeout: waiting time of sender before resending a packet (in seconds)
drop rate: probability a packet will be lost by receiver (in %)

HOW THE PROGRAM WORKS:
After running the program with proper arguments, a prompt will appear waiting for one of two inputs: "sender" or "receiver".

If the user inputs "sender", the program will prompt them for a process name that will receive their message, and then a non-whitespace message. If the user specified as the receiver does not exist or is the sender, the send will fail. If a packet is successfully sent, then more packets will be sent out until <window size> packets are simultaneously being transferred between programs. If an ACK (acknowledgement packet) is not received within <timeout> number of seconds, a TIMEOUT will occur and all presently transferring packets will be resent to the receiver. If five TIMEOUTS occur without an ACK, the send will fail. Once all the packets have corresponding ACKs received, the program will return to the beginning prompt.

If the user inputs "receiver", the program will wait until a signal is received, at which point the program will communicate with the process sending it packets. The receiver has <drop rate> percent chance to lose the packet, meaning another will have to be sent by the sender, and if a packet is not dropped, an ACK will be sent in a maximum of <max delay> seconds. Once all packets have been received, the message is displayed and the program will return to the beginning prompt.

EFFECTS OF THE TIMEOUT PERIOD:
The effect of having a very long TIMEOUT period means that any packet that is not dropped will definitely receive an ACK, and also both processes will have to wait a long time in the case that a packet is dropped.
The effect of having a very short TIMEOUT period means that almost all packets will be resent multiple times due to timing out before the delay even ends, and the sender will likely fail and return to the prompt before receiving all ACKs it is supposed to.

EFFECTS OF THE WINDOW SIZE:
The effect of having a very small window size is more frequent TIMEOUTs by the sender. However this also means fewer out-of-role packet transfers since its less likely that packets would be resent due to other packets not receiving ACKs. The send time of the entire message will be more or less the same, since it is mostly dependent on the max delay of the receiver.
The effect of having a very large window size will be less freqeuent TIMEOUTs by the sender but many more duplicate packets being transferred between the two processes, even after one process has finished. The send time of the entire message will be more or less the same, since it is mostly dependent on the max delay of the receiver.