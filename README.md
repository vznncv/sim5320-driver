# SIM5320 interface library

The SIM5320 driver library for mbed-os.

![SIM5320 board](images/board.jpg)

The SIM5320 documentation can be found here:

- [Hardware description](https://simcom.ee/documents/SIM5320/SIM5320_Hardware_Design_V1.07.pdf)
- [AT command set](https://simcom.ee/documents/SIM5320/SIMCOM_SIM5320_ATC_EN_V2.05.pdf)
- [Other documentation and examples](https://simcom.ee/documents/?dir=SIM5320)

The library allows:

- get GPS coordinates and current UTC time
- send/receive SMS
- resolve host names
- establish TCP connections
- establish UPD connections
- work with FTP/FTPS servers

The library is compatible with a mbed-os 5.13.4 or higher.

## Driver usage

Typical library usage consists of the following steps:

1. Initialization:
   
   1. create `SIM5320` driver instances;
   2. optionally invoke `SIM5320::reset` method to reset device.
   3. invoke `SIM5320::init`. This method checks that device works, and sets some default settings;

2. Usage:

   1. invoke `SIM5320::request_to_start` or `SIM5320CellularDevice::init` method directly;
      It will set the device into full functionality mode.
   2. use GPS/network to do some things
   3. invoke `SIM5320::request_to_stop` or `SIM5320CellularDevice::shutdown` method directly.
      It will set device into low power mode. But in this mode it cannot use network or GPS;

The examples of the GPS/FTP/network/sms usage can be found in the `examples` directory.

## Troubleshooting

If after some AT commands the UART interface configuration was changed and it doesn't work,
you can reset device to factory settings, using the following steps:

1. connect device to PC using usb (usually boards have micro connector);
2. find virtual COM device for AT commands. The PC can find up to 6 virtual com ports,
   so you probably should check each port using baud rate 115200 and `AT` or `AT+CFUN?`;
   commands to check if it's AT command interface.
3. send `AT&F1` command to reset device to factory settings;
4. restart device manually.

## Tests

To run library tests you should:

1. create an empty test project
2. connect modem to you board
3. fill "sim5320-driver.test_*" settings in the you "mbed_app.json"
4. run `mbed test --greentea --tests-by-name "sim5320-driver-tests-*"`
