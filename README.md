# Komplice

K kompilaci kódu je potřeba mít soubor `secrets.yaml` s následujícím obsahem:

```yaml
WiFi_ssid: secret_ssid
WiFi_password: secret_password
espnow_channel: 6 # 1-13 based on your WiFi channel
web_username: web_username
web_password: web_password
```

## Nahrání kódu

Pokud je vysílač v deep sleep modu, tak na něj nemůžeme nahrát kód, proto jej musíme odpojit od USB, a potom zárověň se zmáčknutým tlačítkem BOOT (B) jej připojit k počítači a držet tlačítko do té doby dokud nezačne nahrávání kódu.

## ESP NOW knihovna

Dokumentace k ESP NOW [knihovně](https://github.com/esphome/esphome-docs/pull/4086/files?short_path=ab1e072#diff-ab1e072d37305b336bc6e28977672a2afbcc6d0aef984d5e18b4a660aa4a2681)
K posílání dat není potřeba psaní žádný MAC adress jelikož "peer" se automaticky přidá jelikož je zapnutá funkce:

```yaml
auto_add_peer: true
```

Kanál na přes které komunikuje espnow musí být stejný jako wifi kanál:

```yaml
espnow:
  wifi_channel: '11'
```

Kanál WiFi zjistíme po nahrátí kodů v logu zpráv:

```yaml
[C][wifi:447]:   Channel: 11
```

# Vymazání flash paměti

```
python -m esptool --port COMX erase_flash
```

# [Další informace](hub-slave.md)
