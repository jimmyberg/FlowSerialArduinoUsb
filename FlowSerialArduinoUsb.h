
#ifndef _FLOWSERIALARDUINOUSB_H_
#define _FLOWSERIALARDUINOUSB_H_

#include <FlowSerial>
#include <string>
#include <thread>
#include <semaphore.h>
#include <mutex>
#include <termios.h> //For toptions and friends
#include <stdexcept>

namespace FlowSerial{
	/**
	 * @brief      FlowSerial implementation build for USB devices
	 * @details    It has handy connect function to connect to a Linux USB
	 *             device via /dev/ *
	 */
	class UsbSocket : public BaseSocket{
	public:
		/**
		 * @brief      Constructor for this class.
		 *
		 * @param      iflowRegister    Pointer to array that is being used to
		 *                              store received data.
		 * @param      iregisterLength  Length of array that was given.
		 */
		UsbSocket(uint8_t* iflowRegister, size_t iregisterLength);
		/**
		 * @brief      Destroys the object. Closing all open devices.
		 */
		~UsbSocket();
		/**
		 * @brief      Connect to a device/interface.
		 *
		 * @param[in]  filePath  The file path to the device
		 * @param[in]  baudRate  The baud rate of the device
		 */
		void connectToDevice(const char filePath[], speed_t baudRate);
		/**
		 * @brief      Reads from peer address. This has a timeout functionality
		 *             of 500 ms. It will try three time before throwing an
		 *             exception
		 *
		 * @param[in]  startAddress  The start address where to begin to read
		 *                           from other peer
		 * @param      returnData    Array will be filled with requested data.
		 * @param[in]  size          The required bytes that needs to be read
		 *                           starting from startAddress. Must not be
		 *                           more that the actual size of returnData
		 */
		void read(uint8_t startAddress, uint8_t returnData[], size_t size);
		/**
		 * @brief      Closes the device/interface.
		 */
		void closeDevice();
		/**
		 * @brief      Same as FlowSerial::UsbSocket::update(0);
		 *
		 * @return     true if a message is received.
		 */
		bool update();
		/**
		 * @brief      Checks input stream for available messages. Will block if
		 *             nothing is received. It is advised to use a thread for
		 *             this.
		 *
		 * @param[in]  timeoutMs  The timeout in milliseconds
		 *
		 * @return     True is message is received
		 */
		bool update(uint timeoutMs);
		/**
		 * @brief      Starts an update thread.
		 */
		void startUpdateThread();
		/**
		 * @brief      Stops an update thread if it's running.
		 */
		void stopUpdateThread();
		/**
		 * @brief      Determines if the device is open.
		 *
		 * @return     True if open, False otherwise.
		 */
		bool is_open();
	private:
		int fd = -1;
		bool threadRunning = false;
		void updateThread();
		thread threadChild;
		sem_t producer;
		sem_t consumer;
		virtual void writeToInterface(const uint8_t data[], size_t arraySize);
	};

	class ConnectionError: public runtime_error{
	public:
		ConnectionError(string ierrorMessage = "connection error."):
			runtime_error(ierrorMessage){}
	};
	class CouldNotOpenError: public ConnectionError
	{
	public:
		CouldNotOpenError():ConnectionError("could not open device."){}
	};
	class ReadError: public ConnectionError
	{
	public:
		ReadError():ConnectionError("could not read from device."){}
	};
	class WriteError: public ConnectionError
	{
	public:
		WriteError():ConnectionError("could not write to device."){}
	};
	class TimeoutError: public ConnectionError
	{
	public:
		TimeoutError():ConnectionError("timeout reached waiting for reading of device."){}
	};
}

#endif
