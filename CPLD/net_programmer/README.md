# Сетевой JTAG программатор для Altera Quartus

Копия с [сайта Марсоход](https://marsohod.org/)

Ссылки:
- [Программирование ПЛИС платы M2RPI по сети из Quartus](https://marsohod.org/11-blog/364-rpi-m2rpi-programmer)
- [Сетевой JTAG программатор для Altera Quartus Prime из Raspberry Pi3](https://habr.com/ru/post/343524/)
- [Скачать FPGA проекты, документацию или драйвера](https://marsohod.org/downloads/category/26-narsohod2rpi)

Скрипт для работы через фаервол роутера:
```
#/bin/sh

IP=$(hostname -I)
upnpc -a $IP 8889 8889 tcp
upnpc -a $IP 8888 8888 udp
upnpc -l
```

На машине с Quartus-ом в каталоге bin64 файл jconfig.txt:
```
ipaddr0=ExternalIPAddress
```
 
