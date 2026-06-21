# 🔋 UPS Monitoring via Arduino & OpenWrt (SNMP)

Проект для мониторинга ИБП: Arduino (через UART) -> OpenWrt (SNMP) -> Системы мониторинга (Zabbix/Prometheus).

## 📁 Структура
*   `arduino_mon/` — Исходный код C и Makefile.
    *   👉 [Инструкция по сборке бинарника](arduino_mon/README.md)
*   `README.md` — Данный файл.

---

## 🛠 Настройка SNMPD на OpenWrt 25+ (apk)

### 1. Установка
```bash
apk update
apk add snmpd
```

### 2. Конфигурация `/etc/config/snmpd`
Отредактируйте `/etc/config/snmpd`, добавив блок `config exec` для утилиты `/tmp/ups_reader` (OID `.1.3.6.1.4.1.2021.50`):

```ini
config snmpd general
        option enabled '1'

config agent
        option agentaddress UDP:161,UDP6:161

# ... (остальные базовые настройки: com2sec, group, view, access)

config exec
        option name     'ups'
        option prog     '/tmp/ups_reader'
        option miboid   '.1.3.6.1.4.1.2021.50'
```

### 3. Запуск
```bash
/etc/init.d/snmpd enable
/etc/init.d/snmpd restart
```

---

## 🔍 Проверка (snmpwalk)
Убедитесь, что `/tmp/ups_reader` существует и имеет права на исполнение.
```bash
snmpwalk -v 2c -c public <IP_РОУТЕРА> .1.3.6.1.4.1.2021.50
```
В OID `.1.3.6.1.4.1.2021.50.101.1` ожидается строка с данными (например, `ONLINE BATTV=13.2 LINEV=230`).
