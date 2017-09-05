[![Build Status](https://travis-ci.org/bofh69/AiLight.svg?branch=master)](https://travis-ci.org/bofh69/AiLight)

This is a fork of AiLight that is supposed to set the lamp's colour from
the results from weather stations.

The initial version will only support Holfuy weather stations.

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
