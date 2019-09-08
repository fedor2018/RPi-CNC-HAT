#!/bin/bash

#update the next one to fit your path, 
#it does work for me executed from this config dir
export DEBUG=5
halrun -U

realtime start

pyvcp -c pyvcp iotest.xml &

#increase the sleep if you have a very slow machine
sleep 5

halcmd -f < ./testpanel.hal
#sleep 3
#halrun -U


