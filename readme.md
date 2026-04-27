Chrono Rift - Execution Guide
This guide details how to build and run the Chrono Rift multi-processing OS game using Docker, WSL, and an SFML graphical interface.

Prerequisites
Docker Desktop installed and integrated with your WSL distribution.

VcXsrv (XLaunch) installed on your Windows host to act as the X11 display server.

Step 1: Configure the Display Server
Because the Arbiter uses SFML to render a graphical window out of a Docker container, you must configure Windows to accept the display connection.

Launch XLaunch on Windows.

Leave all default display settings, but on the "Extra Settings" screen, you must check the box for "Disable access control".

Finish the setup to start the server (it will run silently in your system tray).

Step 2: Build the Environment
Open your WSL terminal, navigate to the root of the project directory (where your Dockerfile is located), and build the Docker image. You only need to do this once unless you modify the Dockerfile or requirements.txt.

Bash
docker build -t chrono-rift-env .
Step 3: Start Terminal 1 (The Arbiter & Display)
In your WSL terminal, export your display variable so WSL knows where to send the graphics, then launch the Docker container with the X11 forwarding flags.

Bash
# 1. Export the display variable to route graphics to VcXsrv
export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0.0

# 2. Run the Docker container interactively
docker run -it --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix -v $(pwd):/app chrono-rift-env
Once you are inside the container (root@...:/app#), compile the source code and start the Arbiter:

Bash
# 3. Clean old binaries and compile the new ones
make clean && make

# 4. Run the Arbiter
./arbiter.out
Note: A black SFML window will open on your Windows desktop. Leave Terminal 1 running.

Step 4: Start Terminal 2 (Game Logic & Inputs)
Because the Arbiter requires the graphical window, you must use a second terminal to enter your player inputs for the Human Interfacing Process (hip.out).

Open a new WSL terminal window/tab and connect it to the running container:

Bash
# 1. Find your running container ID
docker ps

# 2. Open a bash session inside that container (replace <ID> with your actual Container ID)
docker exec -it <ID> bash
Once inside the container, launch the Automated Strategic Process (asp.out) in the background, and the Human Interfacing Process (hip.out) in the foreground so you can interact with it:

Bash
# 3. Start the game logic
./asp.out & ./hip.out
Enter your party size when prompted. The entities will instantly populate on the SFML Arbiter window, and you will be able to submit your combat actions through Terminal 2!