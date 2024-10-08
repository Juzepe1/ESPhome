## Nahrání kódu
Pokud je vysílač v deep sleep modu, tak na něj nemůžeme nahrát kód, proto jej musíme odpojit od USB, a potom zárověň se zmáčknutým tlačítkem BOOT (B) jej připojit k počítači a držet tlačítko do té doby dokud nezačne nahrávání kódu.
## Nepřijímání dat
Pokud zapneme dříve vysílač než přijímáč, tak vysílač nemá nikam poslat data o proto počká 5 minut a zkusí to znovu. Pokud máme již připojený přijímač, tak abychom nemuseli čekat 5 minut, tak nám stačí zapnout a vypnout vysílač a data se poté objeví během několika sekund.
## Problémy při komplici
K nahrádí kódu ```1-reciever.yaml``` je potřeba python s verzí 3.12.7 a nižší.
