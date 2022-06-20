

#include "glob.hpp"
#include "UART.hpp"
#include <iostream>
#include <memory>
#include <vector>
#include <string.h>
using namespace std;

#define BAUDRATE 115200
#define UART_DEV_PATTERN "/dev/ttyUSB*"

int main() {
	vector<string> usb_devs = glob(UART_DEV_PATTERN);
	cout << "INFO: USB_Logger: usb_devs:" << endl;
	for(const auto usb_dev: usb_devs){
		cout << "\t" << usb_dev << endl;
	}

	vector<shared_ptr<UART>> serials;
	for(auto usb_dev: usb_devs){
		auto s = new UART(
			usb_dev,
			BAUDRATE
		);
		serials.emplace_back(s);
	}
	if(serials.empty()){
		cout << "WARN: USB_Logger: No any USB UART dev plugged in!" << endl;
		return 0;
	}
	
	shared_ptr<UART> serial = serials[serials.size()-1];
	
	while(true){
		//uint8_t x;
		char x;
		serial->read(x);

		//nase
 
		cout << x << endl;

//nasekraj
		
	}
	
	return 0;
}
