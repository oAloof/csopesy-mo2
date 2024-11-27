Here’s a draft for the **README.txt** file. You can replace `[name]` with your actual name when you’re ready.

---

**README.txt**

---

### Project Information
**Name:** ONG, Julianne | PUA, Kendrick | UY, Nicole | UY, Tyrone

### Instructions to Run the Program

1. **Ensure all files are in the same directory** for proper compilation and execution.
2. **Compile the code** using the following command (using any compatible C++ compiler):

   ```bash
   g++ -std=c++11 -o csopesy_os_emulator main.cpp CLI.cpp Config.cpp ICommand.cpp PrintCommand.cpp Process.cpp ProcessManager.cpp Scheduler.cpp Utils.cpp
   ```

3. **Run the program** by executing the following command:

   ```bash
   ./csopesy_os_emulator
   ```

### Entry Class
The entry class file containing the `main` function is located in:
- **File:** `main.cpp`
  
This file initiates the CLI instance, allowing user interaction with the OS emulator through various commands.

---