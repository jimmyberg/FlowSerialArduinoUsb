#include "FlowSerialArduinoUsb.h"
#include <iostream>
#include <fcntl.h> //For the open parameter bits open(fd,"blabla", <this stuff>);
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
namespace FlowSerial{

	UsbSocket::UsbSocket(uint8_t* iflowRegister, size_t iregisterLength)
		:BaseSocket(iflowRegister, iregisterLength)
	{}

	UsbSocket::~UsbSocket(){
		closeDevice();
		stopUpdateThread();
	}

	void UsbSocket::connectToDevice(const char filePath[], speed_t baudRate){
		if (fd < 0){
			fd = open(filePath, O_RDWR | O_NOCTTY);
			if (fd < 0){
				throw CouldNotOpenError();
			}
			#ifdef _DEBUG_FLOW_SERIAL_
			else{
				cout << "successfully connected to UsbSocket " << filePath << endl;
			}
			#endif
			struct termios toptions;
			// Get current options of "Terminal" (since it is a tty i think)
			if(tcgetattr(fd, &toptions) < 0){
				throw CouldNotOpenError();
			}
			// Adjust baud rate. see "man termios"
			if(cfsetspeed(&toptions, baudRate) < 0){
				throw CouldNotOpenError();
			}
			// Adjust to 8 bits, no parity, no stop bits
			toptions.c_cflag &= ~PARENB;
			toptions.c_cflag &= ~CSTOPB;
			toptions.c_cflag &= ~CSIZE;
			toptions.c_cflag |= CS8;
			// no flow control
			toptions.c_cflag &= ~CRTSCTS;
			toptions.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
			toptions.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl
			toptions.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // make raw
			toptions.c_oflag &= ~OPOST; // make raw

			toptions.c_cc[VMIN]  = 0;
			toptions.c_cc[VTIME] = 20;
			// commit the serial port settings
			tcsetattr(fd, TCSANOW, &toptions);
		}
		else{
			cerr << "Error: Trying opening " << filePath << " but fd >= 0 so it must be already open." << endl;
			throw CouldNotOpenError();
		}
	}

	void UsbSocket::closeDevice(){
		if(fd >= 0){
			if(close(fd) < 0){
				cerr << "Error: could not close the device." << endl;
			}
			else{
				fd = -1;
			}
		}
	}

	bool UsbSocket::update(){
		return update(0);
	}

	bool UsbSocket::update(uint timeoutMs){
		if(fd >= 0){
			if(timeoutMs != 0){
				//Configure time to wait for response
				const uint timeoutNs = (timeoutMs * 1000000) % 1000000000;
				const uint timeoutS = timeoutMs / 1000;
				#ifdef _DEBUG_FLOW_SERIAL_
				cout << "timeout us = " << timeoutNs << " = " << (float)timeoutNs / 1000000 << " ms" << endl;
				cout << "timeout s = " << timeoutS << endl;
				#endif
				struct timespec timeout;
				timeout.tv_sec = timeoutS;
				timeout.tv_nsec = timeoutNs;
				//A set with fd's to check for timeout. Only one fd used here.
				fd_set readDiscriptors;
				FD_ZERO(&readDiscriptors);
				FD_SET(fd, &readDiscriptors);
				#ifdef _DEBUG_FLOW_SERIAL_
				cout << "Serial is open." << endl;
				#endif
				int pselectReturnValue = pselect(fd+1, &readDiscriptors, NULL, NULL, &timeout, NULL);
				if(pselectReturnValue == -1){
					cerr << "Error: FlowSerial USB connection error. pselect function had an error" << endl;
					throw ConnectionError();
				}
				#ifdef _DEBUG_FLOW_SERIAL_
				else if(pselectReturnValue){
					cout << "Debug: FlowSerial Received a message within timeout" << endl;
				}
				#endif
				else if (pselectReturnValue == 0){
					cerr << "Error: FlowSerial USB connection error. timeout reached." << endl;
					throw TimeoutError();
				}
			}
			uint8_t readFileBuffer[256];
			ssize_t receivedBytes = ::read(fd, readFileBuffer, sizeof(readFileBuffer));
			if(receivedBytes < 0){
				throw ReadError();
			}
			#ifdef _DEBUG_FLOW_SERIAL_
			if(receivedBytes > 0)
				cout << "received bytes = " << receivedBytes << endl;
			#endif
			uint arrayMax = sizeof(readFileBuffer) / sizeof(*readFileBuffer);
			if(receivedBytes > arrayMax)
				receivedBytes = arrayMax;
			return handleData(readFileBuffer, receivedBytes);
		}
		#ifdef _DEBUG_FLOW_SERIAL_
		cerr << "Error: Could not read from device/file. File is closed. fd < 0" << endl;
		#endif
		throw ReadError();
		return false;
	}

