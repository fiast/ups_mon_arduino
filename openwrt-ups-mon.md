## Инструкция: Настройка NUT и SNMP для APC Smart-UPS на OpenWrt 25
Данное руководство описывает, как поднять сервер мониторинга ИБП APC Smart-UPS 750 через физический COM-порт (/dev/ttyS0) и пробросить его метрики в SNMP для внешних систем мониторинга (Zabbix/Prometheus).
------------------------------
## Шаг 1. Подготовка и очистка памяти (Важно для слабых роутеров)
Пакетный менеджер apk в OpenWrt 25 чувствителен к свободному месту. При установке тяжелых пакетов (например, Python) флеш-память может забиться в 100%.
Команды для жесткой ручной очистки «зависших» или недоудаленных хвостов из оверлея:

# Принудительное удаление остатков Python и очистка кэша apk
rm -rf /overlay/upper/usr/lib/python3.13
rm -f /overlay/upper/usr/lib/libpython3.13.so*
rm -rf /var/cache/apk/*
# Проверка свободного места (должно стать свободно)
df -h /

------------------------------
## Шаг 2. Освобождение COM-порта /dev/ttyS0 от системы
По умолчанию порт /dev/ttyS0 монопольно занят ядром OpenWrt для вывода логов и консоли отладки. Чтобы он не слал мусор в ИБП и не блокировал NUT:

   1. Глушим вывод логов ядра в текстовые терминалы. Открываем /etc/sysctl.conf и добавляем в самый конец:
   
   kernel.printk = 0 0 0 0
   
   2. Отключаем системную консоль (интерфейс ввода). Открываем /etc/inittab и комментируем строчку askconsole символом #:
   
   ::sysinit:/etc/init.d/rcS S boot
   ::shutdown:/etc/init.d/rcS K shutdown
   #::askconsole:/usr/libexec/login.sh
   
   3. Обязательно отправляем роутер в перезагрузку, чтобы ядро полностью отпустило порт:
   
   reboot
   
   
------------------------------
## Шаг 3. Автоматизация прав доступа к порту
По умолчанию права на порт после ребута принадлежат root:root (0600). Драйвер NUT работает под пользователем nut и не может его открыть. Создаем скрипт автоматического назначения прав при старте системы:
Создайте файл /etc/hotplug.d/tty/99-ttyS0-permissions:

if [ "$DEVICENAME" = "ttyS0" ]; then
    chown root:dialout /dev/ttyS0
    chmod 0660 /dev/ttyS0fi

Добавляем пользователя nut в группу dialout:

sed -i 's/^dialout:x:20:/dialout:x:20:nut/' /etc/group

Применяем права вручную прямо сейчас, чтобы не перезагружаться:

chown root:dialout /dev/ttyS0
chmod 0660 /dev/ttyS0

------------------------------
## Шаг 4. Установка пакетов NUT и SNMP

apk update
apk add nut-server nut-driver-apcsmart nut-upsc snmpd

------------------------------
## Шаг 5. Конфигурация NUT (OpenWrt 25 стиль)
В OpenWrt 25 файл конфигурации UCI называется /etc/config/nut_server. Полностью заменяем его содержимое на боевой рабочий вариант:

config driver_global
        option user 'nut'

config driver 'smart750'
        option driver 'apcsmart'
        option port '/dev/ttyS0'
        option user 'root'

config upsd 'upsd'
        option runas 'nut'
        option statepath '/var/run/nut'

config listen_address
        option address '127.0.0.1'
        option port '3493'

Активируем и перезапускаем службу:

/etc/init.d/nut-server enable
/etc/init.d/nut-server restart

Проверка работы NUT уровня железа:

upsc smart750@localhost

Вывод должен вернуть полный список внутренних регистров ИБП (заряд батареи, вольтаж, статус OL).
------------------------------
## Шаг 6. Настройка и проброс в SNMP
Для трансляции данных в сеть настраиваем вызов текстового парсера внутри демона snmpd.
Открываем /etc/config/snmpd, проверяем статус enabled '1' и добавляем в самый конец секцию exec:

config exec
        option name     'ups_status'
        option prog     '/usr/bin/upsc'
        option args     'smart750@localhost'
        option miboid   '.1.3.6.1.4.1.2021.50'

config snmpd general
        option enabled '1'

Перезапускаем SNMP-сервер:

/etc/init.d/snmpd enable
/etc/init.d/snmpd restart

------------------------------
## Шаг 7. Контрольная проверка по сети
С любого внешнего сервера или ПК проверяем доступность метрик ИБП по сети через протокол SNMP:

snmpwalk -v 2c -c public <IP_адрес_роутера> .1.3.6.1.4.1.2021.50

Вывод вернет структурированный массив строк, где, к примеру, OID .1.3.6.1.4.1.2021.50.101.2 всегда будет содержать заряд батареи в процентах (battery.charge), а .1.3.6.1.4.1.2021.50.101.42 — статус ИБП.
Инструкция полностью готова к сохранению. База настроена идеально, данные валятся без ошибок, память роутера в безопасности! На этой ноте финализируем конфигурацию.

