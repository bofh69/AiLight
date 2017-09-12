[![Build Status](https://travis-ci.org/bofh69/AiLight.svg?branch=master)](https://travis-ci.org/bofh69/AiLight)

This is a fork of AiLight that sets the lamp's colour from the results from holfuy weather stations.

## Status
The code works, but it is a bit rough.

## Setup & Building

### Prerequisites

Install atom and within atom install the platformio-ide plugin.

Platformio can also be installed without Atom. Install python (2.7) and run:
```sh
pip install -U platformio
```

The project's web pages are packaged with gulp, it is done automatically when
the platformio builds the project, but needed modules needs to be installed.

Install npm (it also comes with node.js).

In the project folder run:
```sh
npm update
npm install -g gulp-cli
npm install
```

Copy platformio.exmaple.ini to platform.ini and update it as needed.
Copy src/config.example.h to src/config.h and update it as needed.

### Building the project.

Open the project's dir in Atom and build it via the platformio-ide plugin or build it on the commandline with:
```sh
platformio run -e dev-ota
```

The firmware can be build and uploaded over the air. To do so chose "Run other target..." and "PIO Upload (dev-ota)" or on the command line:
```sh
platformio run -e dev-ota -t upload
```

## Development & Testing

While developing, it is a good idea to always startup with the new module disabled. If the code crashes the OTA functionality will stop as well, forcing you to upload via a cable which can be a PIA.

In main.ino there is a setup function. After EEPROM_read(cfg); add:
```c
  cfg.holfuy_enabled = false;
```
You have to turn it back on after each restart, but you're less likely to brick the device.

If you want any custom settings while developing, put them in src/config.h.


This bash command is used to simulate the response from a holfuy server:
```
while true; do
  echo -e 'HTTP/1.1 200 OK\r\n\r\n101,TestStation,'`date -I`,`date +%H:%M:%S`',3,4,m/s,10,20.1,C,56.8,1019,0 ' |
  sudo nc -n -vvv -l -p 80
done
```

## Credits and License

The **AiLight** Firmware is open-sourced software licensed under the [MIT license](http://opensource.org/licenses/MIT). For the full copyright and license information, please see the <license> file that was distributed with this source code.</license>