	void UsbSocket::startUpdateThread(){
		threadRunning = true;
		sem_init(&producer, 0, 1);
		sem_init(&consumer, 0, 0);
		threadChild = thread(&UsbSocket::updateThread, this);
	}

	void UsbSocket::stopUpdateThread(){
		if(threadRunning == true){
			threadRunning = false;
			closeDevice();
			sem_destroy(&producer);
			sem_destroy(&consumer);
			//Wait for thread to be stopped
			threadChild.join();
		}
	}

	void UsbSocket::writeToInterface(const uint8_t data[], size_t arraySize){
		ssize_t writeRet = ::write(fd, data, arraySize);
		if(writeRet < 0){
			cerr << "Error: could not write to device/file" << endl;
			throw WriteError();
		}
		#ifdef _DEBUG_FLOW_SERIAL_
		for (size_t i = 0; i < arraySize; ++i)
		{
			cout << "byte[" << i << "] = " << +data[i] << endl;
		}
		cout << "Written " << writeRet << " of " << arraySize << " bytes to interface" << endl;
		#endif
	}

	bool UsbSocket::is_open(){
		return fd > 0;
	}

	void UsbSocket::read(uint8_t startAddress, uint8_t returnData[], size_t size){
		clearReturnedData();
		sendReadRequest(startAddress, size);
		// Wait for the data to be reached
		int trials = 0;
		while(returnDataSize() < size){
			if(threadRunning == true){				
				//Used to measure time for timeout
				struct timespec ts;
				while(returnDataSize() < size){
					if(clock_gettime(CLOCK_REALTIME, &ts) == -1){
						throw string("cold not get the time from clock_gettime");
					}
					ts.tv_sec += 1; //500ms
					int retVal = sem_timedwait(&consumer, &ts);
					if(retVal == -1){
						int errnoVal = errno;
						if(errnoVal == ETIMEDOUT){
							if(trials < 5){
								#ifdef _DEBUG_FLOW_SERIAL_
								cout << "Received timeout. bytes received so far = " << returnDataSize() << " of " << size << "bytes" << '\n';
								cout << "Sending another read request." << endl;
								#endif
								//Reset input data
								clearReturnedData();
								//Send another read request
								sendReadRequest(startAddress, size);
								trials++;
							}
							else{
								throw TimeoutError();
							}
						}
						else{
							throw string("Error at semaphore somehow");
						}
					}
					else{
						#ifdef _DEBUG_FLOW_SERIAL_
						cout << "package is being handled. Posting producer." << endl;
						#endif
						sem_post(&producer);
					}
				}
			}
			else{
				try{
					update(500);
				}
				catch (TimeoutError){
					if(trials < 5){
						//Indicates error
						#ifdef _DEBUG_FLOW_SERIAL_qq
						cout << "Received timeout. bytes received so far = " << returnDataSize() << " of " << size << "bytes" << '\n';
						cout << "Sending another read request." << endl;
						#endif
						//Reset input data
						clearReturnedData();
						//Send another read request
						sendReadRequest(startAddress, size);
						trials++;
					}
					else{
						closeDevice();
						throw ReadError();
					}
				}
			}
		}
		getReturnedData(returnData, size);
	}
	
	void UsbSocket::updateThread(){
		while(threadRunning){
			update();
			if(returnDataSize()){
				int retVal = sem_trywait(&producer);
				if(retVal == -1){
					//semaphore unchanged since there was a error.
					int errnoVal = errno;
					if(errnoVal == EAGAIN){
						//Sem was empty it seems
						/*#ifdef _DEBUG_FLOW_SERIAL_
						cout << "thread has made a package available. Semaphore full though." << endl;
						#endif*/
					}
					else{
						throw string("Some error with the sem_trywait() has occurred. It was not empty that i know of.");
					}
				}
				else{
					//Semaphore has been lowered. Lifting the consumer semaphore
					#ifdef _DEBUG_FLOW_SERIAL_
					cout << "thread has made a package available. Posting semaphore." << endl;
					#endif
					retVal = sem_post(&consumer);
				}
			}
		}
	}
}
