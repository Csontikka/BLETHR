@Path=E:\Telink\SDK;E:\Telink\SDK\jre\bin;E:\Telink\SDK\opt\tc32\tools;E:\Telink\SDK\opt\tc32\bin;E:\Telink\SDK\usr\bin;E:\Telink\SDK\bin;%PATH%
@set SWVER=_bthr_v12
@del /Q "ATC%SWVER%.bin"
make -s -j PROJECT_NAME=ATC%SWVER% POJECT_DEF="-DDEVICE_TYPE=DEVICE_LYWSD03MMC"
@if not exist "ATC%SWVER%.bin" goto :error
@del /Q "LKTMZL02%SWVER%.bin"
make -s -j PROJECT_NAME=LKTMZL02%SWVER% POJECT_DEF="-DDEVICE_TYPE=DEVICE_LKTMZL02"
@if not exist "LKTMZL02%SWVER%.bin" goto :error
@del /Q "ZYZTH02P%SWVER%.bin"
make -s -j PROJECT_NAME=ZYZTH02P%SWVER% POJECT_DEF="-DDEVICE_TYPE=DEVICE_ZYZTH01"
@if not exist "ZYZTH02P%SWVER%.bin" goto :error
@exit
:error
echo "Error!"

         