# FlowSerialArduinoUsb

FlowSerial implementation for an USB connection with an Arduino-like board.

# Install
Insatallation can be preformed with the ./library-control.sh script.
use this to install the library
```
./library-control.sh install
```
use this to remove the library
```
./library-control.sh remove
```

# Example main.cpp
Compile with
```
g++ -Wall -std=c++11 main.cpp -lflowserial -lflowserial-ardiuno-usb -pthread -o prog
```
The code:
``` C++
#include <iostream>   //For printing to console
using namespace std;
#include <unistd.h>   //For usleep specific for this example
#include <FlowSerialArduinoUsb> //Include the FlowSerial library here

/**
 * @brief      Main function to test the FlowSerial library
 *
 * @param[in]  argc  number of arguments passed.
 * @param      argv  The arguments passed.
 *
 * @return     0 on success.
 */
int main(int argc, char** argv){
    // Example array so the other party can read/write to it.
    uint8_t flowRegister[32];
    // Construct a UsbSocket object to communicate
    FlowSerial::UsbSocket arduino(flowRegister, 32);
    // Connect to the serial device. Usually /dev/ttyACM0 for Arduino's. Use
    // baud rate of 115200
    arduino.connectToDevice("/dev/ttyACM0", B115200);
    // Set element 0 to 0 to see if it changes.
    arduino.flowRegister[0] = 0;
    // Wait for the Arduino to startup since it will reset on reconnect(except it
    // is a Leonardo instead of an Uno).
    usleep(2000000);
    // Start a thread on the background that will handle incoming data. This is
    // far more efficient because it will block when no data is available and so
    // it will not use any processing power while waiting.
    arduino.startUpdateThread();

    while(1){
        static unsigned int i = 0;
        static uint8_t out[10] = {1,2,3,4,5,6,7,9,10};
        //Show which loop this is
        cout << "\nloop: " << i << '\n';
        //Show if element 0 has changed
        cout << "flowRegister[0] is now: " << static_cast<int>(arduino.flowRegister[0]) << endl;
        //Alternate out with 0 and 1
        out[0] ^= 1;
        cout << "Writing = " << +out[0] << " to Arduino at index 2...";
        cout.flush();
        //Preform a write. Out will be written to element 2. out is of size 10
        arduino.write(2, out, 10);
        cout << "done" << endl;
        uint8_t readBuffer[10];
        cout << "Preform a read from Arduino. index 2...";
        cout.flush();
        //Preform a read from element 2 to confirm reading and writing
        arduino.read(2, readBuffer, 2);
        cout << "done\n" << "received " << +readBuffer[0] << endl;
        //Use sleep to sleep for 10^6 microseconds = 1 second
        usleep(10000);
    }
    return 0;
}
```
