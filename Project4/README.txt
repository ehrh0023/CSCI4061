/* csci4061 S2016 Assignment 4 
 * Name: Caleb Biasco, Dennis Ehrhardt, Meghan Jonas
 * X500: biasc007, ehrh0023, jonas050 */
 
TO COMPILE AND RUN THE WEB SERVER:
Extract assignment4_groupG.tar into a directory of your choice, navigate to the SourceCode directory, and run `make` in that directory. To run the application, enter the following into the command line:

`./web_server <port> ../testing <num_dispatch> <num_worker> <queue_len>`

NOTE: The process can take one more argument for caching. This will be ignored since we did not implement a cache.

port: the port number to be used for "networking" (the testing/urls file uses 9000)
../testing: the path to the "testing" directory with all of the files from the executable's directory
num_dispatch: the number of dispatcher threads to create (max. 100)
num_worker: the number of worker threads to create (max. 100)
queue_len: the length of the bounded buffer that is used for requests (max. 100)

After running the program with proper arguments, text will print indicating if the threads were successfully created.
Open another terminal or a browser from the same machine:

---TERMINAL---
Call `wget -i <path-to-urls>/urls -O <output-name>` to execute an automatic pull and write of the all the test files multiple times to the output file in the directory the request was made from. You can also call `wget http://127.0.0.1:<port>/<path-to-file>/<file-name> -O <output-name>` to request a specific file.

---BROWSER---
Go to the URL http://127.0.0.1:<port>/<path-to-file>/<file-name> to request a specific file.


HOW THE PROGRAM WORKS:
The web server initializes the port and creates the requested number of dispatcher and worker threads at execution. Every dispatcher thread runs `accept_connection()` and waits for a request to be sent, while every worker thread checks if any requests are in and wait if there are none.

The dispatchers and receivers essentially work together as a bounded buffer.

Upon receiving a request, a dispatcher thread will acquire the access lock and prepare that request. If the queue is already full, it will wait until it receives a condition signal. It then puts the appropriate information in the queue, releases the access lock and signals the workers for a new request, then repeats.

If there are no requests in the queue, a worker thread will wait until it receives a condition signal. Upon receiving it, it aquires the access lock, grabs the information from the request queue, then releases the access lock and signals the dispatchers for an empty space. After getting the request info, a worker thread will attempt to open, read, and return that result of that request back to the requester, in the meanwhile writing its request information to a log file. It then loops around after it has returned either the result or an error back to the requester.

The thread functions are designed to run concurrently. No matter how many requests are coming in, only one thread will access the request queue at a time, but then the move onto their processing and returning work and leave the request queue for others to access. This fully utilizes multithreaded operations and is carefully designed to be thread-safe.
