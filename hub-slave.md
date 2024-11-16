# Párování slave s hubem

Pokud je slave úplně nový, tak je potřeba zmáčknout `pairing button` na 3 sekundy čímž ho dáme do párovacího módu. Kde jakmile přijmé první zprávu `ping` od hubu, tak se párování ukončí a slave bude přijímat kalibrační data pouze od hubu.

### Adresy

No yaml souboru je potřeba nastavit správnou adresu slave zařízení v globals sekci yaml souboru.

```yaml
- id: own_address
  type: uint64_t
  initial_value: '0xccd1917c5824'
  restore_value: no
```

Adresu zařízení lze zjistit z logů espnow komunikace, kde se vypíše adresa slave zařízení.

```yaml
[C][espnow:039]:   Own Peer Address: 0xccd1917c5824.
```

## Zapojení

### Spektrálního senzoru

![Schéma zapojení spektrálního senzoru](images/SlaveCircuitDiagram.png)

### Půdního senzoru

![Schéma zapojení půdního senzoru](images/SoilSlave_CircuitDiagram.png)

# Komunikace

### Poslání kalibračních hodnot

1. 1x se zmáčkne tlačítko `Get calibration data`, pokud se nezobrazí všechny hodnoty, tak se zmáčkne tlačítko znovu.
2. Počká se ať že všechny hodnoty aktualizují
3. Úpraví se hodnoty a poté se zmáčkne tlačíkot `Send calibration data`
