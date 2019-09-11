if not "%1" == "rebuild"  goto make
make.exe clean
make.exe all
goto end

:make
make.exe %1

:end