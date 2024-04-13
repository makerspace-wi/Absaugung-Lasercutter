# Absaugung-Lasercutter
Projekttagebuch und Dokumentation zum Bau einer Absauge für den blauen Laser Cutter des Makerspace Wiesbaden
# Neuanfang im September 2023
## April 2024
Nun kann man schon was erkennen - das Prokekt nimmt 'Formen an'.<br><br>
![IMG_4083](https://github.com/makerspace-wi/Absaugung-Lasercutter/assets/42463588/7caaabd1-d47c-443e-bb37-2cc0cce74798)
<br><br>
Im folgenden der Controller. Dieser soll über das MQTT-Protokoll die Druckdifferenz von 2 Filterstufen publizieren, damit unser System erkennen kann, wann Filter oder Aktivkohle gewechselt werden müssen. Der Controller kontrolliert ständig das STATUS-Signal des Laser Cutter Controllers und schaltet die Absauge ein, sobald ein Laser Job anläuft, bzw. wenn der Job beendet ist mit 60-Sekunden Nachlauf wieder ab. Der Controller gibt/regelt auch die Drehzahl des Radiallüfters im Bereich von 0 - 100%.<br><br>
![IMG_4047](https://github.com/makerspace-wi/Absaugung-Lasercutter/assets/42463588/85fd7a4b-b50c-48e3-a376-77f0b59643b4)
<br><br>
Unsere Lösung für den Aktivkohlefilter:
<br><br>
![20240227_000257](https://github.com/makerspace-wi/Absaugung-Lasercutter/assets/42463588/6fd59ab0-0fc3-42bd-ba91-f598edced4b1)
## März 2024
Was lange währt - wird endlich gut!

Es hat 3 Anläufe gebraucht, bis sich endlich in 2023 ein zuverlässiges, motiviertes, passioniertes Team gebildet hat. Das Ziel des Teams ist/war, eine Absaugvorrichtung für unseren 100W CO2 Laser Cutter zu bauen.
Diese soll wesentlich leiser sein, Partikel und Feinstaub der Rauchgase filtern und auch Gerüche minimieren (Aktivkohlefilter).
<br><br>
Die Auswahl für den Radiallüfter fiel auf folgendes Modell:
<img width="1201" alt="Bildschirmfoto 2024-03-24 um 10 43 13" src="https://github.com/makerspace-wi/Absaugung-Lasercutter/assets/42463588/28276b9f-f6ed-42b6-bb7f-64bb34a18ba6">
<br>Es handelt sich dabei um einen leise laufenden 3-Phasen Lüfter, der mittels Frequenzumrichter in der Drehzahl geregelt werden kann.
<br><br>
Im folgenden die erste Entwurfszeichnung (Copyright Orlando E.), gemäß den Vorstellungen des Technik-Teams:<br>
<img width="868" alt="Bildschirmfoto 2024-03-24 um 11 00 09" src="https://github.com/makerspace-wi/Absaugung-Lasercutter/assets/42463588/21055f00-2300-48db-9dea-afb4f44dce12">

<img src="https://user-images.githubusercontent.com/42463588/142736131-a84f6ed4-6690-4161-a7e6-33251ddabb04.jpg" width="400">


