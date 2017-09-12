[![Build Status](https://travis-ci.org/bofh69/AiLight.svg?branch=master)](https://travis-ci.org/bofh69/AiLight)

This is a fork of AiLight that sets the lamp's colour from the results from holfuy weather stations.

## Status
The code works, but it is a bit rough.

## Building
Install atom and with atom install the platformio-ide plugin.

Or without atom:
Install python, run:
```sh
pip install -U platformio
```

Install npm (or node.js). In the project folder run:
```sh
npm update
npm install -g gulp-cli
npm install
```

Copy platformio.exmaple.ini to platform.ini and update it as needed.
Copy src/config.example.h to src/config.h and update it as needed.

Open the project's dir in atom and build it via the platform.io plugin or build it on the commandline with:
```sh
platformio run -e dev-ota
```
or to build & upload via ota:
```sh
platformio run -e dev-ota -t upload
```


## Testing

This bash command is used to simulate the response from a holfuy server:
```
while true; do
  echo -e 'HTTP/1.1 200 OK\r\n\r\n101,TestStation,'`date -I`,`date +%H:%M:%S`',3,4,m/s,10,20.1,C,56.8,1019,0 ' |
  sudo nc -n -vvv -l -p 80
done
```

## Credits and License

The **AiLight** Firmware is open-sourced software licensed under the [MIT license](http://opensource.org/licenses/MIT). For the full copyright and license information, please see the <license> file that was distributed with this source code.</license>
