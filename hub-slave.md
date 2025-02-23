# Párování slave s hubem

Pokud je slave úplně nový, tak je potřeba zmáčknout `pairing button` na 3 sekundy čímž ho dáme do párovacího módu. Kde jakmile přijmé první zprávu `ping` od hubu, tak se párování ukončí a slave bude přijímat kalibrační data pouze od hubu.

## Zapojení

- [0-voltage-measurement](https://app.cirkitdesigner.com/project/3780e226-5e21-4d36-84a3-0c12a56706f7)

- [2-slave-spectral](https://app.cirkitdesigner.com/project/a8899e32-4640-4934-a5bc-0820507179b0)

- [2-slave-spectral-one-button](https://app.cirkitdesigner.com/project/459e2cbb-6655-4559-96c5-b4b0752eb2f2)

- [3-slave-soil](https://app.cirkitdesigner.com/project/c622e81d-66be-4af2-9979-d96a66a5f295)

- [4-slave-air](https://app.cirkitdesigner.com/project/7ada8e99-9087-4b3d-a3a3-c0be8d904bd8)

# Komunikace

### Poslání kalibračních hodnot

1. 1x se zmáčkne tlačítko `Get calibration data`, pokud se nezobrazí všechny hodnoty, tak se zmáčkne tlačítko znovu.
2. Počká se ať že všechny hodnoty aktualizují
3. Úpraví se hodnoty a poté se zmáčkne tlačíkot `Send calibration data`
