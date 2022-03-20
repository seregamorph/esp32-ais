# esp32-ais
## Automatic Identification System receiver ##

### Постановка задачи ###
Хочется иметь возможность в режиме реального времени видеть на карте как координаты своего местонахождения с треком движения,
так и координаты и курсы судов, находящихся в непосредственной близости до нескольких десятков км от нас.
В качестве устройства отображения использовать как сам приемник с тач дисплеем, так и возможность подключить его через BT/Wi-Fi/USB к планшету/смартфону.
При этом устройство (приемник) должно быть автономным и компактным, не тяжелым, с возможностью ношения в кармане одежды.
Иметь возможность работы от собственного аккумулятора до нескольких часов без подзарядки.

Разработка на базе платформы ESP32 как достаточно "продвинутого" чипа.
В качестве ВЧ-части для приема сигнала AIS используется чип Si4x6x или NRX1 от Radiometrix (требует внешнего демодулятора сигнала GMSK).
Для определения собственных координат используется плата GPS аля GT-U7.

Тачскрин на базе контроллера ILI9486 3,5" 320x480 с тач чипом xpt2046 или аналог.
Для отрисовки интерфейса используется библиотека LVGL, портированная на ESP32 https://github.com/lvgl/lv_port_esp32

#### Общая компонентная схема приемника AIS ####
![Компонентная схема приемника AIS](/img/Schematic_component_receiver_AIS%20v0.1.png)

### Первопроходцы ###
В качестве вдохновения для реализации были взяты такие проекты, как:
1. <https://github.com/cmbahadir/AIS-Receiver> AIS приемник на ESP32 и Si4464 с разводкой печатных плат
2. <https://github.com/peterantypas/maiana>  открытый проект для создание приемо-передатчика AIS
3. <https://github.com/astuder/dAISy> бюджетный приемник AIS, обсуждение <https://forum.43oh.com/topic/4833-potm-daisy-a-simple-ais-receiver/>
4. <http://san.net.ru/ais/receiver-add.html> AIS приемник на чипе SI4463, готовый модуль E30-170T20D
5. <https://github.com/IDeTIC-AIS/RX-AIS> AIS приемник на базе чипа RX1 от Radiometrix и Arduino

### Дополнительно: ###
* <https://github.com/ttlappalainen/NMEA2000>  библиотека NMEA2000 C++
* <https://gpsd.gitlab.io/gpsd/AIVDM.html> описание протокола AIS
* <http://ais.tbsalling.dk/> AIS декодер онлайн
* <https://www.aggsoft.com/ais-decoder.htm> AIS декодер онлайн
* <https://www.maritec.co.za/aisvdmvdodecoding> AIS декодер онлайн


### TODO: ###
1. Добавить сенсор электронного компаса, см чипы Bosch BMM150/BNO055, HMC5883L, LSM303, AK09912, готовые сборки аля GY-521, GY-273

### Выкачивание репозитория
```shell
git clone --recurse-submodules -j8 git@github.com:seregamorph/esp32-ais.git
```

### Обновление из репозитория
```shell
git pull origin master
git submodule update --recursive
```

### Синхронизация субмодулей
```shell
git submodule sync
git add components/lvgl_esp32_drivers
git commit
```
